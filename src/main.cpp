#include <QApplication>
#include <QStyleFactory>
#include <QIcon>
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("WipePDF");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("WipePDF");
    app.setWindowIcon(QIcon(":/icons/app.png"));

    // Enable modern Fusion style
    app.setStyle(QStyleFactory::create("Fusion"));

    wipepdf::MainWindow window;
    window.show();

    // If a PDF file path is passed as command line argument, open it immediately
    if (argc > 1) {
        QString file = QString::fromLocal8Bit(argv[1]);
        if (file.endsWith(".pdf", Qt::CaseInsensitive)) {
            window.openPdf(file);
        }
    }

    return app.exec();
}
