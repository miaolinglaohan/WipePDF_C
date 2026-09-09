#include <iostream>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QImage>
#include <QDebug>

int main(int argc, char *argv[]) {
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!ctx) {
        std::cerr << "Failed to init context" << std::endl;
        return 1;
    }
    fz_register_document_handlers(ctx);

    const char *filepath = "test_input/sample_watermark.pdf";
    fz_document *doc = NULL;
    fz_try(ctx) {
        doc = fz_open_document(ctx, filepath);
    } fz_catch(ctx) {
        std::cerr << "Cannot open document: " << fz_caught_message(ctx) << std::endl;
        fz_drop_context(ctx);
        return 1;
    }

    int count = fz_count_pages(ctx, doc);
    std::cout << "Loaded document successfully! Page count: " << count << std::endl;

    if (count > 0) {
        fz_page *page = fz_load_page(ctx, doc, 0);
        fz_rect bounds = fz_bound_page(ctx, page);
        std::cout << "Page 0 bounds: [" << bounds.x0 << ", " << bounds.y0 << ", " 
                  << bounds.x1 << ", " << bounds.y1 << "]" << std::endl;

        float zoom = 150.0f / 72.0f;
        fz_matrix ctm = fz_scale(zoom, zoom);
        fz_colorspace *cs = fz_device_rgb(ctx);
        fz_pixmap *pix = fz_new_pixmap_from_page(ctx, page, ctm, cs, 0);

        int w = fz_pixmap_width(ctx, pix);
        int h = fz_pixmap_height(ctx, pix);
        int stride = fz_pixmap_stride(ctx, pix);
        unsigned char *samples = fz_pixmap_samples(ctx, pix);

        std::cout << "Rendered pixmap: " << w << "x" << h << " stride: " << stride << std::endl;

        QImage qimg(samples, w, h, stride, QImage::Format_RGB888);
        bool saved = qimg.copy().save("test_page0.png");
        std::cout << "Saved test_page0.png: " << (saved ? "SUCCESS" : "FAILED") << std::endl;

        fz_drop_pixmap(ctx, pix);
        fz_drop_page(ctx, page);
    }

    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    return 0;
}
