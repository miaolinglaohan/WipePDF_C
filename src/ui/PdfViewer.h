#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QPixmap>
#include <vector>
#include "core/Element.h"

namespace wipepdf {

class PdfDocument;

class PdfViewer : public QScrollArea {
    Q_OBJECT

public:
    explicit PdfViewer(QWidget *parent = nullptr);

    void setDocument(PdfDocument *doc);
    void clear();

    int currentPage() const { return m_currentPage; }
    void setCurrentPage(int page);

    float zoom() const { return m_zoom; }
    void setZoom(float zoom);
    void zoomIn();
    void zoomOut();
    void fitWidth();

    void setPreviewElements(const std::vector<Element> &elements);
    void setInteractiveElements(const std::vector<Element> &elements);
    void clearHighlights();

signals:
    void pointClicked(int pageIdx, const QPointF &pdfPoint);
    void pageChanged(int pageIdx, int totalPages);
    void fileDropped(const QString &filePath);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    class Canvas;
    Canvas *m_canvas = nullptr;
    PdfDocument *m_doc = nullptr;
    int m_currentPage = 0;
    float m_zoom = 1.0f;
};

class PdfViewer::Canvas : public QWidget {
    Q_OBJECT

public:
    explicit Canvas(PdfViewer *viewer);

    void setDocument(PdfDocument *doc);
    void updatePage(int page, float zoom);
    void setHighlights(const std::vector<Element> &preview, const std::vector<Element> &interactive);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    PdfViewer *m_viewer = nullptr;
    PdfDocument *m_doc = nullptr;
    int m_page = 0;
    float m_zoom = 1.0f;
    QImage m_pageImage;
    QRectF m_pageBounds;
    std::vector<Element> m_previewElements;
    std::vector<Element> m_interactiveElements;
};

} // namespace wipepdf
