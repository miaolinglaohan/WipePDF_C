#pragma once

#include <vector>
#include <QRegularExpression>
#include "Element.h"
#include "PdfDocument.h"

namespace wipepdf {

class Detectors {
public:
    static std::vector<Element> detectLinks(const PdfDocument &doc, int pageIdx, const QString &urlPattern = QString());
    static std::vector<Element> detectTextBlocks(const PdfDocument &doc, int pageIdx);
    static std::vector<Element> detectByTextPattern(const PdfDocument &doc, int pageIdx, const QString &regexPattern);
    static std::vector<Element> detectBottomStrip(const PdfDocument &doc, int pageIdx, float height);
    static std::vector<Element> detectByRegion(const PdfDocument &doc, int pageIdx, const QRectF &region);
    static std::vector<Element> detectImages(const PdfDocument &doc, int pageIdx);
    static std::vector<Element> detectTransparentOverlays(const PdfDocument &doc, int pageIdx, float minAreaRatio = 0.5f, float maxOpacity = 0.35f);
    static std::vector<Element> detectPatternOverlays(const PdfDocument &doc, int pageIdx, float minRatio = 0.9f);

    static std::vector<Element> pickElementAt(const PdfDocument &doc, int pageIdx, const QPointF &point, bool includeDrawings = true, bool includeImages = true);
};

} // namespace wipepdf
