#include "I18n.h"

namespace wipepdf {

I18n &I18n::instance() {
    static I18n inst;
    return inst;
}

I18n::I18n() {
    // Chinese translations
    m_zh["app_title"] = "清印 PDF - 顽固水印/链接清除工具";
    m_zh["app_brand"] = "清印 PDF";
    m_zh["ready"] = "就绪";
    m_zh["input_frame"] = "输入与输出设置";
    m_zh["btn_choose_pdf"] = "打开单文件";
    m_zh["btn_choose_dir"] = "批量文件夹";
    m_zh["btn_close"] = "关闭文档";
    m_zh["no_file_selected"] = "未选择文件";
    m_zh["dir_prefix"] = "目录: %1";
    m_zh["input_label"] = "输入路径: ";
    m_zh["output_mode_frame"] = "输出方式";
    m_zh["btn_save_as"] = "另存为新文件 (_clean)";
    m_zh["btn_overwrite"] = "覆盖原文件 (安全写回)";
    m_zh["custom_output_dir"] = "自定义输出目录 (可选):";
    m_zh["custom_output_placeholder"] = "自定义输出目录 (可选)";
    m_zh["btn_browse"] = "浏览...";
    m_zh["mode_frame"] = "处理模式";
    m_zh["mode_auto"] = "自动规则清除";
    m_zh["mode_interactive"] = "交互点选模式";
    m_zh["auto_options"] = "自动清除选项";
    m_zh["opt_remove_all_links"] = "删除所有超链接";
    m_zh["opt_link_regex"] = "URL 过滤正则 (可选):";
    m_zh["link_regex_placeholder"] = "URL 过滤正则，如 https?://.*";
    m_zh["opt_remove_bottom"] = "清除页面底部区域";
    m_zh["opt_bottom_height"] = "底部高度 (pt):";
    m_zh["label_height"] = "高度 (pt):";
    m_zh["opt_text_regex"] = "按文本正则清除:";
    m_zh["default_text_regex"] = "水印|https?://\\S+|www\\.\\S+|(?:[a-zA-Z0-9-]+\\.)+(?:com|net|org|cn|cc|top|xyz|site|vip|club)\\b";
    m_zh["text_regex_placeholder"] = "文本匹配正则，如 水印|广告";
    m_zh["opt_detect_overlays"] = "检测并清除透明覆盖层 / Pattern 图案";
    m_zh["interactive_panel"] = "交互点选控制";
    m_zh["match_mode_label"] = "匹配维度:";
    m_zh["match_auto"] = "智能综合";
    m_zh["match_position"] = "按页面相对位置";
    m_zh["match_region"] = "按固定区域矩形";
    m_zh["match_text"] = "按文本内容";
    m_zh["match_url"] = "按超链接 URL";
    m_zh["rules_count"] = "当前规则数: %1 条";
    m_zh["btn_undo_rule"] = "撤销 (Ctrl+Z)";
    m_zh["btn_clear_rules"] = "清空所有规则";
    m_zh["hint_interactive"] = "提示: 在右侧页面预览中点击水印元素，即可自动生成跨页清除规则。";
    m_zh["btn_preview"] = "预览检测结果";
    m_zh["btn_process"] = "开始执行清除";
    m_zh["log_title"] = "运行日志与状态";
    m_zh["page_nav"] = "第 %1 / %2 页";
    m_zh["btn_prev"] = "上一页";
    m_zh["btn_next"] = "下一页";
    m_zh["btn_zoom_in"] = "放大 (+)";
    m_zh["btn_zoom_out"] = "缩小 (-)";
    m_zh["btn_fit_width"] = "适应宽度";
    m_zh["switch_lang"] = "English";
    m_zh["theme_dark"] = "🌙 暗黑模式";
    m_zh["theme_light"] = "☀️ 明亮模式";
    m_zh["tip_choose_pdf"] = "打开单个 PDF 文档进行预览和清理";
    m_zh["tip_choose_dir"] = "选择包含多个 PDF 的文件夹进行批量处理";
    m_zh["tip_close_doc"] = "关闭当前文档并释放文件占用";
    m_zh["dialog_choose_pdf"] = "选择 PDF 文件";
    m_zh["dialog_choose_dir"] = "选择 PDF 文件夹";
    m_zh["dialog_choose_out_dir"] = "选择输出目录";
    m_zh["title_notice"] = "提示";
    m_zh["title_error"] = "错误";
    m_zh["title_complete"] = "完成";
    m_zh["status_cleaning"] = "正在执行清理...";
    m_zh["status_failed"] = "处理失败";
    m_zh["batch_complete_msg"] = "批量 PDF 水印清理已完成！";
    m_zh["drag_hint"] = "拖拽 PDF 文件到此处打开";
    m_zh["msg_no_pdf"] = "请先选择需要处理的 PDF 文件";
    m_zh["msg_processing"] = "正在处理: %1";
    m_zh["msg_complete"] = "处理完成！";
    m_zh["log_doc_loaded"] = "已加载: %1 (共 %2 页)";
    m_zh["log_doc_closed"] = "已关闭文档并释放文件占用。";
    m_zh["log_batch_selected"] = "已选择批量输入目录: %1";
    m_zh["log_rule_undone"] = "已撤销上一次添加的规则。";
    m_zh["log_rules_cleared"] = "已清空所有交互点选规则。";
    m_zh["log_rule_added"] = "已添加规则: %1";
    m_zh["log_no_element_picked"] = "点击位置 (%1, %2) 未命中任何可清理元素。";
    m_zh["log_preview_auto"] = "自动模式预览: 共检测到 %1 处水印元素。";
    m_zh["log_preview_interactive"] = "交互模式预览: 共匹配到 %1 处元素。";

    // English translations
    m_en["app_title"] = "WipePDF - Stubborn Watermark & Link Remover";
    m_en["app_brand"] = "WipePDF";
    m_en["ready"] = "Ready";
    m_en["input_frame"] = "Input & Output Settings";
    m_en["btn_choose_pdf"] = "Open PDF";
    m_en["btn_choose_dir"] = "Batch Folder";
    m_en["btn_close"] = "Close";
    m_en["no_file_selected"] = "No file selected";
    m_en["dir_prefix"] = "Folder: %1";
    m_en["input_label"] = "Input: ";
    m_en["output_mode_frame"] = "Output Mode";
    m_en["btn_save_as"] = "Save As New (_clean)";
    m_en["btn_overwrite"] = "Overwrite Original (Safe)";
    m_en["custom_output_dir"] = "Custom Output Dir (Optional):";
    m_en["custom_output_placeholder"] = "Custom output folder (optional)";
    m_en["btn_browse"] = "Browse...";
    m_en["mode_frame"] = "Cleaning Mode";
    m_en["mode_auto"] = "Auto Rules";
    m_en["mode_interactive"] = "Interactive Pick";
    m_en["auto_options"] = "Auto Cleaning Options";
    m_en["opt_remove_all_links"] = "Remove all hyperlinks";
    m_en["opt_link_regex"] = "URL regex filter (optional):";
    m_en["link_regex_placeholder"] = "URL regex filter, e.g. https?://.*";
    m_en["opt_remove_bottom"] = "Clear bottom strip";
    m_en["opt_bottom_height"] = "Strip height (pt):";
    m_en["label_height"] = "Height (pt):";
    m_en["opt_text_regex"] = "Remove text matching regex:";
    m_en["default_text_regex"] = "watermark|https?://\\S+|www\\.\\S+|(?:[a-zA-Z0-9-]+\\.)+(?:com|net|org|cn|cc|top|xyz|site|vip|club)\\b";
    m_en["text_regex_placeholder"] = "Text regex, e.g. watermark|ad";
    m_en["opt_detect_overlays"] = "Detect & clear transparent overlays / patterns";
    m_en["interactive_panel"] = "Interactive Controls";
    m_en["match_mode_label"] = "Match By:";
    m_en["match_auto"] = "Smart Auto";
    m_en["match_position"] = "Relative Position";
    m_en["match_region"] = "Fixed Region Rect";
    m_en["match_text"] = "Text Content";
    m_en["match_url"] = "Link URL";
    m_en["rules_count"] = "Rules added: %1";
    m_en["btn_undo_rule"] = "Undo (Ctrl+Z)";
    m_en["btn_clear_rules"] = "Clear All Rules";
    m_en["hint_interactive"] = "Tip: Click watermark elements directly on the preview to add rules.";
    m_en["btn_preview"] = "Preview Detection";
    m_en["btn_process"] = "Start Cleaning";
    m_en["log_title"] = "Logs & Status";
    m_en["page_nav"] = "Page %1 / %2";
    m_en["btn_prev"] = "Previous";
    m_en["btn_next"] = "Next";
    m_en["btn_zoom_in"] = "Zoom In (+)";
    m_en["btn_zoom_out"] = "Zoom Out (-)";
    m_en["btn_fit_width"] = "Fit Width";
    m_en["switch_lang"] = "中文";
    m_en["theme_dark"] = "🌙 Dark Mode";
    m_en["theme_light"] = "☀️ Light Mode";
    m_en["tip_choose_pdf"] = "Open a single PDF document for preview and cleaning";
    m_en["tip_choose_dir"] = "Select a folder containing multiple PDFs for batch processing";
    m_en["tip_close_doc"] = "Close current document and release file handles";
    m_en["dialog_choose_pdf"] = "Choose PDF File";
    m_en["dialog_choose_dir"] = "Choose PDF Folder";
    m_en["dialog_choose_out_dir"] = "Choose Output Folder";
    m_en["title_notice"] = "Notice";
    m_en["title_error"] = "Error";
    m_en["title_complete"] = "Complete";
    m_en["status_cleaning"] = "Cleaning in progress...";
    m_en["status_failed"] = "Cleaning failed";
    m_en["batch_complete_msg"] = "Batch PDF cleaning completed!";
    m_en["drag_hint"] = "Drag and drop a PDF file here to open";
    m_en["msg_no_pdf"] = "Please select a PDF document first";
    m_en["msg_processing"] = "Processing: %1";
    m_en["msg_complete"] = "Processing completed!";
    m_en["log_doc_loaded"] = "Loaded: %1 (%2 pages)";
    m_en["log_doc_closed"] = "Document closed and file locks released.";
    m_en["log_batch_selected"] = "Selected batch input folder: %1";
    m_en["log_rule_undone"] = "Undone the last added rule.";
    m_en["log_rules_cleared"] = "All interactive rules cleared.";
    m_en["log_rule_added"] = "Rule added: %1";
    m_en["log_no_element_picked"] = "Clicked position (%1, %2) did not hit any cleanable elements.";
    m_en["log_preview_auto"] = "Auto preview: detected %1 watermark elements.";
    m_en["log_preview_interactive"] = "Interactive preview: matched %1 elements.";
}

QString I18n::text(const QString &key) const {
    if (m_lang == Language::Zh) {
        return m_zh.value(key, m_en.value(key, key));
    } else {
        return m_en.value(key, m_zh.value(key, key));
    }
}

} // namespace wipepdf
