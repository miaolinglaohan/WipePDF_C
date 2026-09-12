#include "WatermarkCleaner.h"
#include "Detectors.h"
#include "ContentEdit.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>

namespace wipepdf {

WatermarkCleaner::WatermarkCleaner(ProgressCallback callback)
    : m_progressCallback(std::move(callback)) {}

void WatermarkCleaner::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void WatermarkCleaner::log(const QString &msg) const {
    if (m_progressCallback) {
        m_progressCallback(msg);
    }
}

void WatermarkCleaner::addInteractiveRule(const MatchRule &rule) {
    m_interactiveRules.push_back(rule);
}

bool WatermarkCleaner::popInteractiveRule() {
    if (!m_interactiveRules.empty()) {
        m_interactiveRules.pop_back();
        return true;
    }
    return false;
}

void WatermarkCleaner::clearInteractiveRules() {
    m_interactiveRules.clear();
}

std::vector<Element> WatermarkCleaner::previewAuto(const PdfDocument &doc, const AutoOptions &options) const {
    std::vector<Element> all;
    int pages = doc.pageCount();
    for (int p = 0; p < pages; ++p) {
        if (options.removeAllLinks) {
            auto links = Detectors::detectLinks(doc, p, options.linkUrlPattern);
            all.insert(all.end(), links.begin(), links.end());
        }
        if (options.removeBottomStrip) {
            auto bottom = Detectors::detectBottomStrip(doc, p, options.bottomStripHeight);
            all.insert(all.end(), bottom.begin(), bottom.end());
        }
        if (options.removeByTextPattern && !options.textPattern.isEmpty()) {
            auto textEls = Detectors::detectByTextPattern(doc, p, options.textPattern);
            all.insert(all.end(), textEls.begin(), textEls.end());
        }
        if (options.detectTransparentOverlays) {
            auto overlays = Detectors::detectTransparentOverlays(doc, p, options.transparentMinAreaRatio, options.transparentMaxOpacity);
            all.insert(all.end(), overlays.begin(), overlays.end());
            auto patterns = Detectors::detectPatternOverlays(doc, p, 0.9f);
            all.insert(all.end(), patterns.begin(), patterns.end());
        }
    }
    return all;
}

std::vector<Element> WatermarkCleaner::previewInteractive(const PdfDocument &doc) const {
    std::vector<Element> all;
    int pages = doc.pageCount();
    for (int p = 0; p < pages; ++p) {
        for (const auto &rule : m_interactiveRules) {
            auto matches = Matcher::findMatches(doc, rule, p);
            all.insert(all.end(), matches.begin(), matches.end());
        }
    }
    return all;
}

CleanerResult WatermarkCleaner::process(const QString &inputPath, const QString &outputPath,
                                        const AutoOptions &options, bool useInteractiveRules,
                                        bool overwrite) {
    QFileInfo inInfo(inputPath);
    if (!inInfo.exists()) {
        return {false, QString("Input file not found: %1").arg(inputPath), inputPath};
    }

    QString finalOut = outputPath;
    if (overwrite) {
        finalOut = inputPath;
    } else {
        QFileInfo outInfo(outputPath);
        if (outInfo.isDir() || outputPath.endsWith('/') || outputPath.endsWith('\\') || outInfo.suffix().isEmpty()) {
            QDir outDir(outputPath);
            finalOut = outDir.filePath(inInfo.baseName() + "_clean." + inInfo.suffix());
        } else if (outInfo.absoluteFilePath() == inInfo.absoluteFilePath()) {
            finalOut = inInfo.dir().filePath(inInfo.baseName() + "_clean." + inInfo.suffix());
        }
    }

    QDir().mkpath(QFileInfo(finalOut).dir().path());

    PdfDocument doc;
    QString err;
    if (!doc.open(inputPath, &err)) {
        return {false, QString("Failed to load PDF: %1").arg(err), inputPath};
    }

    int removed = 0;
    removed += applyAutoRules(doc, options);

    if (useInteractiveRules && !m_interactiveRules.empty()) {
        removed += applyInteractiveRules(doc);
    }

    bool saveOk = false;
    if (overwrite) {
        saveOk = doc.saveAtomic(finalOut, 4, true, true, &err);
    } else {
        saveOk = doc.save(finalOut, 4, true, true, &err);
        doc.close();
    }

    if (!saveOk) {
        return {false, QString("Failed to save output file: %1").arg(err), inputPath};
    }

    QString msg = QString("Successfully removed %1 watermark elements -> %2").arg(removed).arg(finalOut);
    return {true, msg, inputPath, finalOut, removed};
}

std::vector<CleanerResult> WatermarkCleaner::processBatch(const QString &inputDir, const QString &outputDir,
                                                         const AutoOptions &options, bool useInteractiveRules,
                                                         bool recursive, bool overwrite) {
    std::vector<CleanerResult> results;
    QDir inDir(inputDir);
    if (!inDir.exists()) return results;

    QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(inputDir, QStringList() << "*.pdf", QDir::Files, flags);

    QStringList files;
    while (it.hasNext()) {
        files << it.next();
    }
    files.sort();

    int total = files.size();
    for (int i = 0; i < total; ++i) {
        const QString &pdfPath = files[i];
        QString relPath = inDir.relativeFilePath(pdfPath);
        QString outPath;
        if (overwrite) {
            outPath = pdfPath;
        } else {
            outPath = QDir(outputDir).filePath(relPath);
        }

        log(QString("[%1/%2] Processing: %3").arg(i + 1).arg(total).arg(relPath));
        CleanerResult res = process(pdfPath, outPath, options, useInteractiveRules, overwrite);
        log(res.message);
        results.push_back(res);
    }

    return results;
}

int WatermarkCleaner::applyAutoRules(PdfDocument &doc, const AutoOptions &options) {
    int count = 0;
    int pages = doc.pageCount();

    for (int p = 0; p < pages; ++p) {
        // 1. Links
        if (options.removeAllLinks) {
            count += doc.deleteAllLinks(p, options.linkUrlPattern);
        }

        // 2. Bottom strip
        if (options.removeBottomStrip) {
            QRectF pRect = doc.pageRect(p);
            QRectF bottom(pRect.left(), pRect.bottom() - options.bottomStripHeight, pRect.width(), options.bottomStripHeight);
            doc.addRedaction(p, bottom);
            doc.applyRedactions(p);
            if (!doc.getTextInRect(p, bottom).trimmed().isEmpty()) {
                count += ContentEdit::removeTextInRegion(doc, p, bottom);
            }
            count++;
        }

        // 3. Text pattern
        if (options.removeByTextPattern && !options.textPattern.isEmpty()) {
            // Filter dedicated watermark streams
            count += doc.filterPageContentStreams(p, [&](const QByteArray &data) {
                if (data.size() > 10000) return false;
                QRegularExpression re(options.textPattern);
                return re.match(QString::fromLatin1(data)).hasMatch();
            });
            // Precise token-level text operator removal from content stream
            count += ContentEdit::removeTextMatchingPattern(doc, p, options.textPattern);
        }

        // 4. Transparent overlays (low-alpha vector drawings & transparent text) + full-page pattern fills
        if (options.detectTransparentOverlays) {
            // First, filter dedicated pattern and transparent watermark streams in /Contents
            count += doc.filterPageContentStreams(p, [&](const QByteArray &data) {
                if (data.size() > 10000) return false;
                if (data.contains("/Pattern cs") || data.contains("/Pattern CS")) return true;
                if ((data.contains("gs") || data.contains("Tr")) && data.contains("BT") &&
                    (data.contains("biaozhun") || data.contains(".org") || data.contains(".com") || data.contains(".cn") || data.contains(".net"))) {
                    return true;
                }
                return false;
            });

            auto overlays = Detectors::detectTransparentOverlays(doc, p, options.transparentMinAreaRatio, options.transparentMaxOpacity);
            for (const auto &el : overlays) {
                if (el.type == ElementType::Text) {
                    if (!el.text.trimmed().isEmpty()) {
                        count += ContentEdit::removeTextByContent(doc, p, el.text.trimmed());
                    }
                } else if (el.type == ElementType::Drawing) {
                    if (el.rect_ratio >= 0.9f) continue;
                    doc.addRedaction(p, el.bbox);
                    doc.applyRedactions(p);
                    count++;
                }
            }
            count += ContentEdit::removeFullPagePatternFills(doc, p, 0.9f);
        }
    }

    return count;
}

int WatermarkCleaner::applyInteractiveRules(PdfDocument &doc) {
    int count = 0;
    int pages = doc.pageCount();

    for (int p = 0; p < pages; ++p) {
        for (const auto &rule : m_interactiveRules) {
            auto matches = Matcher::findMatches(doc, rule, p);
            for (const auto &el : matches) {
                if (removeElement(doc, p, el)) {
                    count++;
                }
            }
        }
    }

    return count;
}

bool WatermarkCleaner::removeElement(PdfDocument &doc, int pageIdx, const Element &el) {
    if (el.type == ElementType::Link) {
        return doc.deleteLink(pageIdx, el.bbox);
    }

    if (el.type == ElementType::Image) {
        if (el.xref > 0) {
            doc.deleteImage(pageIdx, el.xref);
        }
        doc.addRedaction(pageIdx, el.bbox);
        doc.applyRedactions(pageIdx, true);
        return true;
    }

    if (el.type == ElementType::Drawing && el.extra.value("pattern_fill").toBool()) {
        int removed = doc.filterPageContentStreams(pageIdx, [](const QByteArray &data) {
            return data.size() < 10000 && (data.contains("/Pattern cs") || data.contains("/Pattern CS"));
        });
        removed += ContentEdit::removeFullPagePatternFills(doc, pageIdx, 0.5f);
        return removed > 0;
    }

    if (el.type == ElementType::Text) {
        int removed = 0;
        if (!el.text.isEmpty()) {
            // 1. Filter dedicated stream if present
            removed += doc.filterPageContentStreams(pageIdx, [&](const QByteArray &data) {
                if (data.size() > 10000) return false;
                return data.contains(el.text.toLatin1());
            });

            // 2. Remove matching text from content stream
            removed += ContentEdit::removeTextByContent(doc, pageIdx, el.text.trimmed());
        }

        // Fallback: ONLY if the bounding box has NO other text except this element
        if (removed == 0) {
            QString boxText = doc.getTextInRect(pageIdx, el.bbox).trimmed();
            if (!boxText.isEmpty() && boxText == el.text.trimmed()) {
                doc.addRedaction(pageIdx, el.bbox);
                doc.applyRedactions(pageIdx);
                removed++;
            }
        }
        return removed > 0;
    }

    // Generic Drawing
    doc.addRedaction(pageIdx, el.bbox);
    doc.applyRedactions(pageIdx);
    return true;
}

} // namespace wipepdf
