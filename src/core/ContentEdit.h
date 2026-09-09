#pragma once

#include <QByteArray>
#include <QPointF>
#include <QRectF>
#include <vector>
#include "PdfDocument.h"

namespace wipepdf {

struct ContentSegment {
    enum class Kind { Text, Fill };
    Kind kind = Kind::Text;
    int start = 0;
    int end = 0;
    int glyphCount = 0;
    QPointF pos;
    QRectF rect;
    bool patternFill = false;
};

class ContentEdit {
public:
    static std::vector<ContentSegment> parseSegments(const QByteArray &data, bool fourDigitCids = true);
    static QByteArray removeSegments(const QByteArray &data, const std::vector<ContentSegment> &segments);
    static int removeFullPagePatternFills(PdfDocument &doc, int pageIdx, float minRatio = 0.9f);
    static int removeTextInRegion(PdfDocument &doc, int pageIdx, const QRectF &region);
};

} // namespace wipepdf
