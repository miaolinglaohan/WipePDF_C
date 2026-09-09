#pragma once

#include <QString>
#include <QRectF>
#include <QRegularExpression>
#include <vector>
#include "Element.h"
#include "PdfDocument.h"

namespace wipepdf {

struct MatchRule {
    ElementType etype = ElementType::Unknown;
    QString textPattern;
    QString urlPattern;
    QRectF relativeRegion; // (rx, ry, rw, rh) in [0..1]
    float anchorX = -1.0f;  // [0..1], < 0 means unrestricted
    float anchorY = -1.0f;  // [0..1], < 0 means unrestricted
    float positionTolerance = 0.05f;
    float minRectRatio = 0.0f;
    float maxRectRatio = 1.0f;
    QString mode = "auto";

    QString describe() const {
        QStringList parts;
        parts << QString("Type: %1").arg(elementTypeToString(etype));
        if (!textPattern.isEmpty()) parts << QString("Text: /%1/").arg(textPattern);
        if (!urlPattern.isEmpty()) parts << QString("URL: /%1/").arg(urlPattern);
        if (anchorY >= 0.0f) parts << QString("Y-Center: %1±%2").arg(anchorY, 0, 'f', 2).arg(positionTolerance, 0, 'f', 2);
        if (!relativeRegion.isNull()) parts << QString("Region: [%1,%2,%3,%4]")
            .arg(relativeRegion.x(), 0, 'f', 2).arg(relativeRegion.y(), 0, 'f', 2)
            .arg(relativeRegion.width(), 0, 'f', 2).arg(relativeRegion.height(), 0, 'f', 2);
        return parts.join("; ");
    }
};

class Matcher {
public:
    static MatchRule createRuleFromElement(const PdfDocument &doc, int pageIdx, const Element &el, const QString &mode = "auto", float positionTolerance = 0.05f);
    static bool elementMatches(const Element &el, const MatchRule &rule, const QRectF &pageRect);
    static std::vector<Element> findMatches(const PdfDocument &doc, const MatchRule &rule, int pageIdx);
};

} // namespace wipepdf
