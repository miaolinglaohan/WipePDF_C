#include "Matcher.h"
#include "Detectors.h"
#include <cmath>

namespace wipepdf {

MatchRule Matcher::createRuleFromElement(const PdfDocument &doc, int pageIdx, const Element &el, const QString &mode, float positionTolerance) {
    MatchRule rule;
    rule.etype = el.type;
    rule.mode = mode;
    rule.positionTolerance = positionTolerance;

    QRectF pRect = doc.pageRect(pageIdx);
    if (pRect.isEmpty()) return rule;

    float cx = (el.bbox.left() + el.bbox.right()) / 2.0f / pRect.width();
    float cy = (el.bbox.top() + el.bbox.bottom()) / 2.0f / pRect.height();

    QRectF relRegion(
        el.bbox.left() / pRect.width(),
        el.bbox.top() / pRect.height(),
        el.bbox.width() / pRect.width(),
        el.bbox.height() / pRect.height()
    );

    if (mode == "region") {
        rule.relativeRegion = relRegion;
        return rule;
    }

    if (mode == "position") {
        rule.anchorX = cx;
        rule.anchorY = cy;
        return rule;
    }

    if (mode == "text" && el.type == ElementType::Text) {
        rule.textPattern = QRegularExpression::escape(el.text.trimmed());
        return rule;
    }

    if (mode == "url" && el.type == ElementType::Link) {
        rule.urlPattern = QRegularExpression::escape(el.url.trimmed());
        return rule;
    }

    // Auto mode: combine characteristics
    if (el.type == ElementType::Text) {
        rule.textPattern = QRegularExpression::escape(el.text.trimmed());
        rule.anchorY = cy;
    } else if (el.type == ElementType::Link) {
        rule.urlPattern = QRegularExpression::escape(el.url.trimmed());
        rule.anchorY = cy;
    } else if (el.type == ElementType::Image) {
        rule.anchorX = cx;
        rule.anchorY = cy;
        rule.relativeRegion = relRegion;
    } else { // Drawing
        rule.relativeRegion = relRegion;
        rule.anchorY = cy;
    }

    return rule;
}

bool Matcher::elementMatches(const Element &el, const MatchRule &rule, const QRectF &pageRect) {
    if (el.type != rule.etype) return false;
    if (pageRect.isEmpty()) return false;

    // 1. Region match
    if (!rule.relativeRegion.isNull()) {
        QRectF absRegion(
            rule.relativeRegion.x() * pageRect.width(),
            rule.relativeRegion.y() * pageRect.height(),
            rule.relativeRegion.width() * pageRect.width(),
            rule.relativeRegion.height() * pageRect.height()
        );
        if (!el.bbox.intersects(absRegion)) {
            return false;
        }
    }

    // 2. Text match
    if (!rule.textPattern.isEmpty() && el.type == ElementType::Text) {
        QRegularExpression re(rule.textPattern);
        if (!re.match(el.text).hasMatch()) {
            return false;
        }
    }

    // 3. URL match
    if (!rule.urlPattern.isEmpty() && el.type == ElementType::Link) {
        QRegularExpression re(rule.urlPattern);
        if (!re.match(el.url).hasMatch()) {
            return false;
        }
    }

    // 4. Center Anchor match
    float cx = (el.bbox.left() + el.bbox.right()) / 2.0f / pageRect.width();
    float cy = (el.bbox.top() + el.bbox.bottom()) / 2.0f / pageRect.height();

    if (rule.anchorX >= 0.0f) {
        if (std::abs(cx - rule.anchorX) > rule.positionTolerance) return false;
    }
    if (rule.anchorY >= 0.0f) {
        if (std::abs(cy - rule.anchorY) > rule.positionTolerance) return false;
    }

    return true;
}

std::vector<Element> Matcher::findMatches(const PdfDocument &doc, const MatchRule &rule, int pageIdx) {
    std::vector<Element> matches;
    QRectF pRect = doc.pageRect(pageIdx);
    if (pRect.isEmpty()) return matches;

    std::vector<Element> candidates;
    if (rule.etype == ElementType::Link) {
        candidates = Detectors::detectLinks(doc, pageIdx);
    } else if (rule.etype == ElementType::Text) {
        candidates = Detectors::detectTextBlocks(doc, pageIdx);
    } else if (rule.etype == ElementType::Image) {
        candidates = Detectors::detectImages(doc, pageIdx);
    } else if (rule.etype == ElementType::Drawing) {
        candidates = Detectors::detectPatternOverlays(doc, pageIdx, 0.1f);
    }

    for (const auto &el : candidates) {
        if (elementMatches(el, rule, pRect)) {
            matches.push_back(el);
        }
    }

    return matches;
}

} // namespace wipepdf
