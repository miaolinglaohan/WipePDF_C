#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <memory>
#include "PdfViewer.h"
#include "core/PdfDocument.h"
#include "core/WatermarkCleaner.h"

namespace wipepdf {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openPdf(const QString &filePath);
    void closePdf();

private slots:
    void onChoosePdf();
    void onChooseDir();
    void onCloseDoc();
    void onBrowseOutputDir();
    void onModeChanged();
    void onUndoRule();
    void onClearRules();
    void onPreviewDetection();
    void onStartProcess();
    void onSwitchLanguage();
    void onViewerPointClicked(int pageIdx, const QPointF &pdfPt);
    void onViewerPageChanged(int pageIdx, int totalPages);
    void onViewerFileDropped(const QString &filePath);

private:
    void setupUi();
    void setupStyles();
    void retranslateUi();
    void refreshInteractiveHighlights();
    void appendLog(const QString &msg);

    // Core document & cleaner
    std::unique_ptr<PdfDocument> m_doc;
    WatermarkCleaner m_cleaner;
    bool m_isBatch = false;
    QString m_currentInputPath;
    QString m_customOutputDir;

    // UI Widgets
    QLabel *m_brandLabel = nullptr;
    QPushButton *m_langBtn = nullptr;

    // Top navigation
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_zoomOutBtn = nullptr;
    QPushButton *m_zoomInBtn = nullptr;
    QPushButton *m_fitWidthBtn = nullptr;

    // Sidebar Widgets
    QLabel *m_inputFrameTitle = nullptr;
    QPushButton *m_choosePdfBtn = nullptr;
    QPushButton *m_chooseDirBtn = nullptr;
    QPushButton *m_closeDocBtn = nullptr;
    QLabel *m_inputPathLabel = nullptr;

    QLabel *m_outputFrameTitle = nullptr;
    QRadioButton *m_saveAsRadio = nullptr;
    QRadioButton *m_overwriteRadio = nullptr;
    QLineEdit *m_outputDirEdit = nullptr;
    QPushButton *m_browseOutputBtn = nullptr;

    QLabel *m_modeFrameTitle = nullptr;
    QRadioButton *m_autoModeRadio = nullptr;
    QRadioButton *m_interactiveModeRadio = nullptr;
    QStackedWidget *m_modeStack = nullptr;

    // Auto Mode Panel
    QCheckBox *m_chkLinks = nullptr;
    QLineEdit *m_linkRegexEdit = nullptr;
    QCheckBox *m_chkBottom = nullptr;
    QSpinBox *m_bottomHeightSpin = nullptr;
    QCheckBox *m_chkText = nullptr;
    QLineEdit *m_textRegexEdit = nullptr;
    QCheckBox *m_chkOverlays = nullptr;

    // Interactive Mode Panel
    QLabel *m_matchModeLabel = nullptr;
    QComboBox *m_matchModeCombo = nullptr;
    QLabel *m_rulesCountLabel = nullptr;
    QPushButton *m_undoRuleBtn = nullptr;
    QPushButton *m_clearRulesBtn = nullptr;
    QListWidget *m_rulesListWidget = nullptr;
    QLabel *m_interactiveHint = nullptr;

    // Actions
    QPushButton *m_previewBtn = nullptr;
    QPushButton *m_processBtn = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTextEdit *m_logEdit = nullptr;

    // Center Viewer
    PdfViewer *m_viewer = nullptr;
};

} // namespace wipepdf
