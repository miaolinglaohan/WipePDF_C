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
            auto matches = Detectors::detectByTextPattern(doc, p, options.textPattern);
            for (const auto &el : matches) {
                doc.addRedaction(p, el.bbox);
                count++;
            }
            if (!matches.empty()) {
                doc.applyRedactions(p);
                for (const auto &el : matches) {
                    if (!doc.getTextInRect(p, el.bbox).trimmed().isEmpty()) {
                        ContentEdit::removeTextInRegion(doc, p, el.bbox);
                    }
                }
            }
        }

        // 4. Transparent overlays (low-alpha vector drawings) + full-page pattern fills
        // 对齐 Python 版 _apply_auto_rules：
        //   - ratio < 0.9 的透明覆盖层逐个 redact 删除（line_art 覆盖即删）
        //   - 整页(ratio >= 0.9)的交给内容流图案填充清除（min_ratio=0.9）
        if (options.detectTransparentOverlays) {
            auto overlays = Detectors::detectTransparentOverlays(doc, p, options.transparentMinAreaRatio, options.transparentMaxOpacity);
            for (const auto &el : overlays) {
                if (el.rect_ratio >= 0.9f) continue;
                doc.addRedaction(p, el.bbox);
                doc.applyRedactions(p);
                if (!doc.getTextInRect(p, el.bbox).trimmed().isEmpty()) {
                    count += ContentEdit::removeTextInRegion(doc, p, el.bbox);
                }
                count++;
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
        return ContentEdit::removeFullPagePatternFills(doc, pageIdx, 0.5f) > 0;
    }

    // Text or generic Drawing
    doc.addRedaction(pageIdx, el.bbox);
    doc.applyRedactions(pageIdx);
    if (el.type == ElementType::Text && !doc.getTextInRect(pageIdx, el.bbox).trimmed().isEmpty()) {
        ContentEdit::removeTextInRegion(doc, pageIdx, el.bbox);
    }
    return true;
}

} // namespace wipepdf
