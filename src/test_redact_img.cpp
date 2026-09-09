#include <iostream>
#include <QCoreApplication>
#include "core/PdfDocument.h"

using namespace wipepdf;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    PdfDocument doc;
    QString err;
    if (!doc.open("test_input/test1.pdf", &err)) {
        std::cerr << "Open failed" << std::endl;
        return 1;
    }

    QRectF qrBbox(365, 802, 40, 40); // [365, 802, 405, 842]
    std::cout << "Adding redaction for QR code: " << qrBbox.x() << ", " << qrBbox.y() << std::endl;
    doc.addRedaction(0, qrBbox);

    // Test apply redaction with image_method = PDF_REDACT_IMAGE_REMOVE
    int res = doc.applyRedactions(0);
    std::cout << "applyRedactions returned: " << res << std::endl;

    doc.save("test_output/test1_redacted.pdf");
    doc.close();

    std::cout << "Saved test_output/test1_redacted.pdf" << std::endl;
    return 0;
}
