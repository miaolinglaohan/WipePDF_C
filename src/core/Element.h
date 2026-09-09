#pragma once

#include <QString>
#include <QRectF>
#include <QVariantMap>

namespace wipepdf {

enum class ElementType {
    Text,
    Link,
    Drawing,
    Image,
    Unknown
};

inline QString elementTypeToString(ElementType type) {
    switch (type) {
        case ElementType::Text: return "text";
        case ElementType::Link: return "link";
        case ElementType::Drawing: return "drawing";
        case ElementType::Image: return "image";
        default: return "unknown";
    }
}

inline ElementType stringToElementType(const QString &str) {
    if (str == "text") return ElementType::Text;
    if (str == "link") return ElementType::Link;
    if (str == "drawing") return ElementType::Drawing;
    if (str == "image") return ElementType::Image;
    return ElementType::Unknown;
}

struct Element {
    ElementType type = ElementType::Unknown;
    int page = 0;              // 0-based page index
    QRectF bbox;              // Normalized/Page coordinate rect [x0, y0, w, h]
    QString text;             // Text content if text
    QString url;              // Link target if link
    float rect_ratio = 0.0f;  // Area / Page area ratio
    float opacity = 1.0f;     // Opacity if transparent element
    int xref = 0;             // PDF object number (xref) if image/xobject
    QVariantMap extra;

    QString summary() const {
        QString s = QString("[%1] Page %2 BBox(%3, %4, %5, %6)")
            .arg(elementTypeToString(type))
            .arg(page + 1)
            .arg(bbox.x(), 0, 'f', 1)
            .arg(bbox.y(), 0, 'f', 1)
            .arg(bbox.width(), 0, 'f', 1)
            .arg(bbox.height(), 0, 'f', 1);
        if (!text.isEmpty()) {
            QString snippet = text.trimmed();
            if (snippet.length() > 30) snippet = snippet.left(30) + "...";
            s += QString(" Text: \"%1\"").arg(snippet);
        }
        if (!url.isEmpty()) {
            s += QString(" URL: \"%1\"").arg(url);
        }
        return s;
    }
};

} // namespace wipepdf
