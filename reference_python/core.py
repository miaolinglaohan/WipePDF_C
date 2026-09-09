from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, List, Optional, Tuple

import pymupdf as fitz

from . import content_edit as ce
from .detectors import (
    Element,
    detect_bottom_strip,
    detect_by_region,
    detect_by_text_pattern,
    detect_links,
    detect_pattern_overlays,
    detect_transparent_overlays,
    pick_element_at,
)
from .matcher import MatchRule, create_rule_from_element, find_matches
from .i18n import t

@dataclass
class AutoOptions:
    remove_all_links: bool = False
    link_url_pattern: str = ""
    remove_bottom_strip: bool = False
    bottom_strip_height: float = 60.0
    remove_by_text_pattern: bool = False
    text_pattern: str = ""
    detect_transparent_overlays: bool = False
    transparent_min_area_ratio: float = 0.5
    transparent_max_opacity: float = 0.35
    render_fallback: bool = False
    fallback_dpi: int = 200

@dataclass
class CleanerResult:
    success: bool
    message: str
    input_path: Path
    output_path: Optional[Path] = None
    removed_count: int = 0

class WatermarkCleaner:
    def __init__(self, progress_callback: Optional[Callable[[str], None]] = None):
        self.progress_callback = progress_callback or (lambda msg: None)
        self.interactive_rules: List[MatchRule] = []

    def log(self, msg: str) -> None:
        self.progress_callback(msg)

    def pick_element(self, doc: fitz.Document, page_num: int, point: fitz.Point, include_drawings: bool = True, include_images: bool = True) -> List[Element]:
        page = doc[page_num]
        return pick_element_at(page, point, page_num, include_drawings, include_images)

    def create_rule(self, doc: fitz.Document, page_num: int, element: Element, match_by: str = "auto", position_tolerance: float = 0.05) -> MatchRule:
        page = doc[page_num]
        return create_rule_from_element(page, element, match_by, position_tolerance)

    def add_interactive_rule(self, rule: MatchRule) -> None:
        self.interactive_rules.append(rule)

    def clear_interactive_rules(self) -> None:
        self.interactive_rules.clear()

    def preview_auto(self, doc: fitz.Document, options: AutoOptions) -> List[Element]:
        all_elements: List[Element] = []
        for page_num in range(len(doc)):
            page = doc[page_num]
            all_elements.extend(self._preview_auto_page(page, doc, page_num, options))
        return all_elements

    def _preview_auto_page(self, page: fitz.Page, doc: fitz.Document, page_num: int, options: AutoOptions) -> List[Element]:
        elements: List[Element] = []
        if options.remove_all_links:
            links = detect_links(page, page_num)
            if options.link_url_pattern:
                pat = re.compile(options.link_url_pattern)
                links = [el for el in links if pat.search(el.url)]
            elements.extend(links)
        if options.remove_bottom_strip:
            elements.extend(detect_bottom_strip(page, options.bottom_strip_height, page_num))
        if options.remove_by_text_pattern and options.text_pattern:
            elements.extend(detect_by_text_pattern(page, options.text_pattern, page_num))
        if options.detect_transparent_overlays:
            elements.extend(detect_transparent_overlays(page, page_num, options.transparent_min_area_ratio, options.transparent_max_opacity))
            elements.extend(detect_pattern_overlays(page, doc, page_num))
        return elements

    def preview_interactive(self, doc: fitz.Document) -> List[Element]:
        all_elements: List[Element] = []
        for page_num in range(len(doc)):
            page = doc[page_num]
            for rule in self.interactive_rules:
                all_elements.extend(find_matches(page, rule, page_num))
        return all_elements

    def process(self, input_path: Path, output_path: Path, options: AutoOptions, use_interactive_rules: bool = True, backup: bool = True, overwrite: bool = False) -> CleanerResult:
        if not input_path.exists():
            return CleanerResult(False, t("msg_err_load", err=t("err_not_found") + f": {input_path}"), input_path)

        input_path = Path(input_path)
        output_path = Path(output_path)
        if overwrite:
            output_path = input_path
        else:
            if output_path.is_dir() or str(output_path).endswith(("\\", "/")) or not output_path.suffix:
                output_path = output_path / (input_path.stem + "_clean" + input_path.suffix)
            try:
                if output_path.resolve() == input_path.resolve():
                    output_path = output_path.with_name(input_path.stem + "_clean" + input_path.suffix)
            except OSError:
                pass
        output_path.parent.mkdir(parents=True, exist_ok=True)

        try:
            doc = fitz.open(input_path)
        except Exception as exc:
            return CleanerResult(False, t("msg_err_load", err=str(exc)), input_path)

        try:
            removed = 0
            auto_removed = self._apply_auto_rules(doc, options)
            removed += auto_removed

            if use_interactive_rules and self.interactive_rules:
                interactive_removed = self._apply_interactive_rules(doc)
                removed += interactive_removed

            out_doc = doc
            if options.render_fallback:
                out_doc = self._render_fallback_doc(doc, options.fallback_dpi)
                doc.close()

            if overwrite:
                tmp_path = output_path.with_name(output_path.name + ".tmp")
                out_doc.save(tmp_path, garbage=4, deflate=True, clean=True)
                out_doc.close()
                os.replace(tmp_path, output_path)
            else:
                out_doc.save(output_path, garbage=4, deflate=True, clean=True)
                out_doc.close()

            msg = f"{t('success')}: {t('core_interact_match', count=removed)} -> {output_path}"
            return CleanerResult(True, msg, input_path, output_path, removed)

        except Exception as exc:
            if not doc.is_closed:
                doc.close()
            err = str(exc)
            if "cannot remove" in err or "Permission" in err or "denied" in err.lower():
                err = t("core_save_err", err=t("err_permission"))
            return CleanerResult(False, t("msg_err_process", err=err), input_path)

    def process_batch(self, input_dir: Path, output_dir: Path, options: AutoOptions, use_interactive_rules: bool = True, recursive: bool = True, overwrite: bool = False) -> List[CleanerResult]:
        input_dir = Path(input_dir)
        output_dir = Path(output_dir)
        pattern = "**/*.pdf" if recursive else "*.pdf"
        files = sorted(input_dir.glob(pattern))
        results: List[CleanerResult] = []

        for idx, pdf_path in enumerate(files, 1):
            rel_path = pdf_path.relative_to(input_dir)
            out_path = pdf_path if overwrite else output_dir / rel_path
            self.log(f"[{idx}/{len(files)}] {t('msg_processing', path=rel_path)}")
            result = self.process(pdf_path, out_path, options, use_interactive_rules=use_interactive_rules, overwrite=overwrite)
            results.append(result)
            self.log(result.message)

        return results

    def _apply_auto_rules(self, doc: fitz.Document, options: AutoOptions) -> int:
        removed = 0
        for page_num in range(len(doc)):
            page = doc[page_num]
            if options.remove_all_links:
                removed += self._remove_links(page, options.link_url_pattern)
            if options.remove_bottom_strip:
                rect = page.rect
                bottom = fitz.Rect(rect.x0, rect.y1 - options.bottom_strip_height, rect.x1, rect.y1)
                removed += self._remove_region_checked(page, doc, bottom)
            if options.remove_by_text_pattern and options.text_pattern:
                removed += self._remove_text_pattern(page, doc, options.text_pattern)
            if options.detect_transparent_overlays:
                overlays = detect_transparent_overlays(page, page_num, options.transparent_min_area_ratio, options.transparent_max_opacity)
                for el in overlays:
                    if el.rect_ratio < 0.9:
                        removed += self._remove_region_checked(page, doc, el.bbox)
                removed += self._remove_full_page_patterns(page, doc)
        return removed

    def _apply_interactive_rules(self, doc: fitz.Document) -> int:
        removed = 0
        for page_num in range(len(doc)):
            page = doc[page_num]
            for rule in self.interactive_rules:
                matches = find_matches(page, rule, page_num)
                for el in matches:
                    if self._remove_element(page, el, doc):
                        removed += 1
        return removed

    def _remove_links(self, page: fitz.Page, url_pattern: str = "") -> int:
        count = 0
        links = page.get_links()
        pat = re.compile(url_pattern) if url_pattern else None
        for link in links:
            if link.get("kind") != fitz.LINK_URI:
                continue
            if pat is None or pat.search(link.get("uri", "")):
                page.delete_link(link)
                count += 1
        return count

    def _remove_region(self, page: fitz.Page, region: fitz.Rect) -> int:
        region = region & page.rect
        if region.is_empty:
            return 0
        page.add_redact_annot(region)
        page.apply_redactions(images=fitz.PDF_REDACT_IMAGE_NONE)
        return 1

    @staticmethod
    def _text_remains(page: fitz.Page, region: fitz.Rect) -> bool:
        try:
            return bool(page.get_text(clip=region).strip())
        except Exception:
            return False

    def _remove_region_checked(self, page: fitz.Page, doc: fitz.Document, region: fitz.Rect) -> int:
        removed = self._remove_region(page, region)
        if self._text_remains(page, region):
            extra = ce.remove_text_in_region(page, doc, region)
            if extra:
                self.log(t("core_content_edit", count=extra))
                removed += extra
        return removed

    def _remove_full_page_patterns(self, page: fitz.Page, doc: fitz.Document) -> int:
        try:
            n = ce.remove_full_page_pattern_fills(page, doc)
        except Exception:
            return 0
        if n:
            self.log(t("core_found_pattern", count=n))
        return n

    def _remove_text_pattern(self, page: fitz.Page, doc: fitz.Document, pattern: str) -> int:
        count = 0
        pat = re.compile(pattern)
        data = page.get_text("dict", flags=fitz.TEXT_PRESERVE_LIGATURES)
        rects: List[fitz.Rect] = []
        for block in data["blocks"]:
            if block.get("type") != 0:
                continue
            for line in block.get("lines", []):
                spans = line.get("spans", [])
                line_text = "".join(s.get("text", "") for s in spans)
                if not pat.search(line_text):
                    continue
                matched_spans = [s for s in spans if pat.search(s.get("text", ""))]
                if matched_spans:
                    for s in matched_spans:
                        rects.append(fitz.Rect(s["bbox"]))
                else:
                    rects.append(fitz.Rect(line["bbox"]))
        for rect in rects:
            page.add_redact_annot(rect)
            count += 1
        if count:
            page.apply_redactions(images=fitz.PDF_REDACT_IMAGE_NONE)
            for rect in rects:
                if self._text_remains(page, rect):
                    count += ce.remove_text_in_region(page, doc, rect)
        return count

    def _remove_element(self, page: fitz.Page, el: Element, doc: fitz.Document) -> bool:
        if el.etype == "link":
            for link in page.get_links():
                if fitz.Rect(link.get("from", fitz.Rect())) == el.bbox:
                    page.delete_link(link)
                    return True
            return False

        if el.etype == "image":
            return self._remove_image(page, el)

        if el.etype in ("text", "drawing"):
            if el.bbox.is_empty:
                return False
            if el.etype == "drawing" and el.rect_ratio >= 0.9:
                return self._remove_full_page_patterns(page, doc) > 0
            page.add_redact_annot(el.bbox)
            page.apply_redactions(images=fitz.PDF_REDACT_IMAGE_NONE)
            if el.etype == "text" and self._text_remains(page, el.bbox):
                ce.remove_text_in_region(page, doc, el.bbox)
                if self._text_remains(page, el.bbox):
                    self._remove_full_page_patterns(page, doc)
            return True
        return False

    def _remove_image(self, page: fitz.Page, el: Element) -> bool:
        xref = el.extra.get("xref") if el.extra else None
        if xref is None:
            for item in page.get_images(full=True):
                try:
                    rects = page.get_image_rects(item[0])
                except Exception:
                    continue
                for r in rects:
                    if fitz.Rect(r) == el.bbox:
                        xref = item[0]
                        break
                if xref is not None:
                    break
        if xref is None:
            return False
        try:
            page.delete_image(xref)
            return True
        except Exception:
            return False

    def _render_fallback_doc(self, doc: fitz.Document, dpi: int) -> fitz.Document:
        zoom = dpi / 72.0
        mat = fitz.Matrix(zoom, zoom)
        new_doc = fitz.open()
        for page_num in range(len(doc)):
            page = doc[page_num]
            pix = page.get_pixmap(matrix=mat)
            new_page = new_doc.new_page(width=page.rect.width, height=page.rect.height)
            new_page.insert_image(new_page.rect, pixmap=pix)
            self.log(t("core_render_fallback"))
        return new_doc

    @staticmethod
    def render_page(doc: fitz.Document, page_num: int, width: Optional[int] = None, dpi: int = 150) -> fitz.Pixmap:
        page = doc[page_num]
        if width:
            zoom = width / page.rect.width
            mat = fitz.Matrix(zoom, zoom)
        else:
            zoom = dpi / 72.0
            mat = fitz.Matrix(zoom, zoom)
        return page.get_pixmap(matrix=mat)
