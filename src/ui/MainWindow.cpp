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
    m_pageLabel = new QLabel("第 0 / 0 页", this);
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

    m_langBtn = new QPushButton("English", this);
    m_langBtn->setObjectName("langBtn");
    topLayout->addWidget(m_langBtn);

    rootLayout->addWidget(topBar);

    // 2. Main Body Splitter
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Sidebar
    auto *sidebar = new QWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(380);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 14, 14, 14);
    sideLayout->setSpacing(12);

    // Section: Input & Output
    auto *ioGroup = new QGroupBox(this);
    ioGroup->setObjectName("ioGroup");
    auto *ioLayout = new QVBoxLayout(ioGroup);

    auto *btnRow = new QHBoxLayout();
    m_choosePdfBtn = new QPushButton(this);
    m_chooseDirBtn = new QPushButton(this);
    m_closeDocBtn = new QPushButton(this);
    btnRow->addWidget(m_choosePdfBtn);
    btnRow->addWidget(m_chooseDirBtn);
    btnRow->addWidget(m_closeDocBtn);
    ioLayout->addLayout(btnRow);

    m_inputPathLabel = new QLabel("未选择文件", this);
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
    m_outputDirEdit->setPlaceholderText("自定义输出目录 (可选)");
    m_browseOutputBtn = new QPushButton("...", this);
    m_browseOutputBtn->setFixedWidth(36);
    customDirRow->addWidget(m_outputDirEdit);
    customDirRow->addWidget(m_browseOutputBtn);
    ioLayout->addLayout(customDirRow);

    sideLayout->addWidget(ioGroup);

    // Section: Mode Selector
    auto *modeGroup = new QGroupBox(this);
    auto *modeGroupLayout = new QVBoxLayout(modeGroup);
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
    m_linkRegexEdit = new QLineEdit(this);
    m_linkRegexEdit->setPlaceholderText("URL 过滤正则，如 https?://.*");

    m_chkBottom = new QCheckBox(this);
    auto *bottomRow = new QHBoxLayout();
    auto *bottomLbl = new QLabel("高度:", this);
    m_bottomHeightSpin = new QSpinBox(this);
    m_bottomHeightSpin->setRange(10, 400);
    m_bottomHeightSpin->setValue(60);
    bottomRow->addWidget(m_chkBottom);
    bottomRow->addStretch();
    bottomRow->addWidget(bottomLbl);
    bottomRow->addWidget(m_bottomHeightSpin);

    m_chkText = new QCheckBox(this);
    m_textRegexEdit = new QLineEdit(this);
    m_textRegexEdit->setText("水印|www\\..*?\\.com");

    m_chkOverlays = new QCheckBox(this);

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
    m_matchModeCombo->addItem("智能综合", "auto");
    m_matchModeCombo->addItem("相对位置", "position");
    m_matchModeCombo->addItem("固定区域", "region");
    m_matchModeCombo->addItem("文本内容", "text");
    m_matchModeCombo->addItem("链接 URL", "url");
    matchRow->addWidget(m_matchModeLabel);
    matchRow->addWidget(m_matchModeCombo);
    interactiveLayout->addLayout(matchRow);

    m_interactiveHint = new QLabel(this);
    m_interactiveHint->setWordWrap(true);
    m_interactiveHint->setStyleSheet("color: #8ab4f8; font-size: 11px; margin-top: 4px;");
    interactiveLayout->addWidget(m_interactiveHint);

    m_rulesCountLabel = new QLabel("当前规则数: 0 条", this);
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
    sideLayout->addWidget(modeGroup);

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
    QString qss = R"(
        QMainWindow, QWidget {
            background-color: #1a1a20;
            color: #e0e0e6;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
            font-size: 13px;
        }
        #topBar {
            background-color: #24242c;
            border-bottom: 1px solid #33333e;
        }
        #brandLabel {
            font-size: 16px;
            font-weight: bold;
            color: #4da3ff;
        }
        #sidebar {
            background-color: #202028;
            border-right: 1px solid #33333e;
        }
        QGroupBox {
            border: 1px solid #33333e;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 10px;
            font-weight: bold;
        }
        QPushButton {
            background-color: #2d2d38;
            color: #f0f0f5;
            border: 1px solid #444452;
            border-radius: 5px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #383846;
            border-color: #4da3ff;
        }
        QPushButton:pressed {
            background-color: #22222b;
        }
        #processBtn {
            background-color: #007acc;
            border-color: #007acc;
            font-weight: bold;
            color: #ffffff;
        }
        #processBtn:hover {
            background-color: #1f8ad2;
        }
        QLineEdit, QSpinBox, QComboBox, QListWidget, QTextEdit {
            background-color: #282832;
            color: #e0e0e6;
            border: 1px solid #3e3e4c;
            border-radius: 4px;
            padding: 4px 8px;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #4da3ff;
        }
        QProgressBar {
            background-color: #282832;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: #007acc;
            border-radius: 3px;
        }
    )";
    setStyleSheet(qss);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr_("app_title"));
    m_brandLabel->setText(tr_("app_brand"));
    m_choosePdfBtn->setText(tr_("btn_choose_pdf"));
    m_chooseDirBtn->setText(tr_("btn_choose_dir"));
    m_closeDocBtn->setText(tr_("btn_close"));
    m_saveAsRadio->setText(tr_("btn_save_as"));
    m_overwriteRadio->setText(tr_("btn_overwrite"));
    m_autoModeRadio->setText(tr_("mode_auto"));
    m_interactiveModeRadio->setText(tr_("mode_interactive"));

    m_chkLinks->setText(tr_("opt_remove_all_links"));
    m_chkBottom->setText(tr_("opt_remove_bottom"));
    m_chkText->setText(tr_("opt_text_regex"));
    m_chkOverlays->setText(tr_("opt_detect_overlays"));

    m_matchModeLabel->setText(tr_("match_mode_label"));
    m_undoRuleBtn->setText(tr_("btn_undo_rule"));
    m_clearRulesBtn->setText(tr_("btn_clear_rules"));
    m_interactiveHint->setText(tr_("hint_interactive"));
    m_rulesCountLabel->setText(tr_("rules_count").arg(m_cleaner.interactiveRules().size()));

    m_previewBtn->setText(tr_("btn_preview"));
    m_processBtn->setText(tr_("btn_process"));

    m_prevBtn->setText(tr_("btn_prev"));
    m_nextBtn->setText(tr_("btn_next"));
    m_zoomInBtn->setText(tr_("btn_zoom_in"));
    m_zoomOutBtn->setText(tr_("btn_zoom_out"));
    m_fitWidthBtn->setText(tr_("btn_fit_width"));
    m_langBtn->setText(tr_("switch_lang"));

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
        QMessageBox::critical(this, "错误", QString("无法打开 PDF 文件:\n%1").arg(err));
        return;
    }

    m_isBatch = false;
    m_currentInputPath = filePath;
    m_inputPathLabel->setText(QFileInfo(filePath).fileName());
    m_viewer->setDocument(m_doc.get());
    m_statusLabel->setText(tr_("ready"));
    appendLog(QString("已加载: %1 (共 %2 页)").arg(filePath).arg(m_doc->pageCount()));
}

void MainWindow::closePdf() {
    m_viewer->clear();
    if (m_doc && m_doc->isOpen()) {
        m_doc->close();
    }
    m_currentInputPath.clear();
    m_inputPathLabel->setText("未选择文件");
    m_pageLabel->setText("第 0 / 0 页");
}

void MainWindow::onChoosePdf() {
    QString path = QFileDialog::getOpenFileName(this, "选择 PDF 文件", "", "PDF Files (*.pdf)");
    if (!path.isEmpty()) {
        openPdf(path);
    }
}

void MainWindow::onChooseDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择 PDF 文件夹");
    if (!dir.isEmpty()) {
        closePdf();
        m_isBatch = true;
        m_currentInputPath = dir;
        m_inputPathLabel->setText(QString("目录: %1").arg(dir));
        appendLog(QString("已选择批量输入目录: %1").arg(dir));
    }
}

void MainWindow::onCloseDoc() {
    closePdf();
    appendLog("已关闭文档并释放文件占用。");
}

void MainWindow::onBrowseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(this, "选择输出目录");
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
        appendLog("已撤销上一次添加的规则。");
    }
}

void MainWindow::onClearRules() {
    m_cleaner.clearInteractiveRules();
    m_rulesListWidget->clear();
    m_rulesCountLabel->setText(tr_("rules_count").arg(0));
    m_viewer->clearHighlights();
    appendLog("已清空所有交互点选规则。");
}

void MainWindow::onViewerPageChanged(int pageIdx, int totalPages) {
    if (totalPages > 0) {
        m_pageLabel->setText(tr_("page_nav").arg(pageIdx + 1).arg(totalPages));
    } else {
        m_pageLabel->setText("第 0 / 0 页");
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
        appendLog(QString("点击位置 (%1, %2) 未命中任何可清理元素。").arg(pdfPt.x(), 0, 'f', 1).arg(pdfPt.y(), 0, 'f', 1));
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
    appendLog(QString("已添加规则: %1").arg(rule.describe()));
}

void MainWindow::refreshInteractiveHighlights() {
    if (!m_doc || !m_doc->isOpen()) return;
    auto matches = m_cleaner.previewInteractive(*m_doc);
    m_viewer->setInteractiveElements(matches);
}

void MainWindow::onPreviewDetection() {
    if (!m_doc || !m_doc->isOpen()) {
        QMessageBox::warning(this, "提示", tr_("msg_no_pdf"));
        return;
    }

    if (m_interactiveModeRadio->isChecked()) {
        refreshInteractiveHighlights();
        auto matches = m_cleaner.previewInteractive(*m_doc);
        appendLog(QString("交互模式预览: 共匹配到 %1 处元素。").arg(matches.size()));
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
        appendLog(QString("自动模式预览: 共检测到 %1 处水印元素。").arg(detected.size()));
    }
}

void MainWindow::onStartProcess() {
    if (m_currentInputPath.isEmpty()) {
        QMessageBox::warning(this, "提示", tr_("msg_no_pdf"));
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
    m_statusLabel->setText("正在执行清理...");
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
                QMessageBox::information(this, "完成", "批量 PDF 水印清理已完成！");
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
                    QMessageBox::information(this, "完成", res.message);
                } else {
                    m_statusLabel->setText("处理失败");
                    QMessageBox::critical(this, "错误", res.message);
                }
            });
        }
    });
}

void MainWindow::appendLog(const QString &msg) {
    m_logEdit->append(msg);
}

} // namespace wipepdf
