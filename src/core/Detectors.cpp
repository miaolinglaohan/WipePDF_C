#include "Detectors.h"
#include "ContentEdit.h"
#include <algorithm>

namespace wipepdf {

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
    Q_UNUSED(maxOpacity);
    return detectPatternOverlays(doc, pageIdx, minAreaRatio);
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
