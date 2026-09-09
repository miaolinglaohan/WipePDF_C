"""tkinter 桌面 GUI。"""

from __future__ import annotations

import re
import sys
import os
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk
from typing import Optional

import pymupdf as fitz
from PIL import Image, ImageTk

try:
    from tkinterdnd2 import DND_FILES, TkinterDnD
    DND_AVAILABLE = True
except ImportError:
    DND_AVAILABLE = False

from .core import AutoOptions, CleanerResult, WatermarkCleaner
from .detectors import Element
from .matcher import MatchRule, refine_rule
from .i18n import t, set_lang, get_lang

class WatermarkRemoverGUI:
    def __init__(self) -> None:
        self.root = TkinterDnD.Tk() if DND_AVAILABLE else tk.Tk()
        self.root.title(t("app_title"))
        self.root.geometry("1400x900")
        self.root.minsize(1100, 700)

        try:
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")
        icon_path = os.path.join(base_path, "wipepdf.ico")
        if os.path.exists(icon_path):
            self.root.iconbitmap(icon_path)

        self.cleaner = WatermarkCleaner(progress_callback=self.log)
        self.doc: Optional[fitz.Document] = None
        self.input_path: Optional[Path] = None
        self.output_dir: Optional[Path] = None
        self.overwrite_var: Optional[tk.BooleanVar] = None
        self.use_source_dir_var: Optional[tk.BooleanVar] = None
        self.current_page: int = 0
        self.zoom: float = 1.0
        self.render_zoom: float = 1.0
        self.photo_image: Optional[ImageTk.PhotoImage] = None
        self.canvas_image_id: Optional[int] = None
        self.highlight_ids: list[int] = []

        # References for i18n
        self.ui_labels = {}
        self.ui_buttons = {}
        self.ui_frames = {}
        self.ui_radios = {}
        self.ui_checks = {}

        self._build_ui()
        self._setup_drag_drop()
        self.apply_language()
        self.log(t("msg_ready"))

    def _toggle_lang(self):
        new_lang = "en" if get_lang() == "zh" else "zh"
        set_lang(new_lang)
        self.apply_language()

    def apply_language(self):
        self.root.title(t("app_title"))
        self.btn_lang.config(text=t("btn_lang"))
        
        self.ui_frames["io_frame"].config(text=t("input_frame"))
        self.ui_buttons["btn_choose_pdf"].config(text=t("btn_choose_pdf"))
        self.ui_buttons["btn_choose_dir"].config(text=t("btn_choose_input_dir"))
        self.ui_buttons["btn_close_file"].config(text=t("btn_close_file"))
        self._update_input_label()
        
        self.ui_frames["out_frame"].config(text=t("output_frame"))
        self.ui_radios["rb_save_as"].config(text=t("mode_save_as"))
        self.ui_radios["rb_overwrite"].config(text=t("mode_overwrite"))
        self.ui_radios["rb_source_dir"].config(text=t("save_to_source"))
        self.ui_radios["rb_custom_dir"].config(text=t("save_to_custom"))
        self.ui_buttons["btn_select_out_dir"].config(text=t("btn_select_out_dir"))
        self._update_output_label()
        
        self.ui_frames["mode_frame"].config(text=t("mode_frame"))
        self.ui_radios["rb_mode_auto"].config(text=t("mode_auto"))
        self.ui_radios["rb_mode_interactive"].config(text=t("mode_interactive"))
        
        self.ui_frames["auto_frame"].config(text=t("auto_frame"))
        self.ui_checks["chk_links"].config(text=t("chk_links"))
        self.ui_checks["chk_url_pattern"].config(text=t("chk_url_pattern"))
        self.ui_checks["chk_bottom"].config(text=t("chk_bottom"))
        self.ui_checks["chk_text"].config(text=t("chk_text"))
        self.ui_checks["chk_transparent"].config(text=t("chk_transparent"))
        self.ui_checks["chk_fallback"].config(text=t("chk_fallback"))
        # Update regex entry default ONLY if it hasn't been modified heavily, or just skip it.
        # It's better to update it if it's the old default.
        current_regex = self.entry_text_pattern.get()
        if current_regex in [r"水印|www\..*?\.com", r"watermark|www\..*?\.com"]:
            self.entry_text_pattern.delete(0, tk.END)
            self.entry_text_pattern.insert(0, t("default_text_regex"))
            
        self.ui_frames["interactive_frame"].config(text=t("interactive_frame"))
        self.ui_labels["lbl_pick_info"].config(text=t("lbl_pick_info"))
        self.ui_labels["lbl_match_by"].config(text=t("lbl_match_by"))
        self.ui_labels["lbl_tolerance"].config(text=t("lbl_tolerance"))
        self.ui_buttons["btn_clear_rules"].config(text=t("btn_clear_rules"))
        self._update_rules_label()
        self.ui_frames["selected_frame"].config(text=t("selected_frame"))
        
        self.ui_buttons["btn_preview"].config(text=t("btn_preview"))
        self.ui_buttons["btn_process"].config(text=t("btn_process"))
        
        self.ui_buttons["btn_first"].config(text=t("btn_first"))
        self.ui_buttons["btn_prev"].config(text=t("btn_prev"))
        self._update_page_label()
        self.ui_buttons["btn_next"].config(text=t("btn_next"))
        self.ui_buttons["btn_last"].config(text=t("btn_last"))
        self.ui_buttons["btn_zoom_in"].config(text=t("btn_zoom_in"))
        self.ui_buttons["btn_zoom_out"].config(text=t("btn_zoom_out"))
        self.ui_buttons["btn_zoom_fit"].config(text=t("btn_zoom_fit"))

    def _build_ui(self) -> None:
        top_bar = ttk.Frame(self.root)
        top_bar.pack(fill=tk.X, padx=6, pady=2)
        self.btn_lang = ttk.Button(top_bar, command=self._toggle_lang)
        self.btn_lang.pack(side=tk.RIGHT)

        main_pane = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        main_pane.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)

        left_frame = ttk.Frame(main_pane, width=360)
        main_pane.add(left_frame, weight=0)
        left_frame.pack_propagate(False)

        self.left_canvas = tk.Canvas(left_frame, highlightthickness=0)
        left_scrollbar = ttk.Scrollbar(left_frame, orient=tk.VERTICAL, command=self.left_canvas.yview)
        self.left_canvas.configure(yscrollcommand=left_scrollbar.set)
        left_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.left_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        left_inner = ttk.Frame(self.left_canvas)
        self._left_window_id = self.left_canvas.create_window((0, 0), window=left_inner, anchor=tk.NW)
        left_inner.bind("<Configure>", self._on_left_inner_configure)
        self.left_canvas.bind("<Configure>", self._on_left_canvas_configure)
        self.left_canvas.bind("<Enter>", self._bind_left_wheel)
        self.left_canvas.bind("<Leave>", self._unbind_left_wheel)

        io_frame = ttk.LabelFrame(left_inner, padding=8)
        self.ui_frames["io_frame"] = io_frame
        io_frame.pack(fill=tk.X, pady=4)

        self.btn_choose_pdf = ttk.Button(io_frame, command=self._select_file)
        self.ui_buttons["btn_choose_pdf"] = self.btn_choose_pdf
        self.btn_choose_pdf.pack(fill=tk.X, pady=2)

        self.btn_choose_dir = ttk.Button(io_frame, command=self._select_input_dir)
        self.ui_buttons["btn_choose_dir"] = self.btn_choose_dir
        self.btn_choose_dir.pack(fill=tk.X, pady=2)

        self.btn_close_file = ttk.Button(io_frame, command=self._close_file, state=tk.DISABLED)
        self.ui_buttons["btn_close_file"] = self.btn_close_file
        self.btn_close_file.pack(fill=tk.X, pady=2)

        self.lbl_input = ttk.Label(io_frame, wraplength=320)
        self.lbl_input.pack(fill=tk.X, pady=2)

        out_frame = ttk.LabelFrame(left_inner, padding=8)
        self.ui_frames["out_frame"] = out_frame
        out_frame.pack(fill=tk.X, pady=4)

        self.overwrite_var = tk.BooleanVar(value=False)
        self.rb_save_as = ttk.Radiobutton(out_frame, variable=self.overwrite_var, value=False, command=self._on_output_mode_changed)
        self.ui_radios["rb_save_as"] = self.rb_save_as
        self.rb_save_as.pack(anchor=tk.W)
        
        self.rb_overwrite = ttk.Radiobutton(out_frame, variable=self.overwrite_var, value=True, command=self._on_output_mode_changed)
        self.ui_radios["rb_overwrite"] = self.rb_overwrite
        self.rb_overwrite.pack(anchor=tk.W)

        self.sub_save_frame = ttk.Frame(out_frame)
        self.sub_save_frame.pack(fill=tk.X, padx=(16, 0), pady=(6, 0))
        self.use_source_dir_var = tk.BooleanVar(value=True)
        self.rb_source_dir = ttk.Radiobutton(self.sub_save_frame, variable=self.use_source_dir_var, value=True, command=self._on_output_mode_changed)
        self.ui_radios["rb_source_dir"] = self.rb_source_dir
        self.rb_source_dir.pack(anchor=tk.W)
        self.rb_custom_dir = ttk.Radiobutton(self.sub_save_frame, variable=self.use_source_dir_var, value=False, command=self._on_output_mode_changed)
        self.ui_radios["rb_custom_dir"] = self.rb_custom_dir
        self.rb_custom_dir.pack(anchor=tk.W)
        
        self.save_dir_row = ttk.Frame(self.sub_save_frame)
        self.save_dir_row.pack(fill=tk.X, padx=(16, 0), pady=(2, 0))
        self.btn_select_out_dir = ttk.Button(self.save_dir_row, command=self._select_output_dir)
        self.ui_buttons["btn_select_out_dir"] = self.btn_select_out_dir
        self.btn_select_out_dir.pack(side=tk.LEFT)

        self.lbl_output = ttk.Label(out_frame, wraplength=320)
        self.lbl_output.pack(fill=tk.X, pady=(6, 0))

        mode_frame = ttk.LabelFrame(left_inner, padding=8)
        self.ui_frames["mode_frame"] = mode_frame
        mode_frame.pack(fill=tk.X, pady=4)
        self.mode_var = tk.StringVar(value="auto")
        
        self.rb_mode_auto = ttk.Radiobutton(mode_frame, variable=self.mode_var, value="auto", command=self._on_mode_changed)
        self.ui_radios["rb_mode_auto"] = self.rb_mode_auto
        self.rb_mode_auto.pack(anchor=tk.W)
        
        self.rb_mode_interactive = ttk.Radiobutton(mode_frame, variable=self.mode_var, value="interactive", command=self._on_mode_changed)
        self.ui_radios["rb_mode_interactive"] = self.rb_mode_interactive
        self.rb_mode_interactive.pack(anchor=tk.W)

        self.auto_frame = ttk.LabelFrame(left_inner, padding=8)
        self.ui_frames["auto_frame"] = self.auto_frame
        self.auto_frame.pack(fill=tk.X, pady=4)

        self.chk_links_var = tk.BooleanVar(value=True)
        self.chk_links = ttk.Checkbutton(self.auto_frame, variable=self.chk_links_var)
        self.ui_checks["chk_links"] = self.chk_links
        self.chk_links.pack(anchor=tk.W)

        self.chk_url_pattern_var = tk.BooleanVar(value=False)
        self.chk_url_pattern = ttk.Checkbutton(self.auto_frame, variable=self.chk_url_pattern_var)
        self.ui_checks["chk_url_pattern"] = self.chk_url_pattern
        self.chk_url_pattern.pack(anchor=tk.W)
        self.entry_url_pattern = ttk.Entry(self.auto_frame)
        self.entry_url_pattern.insert(0, r"https?://.*")
        self.entry_url_pattern.pack(fill=tk.X, padx=(20, 0), pady=2)

        self.chk_bottom_var = tk.BooleanVar(value=True)
        self.chk_bottom = ttk.Checkbutton(self.auto_frame, variable=self.chk_bottom_var)
        self.ui_checks["chk_bottom"] = self.chk_bottom
        self.chk_bottom.pack(anchor=tk.W)
        self.entry_bottom = ttk.Entry(self.auto_frame)
        self.entry_bottom.insert(0, "60")
        self.entry_bottom.pack(fill=tk.X, padx=(20, 0), pady=2)

        self.chk_text_var = tk.BooleanVar(value=False)
        self.chk_text = ttk.Checkbutton(self.auto_frame, variable=self.chk_text_var)
        self.ui_checks["chk_text"] = self.chk_text
        self.chk_text.pack(anchor=tk.W)
        self.entry_text_pattern = ttk.Entry(self.auto_frame)
        self.entry_text_pattern.pack(fill=tk.X, padx=(20, 0), pady=2)

        self.chk_transparent_var = tk.BooleanVar(value=False)
        self.chk_transparent = ttk.Checkbutton(self.auto_frame, variable=self.chk_transparent_var)
        self.ui_checks["chk_transparent"] = self.chk_transparent
        self.chk_transparent.pack(anchor=tk.W)

        self.chk_fallback_var = tk.BooleanVar(value=False)
        self.chk_fallback = ttk.Checkbutton(self.auto_frame, variable=self.chk_fallback_var)
        self.ui_checks["chk_fallback"] = self.chk_fallback
        self.chk_fallback.pack(anchor=tk.W)

        self.interactive_frame = ttk.LabelFrame(left_inner, padding=8)
        self.ui_frames["interactive_frame"] = self.interactive_frame

        self.lbl_pick_info = ttk.Label(self.interactive_frame, wraplength=320)
        self.ui_labels["lbl_pick_info"] = self.lbl_pick_info
        self.lbl_pick_info.pack(fill=tk.X, pady=2)

        self.lbl_match_by = ttk.Label(self.interactive_frame)
        self.ui_labels["lbl_match_by"] = self.lbl_match_by
        self.lbl_match_by.pack(anchor=tk.W)
        self.match_by_var = tk.StringVar(value="auto")
        combo_match = ttk.Combobox(self.interactive_frame, textvariable=self.match_by_var, values=["auto", "text", "position", "region", "url", "size"], state="readonly")
        combo_match.pack(fill=tk.X, pady=2)

        self.lbl_tolerance = ttk.Label(self.interactive_frame)
        self.ui_labels["lbl_tolerance"] = self.lbl_tolerance
        self.lbl_tolerance.pack(anchor=tk.W)
        self.entry_tolerance = ttk.Entry(self.interactive_frame)
        self.entry_tolerance.insert(0, "0.05")
        self.entry_tolerance.pack(fill=tk.X, pady=2)

        rules_btn_frame = ttk.Frame(self.interactive_frame)
        rules_btn_frame.pack(fill=tk.X, pady=4)

        self.btn_undo_rule = ttk.Button(rules_btn_frame, command=self._undo_last_rule)
        self.ui_buttons["btn_undo_rule"] = self.btn_undo_rule
        self.btn_undo_rule.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 2))

        self.btn_clear_rules = ttk.Button(rules_btn_frame, command=self._clear_rules)
        self.ui_buttons["btn_clear_rules"] = self.btn_clear_rules
        self.btn_clear_rules.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(2, 0))

        self.lbl_rules = ttk.Label(self.interactive_frame, wraplength=320)
        self.lbl_rules.pack(fill=tk.X, pady=2)

        self.selected_frame = ttk.LabelFrame(left_inner, padding=8)
        self.ui_frames["selected_frame"] = self.selected_frame
        self.selected_frame.pack(fill=tk.X, pady=4)
        self.selected_text = tk.Text(self.selected_frame, height=10, wrap=tk.WORD, state=tk.DISABLED)
        self.selected_text.pack(fill=tk.BOTH, expand=True)

        btn_frame = ttk.Frame(left_inner)
        btn_frame.pack(fill=tk.X, pady=8)
        self.btn_preview = ttk.Button(btn_frame, command=self._preview)
        self.ui_buttons["btn_preview"] = self.btn_preview
        self.btn_preview.pack(fill=tk.X, pady=2)
        self.btn_process = ttk.Button(btn_frame, command=self._process)
        self.ui_buttons["btn_process"] = self.btn_process
        self.btn_process.pack(fill=tk.X, pady=2)

        right_frame = ttk.Frame(main_pane)
        main_pane.add(right_frame, weight=1)

        toolbar = ttk.Frame(right_frame)
        toolbar.pack(fill=tk.X, pady=2)
        self.btn_first = ttk.Button(toolbar, command=self._first_page)
        self.ui_buttons["btn_first"] = self.btn_first
        self.btn_first.pack(side=tk.LEFT, padx=2)
        self.btn_prev = ttk.Button(toolbar, command=self._prev_page)
        self.ui_buttons["btn_prev"] = self.btn_prev
        self.btn_prev.pack(side=tk.LEFT, padx=2)
        self.lbl_page = ttk.Label(toolbar)
        self.lbl_page.pack(side=tk.LEFT, padx=6)
        self.btn_next = ttk.Button(toolbar, command=self._next_page)
        self.ui_buttons["btn_next"] = self.btn_next
        self.btn_next.pack(side=tk.LEFT, padx=2)
        self.btn_last = ttk.Button(toolbar, command=self._last_page)
        self.ui_buttons["btn_last"] = self.btn_last
        self.btn_last.pack(side=tk.LEFT, padx=2)
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        self.btn_zoom_in = ttk.Button(toolbar, command=self._zoom_in)
        self.ui_buttons["btn_zoom_in"] = self.btn_zoom_in
        self.btn_zoom_in.pack(side=tk.LEFT, padx=2)
        self.btn_zoom_out = ttk.Button(toolbar, command=self._zoom_out)
        self.ui_buttons["btn_zoom_out"] = self.btn_zoom_out
        self.btn_zoom_out.pack(side=tk.LEFT, padx=2)
        self.btn_zoom_fit = ttk.Button(toolbar, command=self._zoom_fit)
        self.ui_buttons["btn_zoom_fit"] = self.btn_zoom_fit
        self.btn_zoom_fit.pack(side=tk.LEFT, padx=2)

        self.canvas = tk.Canvas(right_frame, bg="#cccccc", highlightthickness=0)
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Button-1>", self._on_canvas_click)
        self.canvas.bind("<MouseWheel>", self._on_mousewheel)
        self.canvas.bind("<Button-4>", self._on_mousewheel)
        self.canvas.bind("<Button-5>", self._on_mousewheel)
        self.root.bind("<Control-z>", lambda e: self._undo_last_rule())

        bottom_frame = ttk.Frame(self.root)
        bottom_frame.pack(fill=tk.X, padx=6, pady=4)
        self.progress = ttk.Progressbar(bottom_frame, mode="determinate")
        self.progress.pack(fill=tk.X, pady=2)
        self.log_text = tk.Text(bottom_frame, height=6, wrap=tk.WORD, state=tk.DISABLED)
        self.log_text.pack(fill=tk.X)

        self._on_mode_changed()
    def _on_left_inner_configure(self, event) -> None:
        self.left_canvas.configure(scrollregion=self.left_canvas.bbox("all"))

    def _on_left_canvas_configure(self, event) -> None:
        self.left_canvas.itemconfigure(self._left_window_id, width=event.width)

    def _bind_left_wheel(self, _event) -> None:
        self.left_canvas.bind_all("<MouseWheel>", self._on_left_wheel)
        self.left_canvas.bind_all("<Button-4>", self._on_left_wheel)
        self.left_canvas.bind_all("<Button-5>", self._on_left_wheel)

    def _unbind_left_wheel(self, _event) -> None:
        self.left_canvas.unbind_all("<MouseWheel>")
        self.left_canvas.unbind_all("<Button-4>")
        self.left_canvas.unbind_all("<Button-5>")

    def _on_left_wheel(self, event) -> None:
        if getattr(event, "delta", 0):
            delta = -1 if event.delta > 0 else 1
        else:
            delta = -1 if event.num == 4 else 1
        self.left_canvas.yview_scroll(delta, "units")

    def _on_output_mode_changed(self) -> None:
        if self.overwrite_var.get():
            self.rb_source_dir.configure(state=tk.DISABLED)
            self.rb_custom_dir.configure(state=tk.DISABLED)
            self._update_output_label()
            self.save_dir_row.pack_forget()
        else:
            self.rb_source_dir.configure(state=tk.NORMAL)
            self.rb_custom_dir.configure(state=tk.NORMAL)
            self._update_output_label()
            if self.use_source_dir_var.get():
                self.save_dir_row.pack_forget()
            else:
                self.save_dir_row.pack(fill=tk.X, padx=(16, 0), pady=(2, 0))
        self._refresh_scroll_region()

    def _update_output_label(self) -> None:
        if self.overwrite_var.get():
            self.lbl_output.config(text=t("lbl_out_overwrite"))
        elif self.use_source_dir_var.get():
            self.lbl_output.config(text=t("lbl_out_source"))
        else:
            d = str(self.output_dir) if self.output_dir else ""
            if d:
                self.lbl_output.config(text=t("lbl_out_custom", path=d))
            else:
                self.lbl_output.config(text=t("lbl_out_unselected"))

    def _update_input_label(self) -> None:
        if not self.input_path:
            self.lbl_input.config(text=t("lbl_input_none"))
        elif self.input_path.is_dir():
            self.lbl_input.config(text=t("lbl_input_dir", path=self.input_path))
        else:
            self.lbl_input.config(text=t("lbl_input_file", path=self.input_path))

    def _refresh_scroll_region(self) -> None:
        self.left_canvas.after_idle(
            lambda: self.left_canvas.configure(scrollregion=self.left_canvas.bbox("all"))
        )

    def _on_mode_changed(self) -> None:
        if self.mode_var.get() == "auto":
            self.auto_frame.pack(fill=tk.X, pady=4)
            self.interactive_frame.pack_forget()
        else:
            self.auto_frame.pack_forget()
            self.interactive_frame.pack(fill=tk.X, pady=4, before=self.selected_frame)
        self._refresh_scroll_region()

    def _select_file(self) -> None:
        path = filedialog.askopenfilename(filetypes=[("PDF files", "*.pdf")])
        if path:
            self._open_input_file(Path(path))

    def _open_input_file(self, path: Path) -> None:
        path = Path(path)
        if not path.is_file() or path.suffix.lower() != ".pdf":
            messagebox.showerror(t("msg_err"), t("msg_err_not_pdf", path=path))
            return
        self.input_path = path
        self._update_input_label()
        self._update_output_label()
        self._load_document()

    def _select_input_dir(self) -> None:
        path = filedialog.askdirectory()
        if path:
            self.input_path = Path(path)
            self._update_input_label()
            self._update_output_label()

    def _select_output_dir(self) -> None:
        path = filedialog.askdirectory()
        if path:
            self.output_dir = Path(path)
            self._update_output_label()

    def _setup_drag_drop(self) -> None:
        if not DND_AVAILABLE:
            return
        try:
            self.root.drop_target_register(DND_FILES)
            self.root.dnd_bind("<<Drop>>", self._on_drop)
            self.log(t("msg_drag_supported"))
        except Exception as exc:
            self.log(t("msg_drag_fail", exc=exc))

    def _on_drop(self, event) -> None:
        data = str(event.data)
        paths: list[str] = []
        raw = data.strip()
        if raw.lower().endswith(".pdf"):
            paths.append(raw)
        try:
            for item in self.root.tk.splitlist(data):
                item = str(item).strip()
                if item and item not in paths:
                    paths.append(item)
        except Exception:
            pass
        for p in paths:
            if p.lower().endswith(".pdf"):
                self._open_input_file(Path(p))
                return
        self.log(t("msg_drag_no_pdf"))

    def _close_file(self) -> None:
        if self.doc is not None:
            try:
                self.doc.close()
            except Exception:
                pass
            self.doc = None
        self.input_path = None
        self.current_page = 0
        self.zoom = 1.0
        self.render_zoom = 1.0
        self.photo_image = None
        self.highlight_ids.clear()
        self.cleaner.clear_interactive_rules()
        self._update_rules_label()
        self.selected_text.config(state=tk.NORMAL)
        self.selected_text.delete("1.0", tk.END)
        self.selected_text.config(state=tk.DISABLED)
        self._update_input_label()
        self._update_page_label()
        self.btn_close_file.config(state=tk.DISABLED)
        self.canvas.delete("all")
        self.log(t("msg_closed"))

    def _load_document(self) -> None:
        if not self.input_path or not self.input_path.is_file():
            return
        try:
            if self.doc:
                self.doc.close()
            self.doc = fitz.open(self.input_path)
            self.current_page = 0
            self.zoom = 1.0
            self.btn_close_file.config(state=tk.NORMAL)
            self.log(t("msg_loaded", name=self.input_path.name, total=len(self.doc)))
            self._render_page()
        except Exception as exc:
            self.doc = None
            self.btn_close_file.config(state=tk.DISABLED)
            messagebox.showerror(t("msg_err_open"), str(exc))

    def _render_page(self) -> None:
        if not self.doc:
            return
        page = self.doc[self.current_page]
        canvas_w = max(self.canvas.winfo_width(), 400)
        base_zoom = canvas_w / page.rect.width
        zoom = base_zoom * self.zoom
        self.render_zoom = zoom
        mat = fitz.Matrix(zoom, zoom)
        pix = page.get_pixmap(matrix=mat)

        mode = "RGBA" if pix.alpha else "RGB"
        img = Image.frombytes(mode, (pix.width, pix.height), pix.samples)
        if mode == "RGBA":
            img = img.convert("RGB")
        self.photo_image = ImageTk.PhotoImage(img)

        self.canvas.delete("all")
        self.canvas_image_id = self.canvas.create_image(
            0, 0, anchor=tk.NW, image=self.photo_image,
        )
        self.canvas.config(scrollregion=(0, 0, pix.width, pix.height))
        self.highlight_ids.clear()
        self._update_page_label()

    def _update_page_label(self):
        if self.doc:
            self.lbl_page.config(text=t("lbl_page_count", page=self.current_page+1, total=len(self.doc)))
        else:
            self.lbl_page.config(text=t("lbl_page_count", page=0, total=0))

    def _page_navigation(self, delta: int) -> None:
        if not self.doc:
            return
        new_page = max(0, min(len(self.doc) - 1, self.current_page + delta))
        if new_page != self.current_page:
            self.current_page = new_page
            self._render_page()

    def _first_page(self) -> None:
        self.current_page = 0
        self._render_page()

    def _prev_page(self) -> None:
        self._page_navigation(-1)

    def _next_page(self) -> None:
        self._page_navigation(1)

    def _last_page(self) -> None:
        if self.doc:
            self.current_page = len(self.doc) - 1
            self._render_page()

    def _zoom_in(self) -> None:
        self.zoom *= 1.25
        self._render_page()

    def _zoom_out(self) -> None:
        self.zoom /= 1.25
        self._render_page()

    def _zoom_fit(self) -> None:
        self.zoom = 1.0
        self._render_page()

    def _on_mousewheel(self, event) -> None:
        if event.delta:
            if event.delta > 0:
                self._zoom_in()
            else:
                self._zoom_out()
        elif event.num == 4:
            self._zoom_in()
        elif event.num == 5:
            self._zoom_out()

    def _canvas_to_pdf_point(self, cx: float, cy: float) -> fitz.Point:
        if self.render_zoom <= 0:
            return fitz.Point(cx, cy)
        return fitz.Point(cx / self.render_zoom, cy / self.render_zoom)

    def _on_canvas_click(self, event) -> None:
        if not self.doc or self.mode_var.get() != "interactive":
            return
        point = self._canvas_to_pdf_point(event.x, event.y)
        candidates = self.cleaner.pick_element(self.doc, self.current_page, point, include_drawings=False, include_images=False)
        if not candidates:
            candidates = self.cleaner.pick_element(self.doc, self.current_page, point, include_drawings=False, include_images=True)
        if not candidates:
            candidates = self.cleaner.pick_element(self.doc, self.current_page, point, include_drawings=True, include_images=True)
        if not candidates:
            self.log(t("msg_click_miss", x=point.x, y=point.y))
            return

        if len(candidates) == 1:
            self._select_element(candidates[0])
        elif len(candidates) == 2 and {c.etype for c in candidates} == {"text", "link"}:
            if self._bboxes_overlap(candidates[0].bbox, candidates[1].bbox):
                for c in candidates:
                    self._select_element(c)
            else:
                self._show_candidate_menu(event, candidates)
        else:
            self._show_candidate_menu(event, candidates)

    @staticmethod
    def _bboxes_overlap(a: fitz.Rect, b: fitz.Rect) -> bool:
        inter = a & b
        if inter.is_empty:
            return False
        smaller = min(a.get_area(), b.get_area())
        return inter.get_area() / smaller > 0.5

    def _show_candidate_menu(self, event, candidates: list[Element]) -> None:
        menu = tk.Menu(self.root, tearoff=0)
        for el in candidates:
            label = f"{el.etype}: {el.content[:30]!r} [{el.bbox}]"
            menu.add_command(label=label, command=lambda e=el: self._select_element(e))
        menu.post(event.x_root, event.y_root)

    def _select_element(self, element: Element) -> None:
        try:
            tolerance = float(self.entry_tolerance.get())
        except ValueError:
            tolerance = 0.05
        rule = self.cleaner.create_rule(self.doc, self.current_page, element, match_by=self.match_by_var.get(), position_tolerance=tolerance)
        self.cleaner.add_interactive_rule(rule)
        self._update_rules_label()
        self._show_selected_info(element, rule)
        self._highlight_element(element)
        self.log(t("msg_rule_added", desc=rule.describe()))

    def _show_selected_info(self, element: Element, rule: MatchRule) -> None:
        self.selected_text.config(state=tk.NORMAL)
        self.selected_text.delete("1.0", tk.END)
        lines = [
            f"{t('sel_type')}: {element.etype}",
            f"{t('sel_page')}: {element.page_num + 1}",
            f"{t('sel_text')}: {element.content!r}",
            f"{t('sel_url')}: {element.url}",
            f"{t('sel_bbox')}: {element.bbox}",
            f"{t('sel_area')}: {element.rect_ratio:.2%}",
            f"{t('sel_opacity')}: {element.opacity}",
            "",
            t("sel_rule") + ":",
            rule.describe(),
            "",
            f"{t('sel_rules_count')}: {len(self.cleaner.interactive_rules)}",
        ]
        self.selected_text.insert(tk.END, "\n".join(lines))
        self.selected_text.config(state=tk.DISABLED)

    def _update_rules_label(self) -> None:
        self.lbl_rules.config(text=t("lbl_rules_count", count=len(self.cleaner.interactive_rules)))

    def _clear_rules(self) -> None:
        self.cleaner.clear_interactive_rules()
        self._update_rules_label()
        self.selected_text.config(state=tk.NORMAL)
        self.selected_text.delete("1.0", tk.END)
        self.selected_text.config(state=tk.DISABLED)
        self.log(t("msg_rules_cleared"))

    def _undo_last_rule(self) -> None:
        if not self.cleaner.interactive_rules:
            return
        
        rule = self.cleaner.interactive_rules.pop()
        
        if self.highlight_ids:
            hid = self.highlight_ids.pop()
            self.canvas.delete(hid)
            
        self._update_rules_label()
        
        self.selected_text.config(state=tk.NORMAL)
        self.selected_text.delete("1.0", tk.END)
        if self.cleaner.interactive_rules:
            lines = [
                f"{t('sel_rule')}:",
                self.cleaner.interactive_rules[-1].describe(),
                "",
                f"{t('sel_rules_count')}: {len(self.cleaner.interactive_rules)}",
            ]
            self.selected_text.insert(tk.END, "\n".join(lines))
        self.selected_text.config(state=tk.DISABLED)
        
        self.log(t("msg_rule_added", desc="Undo: " + rule.describe()[:30] + "..."))

    def _highlight_element(self, element: Element, color: str = "red") -> None:
        total_zoom = self.render_zoom
        x0 = element.bbox.x0 * total_zoom
        y0 = element.bbox.y0 * total_zoom
        x1 = element.bbox.x1 * total_zoom
        y1 = element.bbox.y1 * total_zoom
        hid = self.canvas.create_rectangle(x0, y0, x1, y1, outline=color, width=2)
        self.highlight_ids.append(hid)

    def _build_auto_options(self) -> AutoOptions:
        try:
            bottom_height = float(self.entry_bottom.get())
        except ValueError:
            bottom_height = 60.0

        return AutoOptions(
            remove_all_links=self.chk_links_var.get(),
            link_url_pattern=self.entry_url_pattern.get() if self.chk_url_pattern_var.get() else "",
            remove_bottom_strip=self.chk_bottom_var.get(),
            bottom_strip_height=bottom_height,
            remove_by_text_pattern=self.chk_text_var.get(),
            text_pattern=self.entry_text_pattern.get() if self.chk_text_var.get() else "",
            detect_transparent_overlays=self.chk_transparent_var.get(),
            render_fallback=self.chk_fallback_var.get(),
        )

    def _preview(self) -> None:
        if not self.doc:
            messagebox.showwarning(t("msg_info"), t("msg_warn_no_pdf"))
            return
        options = self._build_auto_options()
        self.canvas.delete("highlight")
        all_elements: list[Element] = []
        if self.mode_var.get() == "auto":
            all_elements = self.cleaner.preview_auto(self.doc, options)
        else:
            all_elements = self.cleaner.preview_interactive(self.doc)

        for el in all_elements:
            if el.page_num == self.current_page:
                self._highlight_element(el, "red")

        by_page: dict[int, int] = {}
        for el in all_elements:
            by_page[el.page_num] = by_page.get(el.page_num, 0) + 1
        summary = " | ".join(t("preview_pg", page=p + 1, count=c) for p, c in sorted(by_page.items()))
        self.log(t("msg_preview_done", count=len(all_elements), summary=summary))

    def _resolve_output_file(self, input_file: Path) -> Path:
        if self.overwrite_var.get():
            return None
        if self.use_source_dir_var.get():
            return input_file.parent / (input_file.stem + "_clean" + input_file.suffix)
        base_dir = self.output_dir or input_file.parent
        return base_dir / (input_file.stem + "_clean" + input_file.suffix)

    def _process(self) -> None:
        if not self.input_path:
            messagebox.showwarning(t("msg_info"), t("msg_err_no_input"))
            return

        overwrite = self.overwrite_var.get()
        use_interactive = self.mode_var.get() == "interactive"
        if use_interactive:
            options = AutoOptions()
        else:
            options = self._build_auto_options()

        if self.input_path.is_file():
            if overwrite:
                if self.doc:
                    self.doc.close()
                    self.doc = None
                result = self.cleaner.process(self.input_path, self.input_path, options, use_interactive_rules=use_interactive, overwrite=True)
                self._load_document()
            else:
                result = self.cleaner.process(self.input_path, self._resolve_output_file(self.input_path), options, use_interactive_rules=use_interactive)
            self.log(result.message)
            if result.success:
                messagebox.showinfo(t("msg_done"), result.message)
            else:
                messagebox.showerror(t("msg_err"), result.message)
        elif self.input_path.is_dir():
            if overwrite:
                prompt = t("msg_overwrite_confirm", input=self.input_path)
                out_dir = self.input_path
            else:
                if self.use_source_dir_var.get():
                    messagebox.showwarning(t("msg_info"), t("msg_err_batch_out"))
                    return
                if not self.output_dir:
                    messagebox.showwarning(t("msg_info"), t("msg_err_no_out_dir"))
                    return
                out_dir = self.output_dir
                prompt = t("msg_batch_confirm", input=self.input_path, output=out_dir)
            if not messagebox.askyesno(t("msg_confirm"), prompt):
                return
            results = self.cleaner.process_batch(self.input_path, out_dir, options, use_interactive_rules=use_interactive, overwrite=overwrite)
            success = sum(1 for r in results if r.success)
            fail = len(results) - success
            self.log(t("msg_batch_done", success=success, fail=fail))
            messagebox.showinfo(t("msg_done"), t("msg_batch_done", success=success, fail=fail))
        else:
            messagebox.showerror(t("msg_err"), t("msg_err_input_invalid"))

    def log(self, msg: str) -> None:
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)

    def run(self) -> None:
        self.root.mainloop()
