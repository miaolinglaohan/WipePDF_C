#pragma once

#include <QString>
#include <functional>
#include <vector>
#include "Element.h"
#include "PdfDocument.h"
#include "Matcher.h"

namespace wipepdf {

struct AutoOptions {
    bool removeAllLinks = false;
    QString linkUrlPattern;
    bool removeBottomStrip = false;
    float bottomStripHeight = 60.0f;
    bool removeByTextPattern = false;
    QString textPattern;
    bool detectTransparentOverlays = false;
    float transparentMinAreaRatio = 0.5f;
    float transparentMaxOpacity = 0.35f;
};

struct CleanerResult {
    bool success = false;
    QString message;
    QString inputPath;
    QString outputPath;
    int removedCount = 0;
};

class WatermarkCleaner {
public:
    using ProgressCallback = std::function<void(const QString &)>;

    explicit WatermarkCleaner(ProgressCallback callback = nullptr);

    void setProgressCallback(ProgressCallback callback);
    void log(const QString &msg) const;

    // Interactive rule management (with Undo support!)
    void addInteractiveRule(const MatchRule &rule);
    bool popInteractiveRule(); // Undo last rule
    void clearInteractiveRules();
    const std::vector<MatchRule> &interactiveRules() const { return m_interactiveRules; }

    // Previews
    std::vector<Element> previewAuto(const PdfDocument &doc, const AutoOptions &options) const;
    std::vector<Element> previewInteractive(const PdfDocument &doc) const;

    // Processing
    CleanerResult process(const QString &inputPath, const QString &outputPath,
                          const AutoOptions &options, bool useInteractiveRules = true,
                          bool overwrite = false);

    std::vector<CleanerResult> processBatch(const QString &inputDir, const QString &outputDir,
                                           const AutoOptions &options, bool useInteractiveRules = true,
                                           bool recursive = true, bool overwrite = false);

private:
    int applyAutoRules(PdfDocument &doc, const AutoOptions &options);
    int applyInteractiveRules(PdfDocument &doc);
    bool removeElement(PdfDocument &doc, int pageIdx, const Element &el);

    ProgressCallback m_progressCallback;
    std::vector<MatchRule> m_interactiveRules;
};

} // namespace wipepdf
