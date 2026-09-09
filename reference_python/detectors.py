"""水印与链接元素检测器。

提供多种检测策略，用于识别 PDF 页面中的：
- 超链接注释
- 文本块
- 矢量路径/透明覆盖层
- 指定区域内的内容
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Callable, Iterable, List, Optional

import pymupdf as fitz


@dataclass
class Element:
    """页面上的可删除元素。"""

    etype: str  # 'link', 'text', 'drawing', 'image'
    page_num: int
    bbox: fitz.Rect
    content: str = ""
    url: str = ""
    opacity: float = 1.0
    fill_color: Optional[tuple] = None
    stroke_color: Optional[tuple] = None
    rect_ratio: float = 0.0  # 元素面积占页面比例
    extra: dict = field(default_factory=dict)

    def __repr__(self) -> str:
        if self.etype == "link":
            return f"Element(link, p{self.page_num}, url={self.url[:40]!r}, bbox={self.bbox})"
        if self.etype == "text":
            text = self.content.replace("\n", " ")[:40]
            return f"Element(text, p{self.page_num}, content={text!r}, bbox={self.bbox})"
        return f"Element({self.etype}, p{self.page_num}, bbox={self.bbox})"


def _effective_opacity(d: dict) -> float:
    """获取 drawing 的有效透明度（opacity 优先，其次 fill/stroke opacity）。"""
    opacity = d.get("opacity")
    if opacity is None:
        opacity = d.get("fill_opacity")
    if opacity is None:
        opacity = d.get("stroke_opacity")
    return 1.0 if opacity is None else float(opacity)


def _area_ratio(bbox: fitz.Rect, page_rect: fitz.Rect) -> float:
    """计算 bbox 面积占页面的比例。"""
    if bbox.is_empty or page_rect.is_empty:
        return 0.0
    inter = bbox & page_rect
    if inter.is_empty:
        return 0.0
    return inter.get_area() / page_rect.get_area()


def detect_links(page: fitz.Page, page_num: int = 0) -> List[Element]:
    """检测页面上的链接注释。"""
    elements: List[Element] = []
    page_rect = page.rect
    for link in page.get_links():
        if link.get("kind") != fitz.LINK_URI:
            continue
        bbox = fitz.Rect(link.get("from", fitz.Rect()))
        elements.append(
            Element(
                etype="link",
                page_num=page_num,
                bbox=bbox,
                url=link.get("uri", ""),
                rect_ratio=_area_ratio(bbox, page_rect),
            )
        )
    return elements


def detect_text_blocks(page: fitz.Page, page_num: int = 0) -> List[Element]:
    """检测页面上的文本块。"""
    elements: List[Element] = []
    page_rect = page.rect
    blocks = page.get_text("blocks", flags=fitz.TEXT_PRESERVE_LIGATURES)
    for block in blocks:
        # block: (x0, y0, x1, y1, text, block_no, block_type)
        x0, y0, x1, y1, text, *_ = block
        bbox = fitz.Rect(x0, y0, x1, y1)
        elements.append(
            Element(
                etype="text",
                page_num=page_num,
                bbox=bbox,
                content=text,
                rect_ratio=_area_ratio(bbox, page_rect),
            )
        )
    return elements


def detect_drawings(page: fitz.Page, page_num: int = 0) -> List[Element]:
    """检测页面上的矢量路径。"""
    elements: List[Element] = []
    page_rect = page.rect
    for i, d in enumerate(page.get_drawings()):
        rect = d.get("rect", fitz.Rect())
        items = d.get("items", [])
        color = d.get("color")
        fill = d.get("fill")
        opacity = _effective_opacity(d)
        # 估算是否为大面积极简路径
        is_rect_like = any(op[0] in ("re", "c") for op in items)
        elements.append(
            Element(
                etype="drawing",
                page_num=page_num,
                bbox=rect,
                opacity=opacity,
                fill_color=fill,
                stroke_color=color,
                rect_ratio=_area_ratio(rect, page_rect),
                extra={"items": items, "is_rect_like": is_rect_like, "index": i},
            )
        )
    return elements


def detect_images(page: fitz.Page, page_num: int = 0) -> List[Element]:
    """检测页面上的图片元素（如二维码水印）。"""
    elements: List[Element] = []
    page_rect = page.rect
    try:
        images = page.get_images(full=True)
    except Exception:
        return elements
    for item in images:
        xref = item[0]
        try:
            rects = page.get_image_rects(xref)
        except Exception:
            rects = []
        for rect in rects:
            bbox = fitz.Rect(rect)
            elements.append(
                Element(
                    etype="image",
                    page_num=page_num,
                    bbox=bbox,
                    rect_ratio=_area_ratio(bbox, page_rect),
                    extra={"xref": xref},
                )
            )
    return elements


def detect_transparent_overlays(
    page: fitz.Page,
    page_num: int = 0,
    min_area_ratio: float = 0.5,
    max_opacity: float = 0.35,
) -> List[Element]:
    """检测可能为透明水印覆盖层的大面积极简矢量路径。"""
    elements: List[Element] = []
    page_rect = page.rect
    for i, d in enumerate(page.get_drawings()):
        rect = d.get("rect", fitz.Rect())
        opacity = _effective_opacity(d)
        items = d.get("items", [])
        fill = d.get("fill")
        area_ratio = _area_ratio(rect, page_rect)
        # 路径类型简单（矩形/曲线）且覆盖面积大、透明度低
        is_rect_like = any(op[0] in ("re", "c") for op in items)
        if area_ratio >= min_area_ratio and opacity <= max_opacity and is_rect_like:
            elements.append(
                Element(
                    etype="drawing",
                    page_num=page_num,
                    bbox=rect,
                    opacity=opacity,
                    fill_color=fill,
                    rect_ratio=area_ratio,
                    extra={"items": items, "is_rect_like": True, "index": i},
                )
            )
    return elements


def detect_by_text_pattern(
    page: fitz.Page,
    pattern: str | re.Pattern,
    page_num: int = 0,
) -> List[Element]:
    """按正则表达式检测文本水印。"""
    if isinstance(pattern, str):
        pattern = re.compile(pattern)
    elements: List[Element] = []
    page_rect = page.rect
    blocks = page.get_text("blocks", flags=fitz.TEXT_PRESERVE_LIGATURES)
    for block in blocks:
        x0, y0, x1, y1, text, *_ = block
        if pattern.search(text):
            bbox = fitz.Rect(x0, y0, x1, y1)
            elements.append(
                Element(
                    etype="text",
                    page_num=page_num,
                    bbox=bbox,
                    content=text,
                    rect_ratio=_area_ratio(bbox, page_rect),
                )
            )
    return elements


def detect_pattern_overlays(
    page: fitz.Page,
    doc: fitz.Document,
    page_num: int = 0,
    min_ratio: float = 0.9,
) -> List[Element]:
    """检测铺满整页的图案填充（透明水印层常见形态）。

    部分 PDF 将水印文字绘制在整页图案（Pattern）中，
    普通图形检测无法发现，需解析内容流。
    """
    from . import content_edit as ce

    elements: List[Element] = []
    try:
        fills = ce.detect_full_page_pattern_fills(page, doc, min_ratio)
    except Exception:
        return elements
    for seg in fills:
        bbox = seg.rect if seg.rect is not None else page.rect
        elements.append(
            Element(
                etype="drawing",
                page_num=page_num,
                bbox=bbox,
                rect_ratio=_area_ratio(bbox, page.rect),
                extra={"pattern_fill": True},
            )
        )
    return elements


def detect_by_region(
    page: fitz.Page,
    region: fitz.Rect,
    page_num: int = 0,
) -> List[Element]:
    """检测指定区域内的所有文本和矢量内容。"""
    elements: List[Element] = []
    region = region & page.rect
    if region.is_empty:
        return elements

    for text_el in detect_text_blocks(page, page_num):
        if not (text_el.bbox & region).is_empty:
            elements.append(text_el)

    for draw_el in detect_drawings(page, page_num):
        if not (draw_el.bbox & region).is_empty:
            elements.append(draw_el)

    for link_el in detect_links(page, page_num):
        if not (link_el.bbox & region).is_empty:
            elements.append(link_el)

    return elements


def detect_bottom_strip(
    page: fitz.Page,
    height: float,
    page_num: int = 0,
) -> List[Element]:
    """检测页面底部指定高度区域内的内容。"""
    rect = page.rect
    bottom = fitz.Rect(rect.x0, rect.y1 - height, rect.x1, rect.y1)
    return detect_by_region(page, bottom, page_num)


def pick_element_at(
    page: fitz.Page,
    point: fitz.Point,
    page_num: int = 0,
    include_drawings: bool = True,
    include_images: bool = True,
) -> List[Element]:
    """获取点击位置命中的所有元素，按面积从小到大排序（小元素优先）。"""
    candidates: List[Element] = []

    for el in detect_links(page, page_num):
        if point in el.bbox:
            candidates.append(el)

    for el in detect_text_blocks(page, page_num):
        if point in el.bbox:
            candidates.append(el)

    if include_drawings:
        for el in detect_drawings(page, page_num):
            if point in el.bbox:
                candidates.append(el)

    if include_images:
        for el in detect_images(page, page_num):
            if point in el.bbox:
                candidates.append(el)

    # 面积小的排在前面，方便精确点选
    candidates.sort(key=lambda e: e.bbox.get_area())
    return candidates


def filter_elements(
    elements: Iterable[Element],
    predicate: Callable[[Element], bool],
) -> List[Element]:
    """通用元素过滤。"""
    return [el for el in elements if predicate(el)]
