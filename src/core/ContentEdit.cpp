#include "ContentEdit.h"
#include <QRegularExpression>
#include <algorithm>
#include <vector>

namespace wipepdf {

struct Matrix {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, e = 0.0f, f = 0.0f;

    static Matrix multiply(const Matrix &m1, const Matrix &m2) {
        return {
            m1.a * m2.a + m1.b * m2.c,
            m1.a * m2.b + m1.b * m2.d,
            m1.c * m2.a + m1.d * m2.c,
            m1.c * m2.b + m1.d * m2.d,
            m1.e * m2.a + m1.f * m2.c + m2.e,
            m1.e * m2.b + m1.f * m2.d + m2.f
        };
    }

    QPointF apply(float x, float y) const {
        return QPointF(a * x + c * y + e, b * x + d * y + f);
    }

    QRectF transformRect(float x, float y, float w, float h) const {
        QPointF p0 = apply(x, y);
        QPointF p1 = apply(x + w, y + h);
        float xMin = std::min(p0.x(), p1.x());
        float yMin = std::min(p0.y(), p1.y());
        float xMax = std::max(p0.x(), p1.x());
        float yMax = std::max(p0.y(), p1.y());
        return QRectF(xMin, yMin, xMax - xMin, yMax - yMin);
    }
};

static int countHexGlyphs(const QByteArray &hexTok, bool fourDigit) {
    int digits = 0;
    for (int i = 1; i < hexTok.size() - 1; ++i) {
        char c = hexTok.at(i);
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
            digits++;
        }
    }
    if (digits == 0) return 0;
    if (fourDigit && digits % 4 == 0) return digits / 4;
    return digits / 2;
}

std::vector<ContentSegment> ContentEdit::parseSegments(const QByteArray &data, bool fourDigitCids) {
    std::vector<ContentSegment> segments;

    static const QRegularExpression tokenRe(
        "([+-]?\\d+(?:\\.\\d+)?)|(<[0-9A-Fa-f\\s]*>)|(\\((?:\\\\.|[^()\\\\])*\\))|([A-Za-z*'\"/]+)|([<>\\[\\]{}])"
    );

    Matrix ctm;
    std::vector<Matrix> stack;
    std::vector<float> numBuf;
    bool inText = false;
    int textStart = -1;
    Matrix tm;
    int glyphCount = 0;

    struct LastRe {
        float x = 0, y = 0, w = 0, h = 0;
        Matrix mtx;
        int start = -1;
    } lastRe;

    auto it = tokenRe.globalMatch(QString::fromLatin1(data));
    while (it.hasNext()) {
        auto m = it.next();
        QString tokStr = m.captured(0).trimmed();
        if (tokStr.isEmpty()) continue;

        QByteArray tok = tokStr.toLatin1();
        int mStart = static_cast<int>(m.capturedStart(0));
        int mEnd = static_cast<int>(m.capturedEnd(0));

        if (tok == "q") {
            stack.push_back(ctm);
            numBuf.clear();
            continue;
        }
        if (tok == "Q") {
            if (!stack.empty()) {
                ctm = stack.back();
                stack.pop_back();
            }
            numBuf.clear();
            continue;
        }
        if (tok == "BT") {
            inText = true;
            textStart = mStart;
            tm = Matrix();
            glyphCount = 0;
            numBuf.clear();
            continue;
        }
        if (tok == "ET") {
            if (inText && textStart >= 0) {
                QPointF pos = ctm.apply(tm.e, tm.f);
                ContentSegment seg;
                seg.kind = ContentSegment::Kind::Text;
                seg.start = textStart;
                seg.end = mEnd;
                seg.glyphCount = glyphCount;
                seg.pos = pos;
                segments.push_back(seg);
            }
            inText = false;
            numBuf.clear();
            continue;
        }
        if (tok == "cm" && numBuf.size() >= 6) {
            size_t n = numBuf.size();
            Matrix mcm = {numBuf[n-6], numBuf[n-5], numBuf[n-4], numBuf[n-3], numBuf[n-2], numBuf[n-1]};
            ctm = Matrix::multiply(ctm, mcm);
            numBuf.clear();
            continue;
        }
        if (tok == "Tm" && numBuf.size() >= 6) {
            size_t n = numBuf.size();
            tm = {numBuf[n-6], numBuf[n-5], numBuf[n-4], numBuf[n-3], numBuf[n-2], numBuf[n-1]};
            numBuf.clear();
            continue;
        }
        if (tok == "re" && numBuf.size() >= 4) {
            size_t n = numBuf.size();
            lastRe = {numBuf[n-4], numBuf[n-3], numBuf[n-2], numBuf[n-1], ctm, mStart};
            numBuf.clear();
            continue;
        }
        if (tok == "f" || tok == "F" || tok == "f*" || tok == "B" || tok == "B*" || tok == "b" || tok == "b*") {
            if (lastRe.start >= 0) {
                QRectF rect = lastRe.mtx.transformRect(lastRe.x, lastRe.y, lastRe.w, lastRe.h);
                int checkStart = std::max(0, mStart - 200);
                QByteArray contextWindow = data.mid(checkStart, mStart - checkStart);
                bool pattern = contextWindow.contains("scn") || contextWindow.contains("SCN");

                ContentSegment seg;
                seg.kind = ContentSegment::Kind::Fill;
                seg.start = lastRe.start;
                seg.end = mEnd;
                seg.rect = rect;
                seg.patternFill = pattern;
                segments.push_back(seg);
            }
            numBuf.clear();
            continue;
        }

        if (tok.startsWith('<') && tok.endsWith('>')) {
            if (inText) {
                glyphCount += countHexGlyphs(tok, fourDigitCids);
            }
            numBuf.clear();
            continue;
        }

        bool ok = false;
        float val = tokStr.toFloat(&ok);
        if (ok) {
            numBuf.push_back(val);
            continue;
        }

        numBuf.clear();
    }

    return segments;
}

QByteArray ContentEdit::removeSegments(const QByteArray &data, const std::vector<ContentSegment> &segments) {
    if (segments.empty()) return data;

    std::vector<ContentSegment> sortedSegs = segments;
    std::sort(sortedSegs.begin(), sortedSegs.end(), [](const ContentSegment &a, const ContentSegment &b) {
        return a.start < b.start;
    });

    QByteArray out;
    int prev = 0;
    for (const auto &seg : sortedSegs) {
        if (seg.start < prev) continue; // skip overlapping
        out.append(data.mid(prev, seg.start - prev));
        prev = seg.end;
    }
    if (prev < data.size()) {
        out.append(data.mid(prev));
    }
    return out;
}

int ContentEdit::removeFullPagePatternFills(PdfDocument &doc, int pageIdx, float minRatio) {
    QByteArray stream = doc.getPageContentStream(pageIdx);
    if (stream.isEmpty()) return 0;

    QRectF pageRect = doc.pageRect(pageIdx);
    float pageArea = pageRect.width() * pageRect.height();
    if (pageArea <= 0.0f) return 0;

    auto segs = parseSegments(stream);
    std::vector<ContentSegment> toRemove;
    for (const auto &seg : segs) {
        if (seg.kind == ContentSegment::Kind::Fill && seg.patternFill) {
            float area = seg.rect.width() * seg.rect.height();
            if (area / pageArea >= minRatio) {
                toRemove.push_back(seg);
            }
        }
    }

    if (toRemove.empty()) return 0;

    QByteArray newStream = removeSegments(stream, toRemove);
    doc.setPageContentStream(pageIdx, newStream);
    return static_cast<int>(toRemove.size());
}

int ContentEdit::removeTextInRegion(PdfDocument &doc, int pageIdx, const QRectF &region) {
    QByteArray stream = doc.getPageContentStream(pageIdx);
    if (stream.isEmpty()) return 0;

    auto segs = parseSegments(stream);
    std::vector<ContentSegment> toRemove;
    for (const auto &seg : segs) {
        if (seg.kind == ContentSegment::Kind::Text) {
            if (region.contains(seg.pos)) {
                toRemove.push_back(seg);
            }
        }
    }

    if (toRemove.empty()) return 0;

    QByteArray newStream = removeSegments(stream, toRemove);
    doc.setPageContentStream(pageIdx, newStream);
    return static_cast<int>(toRemove.size());
}

} // namespace wipepdf
