#include <iostream>
#include <QCoreApplication>
#include "core/PdfDocument.h"
#include "core/Detectors.h"
#include "core/Matcher.h"
#include "core/WatermarkCleaner.h"

using namespace wipepdf;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    PdfDocument doc;
    QString err;
    if (!doc.open("test_input/test1.pdf", &err)) {
        std::cerr << "Failed to open test1.pdf: " << err.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Opened test1.pdf, pages: " << doc.pageCount() << std::endl;

    // Test 1: Check getImages
    auto images = doc.getImages(0);
    std::cout << "Page 0 images count: " << images.size() << std::endl;
    for (const auto &img : images) {
        std::cout << "  Image: " << img.summary().toStdString() 
                  << " | xref: " << img.xref << std::endl;
    }

    // Test 2: Click at exact user coordinate (392.0, 815.0) from screenshot
    QPointF pt(392.0, 815.0);
    auto picked = Detectors::pickElementAt(doc, 0, pt);
    std::cout << "Pick at (392, 815) count: " << picked.size() << std::endl;
    for (const auto &p : picked) {
        std::cout << "  Picked: " << p.summary().toStdString() << std::endl;
    }

    if (picked.empty()) {
        std::cerr << "FAILED: No element picked at (392, 815)!" << std::endl;
        return 1;
    }

    // Test 3: Create rule and find matches
    MatchRule rule = Matcher::createRuleFromElement(doc, 0, picked[0]);
    std::cout << "Created rule: " << rule.describe().toStdString() << std::endl;

    auto matches = Matcher::findMatches(doc, rule, 0);
    std::cout << "Matches on page 0: " << matches.size() << std::endl;

    doc.close();

    // Test 4: WatermarkCleaner with interactive rule to delete QR code
    WatermarkCleaner cleaner;
    cleaner.addInteractiveRule(rule);

    AutoOptions opts;
    CleanerResult res = cleaner.process("test_input/test1.pdf", "test_output/test1_qr_removed_cpp.pdf", opts, true, false);
    std::cout << "Cleaning result: " << (res.success ? "SUCCESS" : "FAILED") 
              << " | Removed: " << res.removedCount << std::endl;

    if (!res.success) {
        return 1;
    }

    std::cout << "VERIFICATION COMPLETE: QR code can now be picked and removed!" << std::endl;
    return 0;
}
