#include <iostream>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <QCoreApplication>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if (!ctx) {
        std::cerr << "Failed to create fz_context" << std::endl;
        return 1;
    }
    fz_register_document_handlers(ctx);
    std::cout << "MuPDF & Qt6 initialized successfully!" << std::endl;
    fz_drop_context(ctx);
    return 0;
}
