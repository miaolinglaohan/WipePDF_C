#include "PdfDocument.h"


#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <cmath>

namespace wipepdf {

static inline QRectF fzRectToQRectF(const fz_rect &r) {
    return QRectF(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0);
}

static inline fz_rect qRectFToFzRect(const QRectF &q) {
    return fz_make_rect(static_cast<float>(q.left()),
                        static_cast<float>(q.top()),
                        static_cast<float>(q.right()),
                        static_cast<float>(q.bottom()));
}

PdfDocument::PdfDocument() = default;

PdfDocument::~PdfDocument() {
    close();
}

PdfDocument::PdfDocument(PdfDocument &&other) noexcept
    : m_filePath(std::move(other.m_filePath)),
      m_ctx(other.m_ctx),
      m_fzDoc(other.m_fzDoc),
      m_pdfDoc(other.m_pdfDoc),
      m_pageCount(other.m_pageCount) {
    other.m_ctx = nullptr;
    other.m_fzDoc = nullptr;
    other.m_pdfDoc = nullptr;
    other.m_pageCount = 0;
}

PdfDocument &PdfDocument::operator=(PdfDocument &&other) noexcept {
    if (this != &other) {
        close();
        m_filePath = std::move(other.m_filePath);
        m_ctx = other.m_ctx;
        m_fzDoc = other.m_fzDoc;
        m_pdfDoc = other.m_pdfDoc;
        m_pageCount = other.m_pageCount;

        other.m_ctx = nullptr;
        other.m_fzDoc = nullptr;
        other.m_pdfDoc = nullptr;
        other.m_pageCount = 0;
    }
    return *this;
}

bool PdfDocument::open(const QString &filePath, QString *errorMsg) {
    close();

    m_ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!m_ctx) {
        if (errorMsg) *errorMsg = "Failed to initialize MuPDF context";
        return false;
    }
    fz_register_document_handlers(m_ctx);

    m_filePath = filePath;
    QByteArray utf8Path = filePath.toUtf8();

    fz_try(m_ctx) {
        m_fzDoc = fz_open_document(m_ctx, utf8Path.constData());
    } fz_catch(m_ctx) {
        if (errorMsg) *errorMsg = QString::fromUtf8(fz_caught_message(m_ctx));
        close();
        return false;
    }

    m_pdfDoc = pdf_document_from_fz_document(m_ctx, m_fzDoc);
    if (!m_pdfDoc) {
        if (errorMsg) *errorMsg = "Opened document is not a valid PDF document";
        close();
        return false;
    }

    m_pageCount = fz_count_pages(m_ctx, m_fzDoc);
    return true;
}

void PdfDocument::close() {
    if (m_ctx) {
        if (m_fzDoc) {
            fz_drop_document(m_ctx, m_fzDoc);
            m_fzDoc = nullptr;
            m_pdfDoc = nullptr;
        }
        fz_drop_context(m_ctx);
        m_ctx = nullptr;
    }
    m_pageCount = 0;
    m_filePath.clear();
}

bool PdfDocument::isOpen() const {
    return m_ctx != nullptr && m_pdfDoc != nullptr;
}

int PdfDocument::pageCount() const {
    return m_pageCount;
}

QRectF PdfDocument::pageRect(int pageIdx) const {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return QRectF();

    fz_page *page = fz_load_page(m_ctx, m_fzDoc, pageIdx);
    if (!page) return QRectF();

    fz_rect r = fz_bound_page(m_ctx, page);
    fz_drop_page(m_ctx, page);
    return fzRectToQRectF(r);
}

QImage PdfDocument::renderPage(int pageIdx, float dpi, int targetWidth) const {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return QImage();

    fz_page *page = fz_load_page(m_ctx, m_fzDoc, pageIdx);
    if (!page) return QImage();

    fz_rect bounds = fz_bound_page(m_ctx, page);
    float pageWidth = bounds.x1 - bounds.x0;
    float zoom = 1.0f;
    if (targetWidth > 0 && pageWidth > 0.0f) {
        zoom = static_cast<float>(targetWidth) / pageWidth;
    } else {
        zoom = dpi / 72.0f;
    }

    fz_matrix ctm = fz_scale(zoom, zoom);
    fz_pixmap *pix = fz_new_pixmap_from_page(m_ctx, page, ctm, fz_device_rgb(m_ctx), 0);
    if (!pix) {
        fz_drop_page(m_ctx, page);
        return QImage();
    }

    int w = fz_pixmap_width(m_ctx, pix);
    int h = fz_pixmap_height(m_ctx, pix);
    int stride = fz_pixmap_stride(m_ctx, pix);
    unsigned char *samples = fz_pixmap_samples(m_ctx, pix);

    QImage img(samples, w, h, stride, QImage::Format_RGB888);
    QImage result = img.copy();

    fz_drop_pixmap(m_ctx, pix);
    fz_drop_page(m_ctx, page);
    return result;
}

std::vector<TextSpan> PdfDocument::getTextBlocks(int pageIdx) const {
    std::vector<TextSpan> spans;
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return spans;

    fz_page *page = fz_load_page(m_ctx, m_fzDoc, pageIdx);
    if (!page) return spans;

    fz_stext_page *stext = fz_new_stext_page_from_page(m_ctx, page, NULL);
    if (stext) {
        for (fz_stext_block *block = stext->first_block; block; block = block->next) {
            if (block->type == FZ_STEXT_BLOCK_TEXT) {
                for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
                    QString lineText;
                    for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
                        if (ch->c > 0) {
                            lineText.append(QChar(ch->c));
                        }
                    }
                    if (!lineText.trimmed().isEmpty()) {
                        spans.push_back({fzRectToQRectF(line->bbox), lineText});
                    }
                }
            }
        }
        fz_drop_stext_page(m_ctx, stext);
    }
    fz_drop_page(m_ctx, page);
    return spans;
}

QString PdfDocument::getTextInRect(int pageIdx, const QRectF &rect) const {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return QString();

    fz_page *page = fz_load_page(m_ctx, m_fzDoc, pageIdx);
    if (!page) return QString();

    QString result;
    fz_stext_page *stext = fz_new_stext_page_from_page(m_ctx, page, NULL);
    if (stext) {
        for (fz_stext_block *block = stext->first_block; block; block = block->next) {
            if (block->type == FZ_STEXT_BLOCK_TEXT) {
                for (fz_stext_line *line = block->u.t.first_line; line; line = line->next) {
                    QRectF lineRect = fzRectToQRectF(line->bbox);
                    if (rect.intersects(lineRect)) {
                        for (fz_stext_char *ch = line->first_char; ch; ch = ch->next) {
                            if (ch->c > 0) {
                                result.append(QChar(ch->c));
                            }
                        }
                        result.append('\n');
                    }
                }
            }
        }
        fz_drop_stext_page(m_ctx, stext);
    }
    fz_drop_page(m_ctx, page);
    return result.trimmed();
}

std::vector<Element> PdfDocument::getLinks(int pageIdx) const {
    std::vector<Element> elements;
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return elements;

    fz_page *page = fz_load_page(m_ctx, m_fzDoc, pageIdx);
    if (!page) return elements;

    fz_rect pbounds = fz_bound_page(m_ctx, page);
    float pArea = (pbounds.x1 - pbounds.x0) * (pbounds.y1 - pbounds.y0);

    fz_link *links = fz_load_links(m_ctx, page);
    for (fz_link *link = links; link; link = link->next) {
        if (!link->uri) continue;
        QRectF bbox = fzRectToQRectF(link->rect);
        Element el;
        el.type = ElementType::Link;
        el.page = pageIdx;
        el.bbox = bbox;
        el.url = QString::fromUtf8(link->uri);
        if (pArea > 0.0f) {
            el.rect_ratio = static_cast<float>(bbox.width() * bbox.height()) / pArea;
        }
        elements.push_back(el);
    }
    fz_drop_link(m_ctx, links);
    fz_drop_page(m_ctx, page);
    return elements;
}

bool PdfDocument::deleteLink(int pageIdx, const QRectF &rect) {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return false;

    pdf_page *page = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
    if (!page) return false;

    bool deleted = false;
    pdf_annot *annot = pdf_first_annot(m_ctx, page);
    while (annot) {
        pdf_annot *next = pdf_next_annot(m_ctx, annot);
        if (pdf_annot_type(m_ctx, annot) == PDF_ANNOT_LINK) {
            fz_rect r = pdf_bound_annot(m_ctx, annot);
            QRectF annotRect = fzRectToQRectF(r);
            if (std::abs(annotRect.x() - rect.x()) < 2.0 &&
                std::abs(annotRect.y() - rect.y()) < 2.0 &&
                std::abs(annotRect.width() - rect.width()) < 2.0 &&
                std::abs(annotRect.height() - rect.height()) < 2.0) {
                pdf_delete_annot(m_ctx, page, annot);
                deleted = true;
            }
        }
        annot = next;
    }
    pdf_drop_page(m_ctx, page);
    return deleted;
}

int PdfDocument::deleteAllLinks(int pageIdx, const QString &urlRegex) {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return 0;

    pdf_page *page = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
    if (!page) return 0;

    QRegularExpression re(urlRegex);
    bool useRegex = !urlRegex.isEmpty();

    int count = 0;
    pdf_annot *annot = pdf_first_annot(m_ctx, page);
    while (annot) {
        pdf_annot *next = pdf_next_annot(m_ctx, annot);
        if (pdf_annot_type(m_ctx, annot) == PDF_ANNOT_LINK) {
            bool shouldDelete = true;
            if (useRegex) {
                pdf_obj *obj = pdf_annot_obj(m_ctx, annot);
                pdf_obj *action = pdf_dict_get(m_ctx, obj, PDF_NAME(A));
                const char *uri = pdf_dict_get_string(m_ctx, action, PDF_NAME(URI), NULL);
                if (uri) {
                    QString sUri = QString::fromUtf8(uri);
                    shouldDelete = re.match(sUri).hasMatch();
                } else {
                    shouldDelete = false;
                }
            }
            if (shouldDelete) {
                pdf_delete_annot(m_ctx, page, annot);
                count++;
            }
        }
        annot = next;
    }
    pdf_drop_page(m_ctx, page);
    return count;
}

std::vector<Element> PdfDocument::getImages(int pageIdx) const {
    std::vector<Element> elements;
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return elements;

    fz_page *page = fz_load_page(m_ctx, m_fzDoc, pageIdx);
    if (!page) return elements;

    fz_rect pbounds = fz_bound_page(m_ctx, page);
    float pArea = (pbounds.x1 - pbounds.x0) * (pbounds.y1 - pbounds.y0);

    fz_stext_options opts;
    fz_init_stext_options(m_ctx, &opts);
    opts.flags = FZ_STEXT_PRESERVE_IMAGES;

    fz_stext_page *stext = fz_new_stext_page_from_page(m_ctx, page, &opts);
    if (stext) {
        pdf_page *ppage = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
        pdf_obj *res = ppage ? pdf_page_resources(m_ctx, ppage) : nullptr;
        pdf_obj *xobjs = res ? pdf_dict_get(m_ctx, res, PDF_NAME(XObject)) : nullptr;
        int numXobjs = xobjs ? pdf_dict_len(m_ctx, xobjs) : 0;

        for (fz_stext_block *block = stext->first_block; block; block = block->next) {
            if (block->type == FZ_STEXT_BLOCK_IMAGE) {
                QRectF bbox = fzRectToQRectF(block->bbox);
                Element el;
                el.type = ElementType::Image;
                el.page = pageIdx;
                el.bbox = bbox;
                if (pArea > 0.0f) {
                    el.rect_ratio = static_cast<float>(bbox.width() * bbox.height()) / pArea;
                }

                if (block->u.i.image && xobjs) {
                    unsigned char blockDigest[16] = {0};
                    fz_image_digest(m_ctx, block->u.i.image, blockDigest);
                    for (int xi = 0; xi < numXobjs; ++xi) {
                        pdf_obj *val = pdf_dict_get_val(m_ctx, xobjs, xi);
                        pdf_obj *subtype = pdf_dict_get(m_ctx, val, PDF_NAME(Subtype));
                        if (pdf_name_eq(m_ctx, subtype, PDF_NAME(Image))) {
                            fz_image *xim = nullptr;
                            fz_try(m_ctx) {
                                xim = pdf_load_image(m_ctx, m_pdfDoc, val);
                            } fz_catch(m_ctx) {
                                xim = nullptr;
                            }
                            if (xim) {
                                unsigned char ximDigest[16] = {0};
                                fz_image_digest(m_ctx, xim, ximDigest);
                                if (memcmp(blockDigest, ximDigest, 16) == 0) {
                                    el.xref = pdf_to_num(m_ctx, val);
                                    el.extra["xref"] = el.xref;
                                    fz_drop_image(m_ctx, xim);
                                    break;
                                }
                                fz_drop_image(m_ctx, xim);
                            }
                        }
                    }
                }

                elements.push_back(el);
            }
        }
        if (ppage) pdf_drop_page(m_ctx, ppage);
        fz_drop_stext_page(m_ctx, stext);
    }
    fz_drop_page(m_ctx, page);
    return elements;
}

bool PdfDocument::deleteImage(int pageIdx, int xref) {
    Q_UNUSED(pageIdx);
    if (!isOpen() || xref <= 0) return false;
    // Replace XObject with empty stream
    pdf_obj *obj = pdf_new_indirect(m_ctx, m_pdfDoc, xref, 0);
    if (obj) {
        fz_buffer *buf = fz_new_buffer(m_ctx, 0);
        pdf_update_stream(m_ctx, m_pdfDoc, obj, buf, 0);
        fz_drop_buffer(m_ctx, buf);
        pdf_drop_obj(m_ctx, obj);
        return true;
    }
    return false;
}

bool PdfDocument::addRedaction(int pageIdx, const QRectF &rect) {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return false;

    pdf_page *page = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
    if (!page) return false;

    pdf_annot *annot = pdf_create_annot(m_ctx, page, PDF_ANNOT_REDACT);
    if (annot) {
        fz_rect r = qRectFToFzRect(rect);
        pdf_set_annot_rect(m_ctx, annot, r);
        pdf_drop_page(m_ctx, page);
        return true;
    }
    pdf_drop_page(m_ctx, page);
    return false;
}

int PdfDocument::applyRedactions(int pageIdx, bool removeImages) {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return 0;

    pdf_page *page = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
    if (!page) return 0;

    pdf_redact_options opts = {0};
    opts.black_boxes = 0;
    opts.image_method = removeImages ? 1 : 0; // 1 = PDF_REDACT_IMAGE_REMOVE
    opts.line_art = 1;
    opts.text = 0;

    int res = pdf_redact_page(m_ctx, m_pdfDoc, page, &opts);
    pdf_drop_page(m_ctx, page);
    return res;
}

QByteArray PdfDocument::getPageContentStream(int pageIdx) const {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return QByteArray();

    pdf_page *page = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
    if (!page) return QByteArray();

    pdf_obj *contents = pdf_page_contents(m_ctx, page);
    if (!contents) {
        pdf_drop_page(m_ctx, page);
        return QByteArray();
    }

    QByteArray result;
    if (pdf_is_array(m_ctx, contents)) {
        int n = pdf_array_len(m_ctx, contents);
        for (int i = 0; i < n; ++i) {
            pdf_obj *item = pdf_array_get(m_ctx, contents, i);
            fz_buffer *buf = nullptr;
            fz_try(m_ctx) {
                buf = pdf_load_stream(m_ctx, item);
            } fz_catch(m_ctx) {
                buf = nullptr;
            }
            if (buf) {
                unsigned char *data = nullptr;
                size_t len = fz_buffer_storage(m_ctx, buf, &data);
                if (data && len > 0) {
                    if (!result.isEmpty()) result.append("\n");
                    result.append(reinterpret_cast<const char *>(data), static_cast<int>(len));
                }
                fz_drop_buffer(m_ctx, buf);
            }
        }
    } else {
        fz_buffer *buf = nullptr;
        fz_try(m_ctx) {
            buf = pdf_load_stream(m_ctx, contents);
        } fz_catch(m_ctx) {
            buf = nullptr;
        }

        if (buf) {
            unsigned char *data = nullptr;
            size_t len = fz_buffer_storage(m_ctx, buf, &data);
            if (data && len > 0) {
                result = QByteArray(reinterpret_cast<const char *>(data), static_cast<int>(len));
            }
            fz_drop_buffer(m_ctx, buf);
        }
    }

    pdf_drop_page(m_ctx, page);
    return result;
}

bool PdfDocument::setPageContentStream(int pageIdx, const QByteArray &bytes) {
    if (!isOpen() || pageIdx < 0 || pageIdx >= m_pageCount) return false;

    pdf_page *page = pdf_load_page(m_ctx, m_pdfDoc, pageIdx);
    if (!page) return false;

    pdf_obj *contents = pdf_page_contents(m_ctx, page);
    if (!contents) {
        pdf_drop_page(m_ctx, page);
        return false;
    }

    fz_buffer *buf = fz_new_buffer_from_copied_data(m_ctx, reinterpret_cast<const unsigned char *>(bytes.constData()), bytes.size());
    if (!buf) {
        pdf_drop_page(m_ctx, page);
        return false;
    }

    if (pdf_is_array(m_ctx, contents)) {
        int n = pdf_array_len(m_ctx, contents);
        if (n > 0) {
            pdf_obj *firstItem = pdf_array_get(m_ctx, contents, 0);
            pdf_update_stream(m_ctx, m_pdfDoc, firstItem, buf, 0);
            // Clear remaining streams in array so they don't produce leftover content
            for (int i = 1; i < n; ++i) {
                pdf_obj *otherItem = pdf_array_get(m_ctx, contents, i);
                fz_buffer *emptyBuf = fz_new_buffer(m_ctx, 0);
                pdf_update_stream(m_ctx, m_pdfDoc, otherItem, emptyBuf, 0);
                fz_drop_buffer(m_ctx, emptyBuf);
            }
        }
    } else {
        pdf_update_stream(m_ctx, m_pdfDoc, contents, buf, 0);
    }

    fz_drop_buffer(m_ctx, buf);
    pdf_drop_page(m_ctx, page);
    return true;
}

bool PdfDocument::save(const QString &outputPath, int garbage, bool deflate, bool clean, QString *errorMsg) {
    if (!isOpen()) {
        if (errorMsg) *errorMsg = "No document is currently open";
        return false;
    }

    pdf_write_options opts = {0};
    pdf_init_write_options(m_ctx, &opts);
    opts.do_garbage = garbage;
    opts.do_compress = deflate ? 1 : 0;
    opts.do_compress_images = deflate ? 1 : 0;
    opts.do_compress_fonts = deflate ? 1 : 0;
    opts.do_clean = clean ? 1 : 0;

    QByteArray outPathBytes = outputPath.toUtf8();
    fz_try(m_ctx) {
        pdf_save_document(m_ctx, m_pdfDoc, outPathBytes.constData(), &opts);
    } fz_catch(m_ctx) {
        if (errorMsg) *errorMsg = QString::fromUtf8(fz_caught_message(m_ctx));
        return false;
    }
    return true;
}

bool PdfDocument::saveAtomic(const QString &targetPath, int garbage, bool deflate, bool clean, QString *errorMsg) {
    QString tmpPath = targetPath + ".tmp";
    if (QFile::exists(tmpPath)) {
        QFile::remove(tmpPath);
    }

    if (!save(tmpPath, garbage, deflate, clean, errorMsg)) {
        return false;
    }

    // Crucial step from TASKBOOK.md: Release the open file lock before replacing on Windows!
    QString originalPath = m_filePath;
    close();

    if (QFile::exists(targetPath)) {
        if (!QFile::remove(targetPath)) {
            if (errorMsg) *errorMsg = QString("Failed to remove original file during replace: %1").arg(targetPath);
            return false;
        }
    }

    if (!QFile::rename(tmpPath, targetPath)) {
        if (errorMsg) *errorMsg = QString("Failed to rename temporary file to: %1").arg(targetPath);
        return false;
    }

    // Reopen document from replaced path
    return open(targetPath, errorMsg);
}

} // namespace wipepdf
