#include "PdfViewer.h"
#include "core/PdfDocument.h"
#include "i18n/I18n.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <cmath>

namespace wipepdf {

PdfViewer::PdfViewer(QWidget *parent) : QScrollArea(parent) {
    setAcceptDrops(true);
    setAlignment(Qt::AlignCenter);
    setStyleSheet("QScrollArea { background-color: #1a1a22; border: none; }");

    m_canvas = new Canvas(this);
    setWidget(m_canvas);
    setWidgetResizable(false);
}

void PdfViewer::setDocument(PdfDocument *doc) {
    m_doc = doc;
    m_currentPage = 0;
    m_canvas->setDocument(doc);
    if (m_doc && m_doc->isOpen()) {
        emit pageChanged(m_currentPage, m_doc->pageCount());
    } else {
        emit pageChanged(0, 0);
    }
}

void PdfViewer::clear() {
    setDocument(nullptr);
}

void PdfViewer::setCurrentPage(int page) {
    if (!m_doc || !m_doc->isOpen()) return;
    int total = m_doc->pageCount();
    if (page < 0) page = 0;
    if (page >= total) page = total - 1;

    m_currentPage = page;
    m_canvas->updatePage(m_currentPage, m_zoom);
    emit pageChanged(m_currentPage, total);
}

void PdfViewer::setZoom(float zoom) {
    if (zoom < 0.2f) zoom = 0.2f;
    if (zoom > 5.0f) zoom = 5.0f;
    m_zoom = zoom;
    m_canvas->updatePage(m_currentPage, m_zoom);
}

void PdfViewer::zoomIn() {
    setZoom(m_zoom * 1.25f);
}

void PdfViewer::zoomOut() {
    setZoom(m_zoom / 1.25f);
}

void PdfViewer::fitWidth() {
    if (!m_doc || !m_doc->isOpen()) return;
    QRectF pRect = m_doc->pageRect(m_currentPage);
    if (pRect.width() <= 0.0f) return;

    int viewportW = viewport()->width() - 40;
    if (viewportW > 100) {
        setZoom(static_cast<float>(viewportW) / pRect.width());
    }
}

void PdfViewer::setPreviewElements(const std::vector<Element> &elements) {
    m_canvas->setHighlights(elements, {});
}

void PdfViewer::setInteractiveElements(const std::vector<Element> &elements) {
    m_canvas->setHighlights(std::vector<Element>(), elements);
}

void PdfViewer::clearHighlights() {
    m_canvas->setHighlights({}, {});
}

void PdfViewer::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const auto &url : event->mimeData()->urls()) {
            if (url.toLocalFile().endsWith(".pdf", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void PdfViewer::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const auto &url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
                emit fileDropped(path);
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void PdfViewer::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QScrollArea::wheelEvent(event);
    }
}

// ----------------- Canvas Implementation -----------------

PdfViewer::Canvas::Canvas(PdfViewer *viewer)
    : QWidget(viewer), m_viewer(viewer) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void PdfViewer::Canvas::setDocument(PdfDocument *doc) {
    m_doc = doc;
    m_page = 0;
    updatePage(m_page, m_zoom);
}

void PdfViewer::Canvas::updatePage(int page, float zoom) {
    m_page = page;
    m_zoom = zoom;

    if (!m_doc || !m_doc->isOpen()) {
        m_pageImage = QImage();
        resize(0, 0);
        update();
        return;
    }

    m_pageBounds = m_doc->pageRect(m_page);
    int targetW = static_cast<int>(m_pageBounds.width() * m_zoom);
    int targetH = static_cast<int>(m_pageBounds.height() * m_zoom);

    // High quality rendering
    m_pageImage = m_doc->renderPage(m_page, 72.0f * m_zoom, targetW);

    resize(targetW + 40, targetH + 40);
    update();
}

void PdfViewer::Canvas::setHighlights(const std::vector<Element> &preview, const std::vector<Element> &interactive) {
    m_previewElements = preview;
    m_interactiveElements = interactive;
    update();
}

void PdfViewer::setTheme(bool isDark) {
    if (isDark) {
        setStyleSheet("QScrollArea { background-color: #1a1a22; border: none; }");
    } else {
        setStyleSheet("QScrollArea { background-color: #e8ecf1; border: none; }");
    }
    m_canvas->setTheme(isDark);
}

void PdfViewer::Canvas::setTheme(bool isDark) {
    m_isDark = isDark;
    update();
}

void PdfViewer::Canvas::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_pageImage.isNull()) {
        painter.setPen(m_isDark ? QColor(160, 160, 175) : QColor(100, 116, 139));
        painter.drawText(rect(), Qt::AlignCenter, tr_("drag_hint"));
        return;
    }

    int pageX = 20;
    int pageY = 20;
    int pageW = static_cast<int>(m_pageBounds.width() * m_zoom);
    int pageH = static_cast<int>(m_pageBounds.height() * m_zoom);
    QRect pageRect(pageX, pageY, pageW, pageH);

    // Draw shadow
    painter.fillRect(pageRect.adjusted(3, 3, 5, 5), m_isDark ? QColor(0, 0, 0, 120) : QColor(0, 0, 0, 45));

    // Draw white paper base & page content
    painter.fillRect(pageRect, Qt::white);
    painter.drawImage(pageRect, m_pageImage);

    // Draw preview elements (dashed cyan)
    painter.setPen(QPen(QColor(0, 180, 255), 2, Qt::DashLine));
    painter.setBrush(QColor(0, 180, 255, 35));
    for (const auto &el : m_previewElements) {
        if (el.page == m_page) {
            QRectF r(
                pageX + el.bbox.x() * m_zoom,
                pageY + el.bbox.y() * m_zoom,
                el.bbox.width() * m_zoom,
                el.bbox.height() * m_zoom
            );
            painter.drawRect(r);
        }
    }

    // Draw interactive selected elements (solid red)
    painter.setPen(QPen(QColor(255, 45, 60), 2.5, Qt::SolidLine));
    painter.setBrush(QColor(255, 45, 60, 50));
    for (const auto &el : m_interactiveElements) {
        if (el.page == m_page) {
            QRectF r(
                pageX + el.bbox.x() * m_zoom,
                pageY + el.bbox.y() * m_zoom,
                el.bbox.width() * m_zoom,
                el.bbox.height() * m_zoom
            );
            painter.drawRect(r);
        }
    }
}

void PdfViewer::Canvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_doc || !m_doc->isOpen()) {
        QWidget::mousePressEvent(event);
        return;
    }

    int pageX = 20;
    int pageY = 20;
    float clickX = event->position().x() - pageX;
    float clickY = event->position().y() - pageY;

    if (clickX >= 0 && clickX <= m_pageBounds.width() * m_zoom &&
        clickY >= 0 && clickY <= m_pageBounds.height() * m_zoom) {
        QPointF pdfPt(clickX / m_zoom, clickY / m_zoom);
        emit m_viewer->pointClicked(m_page, pdfPt);
    }
}

} // namespace wipepdf
