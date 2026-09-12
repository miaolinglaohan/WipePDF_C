#include "MainWindow.h"
#include "i18n/I18n.h"
#include "core/Detectors.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QShortcut>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFileInfo>

namespace wipepdf {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_doc(std::make_unique<PdfDocument>()) {
    setupUi();
    setupStyles();
    retranslateUi();

    // Undo shortcut Ctrl+Z
    auto *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::onUndoRule);

    // Cleaner progress logging
    m_cleaner.setProgressCallback([this](const QString &msg) {
        QMetaObject::invokeMethod(this, [this, msg]() {
            appendLog(msg);
        });
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    resize(1280, 850);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    setWindowIcon(QIcon(":/icons/app.png"));

    // 1. Top Bar
    auto *topBar = new QWidget(this);
    topBar->setObjectName("topBar");
    topBar->setFixedHeight(50);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 0, 16, 0);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QPixmap(":/icons/app.png").scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    topLayout->addWidget(iconLabel);
    topLayout->addSpacing(8);

    m_brandLabel = new QLabel(this);
    m_brandLabel->setObjectName("brandLabel");
    topLayout->addWidget(m_brandLabel);

    topLayout->addSpacing(30);

    // Navigation buttons
    m_prevBtn = new QPushButton(this);
    m_pageLabel = new QLabel(this);
    m_nextBtn = new QPushButton(this);
    topLayout->addWidget(m_prevBtn);
    topLayout->addWidget(m_pageLabel);
    topLayout->addWidget(m_nextBtn);

    topLayout->addSpacing(20);

    m_zoomOutBtn = new QPushButton(this);
    m_zoomInBtn = new QPushButton(this);
    m_fitWidthBtn = new QPushButton(this);
    topLayout->addWidget(m_zoomOutBtn);
    topLayout->addWidget(m_zoomInBtn);
    topLayout->addWidget(m_fitWidthBtn);

    topLayout->addStretch();

    // Theme toggle button
    m_themeBtn = new QPushButton(this);
    m_themeBtn->setObjectName("themeBtn");
    topLayout->addWidget(m_themeBtn);
    topLayout->addSpacing(6);

    // Language switch button
    m_langBtn = new QPushButton(this);
    m_langBtn->setObjectName("langBtn");
    topLayout->addWidget(m_langBtn);

    rootLayout->addWidget(topBar);

    // 2. Main Body Splitter
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Sidebar
    auto *sidebar = new QWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(410);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 14, 14, 14);
    sideLayout->setSpacing(12);

    // Section: Input & Output
    m_ioGroup = new QGroupBox(this);
    m_ioGroup->setObjectName("ioGroup");
    auto *ioLayout = new QVBoxLayout(m_ioGroup);
    ioLayout->setSpacing(8);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);
    m_choosePdfBtn = new QPushButton(this);
    m_chooseDirBtn = new QPushButton(this);
    m_closeDocBtn = new QPushButton(this);
    btnRow->addWidget(m_choosePdfBtn);
    btnRow->addWidget(m_chooseDirBtn);
    btnRow->addWidget(m_closeDocBtn);
    ioLayout->addLayout(btnRow);

    m_inputPathLabel = new QLabel(this);
    m_inputPathLabel->setWordWrap(true);
    m_inputPathLabel->setStyleSheet("color: #9aa0a6; font-size: 11px;");
    ioLayout->addWidget(m_inputPathLabel);

    // Output mode
    auto *outModeRow = new QHBoxLayout();
    m_saveAsRadio = new QRadioButton(this);
    m_saveAsRadio->setChecked(true);
    m_overwriteRadio = new QRadioButton(this);
    outModeRow->addWidget(m_saveAsRadio);
    outModeRow->addWidget(m_overwriteRadio);
    ioLayout->addLayout(outModeRow);

    auto *customDirRow = new QHBoxLayout();
    m_outputDirEdit = new QLineEdit(this);
    m_browseOutputBtn = new QPushButton("...", this);
    m_browseOutputBtn->setFixedWidth(36);
    customDirRow->addWidget(m_outputDirEdit);
    customDirRow->addWidget(m_browseOutputBtn);
    ioLayout->addLayout(customDirRow);

    sideLayout->addWidget(m_ioGroup);

    // Section: Mode Selector
    m_modeGroup = new QGroupBox(this);
    m_modeGroup->setObjectName("modeGroup");
    auto *modeGroupLayout = new QVBoxLayout(m_modeGroup);
    auto *modeRow = new QHBoxLayout();
    m_autoModeRadio = new QRadioButton(this);
    m_autoModeRadio->setChecked(true);
    m_interactiveModeRadio = new QRadioButton(this);
    modeRow->addWidget(m_autoModeRadio);
    modeRow->addWidget(m_interactiveModeRadio);
    modeGroupLayout->addLayout(modeRow);

    m_modeStack = new QStackedWidget(this);

    // Auto Mode Page
    auto *autoPage = new QWidget(this);
    auto *autoLayout = new QVBoxLayout(autoPage);
    autoLayout->setContentsMargins(0, 8, 0, 0);

    m_chkLinks = new QCheckBox(this);
    m_chkLinks->setChecked(true);
    m_linkRegexEdit = new QLineEdit(this);

    m_chkBottom = new QCheckBox(this);
    auto *bottomRow = new QHBoxLayout();
    m_bottomHeightLabel = new QLabel(this);
    m_bottomHeightSpin = new QSpinBox(this);
    m_bottomHeightSpin->setRange(10, 400);
    m_bottomHeightSpin->setValue(60);
    bottomRow->addWidget(m_chkBottom);
    bottomRow->addStretch();
    bottomRow->addWidget(m_bottomHeightLabel);
    bottomRow->addWidget(m_bottomHeightSpin);

    m_chkText = new QCheckBox(this);
    m_chkText->setChecked(true);
    m_textRegexEdit = new QLineEdit(this);
    m_textRegexEdit->setText("水印|https?://\\S+|www\\.\\S+|(?:[a-zA-Z0-9-]+\\.)+(?:com|net|org|cn|cc|top|xyz|site|vip|club)\\b");

    m_chkOverlays = new QCheckBox(this);
    m_chkOverlays->setChecked(true);

    autoLayout->addWidget(m_chkLinks);
    autoLayout->addWidget(m_linkRegexEdit);
    autoLayout->addLayout(bottomRow);
    autoLayout->addWidget(m_chkText);
    autoLayout->addWidget(m_textRegexEdit);
    autoLayout->addWidget(m_chkOverlays);
    autoLayout->addStretch();

    m_modeStack->addWidget(autoPage);

    // Interactive Mode Page
    auto *interactivePage = new QWidget(this);
    auto *interactiveLayout = new QVBoxLayout(interactivePage);
    interactiveLayout->setContentsMargins(0, 8, 0, 0);

    auto *matchRow = new QHBoxLayout();
    m_matchModeLabel = new QLabel(this);
    m_matchModeCombo = new QComboBox(this);
    matchRow->addWidget(m_matchModeLabel);
    matchRow->addWidget(m_matchModeCombo);
    interactiveLayout->addLayout(matchRow);

    m_interactiveHint = new QLabel(this);
    m_interactiveHint->setWordWrap(true);
    m_interactiveHint->setStyleSheet("color: #8ab4f8; font-size: 11px; margin-top: 4px;");
    interactiveLayout->addWidget(m_interactiveHint);

    m_rulesCountLabel = new QLabel(this);
    interactiveLayout->addWidget(m_rulesCountLabel);

    m_rulesListWidget = new QListWidget(this);
    m_rulesListWidget->setFixedHeight(120);
    interactiveLayout->addWidget(m_rulesListWidget);

    auto *ruleBtnRow = new QHBoxLayout();
    m_undoRuleBtn = new QPushButton(this);
    m_clearRulesBtn = new QPushButton(this);
    ruleBtnRow->addWidget(m_undoRuleBtn);
    ruleBtnRow->addWidget(m_clearRulesBtn);
    interactiveLayout->addLayout(ruleBtnRow);

    m_modeStack->addWidget(interactivePage);
    modeGroupLayout->addWidget(m_modeStack);
    sideLayout->addWidget(m_modeGroup);

    // Section: Actions
    auto *actionRow = new QHBoxLayout();
    m_previewBtn = new QPushButton(this);
    m_processBtn = new QPushButton(this);
    m_processBtn->setObjectName("processBtn");
    actionRow->addWidget(m_previewBtn);
    actionRow->addWidget(m_processBtn);
    sideLayout->addLayout(actionRow);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    sideLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #20c997; font-weight: bold; font-size: 12px;");
    sideLayout->addWidget(m_statusLabel);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setFixedHeight(140);
    sideLayout->addWidget(m_logEdit);

    splitter->addWidget(sidebar);

    // Right Canvas
    m_viewer = new PdfViewer(this);
    splitter->addWidget(m_viewer);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(splitter);

    // Connections
    connect(m_choosePdfBtn, &QPushButton::clicked, this, &MainWindow::onChoosePdf);
    connect(m_chooseDirBtn, &QPushButton::clicked, this, &MainWindow::onChooseDir);
    connect(m_closeDocBtn, &QPushButton::clicked, this, &MainWindow::onCloseDoc);
    connect(m_browseOutputBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(m_autoModeRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    connect(m_interactiveModeRadio, &QRadioButton::toggled, this, &MainWindow::onModeChanged);
    connect(m_undoRuleBtn, &QPushButton::clicked, this, &MainWindow::onUndoRule);
    connect(m_clearRulesBtn, &QPushButton::clicked, this, &MainWindow::onClearRules);
    connect(m_previewBtn, &QPushButton::clicked, this, &MainWindow::onPreviewDetection);
    connect(m_processBtn, &QPushButton::clicked, this, &MainWindow::onStartProcess);
    connect(m_themeBtn, &QPushButton::clicked, this, &MainWindow::onSwitchTheme);
    connect(m_langBtn, &QPushButton::clicked, this, &MainWindow::onSwitchLanguage);

    connect(m_prevBtn, &QPushButton::clicked, [this]() { m_viewer->setCurrentPage(m_viewer->currentPage() - 1); });
    connect(m_nextBtn, &QPushButton::clicked, [this]() { m_viewer->setCurrentPage(m_viewer->currentPage() + 1); });
    connect(m_zoomInBtn, &QPushButton::clicked, m_viewer, &PdfViewer::zoomIn);
    connect(m_zoomOutBtn, &QPushButton::clicked, m_viewer, &PdfViewer::zoomOut);
    connect(m_fitWidthBtn, &QPushButton::clicked, m_viewer, &PdfViewer::fitWidth);

    connect(m_viewer, &PdfViewer::pageChanged, this, &MainWindow::onViewerPageChanged);
    connect(m_viewer, &PdfViewer::pointClicked, this, &MainWindow::onViewerPointClicked);
    connect(m_viewer, &PdfViewer::fileDropped, this, &MainWindow::onViewerFileDropped);
}

void MainWindow::setupStyles() {
    if (m_theme == Theme::Dark) {
        QString qss = R"(
            QMainWindow, QWidget {
                background-color: #1a1a22;
                color: #e2e4ec;
                font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
                font-size: 13px;
            }
            #topBar {
                background-color: #22222c;
                border-bottom: 1px solid #323242;
            }
            #brandLabel {
                font-size: 16px;
                font-weight: bold;
                color: #4da3ff;
            }
            #sidebar {
                background-color: #1f1f28;
                border-right: 1px solid #323242;
            }
            QGroupBox {
                border: 1px solid #363648;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 12px;
                font-weight: bold;
                color: #b0b4c8;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 4px;
            }
            QPushButton {
                background-color: #2b2b38;
                color: #f0f2fa;
                border: 1px solid #424255;
                border-radius: 5px;
                padding: 5px 8px;
            }
            QPushButton:hover {
                background-color: #363648;
                border-color: #4da3ff;
            }
            QPushButton:pressed {
                background-color: #20202a;
            }
            QPushButton:disabled {
                background-color: #22222c;
                color: #636375;
                border-color: #323242;
            }
            #processBtn {
                background-color: #007acc;
                border-color: #007acc;
                font-weight: bold;
                color: #ffffff;
            }
            #processBtn:hover {
                background-color: #0098ff;
                border-color: #0098ff;
            }
            QLineEdit, QSpinBox, QComboBox, QListWidget, QTextEdit {
                background-color: #262634;
                color: #e2e4ec;
                border: 1px solid #3e3e52;
                border-radius: 4px;
                padding: 4px 8px;
            }
            QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus {
                border-color: #4da3ff;
            }
            QProgressBar {
                background-color: #262634;
                border-radius: 3px;
            }
            QProgressBar::chunk {
                background-color: #007acc;
                border-radius: 3px;
            }
            QCheckBox, QRadioButton {
                color: #e2e4ec;
                spacing: 7px;
            }
            QCheckBox::indicator {
                width: 16px;
                height: 16px;
                border: 2px solid #7c7c96;
                border-radius: 4px;
                background-color: #262634;
            }
            QCheckBox::indicator:hover {
                border-color: #4da3ff;
                background-color: #303042;
            }
            QCheckBox::indicator:checked {
                background-color: #007acc;
                border: 2px solid #007acc;
                image: url(:/icons/check_white.png);
            }
            QRadioButton::indicator {
                width: 16px;
                height: 16px;
                border: 2px solid #7c7c96;
                border-radius: 9px;
                background-color: #262634;
            }
            QRadioButton::indicator:hover {
                border-color: #4da3ff;
                background-color: #303042;
            }
            QRadioButton::indicator:checked {
                border: 2px solid #007acc;
                background-color: #262634;
                image: url(:/icons/radio_dot_blue.png);
            }
            QScrollBar:vertical {
                background: #1a1a22;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: #3e3e52;
                min-height: 20px;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical:hover {
                background: #565672;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )";
        setStyleSheet(qss);
        m_viewer->setTheme(true);
    } else {
        // Light Theme
        QString qss = R"(
            QMainWindow, QWidget {
                background-color: #f1f5f9;
                color: #1e293b;
                font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
                font-size: 13px;
            }
            #topBar {
                background-color: #ffffff;
                border-bottom: 1px solid #e2e8f0;
            }
            #brandLabel {
                font-size: 16px;
                font-weight: bold;
                color: #0284c7;
            }
            #sidebar {
                background-color: #ffffff;
                border-right: 1px solid #e2e8f0;
            }
            QGroupBox {
                border: 1px solid #cbd5e1;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 12px;
                font-weight: bold;
                color: #334155;
                background-color: #f8fafc;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 4px;
            }
            QPushButton {
                background-color: #ffffff;
                color: #1e293b;
                border: 1px solid #cbd5e1;
                border-radius: 5px;
                padding: 5px 8px;
            }
            QPushButton:hover {
                background-color: #f1f5f9;
                border-color: #0284c7;
            }
            QPushButton:pressed {
                background-color: #e2e8f0;
            }
            QPushButton:disabled {
                background-color: #f8fafc;
                color: #94a3b8;
                border-color: #e2e8f0;
            }
            #processBtn {
                background-color: #0284c7;
                border-color: #0284c7;
                font-weight: bold;
                color: #ffffff;
            }
            #processBtn:hover {
                background-color: #0369a1;
                border-color: #0369a1;
            }
            QLineEdit, QSpinBox, QComboBox, QListWidget, QTextEdit {
                background-color: #ffffff;
                color: #1e293b;
                border: 1px solid #cbd5e1;
                border-radius: 4px;
                padding: 4px 8px;
            }
            QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus {
                border-color: #0284c7;
            }
            QProgressBar {
                background-color: #e2e8f0;
                border-radius: 3px;
            }
            QProgressBar::chunk {
                background-color: #0284c7;
                border-radius: 3px;
            }
            QCheckBox, QRadioButton {
                color: #1e293b;
                spacing: 7px;
            }
            QCheckBox::indicator {
                width: 16px;
                height: 16px;
                border: 2px solid #94a3b8;
                border-radius: 4px;
                background-color: #ffffff;
            }
            QCheckBox::indicator:hover {
                border-color: #0284c7;
                background-color: #f8fafc;
            }
            QCheckBox::indicator:checked {
                background-color: #0284c7;
                border: 2px solid #0284c7;
                image: url(:/icons/check_white.png);
            }
            QRadioButton::indicator {
                width: 16px;
                height: 16px;
                border: 2px solid #94a3b8;
                border-radius: 9px;
                background-color: #ffffff;
            }
            QRadioButton::indicator:hover {
                border-color: #0284c7;
                background-color: #f8fafc;
            }
            QRadioButton::indicator:checked {
                border: 2px solid #0284c7;
                background-color: #ffffff;
                image: url(:/icons/radio_dot_blue.png);
            }
            QScrollBar:vertical {
                background: #f1f5f9;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: #cbd5e1;
                min-height: 20px;
                border-radius: 5px;
            }
            QScrollBar::handle:vertical:hover {
                background: #94a3b8;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )";
        setStyleSheet(qss);
        m_viewer->setTheme(false);
    }
}

void MainWindow::onSwitchTheme() {
    if (m_theme == Theme::Dark) {
        m_theme = Theme::Light;
    } else {
        m_theme = Theme::Dark;
    }
    setupStyles();
    m_themeBtn->setText(m_theme == Theme::Dark ? tr_("theme_light") : tr_("theme_dark"));
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr_("app_title"));
    m_brandLabel->setText(tr_("app_brand"));

    // Group box headers
    m_ioGroup->setTitle(tr_("input_frame"));
    m_modeGroup->setTitle(tr_("mode_frame"));

    // Top action buttons
    m_choosePdfBtn->setText(tr_("btn_choose_pdf"));
    m_choosePdfBtn->setToolTip(tr_("tip_choose_pdf"));
    m_chooseDirBtn->setText(tr_("btn_choose_dir"));
    m_chooseDirBtn->setToolTip(tr_("tip_choose_dir"));
    m_closeDocBtn->setText(tr_("btn_close"));
    m_closeDocBtn->setToolTip(tr_("tip_close_doc"));

    // Radio modes
    m_saveAsRadio->setText(tr_("btn_save_as"));
    m_overwriteRadio->setText(tr_("btn_overwrite"));
    m_autoModeRadio->setText(tr_("mode_auto"));
    m_interactiveModeRadio->setText(tr_("mode_interactive"));

    // Auto Mode options
    m_chkLinks->setText(tr_("opt_remove_all_links"));
    m_chkBottom->setText(tr_("opt_remove_bottom"));
    m_bottomHeightLabel->setText(tr_("label_height"));
    m_chkText->setText(tr_("opt_text_regex"));
    m_chkOverlays->setText(tr_("opt_detect_overlays"));

    // Placeholders
    m_outputDirEdit->setPlaceholderText(tr_("custom_output_placeholder"));
    m_linkRegexEdit->setPlaceholderText(tr_("link_regex_placeholder"));
    m_textRegexEdit->setPlaceholderText(tr_("text_regex_placeholder"));

    // Default text regex
    QString currentRegex = m_textRegexEdit->text().trimmed();
    if (currentRegex.isEmpty() || currentRegex == "水印|www\\..*?\\.com" || currentRegex == "watermark|www\\..*?\\.com" ||
        currentRegex.startsWith("水印|https?://") || currentRegex.startsWith("watermark|https?://")) {
        m_textRegexEdit->setText(tr_("default_text_regex"));
    }

    // Input path label
    if (m_currentInputPath.isEmpty()) {
        m_inputPathLabel->setText(tr_("no_file_selected"));
    } else if (m_isBatch) {
        m_inputPathLabel->setText(tr_("dir_prefix").arg(m_currentInputPath));
    } else {
        m_inputPathLabel->setText(QFileInfo(m_currentInputPath).fileName());
    }

    // Match mode combobox
    QString curMatchKey = m_matchModeCombo->currentData().toString();
    if (curMatchKey.isEmpty()) curMatchKey = "auto";
    m_matchModeCombo->blockSignals(true);
    m_matchModeCombo->clear();
    m_matchModeCombo->addItem(tr_("match_auto"), "auto");
    m_matchModeCombo->addItem(tr_("match_position"), "position");
    m_matchModeCombo->addItem(tr_("match_region"), "region");
    m_matchModeCombo->addItem(tr_("match_text"), "text");
    m_matchModeCombo->addItem(tr_("match_url"), "url");
    int matchIdx = m_matchModeCombo->findData(curMatchKey);
    if (matchIdx >= 0) m_matchModeCombo->setCurrentIndex(matchIdx);
    m_matchModeCombo->blockSignals(false);

    m_matchModeLabel->setText(tr_("match_mode_label"));
    m_undoRuleBtn->setText(tr_("btn_undo_rule"));
    m_clearRulesBtn->setText(tr_("btn_clear_rules"));
    m_interactiveHint->setText(tr_("hint_interactive"));
    m_rulesCountLabel->setText(tr_("rules_count").arg(m_cleaner.interactiveRules().size()));

    m_previewBtn->setText(tr_("btn_preview"));
    m_processBtn->setText(tr_("btn_process"));

    // Top Navigation
    m_prevBtn->setText(tr_("btn_prev"));
    m_nextBtn->setText(tr_("btn_next"));
    m_zoomInBtn->setText(tr_("btn_zoom_in"));
    m_zoomOutBtn->setText(tr_("btn_zoom_out"));
    m_fitWidthBtn->setText(tr_("btn_fit_width"));
    m_langBtn->setText(tr_("switch_lang"));
    m_themeBtn->setText(m_theme == Theme::Dark ? tr_("theme_light") : tr_("theme_dark"));

    int curPage = (m_doc && m_doc->isOpen()) ? (m_viewer->currentPage() + 1) : 0;
    int totPage = (m_doc && m_doc->isOpen()) ? m_doc->pageCount() : 0;
    m_pageLabel->setText(tr_("page_nav").arg(curPage).arg(totPage));

    if (m_statusLabel->text().isEmpty() || m_statusLabel->text() == "就绪" || m_statusLabel->text() == "Ready") {
        m_statusLabel->setText(tr_("ready"));
    }
}

void MainWindow::onSwitchLanguage() {
    if (I18n::instance().currentLanguage() == Language::Zh) {
        I18n::instance().setLanguage(Language::En);
    } else {
        I18n::instance().setLanguage(Language::Zh);
    }
    retranslateUi();
}

void MainWindow::openPdf(const QString &filePath) {
    closePdf();
    QString err;
    if (!m_doc->open(filePath, &err)) {
        QMessageBox::critical(this, tr_("title_error"), QString("无法打开 PDF 文件:\n%1").arg(err));
        return;
    }

    m_isBatch = false;
    m_currentInputPath = filePath;
    m_inputPathLabel->setText(QFileInfo(filePath).fileName());
    m_viewer->setDocument(m_doc.get());
    m_statusLabel->setText(tr_("ready"));
    appendLog(tr_("log_doc_loaded").arg(filePath).arg(m_doc->pageCount()));
}

void MainWindow::closePdf() {
    m_viewer->clear();
    if (m_doc && m_doc->isOpen()) {
        m_doc->close();
    }
    m_currentInputPath.clear();
    m_inputPathLabel->setText(tr_("no_file_selected"));
    m_pageLabel->setText(tr_("page_nav").arg(0).arg(0));
}

void MainWindow::onChoosePdf() {
    QString path = QFileDialog::getOpenFileName(this, tr_("dialog_choose_pdf"), "", "PDF Files (*.pdf)");
    if (!path.isEmpty()) {
        openPdf(path);
    }
}

void MainWindow::onChooseDir() {
    QString dir = QFileDialog::getExistingDirectory(this, tr_("dialog_choose_dir"));
    if (!dir.isEmpty()) {
        closePdf();
        m_isBatch = true;
        m_currentInputPath = dir;
        m_inputPathLabel->setText(tr_("dir_prefix").arg(dir));
        appendLog(tr_("log_batch_selected").arg(dir));
    }
}

void MainWindow::onCloseDoc() {
    closePdf();
    appendLog(tr_("log_doc_closed"));
}

void MainWindow::onBrowseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, tr_("dialog_choose_out_dir"));
    if (!dir.isEmpty()) {
        m_outputDirEdit->setText(dir);
        m_customOutputDir = dir;
    }
}

void MainWindow::onModeChanged() {
    if (m_autoModeRadio->isChecked()) {
        m_modeStack->setCurrentIndex(0);
    } else {
        m_modeStack->setCurrentIndex(1);
    }
}

void MainWindow::onUndoRule() {
    if (m_cleaner.popInteractiveRule()) {
        int count = m_rulesListWidget->count();
        if (count > 0) {
            delete m_rulesListWidget->takeItem(count - 1);
        }
        m_rulesCountLabel->setText(tr_("rules_count").arg(m_cleaner.interactiveRules().size()));
        refreshInteractiveHighlights();
        appendLog(tr_("log_rule_undone"));
    }
}

void MainWindow::onClearRules() {
    m_cleaner.clearInteractiveRules();
    m_rulesListWidget->clear();
    m_rulesCountLabel->setText(tr_("rules_count").arg(0));
    m_viewer->clearHighlights();
    appendLog(tr_("log_rules_cleared"));
}

void MainWindow::onViewerPageChanged(int pageIdx, int totalPages) {
    if (totalPages > 0) {
        m_pageLabel->setText(tr_("page_nav").arg(pageIdx + 1).arg(totalPages));
    } else {
        m_pageLabel->setText(tr_("page_nav").arg(0).arg(0));
    }
}

void MainWindow::onViewerFileDropped(const QString &filePath) {
    openPdf(filePath);
}

void MainWindow::onViewerPointClicked(int pageIdx, const QPointF &pdfPt) {
    if (!m_interactiveModeRadio->isChecked() || !m_doc || !m_doc->isOpen()) {
        return;
    }

    auto picked = Detectors::pickElementAt(*m_doc, pageIdx, pdfPt);
    if (picked.empty()) {
        appendLog(tr_("log_no_element_picked").arg(pdfPt.x(), 0, 'f', 1).arg(pdfPt.y(), 0, 'f', 1));
        return;
    }

    // If multiple overlapping elements, show context menu
    Element targetElement = picked[0];
    if (picked.size() > 1) {
        QMenu menu(this);
        for (size_t i = 0; i < picked.size(); ++i) {
            auto *act = menu.addAction(picked[i].summary());
            act->setData(static_cast<int>(i));
        }
        QAction *chosen = menu.exec(QCursor::pos());
        if (!chosen) return;
        targetElement = picked[chosen->data().toInt()];
    }

    QString mode = m_matchModeCombo->currentData().toString();
    MatchRule rule = Matcher::createRuleFromElement(*m_doc, pageIdx, targetElement, mode);
    m_cleaner.addInteractiveRule(rule);

    m_rulesListWidget->addItem(rule.describe());
    m_rulesCountLabel->setText(tr_("rules_count").arg(m_cleaner.interactiveRules().size()));

    refreshInteractiveHighlights();
    appendLog(tr_("log_rule_added").arg(rule.describe()));
}

void MainWindow::refreshInteractiveHighlights() {
    if (!m_doc || !m_doc->isOpen()) return;
    auto matches = m_cleaner.previewInteractive(*m_doc);
    m_viewer->setInteractiveElements(matches);
}

void MainWindow::onPreviewDetection() {
    if (!m_doc || !m_doc->isOpen()) {
        QMessageBox::warning(this, tr_("title_notice"), tr_("msg_no_pdf"));
        return;
    }

    if (m_interactiveModeRadio->isChecked()) {
        refreshInteractiveHighlights();
        auto matches = m_cleaner.previewInteractive(*m_doc);
        appendLog(tr_("log_preview_interactive").arg(matches.size()));
    } else {
        AutoOptions opts;
        opts.removeAllLinks = m_chkLinks->isChecked();
        opts.linkUrlPattern = m_linkRegexEdit->text().trimmed();
        opts.removeBottomStrip = m_chkBottom->isChecked();
        opts.bottomStripHeight = static_cast<float>(m_bottomHeightSpin->value());
        opts.removeByTextPattern = m_chkText->isChecked();
        opts.textPattern = m_textRegexEdit->text().trimmed();
        opts.detectTransparentOverlays = m_chkOverlays->isChecked();

        auto detected = m_cleaner.previewAuto(*m_doc, opts);
        m_viewer->setPreviewElements(detected);
        appendLog(tr_("log_preview_auto").arg(detected.size()));
    }
}

void MainWindow::onStartProcess() {
    if (m_currentInputPath.isEmpty()) {
        QMessageBox::warning(this, tr_("title_notice"), tr_("msg_no_pdf"));
        return;
    }

    AutoOptions opts;
    opts.removeAllLinks = m_chkLinks->isChecked();
    opts.linkUrlPattern = m_linkRegexEdit->text().trimmed();
    opts.removeBottomStrip = m_chkBottom->isChecked();
    opts.bottomStripHeight = static_cast<float>(m_bottomHeightSpin->value());
    opts.removeByTextPattern = m_chkText->isChecked();
    opts.textPattern = m_textRegexEdit->text().trimmed();
    opts.detectTransparentOverlays = m_chkOverlays->isChecked();

    bool useInteractive = m_interactiveModeRadio->isChecked();
    bool overwrite = m_overwriteRadio->isChecked();
    QString outDir = m_outputDirEdit->text().trimmed();

    m_progressBar->setValue(20);
    m_statusLabel->setText(tr_("status_cleaning"));
    m_processBtn->setEnabled(false);

    // If single file & overwrite: close viewer first to release Windows file lock!
    QString originalFile = m_currentInputPath;
    if (!m_isBatch && overwrite) {
        m_viewer->clear();
        m_doc->close();
    }

    (void)QtConcurrent::run([this, originalFile, outDir, opts, useInteractive, overwrite]() {
        if (m_isBatch) {
            QString targetOut = outDir.isEmpty() ? m_currentInputPath + "_cleaned" : outDir;
            m_cleaner.processBatch(m_currentInputPath, targetOut, opts, useInteractive, true, overwrite);
            QMetaObject::invokeMethod(this, [this]() {
                m_progressBar->setValue(100);
                m_statusLabel->setText(tr_("msg_complete"));
                m_processBtn->setEnabled(true);
                QMessageBox::information(this, tr_("title_complete"), tr_("batch_complete_msg"));
            });
        } else {
            QString outPath = outDir.isEmpty() ? originalFile : outDir;
            CleanerResult res = m_cleaner.process(originalFile, outPath, opts, useInteractive, overwrite);
            QMetaObject::invokeMethod(this, [this, res, originalFile, overwrite]() {
                m_progressBar->setValue(100);
                m_processBtn->setEnabled(true);
                if (res.success) {
                    m_statusLabel->setText(tr_("msg_complete"));
                    appendLog(res.message);
                    // Reopen cleaned file
                    openPdf(overwrite ? originalFile : res.outputPath);
                    QMessageBox::information(this, tr_("title_complete"), res.message);
                } else {
                    m_statusLabel->setText(tr_("status_failed"));
                    QMessageBox::critical(this, tr_("title_error"), res.message);
                }
            });
        }
    });
}

void MainWindow::appendLog(const QString &msg) {
    m_logEdit->append(msg);
}

} // namespace wipepdf
