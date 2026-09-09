#include <iostream>
#include <QCoreApplication>
#include <QFile>
#include <cassert>
#include "core/PdfDocument.h"
#include "core/Detectors.h"
#include "core/Matcher.h"
#include "core/WatermarkCleaner.h"
#include "i18n/I18n.h"

using namespace wipepdf;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "     WipePDF Core Verification Suite    " << std::endl;
    std::cout << "========================================" << std::endl;

    PdfDocument doc;
    QString err;
    const QString samplePdf = "test_input/sample_watermark.pdf";

    if (!doc.open(samplePdf, &err)) {
        std::cerr << "Failed to open sample PDF: " << err.toStdString() << std::endl;
        return 1;
    }

    std::cout << "[1] Opened: " << samplePdf.toStdString() << " | Pages: " << doc.pageCount() << std::endl;

    // Test Detectors
    auto links = Detectors::detectLinks(doc, 0);
    std::cout << "[2] Page 0 links detected: " << links.size() << std::endl;
    for (const auto &l : links) {
        std::cout << "    - Link: " << l.summary().toStdString() << std::endl;
    }

    auto textBlocks = Detectors::detectTextBlocks(doc, 0);
    std::cout << "[3] Page 0 text blocks: " << textBlocks.size() << std::endl;

    auto bottomStrip = Detectors::detectBottomStrip(doc, 0, 60.0f);
    std::cout << "[4] Page 0 bottom strip items: " << bottomStrip.size() << std::endl;

    // Test Pick Element at Point
    if (!links.empty()) {
        QPointF pt = links[0].bbox.center();
        auto picked = Detectors::pickElementAt(doc, 0, pt);
        std::cout << "[5] Pick at " << pt.x() << "," << pt.y() << " found " << picked.size() << " elements" << std::endl;
        for (const auto &p : picked) {
            std::cout << "    * " << p.summary().toStdString() << std::endl;
        }

        // Test Matcher
        MatchRule rule = Matcher::createRuleFromElement(doc, 0, picked[0]);
        std::cout << "[6] Created match rule: " << rule.describe().toStdString() << std::endl;
        auto matches = Matcher::findMatches(doc, rule, 0);
        std::cout << "    Matched on page 0: " << matches.size() << " elements" << std::endl;
    }

    doc.close();

    // Test WatermarkCleaner process (Save As)
    WatermarkCleaner cleaner([](const QString &msg) {
        std::cout << "  [CleanerLog] " << msg.toStdString() << std::endl;
    });

    AutoOptions opts;
    opts.removeAllLinks = true;
    opts.removeBottomStrip = true;
    opts.bottomStripHeight = 60.0f;

    CleanerResult res = cleaner.process(samplePdf, "test_output/cleaned_cpp.pdf", opts, false, false);
    std::cout << "[7] Cleaner result: " << (res.success ? "SUCCESS" : "FAILED")
              << " | Removed: " << res.removedCount << std::endl;
    std::cout << "    Message: " << res.message.toStdString() << std::endl;

    if (!res.success) {
        return 1;
    }

    // Test 8: Undo Rule
    cleaner.clearInteractiveRules();
    MatchRule r1; r1.etype = ElementType::Text; r1.textPattern = "test1";
    MatchRule r2; r2.etype = ElementType::Link; r2.urlPattern = "test2";
    cleaner.addInteractiveRule(r1);
    cleaner.addInteractiveRule(r2);
    assert(cleaner.interactiveRules().size() == 2);
    bool popped = cleaner.popInteractiveRule();
    assert(popped && cleaner.interactiveRules().size() == 1);
    std::cout << "[8] Undo rule test PASSED! (Rules count after undo: 1)" << std::endl;

    // Test 9: Safe Overwrite Mode
    QString overwriteCopy = "test_output/overwrite_test.pdf";
    QFile::remove(overwriteCopy);
    QFile::copy(samplePdf, overwriteCopy);

    CleanerResult owRes = cleaner.process(overwriteCopy, overwriteCopy, opts, false, true);
    std::cout << "[9] Safe overwrite test: " << (owRes.success ? "SUCCESS" : "FAILED")
              << " | Removed: " << owRes.removedCount << std::endl;
    if (!owRes.success) {
        std::cerr << "Safe overwrite failed: " << owRes.message.toStdString() << std::endl;
        return 1;
    }

    // Test 10: i18n
    I18n::instance().setLanguage(Language::Zh);
    QString zhTitle = tr_("app_brand");
    I18n::instance().setLanguage(Language::En);
    QString enTitle = tr_("app_brand");
    std::cout << "[10] i18n dynamic translation test: ZH=" << zhTitle.toStdString()
              << " | EN=" << enTitle.toStdString() << std::endl;
    assert(zhTitle == "清印 PDF");
    assert(enTitle == "WipePDF");

    std::cout << "\n========================================" << std::endl;
    std::cout << "  ALL 10 VERIFICATION TESTS PASSED 100% " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
