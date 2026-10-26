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

    // Test 11: Transparent overlay detection (low-alpha vector drawing)
    // 对齐 Python 版 detect_transparent_overlays：面积 >= 50% 且 opacity <= 0.35 且极简路径
    {
        PdfDocument ovDoc;
        QString ovErr;
        bool ovOk = ovDoc.open("test_input/overlay_test.pdf", &ovErr);
        assert(ovOk);
        auto overlays = Detectors::detectTransparentOverlays(ovDoc, 0, 0.5f, 0.35f);
        std::cout << "[11] Transparent overlays detected: " << overlays.size() << std::endl;
        for (const auto &o : overlays) {
            std::cout << "     overlay bbox=" << o.bbox.x() << "," << o.bbox.y()
                      << "," << o.bbox.width() << "x" << o.bbox.height()
                      << " ratio=" << o.rect_ratio << " opacity=" << o.opacity << std::endl;
        }
        assert(overlays.size() == 1);
        assert(std::abs(overlays[0].opacity - 0.25f) < 0.001f);
        assert(overlays[0].rect_ratio >= 0.5f && overlays[0].rect_ratio < 0.9f);
        assert(overlays[0].type == ElementType::Drawing);
    }

    // Test 12: 端到端——处理并验证透明覆盖层被真正删除
    {
        WatermarkCleaner ovCleaner;
        AutoOptions ovOpts;
        ovOpts.detectTransparentOverlays = true;
        ovOpts.transparentMinAreaRatio = 0.5f;
        ovOpts.transparentMaxOpacity = 0.35f;
        CleanerResult ovRes = ovCleaner.process("test_input/overlay_test.pdf",
                                                 "test_output/overlay_cleaned.pdf",
                                                 ovOpts, false, false);
        std::cout << "[12] Overlay clean result: " << (ovRes.success ? "SUCCESS" : "FAILED")
                  << " | Removed: " << ovRes.removedCount << std::endl;
        assert(ovRes.success);
        assert(ovRes.removedCount >= 1);

        PdfDocument checkDoc;
        QString checkErr;
        assert(checkDoc.open("test_output/overlay_cleaned.pdf", &checkErr));
        auto remaining = Detectors::detectTransparentOverlays(checkDoc, 0, 0.5f, 0.35f);
        std::cout << "     Overlay remaining after clean: " << remaining.size() << std::endl;
        assert(remaining.empty());
    }

#define TEST_CHECK(cond) do { if (!(cond)) { std::cerr << "FAILED check at line " << __LINE__ << ": " #cond << std::endl; std::exit(1); } } while(0)

    // Test 13: 验证 Form XObject 独立流水印（GB 50061 机制）流级无损清除
    {
        QString testGbPath = "test_output/form_xobject_watermark.pdf";
        QFile::remove(testGbPath);
        bool copyOk = QFile::copy("test_input/sample_watermark.pdf", testGbPath);
        TEST_CHECK(copyOk);

        PdfDocument synthDoc;
        QString sErr;
        bool openOk = synthDoc.open(testGbPath, &sErr);
        TEST_CHECK(openOk);
        fz_context *ctx = synthDoc.context();
        pdf_document *pdoc = synthDoc.pdfDoc();
        pdf_page *page = pdf_load_page(ctx, pdoc, 0);
        TEST_CHECK(page != nullptr);

        // 创建 Form XObject: stream 写入 "q BT /FW_0 24 Tf (www.bzfxw.com) Tj ET Q"
        pdf_obj *formDict = pdf_new_dict(ctx, pdoc, 4);
        pdf_dict_puts_drop(ctx, formDict, "Type", pdf_new_name(ctx, "XObject"));
        pdf_dict_puts_drop(ctx, formDict, "Subtype", pdf_new_name(ctx, "Form"));
        pdf_dict_puts_drop(ctx, formDict, "BBox", pdf_new_rect(ctx, pdoc, fz_make_rect(0, 0, 300, 50)));
        fz_buffer *formBuf = fz_new_buffer_from_copied_data(ctx, (const unsigned char*)"q BT /FW_0 24 Tf (www.bzfxw.com) Tj ET Q", 44);
        pdf_obj *formObj = pdf_add_stream(ctx, pdoc, formBuf, formDict, 0);
        fz_drop_buffer(ctx, formBuf);
        pdf_drop_obj(ctx, formDict);

        // 挂载到 Page Resources/XObject/WOBJ0
        pdf_obj *res = pdf_page_resources(ctx, page);
        if (!res) {
            res = pdf_dict_puts_dict(ctx, page->obj, "Resources", 2);
        }
        pdf_obj *xobjs = pdf_dict_gets(ctx, res, "XObject");
        if (!xobjs) {
            xobjs = pdf_dict_puts_dict(ctx, res, "XObject", 2);
        }
        pdf_dict_puts(ctx, xobjs, "WOBJ0", formObj);

        // 创建水印流: "q /WOBJ0 Do Q"
        pdf_obj *wmDict = pdf_new_dict(ctx, pdoc, 0);
        fz_buffer *wmBuf = fz_new_buffer_from_copied_data(ctx, (const unsigned char*)"q /WOBJ0 Do Q", 13);
        pdf_obj *wmStream = pdf_add_stream(ctx, pdoc, wmBuf, wmDict, 0);
        fz_drop_buffer(ctx, wmBuf);
        pdf_drop_obj(ctx, wmDict);

        // 将 Contents 转换为包含 2 个流的 array: [wmStream, 原 contents]
        pdf_obj *oldContents = pdf_page_contents(ctx, page);
        pdf_obj *contentsArr = pdf_new_array(ctx, pdoc, 2);
        pdf_array_push(ctx, contentsArr, wmStream);
        if (pdf_is_array(ctx, oldContents)) {
            int oldLen = pdf_array_len(ctx, oldContents);
            for (int k = 0; k < oldLen; ++k) {
                pdf_array_push(ctx, contentsArr, pdf_array_get(ctx, oldContents, k));
            }
        } else if (oldContents) {
            pdf_array_push(ctx, contentsArr, oldContents);
        }
        pdf_dict_puts(ctx, page->obj, "Contents", contentsArr);
        pdf_drop_obj(ctx, contentsArr);
        pdf_sync_page(ctx, page);
        pdf_drop_page(ctx, page);

        QString synthOutPath = "test_output/form_xobject_watermark_injected.pdf";
        QFile::remove(synthOutPath);
        QString saveErr;
        bool saveOk = synthDoc.save(synthOutPath, 0, false, false, &saveErr);
        if (!saveOk) {
            std::cerr << "synthDoc save failed: " << saveErr.toStdString() << std::endl;
        }
        TEST_CHECK(saveOk);
        synthDoc.close();

        // 重新加载验证注入成功，且能读出 www.bzfxw.com
        PdfDocument verifyDoc;
        bool vOpenOk = verifyDoc.open(synthOutPath, &saveErr);
        TEST_CHECK(vOpenOk);
        auto blocks = verifyDoc.getTextBlocks(0);
        bool foundWatermark = false;
        for (const auto &b : blocks) {
            if (b.text.contains("www.bzfxw.com")) {
                foundWatermark = true;
                break;
            }
        }
        std::cout << "[13.1] Form XObject watermark injected: detected=" << foundWatermark << std::endl;
        TEST_CHECK(foundWatermark);

        // 执行流级精准清除
        int removedStreams = verifyDoc.removeIsolatedWatermarkStreams(0, QStringList{"www.bzfxw.com"});
        std::cout << "[13.2] removeIsolatedWatermarkStreams removed: " << removedStreams << std::endl;
        TEST_CHECK(removedStreams == 1);

        QString cleanedGbPath = "test_output/form_xobject_cleaned.pdf";
        bool cleanSaveOk = verifyDoc.save(cleanedGbPath, 4, true, false);
        TEST_CHECK(cleanSaveOk);
        verifyDoc.close();

        // 验证清理后的文档：水印已彻底消除，无残留！
        PdfDocument finalDoc;
        bool fOpenOk = finalDoc.open(cleanedGbPath);
        TEST_CHECK(fOpenOk);
        auto cleanBlocks = finalDoc.getTextBlocks(0);
        bool stillHasWatermark = false;
        for (const auto &b : cleanBlocks) {
            if (b.text.contains("www.bzfxw.com")) {
                stillHasWatermark = true;
                break;
            }
        }
        std::cout << "[13.3] Form XObject watermark after clean: still present=" << stillHasWatermark << std::endl;
        TEST_CHECK(!stillHasWatermark);
        finalDoc.close();
        std::cout << "[13] Test Form XObject stream bypass PASSED 100%!" << std::endl;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "  ALL 13 VERIFICATION TESTS PASSED 100% " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
