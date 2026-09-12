#include "Detectors.h"
#include "ContentEdit.h"
#include <algorithm>

namespace wipepdf {

namespace {

// ---- 透明覆盖层检测（对齐 Python 版 page.get_drawings 语义）----
// 通过自定义 fz_device 捕获矢量填充/描边操作：
// alpha 参数即 ExtGState (ca/CA) 透明度，正是 PyMuPDF get_drawings 的底层机制。

struct DrawingOp {
    fz_rect bbox;
    float alpha = 1.0f;
    bool rectLike = false; // 路径含 're' 或贝塞尔 'c' 段（PyMuPDF is_rect_like）
    bool isText = false;   // 是否为文本绘制
    QString text;
};

struct PathShape {
    bool rectLike = false;
};

void shapeMoveTo(fz_context *, void *, float, float) {}
void shapeLineTo(fz_context *, void *, float, float) {}
void shapeBezier(fz_context *, void *arg, float, float, float, float, float, float) {
    static_cast<PathShape *>(arg)->rectLike = true;
}
void shapeClose(fz_context *, void *) {}
void shapeQuadTo(fz_context *, void *, float, float, float, float) {} // 'qu' 段不计入 rect-like
void shapeCurveToV(fz_context *, void *arg, float, float, float, float) {
    static_cast<PathShape *>(arg)->rectLike = true;
}
void shapeCurveToY(fz_context *, void *arg, float, float, float, float) {
    static_cast<PathShape *>(arg)->rectLike = true;
}
void shapeRectTo(fz_context *, void *arg, float, float, float, float) {
    static_cast<PathShape *>(arg)->rectLike = true;
}

struct OverlayDevice {
    fz_device super;
    std::vector<DrawingOp> *ops; // 调用方持有（device 内存为 calloc，不能含 C++ 对象）
};

void collectPathShape(fz_context *ctx, const fz_path *path, PathShape &shape) {
    fz_path_walker walker;
    walker.moveto = shapeMoveTo;
    walker.lineto = shapeLineTo;
    walker.curveto = shapeBezier;
    walker.closepath = shapeClose;
    walker.quadto = shapeQuadTo;
    walker.curvetov = shapeCurveToV;
    walker.curvetoy = shapeCurveToY;
    walker.rectto = shapeRectTo;
    fz_walk_path(ctx, path, &walker, &shape);
}

void overlayFillPath(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd,
                     fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha,
                     fz_color_params color_params)
{
    (void)even_odd; (void)cs; (void)color; (void)color_params;
    PathShape shape;
    collectPathShape(ctx, path, shape);
    DrawingOp op;
    op.bbox = fz_bound_path(ctx, path, NULL, ctm);
    op.alpha = alpha;
    op.rectLike = shape.rectLike;
    reinterpret_cast<OverlayDevice *>(dev)->ops->push_back(op);
}

void overlayStrokePath(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke,
                       fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha,
                       fz_color_params color_params)
{
    (void)cs; (void)color; (void)color_params;
    PathShape shape;
    collectPathShape(ctx, path, shape);
    DrawingOp op;
    op.bbox = fz_bound_path(ctx, path, stroke, ctm);
    op.alpha = alpha;
    op.rectLike = shape.rectLike;
    reinterpret_cast<OverlayDevice *>(dev)->ops->push_back(op);
}

// 其余回调全部 no-op（保证 device 永不解引用 NULL 回调）
void ovClose(fz_context *, fz_device *) {}
void ovDrop(fz_context *, fz_device *) {}
void ovClipPath(fz_context *, fz_device *, const fz_path *, int, fz_matrix, fz_rect) {}
void ovClipStrokePath(fz_context *, fz_device *, const fz_path *, const fz_stroke_state *, fz_matrix, fz_rect) {}
static QString extractTextFromFzText(const fz_text *text) {
    if (!text) return QString();
    QString str;
    for (fz_text_span *span = text->head; span; span = span->next) {
        for (int i = 0; i < span->len; ++i) {
            int ucs = span->items[i].ucs;
            if (ucs > 0) {
                str.append(QChar(ucs));
            } else if (span->items[i].cid >= 32 && span->items[i].cid < 127) {
                str.append(QChar(span->items[i].cid));
            }
        }
    }
    return str;
}

void ovFillText(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
                fz_colorspace *cs, const float *color, float alpha, fz_color_params color_params)
{
    (void)cs; (void)color; (void)color_params;
    DrawingOp op;
    op.bbox = fz_bound_text(ctx, text, NULL, ctm);
    op.alpha = alpha;
    op.rectLike = true;
    op.isText = true;
    op.text = extractTextFromFzText(text);
    reinterpret_cast<OverlayDevice *>(dev)->ops->push_back(op);
}

void ovStrokeText(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke,
                  fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params color_params)
{
    (void)cs; (void)color; (void)color_params;
    DrawingOp op;
    op.bbox = fz_bound_text(ctx, text, stroke, ctm);
    op.alpha = alpha;
    op.rectLike = true;
    op.isText = true;
    op.text = extractTextFromFzText(text);
    reinterpret_cast<OverlayDevice *>(dev)->ops->push_back(op);
}
void ovClipText(fz_context *, fz_device *, const fz_text *, fz_matrix, fz_rect) {}
void ovClipStrokeText(fz_context *, fz_device *, const fz_text *, const fz_stroke_state *, fz_matrix, fz_rect) {}
void ovIgnoreText(fz_context *, fz_device *, const fz_text *, fz_matrix) {}
void ovFillShade(fz_context *, fz_device *, fz_shade *, fz_matrix, float, fz_color_params) {}
void ovFillImage(fz_context *, fz_device *, fz_image *, fz_matrix, float, fz_color_params) {}
void ovFillImageMask(fz_context *, fz_device *, fz_image *, fz_matrix, fz_colorspace *, const float *, float, fz_color_params) {}
void ovClipImageMask(fz_context *, fz_device *, fz_image *, fz_matrix, fz_rect) {}
void ovPopClip(fz_context *, fz_device *) {}
void ovBeginMask(fz_context *, fz_device *, fz_rect, int, fz_colorspace *, const float *, fz_color_params) {}
void ovEndMask(fz_context *, fz_device *, fz_function *) {}
void ovBeginGroup(fz_context *, fz_device *, fz_rect, fz_colorspace *, int, int, int, float) {}
void ovEndGroup(fz_context *, fz_device *) {}
int ovBeginTile(fz_context *, fz_device *, fz_rect, fz_rect, float, float, fz_matrix, int, int) { return 0; }
void ovEndTile(fz_context *, fz_device *) {}
void ovRenderFlags(fz_context *, fz_device *, int, int) {}
void ovSetDefaultColorspaces(fz_context *, fz_device *, fz_default_colorspaces *) {}
void ovBeginLayer(fz_context *, fz_device *, const char *) {}
void ovEndLayer(fz_context *, fz_device *) {}
void ovBeginStructure(fz_context *, fz_device *, fz_structure, const char *, int) {}
void ovEndStructure(fz_context *, fz_device *) {}
void ovBeginMetatext(fz_context *, fz_device *, fz_metatext, const char *) {}
void ovEndMetatext(fz_context *, fz_device *) {}

} // namespace

std::vector<Element> Detectors::detectLinks(const PdfDocument &doc, int pageIdx, const QString &urlPattern) {
    auto links = doc.getLinks(pageIdx);
    if (urlPattern.isEmpty()) {
        return links;
    }

    QRegularExpression re(urlPattern);
    std::vector<Element> filtered;
    for (const auto &el : links) {
        if (re.match(el.url).hasMatch()) {
            filtered.push_back(el);
        }
    }
    return filtered;
}

std::vector<Element> Detectors::detectTextBlocks(const PdfDocument &doc, int pageIdx) {
    std::vector<Element> elements;
    auto spans = doc.getTextBlocks(pageIdx);
    QRectF pRect = doc.pageRect(pageIdx);
    float pArea = pRect.width() * pRect.height();

    for (const auto &span : spans) {
        Element el;
        el.type = ElementType::Text;
        el.page = pageIdx;
        el.bbox = span.bbox;
        el.text = span.text;
        if (pArea > 0.0f) {
            el.rect_ratio = static_cast<float>(span.bbox.width() * span.bbox.height()) / pArea;
        }
        elements.push_back(el);
    }
    return elements;
}

std::vector<Element> Detectors::detectByTextPattern(const PdfDocument &doc, int pageIdx, const QString &regexPattern) {
    std::vector<Element> elements;
    if (regexPattern.isEmpty()) return elements;

    QRegularExpression re(regexPattern);
    auto allBlocks = detectTextBlocks(doc, pageIdx);
    for (const auto &el : allBlocks) {
        if (re.match(el.text).hasMatch()) {
            elements.push_back(el);
        }
    }
    return elements;
}

std::vector<Element> Detectors::detectBottomStrip(const PdfDocument &doc, int pageIdx, float height) {
    QRectF pRect = doc.pageRect(pageIdx);
    if (pRect.isEmpty()) return {};

    QRectF bottomRect(pRect.left(), pRect.bottom() - height, pRect.width(), height);
    return detectByRegion(doc, pageIdx, bottomRect);
}

std::vector<Element> Detectors::detectByRegion(const PdfDocument &doc, int pageIdx, const QRectF &region) {
    std::vector<Element> elements;

    auto textBlocks = detectTextBlocks(doc, pageIdx);
    for (const auto &el : textBlocks) {
        if (el.bbox.intersects(region)) {
            elements.push_back(el);
        }
    }

    auto links = detectLinks(doc, pageIdx);
    for (const auto &el : links) {
        if (el.bbox.intersects(region)) {
            elements.push_back(el);
        }
    }

    auto images = detectImages(doc, pageIdx);
    for (const auto &el : images) {
        if (el.bbox.intersects(region)) {
            elements.push_back(el);
        }
    }

    return elements;
}

std::vector<Element> Detectors::detectImages(const PdfDocument &doc, int pageIdx) {
    return doc.getImages(pageIdx);
}

std::vector<Element> Detectors::detectTransparentOverlays(const PdfDocument &doc, int pageIdx, float minAreaRatio, float maxOpacity) {
    std::vector<Element> elements;
    if (!doc.isOpen() || pageIdx < 0 || pageIdx >= doc.pageCount()) return elements;

    fz_context *ctx = doc.context();
    fz_page *page = fz_load_page(ctx, doc.fzDoc(), pageIdx);
    if (!page) return elements;

    fz_rect pbounds = fz_bound_page(ctx, page);
    float pArea = (pbounds.x1 - pbounds.x0) * (pbounds.y1 - pbounds.y0);

    std::vector<DrawingOp> ops;
    OverlayDevice *odev = nullptr;
    fz_try(ctx) {
        odev = fz_new_derived_device(ctx, OverlayDevice);
        odev->super.close_device = ovClose;
        odev->super.drop_device = ovDrop;
        odev->super.fill_path = overlayFillPath;
        odev->super.stroke_path = overlayStrokePath;
        odev->super.clip_path = ovClipPath;
        odev->super.clip_stroke_path = ovClipStrokePath;
        odev->super.fill_text = ovFillText;
        odev->super.stroke_text = ovStrokeText;
        odev->super.clip_text = ovClipText;
        odev->super.clip_stroke_text = ovClipStrokeText;
        odev->super.ignore_text = ovIgnoreText;
        odev->super.fill_shade = ovFillShade;
        odev->super.fill_image = ovFillImage;
        odev->super.fill_image_mask = ovFillImageMask;
        odev->super.clip_image_mask = ovClipImageMask;
        odev->super.pop_clip = ovPopClip;
        odev->super.begin_mask = ovBeginMask;
        odev->super.end_mask = ovEndMask;
        odev->super.begin_group = ovBeginGroup;
        odev->super.end_group = ovEndGroup;
        odev->super.begin_tile = ovBeginTile;
        odev->super.end_tile = ovEndTile;
        odev->super.render_flags = ovRenderFlags;
        odev->super.set_default_colorspaces = ovSetDefaultColorspaces;
        odev->super.begin_layer = ovBeginLayer;
        odev->super.end_layer = ovEndLayer;
        odev->super.begin_structure = ovBeginStructure;
        odev->super.end_structure = ovEndStructure;
        odev->super.begin_metatext = ovBeginMetatext;
        odev->super.end_metatext = ovEndMetatext;
        odev->ops = &ops;
        fz_run_page_contents(ctx, page, reinterpret_cast<fz_device *>(odev), fz_make_matrix(1, 0, 0, 1, 0, 0), NULL);
    }
    fz_always(ctx) {
        if (odev) {
            fz_close_device(ctx, reinterpret_cast<fz_device *>(odev));
            fz_drop_device(ctx, reinterpret_cast<fz_device *>(odev));
        }
    }
    fz_catch(ctx) {
        // 内容流损坏时保留已收集的部分结果
    }
    fz_drop_page(ctx, page);

    if (pArea <= 0.0f) return elements;

    for (const auto &op : ops) {
        if (!op.rectLike || op.alpha > maxOpacity) continue;
        fz_rect inter = fz_intersect_rect(op.bbox, pbounds);
        float area = (inter.x1 - inter.x0) * (inter.y1 - inter.y0);
        if (area <= 0.0f) continue;
        float ratio = area / pArea;

        // 对于低透明度矢量背景覆盖层，要求 ratio >= minAreaRatio（避免误伤常规微小线条）
        // 对于完全透明/低透明度的文字（如水印超链），不设苛刻面积下限（只要具有有效区域）
        if (!op.isText && ratio < minAreaRatio) continue;
        if (op.isText && ratio < 0.0005f) continue;

        Element el;
        el.type = op.isText ? ElementType::Text : ElementType::Drawing;
        el.page = pageIdx;
        el.bbox = QRectF(op.bbox.x0, op.bbox.y0, op.bbox.x1 - op.bbox.x0, op.bbox.y1 - op.bbox.y0);
        el.opacity = op.alpha;
        el.rect_ratio = ratio;
        el.extra["transparent_overlay"] = true;
        if (op.isText) {
            el.text = op.text.isEmpty() ? doc.getTextInRect(pageIdx, el.bbox).trimmed() : op.text.trimmed();
            el.extra["transparent_text"] = true;
        }
        elements.push_back(el);
    }
    return elements;
}

std::vector<Element> Detectors::detectPatternOverlays(const PdfDocument &doc, int pageIdx, float minRatio) {
    std::vector<Element> elements;
    QByteArray stream = doc.getPageContentStream(pageIdx);
    if (stream.isEmpty()) return elements;

    QRectF pRect = doc.pageRect(pageIdx);
    float pArea = pRect.width() * pRect.height();
    if (pArea <= 0.0f) return elements;

    auto segs = ContentEdit::parseSegments(stream);
    for (const auto &seg : segs) {
        if (seg.kind == ContentSegment::Kind::Fill && seg.patternFill) {
            float area = seg.rect.width() * seg.rect.height();
            float ratio = area / pArea;
            if (ratio >= minRatio) {
                Element el;
                el.type = ElementType::Drawing;
                el.page = pageIdx;
                el.bbox = seg.rect.isNull() ? pRect : seg.rect;
                el.rect_ratio = ratio;
                el.extra["pattern_fill"] = true;
                elements.push_back(el);
            }
        }
    }
    return elements;
}

std::vector<Element> Detectors::pickElementAt(const PdfDocument &doc, int pageIdx, const QPointF &point, bool includeDrawings, bool includeImages) {
    std::vector<Element> candidates;

    // 1. Links
    auto links = detectLinks(doc, pageIdx);
    for (const auto &el : links) {
        if (el.bbox.adjusted(-3, -3, 3, 3).contains(point)) {
            candidates.push_back(el);
        }
    }

    // 2. Text blocks
    auto textBlocks = detectTextBlocks(doc, pageIdx);
    for (const auto &el : textBlocks) {
        if (el.bbox.adjusted(-3, -3, 3, 3).contains(point)) {
            candidates.push_back(el);
        }
    }

    // 3. Images
    if (includeImages) {
        auto images = detectImages(doc, pageIdx);
        for (const auto &el : images) {
            if (el.bbox.adjusted(-4, -4, 4, 4).contains(point)) {
                candidates.push_back(el);
            }
        }
    }

    // 4. Drawings / Overlays
    if (includeDrawings) {
        auto drawings = detectPatternOverlays(doc, pageIdx, 0.1f);
        for (const auto &el : drawings) {
            if (el.bbox.adjusted(-3, -3, 3, 3).contains(point)) {
                candidates.push_back(el);
            }
        }
    }

    // Sort by bounding box area ascending (smaller items first for precise picking)
    std::sort(candidates.begin(), candidates.end(), [](const Element &a, const Element &b) {
        float areaA = a.bbox.width() * a.bbox.height();
        float areaB = b.bbox.width() * b.bbox.height();
        return areaA < areaB;
    });

    return candidates;
}

} // namespace wipepdf
