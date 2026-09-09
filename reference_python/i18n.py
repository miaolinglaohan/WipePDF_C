import ctypes
import os
import sys

TRANSLATIONS = {
    "zh": {
        "app_title": "清印 PDF - 顽固水印/链接清除工具",
        "btn_lang": "English",
        
        # IO
        "input_frame": "输入文件",
        "btn_choose_pdf": "选择单个 PDF",
        "btn_choose_input_dir": "选择输入文件夹",
        "btn_close_file": "关闭文件",
        "lbl_input_none": "输入：未选择",
        "lbl_input_file": "输入：{path}",
        "lbl_input_dir": "输入：{path}（目录）",
        
        # Output
        "output_frame": "输出方式",
        "mode_save_as": "另存新文件（名加 _clean 后缀）",
        "mode_overwrite": "覆盖原文件（处理后直接覆盖）",
        "save_to_source": "保存到原目录",
        "save_to_custom": "保存到指定目录",
        "btn_select_out_dir": "选择目录",
        "lbl_out_overwrite": "输出：覆盖原文件",
        "lbl_out_source": "输出：原目录，文件名加 _clean 后缀",
        "lbl_out_custom": "输出：{path}，文件名加 _clean 后缀",
        "lbl_out_unselected": "输出：未选择，文件名加 _clean 后缀",
        
        # Mode
        "mode_frame": "工作模式",
        "mode_auto": "自动清除（按规则）",
        "mode_interactive": "交互点选（点选水印后遍历全页）",
        
        # Auto
        "auto_frame": "自动清除选项",
        "chk_links": "删除所有超链接",
        "chk_url_pattern": "仅删除匹配 URL 正则的链接",
        "chk_bottom": "清除页面底部区域（高度 px）",
        "chk_text": "按文本正则清除",
        "chk_transparent": "检测并清除透明覆盖层",
        "chk_fallback": "渲染兜底（重建为图片页，牺牲可选文本）",
        "default_text_regex": r"水印|www\..*?\.com",
        
        # Interactive
        "interactive_frame": "交互点选",
        "lbl_pick_info": "在右侧页面点击水印元素",
        "lbl_match_by": "匹配方式：",
        "lbl_tolerance": "位置容差（相对 0~1）：",
        "btn_clear_rules": "清除所有交互规则",
        "lbl_rules_count": "当前规则：{count} 条",
        "selected_frame": "已选元素 / 规则预览",
        
        # Buttons
        "btn_preview": "预览检测",
        "btn_process": "开始处理",
        
        # Toolbar
        "btn_first": "⏮ 首页",
        "btn_prev": "◀ 上一页",
        "lbl_page_count": "第 {page} / {total} 页",
        "btn_next": "下一页 ▶",
        "btn_last": "末页 ⏭",
        "btn_zoom_in": "放大",
        "btn_zoom_out": "缩小",
        "btn_zoom_fit": "适配",
        
        # Messages
        "msg_ready": "就绪。请选择或拖入 PDF 文件。",
        "msg_drag_supported": "支持拖拽：将 PDF 文件拖入窗口即可打开。",
        "msg_drag_fail": "拖拽功能初始化失败：{exc}",
        "msg_drag_no_pdf": "拖入的内容中没有 PDF 文件。",
        "msg_closed": "已关闭文件，释放 PDF 占用。",
        "msg_loaded": "已加载：{name}，共 {total} 页",
        "msg_err_not_pdf": "不是有效的 PDF 文件：{path}",
        "msg_err_open": "打开失败",
        "msg_err_no_input": "请先选择输入文件或文件夹",
        "msg_err_no_out_dir": "请先在「输出方式」中选择输出目录。",
        "msg_err_batch_out": "批量处理必须指定输出目录，请在「输出方式」中选择指定目录。",
        "msg_err_input_invalid": "输入路径无效",
        "msg_warn_no_pdf": "请先选择 PDF 文件",
        "msg_info": "提示",
        "msg_err": "错误",
        "msg_done": "完成",
        "msg_confirm": "确认",
        
        "msg_click_miss": "点击位置 ({x:.1f}, {y:.1f}) 未命中任何元素",
        "msg_rule_added": "已添加规则：{desc}",
        "msg_rules_cleared": "已清除所有交互规则",
        "msg_preview_done": "预览完成，共检测到 {count} 个元素。{summary}",
        "msg_batch_confirm": "将批量处理目录：\n{input}\n输出到：\n{output}\n是否继续？",
        "msg_overwrite_confirm": "将覆盖目录内所有 PDF 原文件：\n{input}\n此操作不可撤销，是否继续？",
        "msg_batch_done": "批量处理完成：成功 {success}，失败 {fail}",
        
        # core messages
        "msg_err_load": "加载文件失败: {err}",
        "msg_err_process": "处理失败: {err}",
        "msg_processing": "处理: {path}",
        "core_save_err": "保存失败: {err}",
        "core_content_edit": "内容流重写：精确定位并删除了 {count} 处匹配文本",
        "core_found_pattern": "清除了 {count} 个图案透明填充",
        "core_render_fallback": "使用了渲染兜底重构页面",
        "core_interact_match": "共清除 {count} 个匹配项",
        "success": "处理成功",

        # selected info panel
        "sel_type": "类型",
        "sel_page": "页码",
        "sel_text": "文本内容",
        "sel_url": "链接地址",
        "sel_bbox": "位置",
        "sel_area": "面积占比",
        "sel_opacity": "透明度",
        "sel_rule": "规则",
        "sel_rules_count": "规则总数",
        "preview_pg": "第{page}页:{count}",
        "err_not_found": "文件不存在",
        "err_permission": "无权写入或文件被占用",
    },
    "en": {
        "app_title": "WipePDF - Stubborn watermark & link remover",
        "btn_lang": "中文",
        
        # IO
        "input_frame": "Input Files",
        "btn_choose_pdf": "Select Single PDF",
        "btn_choose_input_dir": "Select Input Folder",
        "btn_close_file": "Close File",
        "lbl_input_none": "Input: Not selected",
        "lbl_input_file": "Input: {path}",
        "lbl_input_dir": "Input: {path} (Directory)",
        
        # Output
        "output_frame": "Output Options",
        "mode_save_as": "Save as new file (with _clean suffix)",
        "mode_overwrite": "Overwrite original file (Danger)",
        "save_to_source": "Save to source directory",
        "save_to_custom": "Save to custom directory",
        "btn_select_out_dir": "Select Directory",
        "lbl_out_overwrite": "Output: Overwrite original",
        "lbl_out_source": "Output: Source directory, with _clean suffix",
        "lbl_out_custom": "Output: {path}, with _clean suffix",
        "lbl_out_unselected": "Output: Not selected, with _clean suffix",
        
        # Mode
        "mode_frame": "Operation Mode",
        "mode_auto": "Auto Removal (by rules)",
        "mode_interactive": "Interactive (click to match)",
        
        # Auto
        "auto_frame": "Auto Rules Options",
        "chk_links": "Remove all hyperlinks",
        "chk_url_pattern": "Only remove links matching URL regex",
        "chk_bottom": "Clear page bottom area (height px)",
        "chk_text": "Remove text by regex",
        "chk_transparent": "Detect & remove transparent overlays",
        "chk_fallback": "Render fallback (rebuild as image, lose selectable text)",
        "default_text_regex": r"watermark|www\..*?\.com",
        
        # Interactive
        "interactive_frame": "Interactive Click",
        "lbl_pick_info": "Click watermark element on right preview",
        "lbl_match_by": "Match by:",
        "lbl_tolerance": "Position tolerance (0~1):",
        "btn_clear_rules": "Clear all rules",
        "lbl_rules_count": "Current rules: {count}",
        "selected_frame": "Selected Element / Rule Preview",
        
        # Buttons
        "btn_preview": "Preview",
        "btn_process": "Process",
        
        # Toolbar
        "btn_first": "⏮ First",
        "btn_prev": "◀ Prev",
        "lbl_page_count": "Page {page} / {total}",
        "btn_next": "Next ▶",
        "btn_last": "Last ⏭",
        "btn_zoom_in": "Zoom In",
        "btn_zoom_out": "Zoom Out",
        "btn_zoom_fit": "Fit",
        
        # Messages
        "msg_ready": "Ready. Select or drag a PDF file.",
        "msg_drag_supported": "Drag and drop supported: drag PDF to open.",
        "msg_drag_fail": "Drag init failed: {exc}",
        "msg_drag_no_pdf": "No PDF found in dropped files.",
        "msg_closed": "File closed.",
        "msg_loaded": "Loaded: {name}, {total} pages",
        "msg_err_not_pdf": "Not a valid PDF: {path}",
        "msg_err_open": "Open Failed",
        "msg_err_no_input": "Please select input file or folder first",
        "msg_err_no_out_dir": "Please select output directory in Output Options.",
        "msg_err_batch_out": "Batch processing requires a custom output directory.",
        "msg_err_input_invalid": "Invalid input path",
        "msg_warn_no_pdf": "Please select a PDF file first",
        "msg_info": "Info",
        "msg_err": "Error",
        "msg_done": "Done",
        "msg_confirm": "Confirm",
        
        "msg_click_miss": "Click at ({x:.1f}, {y:.1f}) missed any elements",
        "msg_rule_added": "Rule added: {desc}",
        "msg_rules_cleared": "Cleared all interactive rules",
        "msg_preview_done": "Preview done. Detected {count} elements. {summary}",
        "msg_batch_confirm": "Batch process directory:\n{input}\nOutput to:\n{output}\nContinue?",
        "msg_overwrite_confirm": "Will OVERWRITE all PDFs in:\n{input}\nThis cannot be undone. Continue?",
        "msg_batch_done": "Batch processing complete: {success} success, {fail} failed",
        
        # core messages
        "msg_err_load": "Load failed: {err}",
        "msg_err_process": "Process failed: {err}",
        "msg_processing": "Processing: {path}",
        "core_save_err": "Save failed: {err}",
        "core_content_edit": "Content rewritten: accurately removed {count} texts",
        "core_found_pattern": "Removed {count} pattern transparent fills",
"core_render_fallback": "Used render fallback to reconstruct page",
        "core_interact_match": "Removed {count} matched elements",
        "success": "Success",

        # selected info panel
        "sel_type": "Type",
        "sel_page": "Page",
        "sel_text": "Text",
        "sel_url": "URL",
        "sel_bbox": "BBox",
        "sel_area": "Area",
        "sel_opacity": "Opacity",
        "sel_rule": "Rule",
        "sel_rules_count": "Rules count",
        "preview_pg": "Pg{page}:{count}",
        "err_not_found": "File not found",
        "err_permission": "Permission denied or file in use",
    },
}

current_lang = "zh"

def detect_lang():
    if sys.platform == "win32":
        try:
            lang_id = ctypes.windll.kernel32.GetUserDefaultUILanguage()
            primary_lang = lang_id & 0x3ff
            if primary_lang == 0x04:
                return "zh"
            return "en"
        except:
            return "en"
    else:
        import locale
        loc = locale.getdefaultlocale()[0]
        if loc and loc.startswith("zh"):
            return "zh"
        return "en"

def set_lang(lang):
    global current_lang
    if lang in TRANSLATIONS:
        current_lang = lang

def get_lang():
    return current_lang

def t(key, **kwargs):
    text = TRANSLATIONS.get(current_lang, {}).get(key, TRANSLATIONS["en"].get(key, key))
    if kwargs:
        try:
            return text.format(**kwargs)
        except KeyError:
            return text
    return text

set_lang(detect_lang())
