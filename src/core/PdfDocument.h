#pragma once

#include <QString>
#include <QRectF>
#include <QImage>
#include <vector>
#include <memory>
#include "Element.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

namespace wipepdf {

struct TextSpan {
    QRectF bbox;
    QString text;
};

class PdfDocument {
public:
    PdfDocument();
    ~PdfDocument();

    // Prevent copying
    PdfDocument(const PdfDocument &) = delete;
    PdfDocument &operator=(const PdfDocument &) = delete;

    // Allow moving
    PdfDocument(PdfDocument &&other) noexcept;
    PdfDocument &operator=(PdfDocument &&other) noexcept;

    bool open(const QString &filePath, QString *errorMsg = nullptr);
    void close();
    bool isOpen() const;

    QString filePath() const { return m_filePath; }
    int pageCount() const;
    QRectF pageRect(int pageIdx) const;

    // Rendering
    QImage renderPage(int pageIdx, float dpi = 150.0f, int targetWidth = 0) const;

    // Text analysis
    std::vector<TextSpan> getTextBlocks(int pageIdx) const;
    QString getTextInRect(int pageIdx, const QRectF &rect) const;

    // Links
    std::vector<Element> getLinks(int pageIdx) const;
    bool deleteLink(int pageIdx, const QRectF &rect);
    int deleteAllLinks(int pageIdx, const QString &urlRegex = QString());

    // Images
    std::vector<Element> getImages(int pageIdx) const;
    bool deleteImage(int pageIdx, int xref);

    // Redactions
    bool addRedaction(int pageIdx, const QRectF &rect);
    int applyRedactions(int pageIdx, bool removeImages = false);

    // Raw content stream access
    QByteArray getPageContentStream(int pageIdx) const;
    bool setPageContentStream(int pageIdx, const QByteArray &bytes);

    // Save
    bool save(const QString &outputPath, int garbage = 4, bool deflate = true, bool clean = true, QString *errorMsg = nullptr);
    bool saveAtomic(const QString &targetPath, int garbage = 4, bool deflate = true, bool clean = true, QString *errorMsg = nullptr);

    // Low-level handle access
    fz_context *context() const { return m_ctx; }
    fz_document *fzDoc() const { return m_fzDoc; }
    pdf_document *pdfDoc() const { return m_pdfDoc; }

private:
    QString m_filePath;
    mutable fz_context *m_ctx = nullptr;
    mutable fz_document *m_fzDoc = nullptr;
    mutable pdf_document *m_pdfDoc = nullptr;
    int m_pageCount = 0;
};

} // namespace wipepdf
