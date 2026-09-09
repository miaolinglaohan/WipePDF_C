"""交互式点选元素的跨页匹配器。

用户点选一个水印元素后，根据该元素生成匹配规则，
然后在所有页面查找相同/相似元素，统一清除。
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import List, Optional

import pymupdf as fitz

from .detectors import (
    Element,
    detect_bottom_strip,
    detect_by_region,
    detect_by_text_pattern,
    detect_drawings,
    detect_images,
    detect_links,
    detect_text_blocks,
)


@dataclass
class MatchRule:
    """由点选元素生成的跨页匹配规则。"""

    etype: str  # 'link', 'text', 'drawing', 'image'
    text_pattern: Optional[re.Pattern] = None
    url_pattern: Optional[re.Pattern] = None
    # 相对页面尺寸的比例
    min_width_ratio: float = 0.0
    max_width_ratio: float = 1.0
    min_height_ratio: float = 0.0
    max_height_ratio: float = 1.0
    min_rect_ratio: float = 0.0
    max_rect_ratio: float = 1.0
    # 相对位置（元素中心）
    anchor_x: Optional[float] = None  # 0.0~1.0，None 表示不限制
    anchor_y: Optional[float] = None
    position_tolerance: float = 0.05  # 相对位置容差
    # 透明度
    min_opacity: float = 0.0
    max_opacity: float = 1.0
    # 区域模式：直接指定相对 rect
    relative_region: Optional[tuple] = None  # (x0, y0, x1, y1) in 0~1

    # 文本匹配时是否忽略空白差异
    ignore_whitespace: bool = False

    extra: dict = field(default_factory=dict)

    def describe(self) -> str:
        parts = [f"类型={self.etype}"]
        if self.text_pattern is not None:
            parts.append(f"文本正则={self.text_pattern.pattern}")
        if self.url_pattern is not None:
            parts.append(f"URL正则={self.url_pattern.pattern}")
        if self.relative_region is not None:
            parts.append(f"区域={self.relative_region}")
        if self.anchor_y is not None:
            parts.append(f"垂直中心≈{self.anchor_y:.2f}±{self.position_tolerance:.2f}")
        if self.min_rect_ratio > 0:
            parts.append(f"面积比≥{self.min_rect_ratio:.2f}")
        return "; ".join(parts)


def _normalize_text(text: str) -> str:
    """归一化文本用于比较。"""
    return re.sub(r"\s+", "", text)


def _bbox_relative_center(bbox: fitz.Rect, page_rect: fitz.Rect) -> tuple:
    """返回 bbox 中心在页面上的相对坐标。"""
    cx = (bbox.x0 + bbox.x1) / 2 / page_rect.width
    cy = (bbox.y0 + bbox.y1) / 2 / page_rect.height
    return cx, cy


def _bbox_relative_size(bbox: fitz.Rect, page_rect: fitz.Rect) -> tuple:
    """返回 bbox 相对页面宽高。"""
    w = bbox.width / page_rect.width
    h = bbox.height / page_rect.height
    return w, h


def element_matches(el: Element, rule: MatchRule, page_rect: fitz.Rect) -> bool:
    """判断单个元素是否符合匹配规则。"""
    if el.etype != rule.etype:
        return False

    # 区域模式
    if rule.relative_region is not None:
        rx0, ry0, rx1, ry1 = rule.relative_region
        region = fitz.Rect(
            rx0 * page_rect.width,
            ry0 * page_rect.height,
            rx1 * page_rect.width,
            ry1 * page_rect.height,
        )
        if (el.bbox & region).is_empty:
            return False

    # 文本匹配
    if rule.text_pattern is not None:
        text = el.content
        if rule.ignore_whitespace:
            text = _normalize_text(text)
        if not rule.text_pattern.search(text):
            return False

    # URL 匹配（仅 link）
    if rule.url_pattern is not None and el.etype == "link":
        if not rule.url_pattern.search(el.url):
            return False

    # 尺寸匹配
    w_ratio, h_ratio = _bbox_relative_size(el.bbox, page_rect)
    if not (rule.min_width_ratio <= w_ratio <= rule.max_width_ratio):
        return False
    if not (rule.min_height_ratio <= h_ratio <= rule.max_height_ratio):
        return False
    if not (rule.min_rect_ratio <= el.rect_ratio <= rule.max_rect_ratio):
        return False

    # 位置匹配
    if rule.anchor_x is not None or rule.anchor_y is not None:
        cx, cy = _bbox_relative_center(el.bbox, page_rect)
        tol = rule.position_tolerance
        if rule.anchor_x is not None and abs(cx - rule.anchor_x) > tol:
            return False
        if rule.anchor_y is not None and abs(cy - rule.anchor_y) > tol:
            return False

    # 透明度
    if not (rule.min_opacity <= el.opacity <= rule.max_opacity):
        return False

    return True


def find_matches(page: fitz.Page, rule: MatchRule, page_num: int = 0) -> List[Element]:
    """在指定页面查找所有匹配规则的水印元素。"""
    page_rect = page.rect
    candidates: List[Element] = []

    if rule.etype == "link":
        candidates = detect_links(page, page_num)
    elif rule.etype == "text":
        candidates = detect_text_blocks(page, page_num)
    elif rule.etype == "drawing":
        candidates = detect_drawings(page, page_num)
    elif rule.etype == "image":
        candidates = detect_images(page, page_num)
    elif rule.relative_region is not None:
        # 通用区域模式，不限元素类型
        candidates = detect_by_region(page, _relative_rect(page, rule.relative_region), page_num)

    return [el for el in candidates if element_matches(el, rule, page_rect)]


def _relative_rect(page: fitz.Page, relative_region: tuple) -> fitz.Rect:
    rx0, ry0, rx1, ry1 = relative_region
    rect = page.rect
    return fitz.Rect(
        rx0 * rect.width,
        ry0 * rect.height,
        rx1 * rect.width,
        ry1 * rect.height,
    )


def create_rule_from_element(
    page: fitz.Page,
    element: Element,
    match_by: str = "auto",
    position_tolerance: float = 0.05,
) -> MatchRule:
    """根据用户点选的元素生成匹配规则。

    match_by:
      - 'auto': 自动判断，文本型优先按文本，大面积按尺寸/位置，链接按 URL/位置。
      - 'text': 按文本内容精确匹配。
      - 'position': 按相对位置匹配。
      - 'region': 按元素所在相对区域匹配。
      - 'url': 按链接 URL 匹配（仅 link）。
    """
    page_rect = page.rect
    cx, cy = _bbox_relative_center(element.bbox, page_rect)
    w_ratio, h_ratio = _bbox_relative_size(element.bbox, page_rect)

    # 默认规则
    rule = MatchRule(
        etype=element.etype,
        anchor_x=cx,
        anchor_y=cy,
        position_tolerance=position_tolerance,
        min_width_ratio=max(0.0, w_ratio * 0.7),
        max_width_ratio=min(1.0, w_ratio * 1.3 + 0.05),
        min_height_ratio=max(0.0, h_ratio * 0.7),
        max_height_ratio=min(1.0, h_ratio * 1.3 + 0.05),
        min_opacity=max(0.0, element.opacity - 0.1),
        max_opacity=min(1.0, element.opacity + 0.1),
    )

    if match_by == "auto":
        if element.etype == "link":
            # 链接优先按 URL，其次按位置
            if element.url:
                rule.url_pattern = re.compile(re.escape(element.url))
            match_by = "position"
        elif element.etype == "text":
            # 小文本优先按文本内容，否则按位置
            if element.content.strip() and element.rect_ratio < 0.1:
                match_by = "text"
            else:
                match_by = "position"
        elif element.etype == "drawing":
            # 大面积透明层按尺寸比例
            if element.rect_ratio > 0.4:
                rule.min_rect_ratio = max(0.3, element.rect_ratio * 0.7)
                rule.max_rect_ratio = min(1.0, element.rect_ratio * 1.05)
                rule.anchor_x = None
                rule.anchor_y = None
                match_by = "size"
            else:
                match_by = "position"
        elif element.etype == "image":
            # 图片（二维码等）默认按位置匹配
            match_by = "position"

    if match_by == "text":
        text = element.content.strip()
        if text:
            # 对长文本取前 30 个字符做精确匹配，避免换行影响
            sample = text[:30]
            escaped = re.escape(sample)
            rule.text_pattern = re.compile(escaped)
            rule.anchor_x = None
            rule.anchor_y = None

    elif match_by == "url":
        if element.url:
            # 对 URL 做域名级模糊匹配
            parsed = re.sub(r"^https?://", "", element.url)
            domain = parsed.split("/")[0]
            if domain:
                rule.url_pattern = re.compile(re.escape(domain))
        rule.anchor_x = None
        rule.anchor_y = None

    elif match_by == "region":
        # 以元素 bbox 扩展一定比例作为相对区域
        margin = 0.02
        rx0 = max(0.0, element.bbox.x0 / page_rect.width - margin)
        ry0 = max(0.0, element.bbox.y0 / page_rect.height - margin)
        rx1 = min(1.0, element.bbox.x1 / page_rect.width + margin)
        ry1 = min(1.0, element.bbox.y1 / page_rect.height + margin)
        rule.relative_region = (rx0, ry0, rx1, ry1)
        rule.anchor_x = None
        rule.anchor_y = None

    elif match_by == "size":
        # 仅保留尺寸比例限制
        rule.anchor_x = None
        rule.anchor_y = None

    elif match_by == "position":
        # 保持默认位置规则
        pass

    return rule


def refine_rule(
    rule: MatchRule,
    text_pattern: Optional[str] = None,
    url_pattern: Optional[str] = None,
    position_tolerance: Optional[float] = None,
    min_rect_ratio: Optional[float] = None,
    max_rect_ratio: Optional[float] = None,
) -> MatchRule:
    """允许用户手动微调规则参数。"""
    new_rule = MatchRule(
        etype=rule.etype,
        text_pattern=re.compile(text_pattern) if text_pattern else rule.text_pattern,
        url_pattern=re.compile(url_pattern) if url_pattern else rule.url_pattern,
        min_width_ratio=rule.min_width_ratio,
        max_width_ratio=rule.max_width_ratio,
        min_height_ratio=rule.min_height_ratio,
        max_height_ratio=rule.max_height_ratio,
        min_rect_ratio=min_rect_ratio if min_rect_ratio is not None else rule.min_rect_ratio,
        max_rect_ratio=max_rect_ratio if max_rect_ratio is not None else rule.max_rect_ratio,
        anchor_x=rule.anchor_x,
        anchor_y=rule.anchor_y,
        position_tolerance=position_tolerance if position_tolerance is not None else rule.position_tolerance,
        min_opacity=rule.min_opacity,
        max_opacity=rule.max_opacity,
        relative_region=rule.relative_region,
        ignore_whitespace=rule.ignore_whitespace,
        extra=rule.extra,
    )
    return new_rule
