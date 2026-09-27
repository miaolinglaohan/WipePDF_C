#include <iostream>
#include <QCoreApplication>
#include <QFile>
#include <cassert>
#include "core/PdfDocument.h"
#include "core/Detectors.h"
#include "core/WatermarkCleaner.h"

using namespace wipepdf;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "    Testing GB+2536-2025.pdf Cleaning   " << std::endl;
    std::cout << "========================================" << std::endl;

    const QString inputPath = "build/GB+2536-2025.pdf";
    const QString outputPath = "build/GB_clean_cpp.pdf";

    // 该测试依赖真实标准文档 GB+2536-2025.pdf（较大/版权原因未纳入仓库）。
    // 文档缺失时优雅跳过，不视为失败；后续将文件放回即可启用。
    if (!QFile::exists(inputPath)) {
        std::cout << "SKIP: test document not found at " << inputPath.toStdString() << std::endl;
        std::cout << "      Place GB+2536-2025.pdf there to enable this test." << std::endl;
        return 0;
    }

    PdfDocument doc;
    QString err;
    if (!doc.open(inputPath, &err)) {
        std::cerr << "Failed to open " << inputPath.toStdString() << ": " << err.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Original doc opened: " << doc.pageCount() << " pages." << std::endl;

    // Check page 19 text before clean
    QString beforeText = doc.getTextInRect(19, QRectF(0, 0, 600, 900));
    bool hasE21 = beforeText.contains("E.2.1");
    bool hasE28 = beforeText.contains("E.2.8");
    bool hasE3 = beforeText.contains("E.3");
    std::cout << "Before clean: has E.2.1=" << hasE21 << ", has E.2.8=" << hasE28 << ", has E.3=" << hasE3 << std::endl;
    assert(hasE21 && hasE28 && hasE3);

    doc.close();

    WatermarkCleaner cleaner([](const QString &msg) {
        std::cout << "  [CleanerLog] " << msg.toStdString() << std::endl;
    });

    AutoOptions opts;
    opts.removeAllLinks = true;
    opts.detectTransparentOverlays = true;
    opts.transparentMinAreaRatio = 0.5f;
    opts.transparentMaxOpacity = 0.35f;

    CleanerResult res = cleaner.process(inputPath, outputPath, opts, false, false);
    std::cout << "Cleaner result: " << (res.success ? "SUCCESS" : "FAILED")
              << ", removedCount=" << res.removedCount << std::endl;
    assert(res.success);

    // Verify cleaned document
    PdfDocument cleanDoc;
    if (!cleanDoc.open(outputPath, &err)) {
        std::cerr << "Failed to open cleaned doc: " << err.toStdString() << std::endl;
        return 1;
    }

    // Verify page 19 (Page 20) text is 100% preserved
    QString afterText = cleanDoc.getTextInRect(19, QRectF(0, 0, 600, 900));
    bool afterE21 = afterText.contains("E.2.1");
    bool afterE28 = afterText.contains("E.2.8");
    bool afterE3 = afterText.contains("E.3");
    std::cout << "After clean: has E.2.1=" << afterE21 << ", has E.2.8=" << afterE28 << ", has E.3=" << afterE3 << std::endl;
    assert(afterE21 && afterE28 && afterE3);

    // Check in the center box (180..420, 319..530)
    QRectF centerBox(180, 319, 240, 210);
    QString centerText = cleanDoc.getTextInRect(19, centerBox);
    std::cout << "Center box text after clean (length " << centerText.length() << "):\n"
              << centerText.trimmed().toStdString() << std::endl;
    assert(!centerText.trimmed().isEmpty());

    // Verify no biaozhun text remains in centerBox
    assert(!centerText.contains("biaozhun"));

    // Render page 19 to image for visual inspection
    QImage img = cleanDoc.renderPage(19, 150.0f);
    img.save("build/GB_clean_cpp_p19.png");
    std::cout << "Saved build/GB_clean_cpp_p19.png successfully!" << std::endl;

    cleanDoc.close();

    std::cout << "\n========================================" << std::endl;
    std::cout << "   GB+2536-2025 TEST PASSED 100%!       " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
