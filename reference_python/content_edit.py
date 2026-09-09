"""内容流级编辑：精确移除顽固水印文本/矢量。

针对 PyMuPDF redaction 无法处理的 PDF（典型特征：翻转坐标系 +
CID 子集字体 + 逐字形独立绘制的内容流，或水印绘制在整页图案
Pattern 中），直接解析并重写页面内容流：

- 解析内容流，把每个 BT..ET 文本段与 re..f 填充段切成独立片段；
- 文本段与 Page.get_texttrace() 提取的字形按顺序对齐，
  获得每段的真实渲染 bbox，删除 bbox 与目标区域相交的文本段；
- 检测并删除覆盖整页的图案填充段（铺满整页的透明元素）。
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import List, Optional, Tuple

import pymupdf as fitz


@dataclass
class ContentSegment:
    """内容流中的一个可删除片段。"""

    kind: str  # 'text' | 'fill'
    start: int  # 片段起点偏移（含 BT / re）
    end: int  # 片段终点偏移（含 ET / 填充符）
    glyph_count: int = 0  # 文本段绘制的字形数量（用于与提取字符对齐）
    pos: Optional[Tuple[float, float]] = None  # 文本基线原点（PDF 底部坐标）
    rect: Optional[fitz.Rect] = None  # fill 片段矩形（页面坐标）
    pattern_fill: bool = False  # fill 是否使用图案颜色（scn）

    def __repr__(self) -> str:
        if self.kind == "text":
            return f"Segment(text, {self.start}-{self.end}, glyphs={self.glyph_count}, pos={self.pos})"
        return f"Segment(fill, {self.start}-{self.end}, rect={self.rect}, pattern={self.pattern_fill})"


TOKEN_RE = re.compile(
    rb"""
    \s*(?:
        (?P<num>[+-]?\d+(?:\.\d+)?)
      | (?P<hex><[0-9A-Fa-f\s]*>)
      | (?P<str>\((?:\\.|[^()\\])*\))
      | (?P<op>[A-Za-z*'"]+)
      | (?P<bop>[<>\[\]{}])
    )
    """,
    re.VERBOSE,
)

FILL_OPS = {b"f", b"F", b"f*", b"B", b"B*", b"b", b"b*"}


def _matmul(m1: tuple, m2: tuple) -> tuple:
    """矩阵乘法（PDF CTM 约定 a b c d e f）。"""
    a1, b1, c1, d1, e1, f1 = m1
    a2, b2, c2, d2, e2, f2 = m2
    return (
        a1 * a2 + b1 * c2,
        a1 * b2 + b1 * d2,
        c1 * a2 + d1 * c2,
        c1 * b2 + d1 * d2,
        e1 * a2 + f1 * c2 + e2,
        e1 * b2 + f1 * d2 + f2,
    )


def _apply(m: tuple, x: float, y: float) -> Tuple[float, float]:
    a, b, c, d, e, f = m
    return (a * x + c * y + e, b * x + d * y + f)


def _transform_rect(m: tuple, x: float, y: float, w: float, h: float) -> fitz.Rect:
    p0 = _apply(m, x, y)
    p1 = _apply(m, x + w, y + h)
    return fitz.Rect(min(p0[0], p1[0]), min(p0[1], p1[1]), max(p0[0], p1[0]), max(p0[1], p1[1]))


def _is_hex_token(tok: bytes) -> bool:
    return (
        len(tok) >= 2
        and tok.startswith(b"<")
        and tok.endswith(b">")
        and not tok.startswith(b"<<")
        and not tok.endswith(b">>")
    )


def _token_glyph_count(tok: bytes, four_digit_cids: bool) -> int:
    """估算一个 hex 字符串 token 包含的字形数量。"""
    inner = tok[1:-1].replace(b" ", b"").replace(b"\r", b"").replace(b"\n", b"").replace(b"\t", b"")
    digits = len(inner)
    if digits == 0:
        return 0
    if four_digit_cids and digits % 4 == 0:
        return digits // 4
    if digits % 2 == 0:
        return digits // 2
    return digits // 2 + 1


def parse_segments(data: bytes, four_digit_cids: bool = True) -> List[ContentSegment]:
    """解析内容流，返回文本段与填充段列表（按流内顺序）。"""
    segments: List[ContentSegment] = []
    stack: List[tuple] = []
    ctm = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
    num_buf: List[float] = []
    in_text = False
    text_start: Optional[int] = None
    tm = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
    glyph_count = 0
    last_re: Optional[Tuple[float, float, float, float, tuple]] = None
    re_start: Optional[int] = None
    prev_bytes = b""

    for m in TOKEN_RE.finditer(data):
        tok = m.group(0).strip()
        if not tok:
            continue
        prev_bytes = data[max(0, m.start() - 200) : m.start()]

        if tok == b"q":
            stack.append(ctm)
            num_buf.clear()
            continue
        if tok == b"Q":
            if stack:
                ctm = stack.pop()
            num_buf.clear()
            continue
        if tok == b"BT":
            in_text = True
            text_start = m.start()
            tm = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)
            glyph_count = 0
            num_buf.clear()
            continue
        if tok == b"ET":
            if in_text and text_start is not None:
                x, y = _apply(ctm, tm[4], tm[5])
                segments.append(
                    ContentSegment(
                        kind="text",
                        start=text_start,
                        end=m.end(),
                        glyph_count=glyph_count,
                        pos=(x, y),
                    )
                )
            in_text = False
            num_buf.clear()
            continue
        if tok == b"cm" and len(num_buf) >= 6:
            ctm = _matmul(ctm, tuple(num_buf[-6:]))
            num_buf.clear()
            continue
        if tok == b"Tm" and len(num_buf) >= 6:
            tm = tuple(num_buf[-6:])
            num_buf.clear()
            continue
        if tok == b"re" and len(num_buf) >= 4:
            x, y, w, h = num_buf[-4:]
            last_re = (x, y, w, h, ctm)
            re_start = m.start()
            num_buf.clear()
            continue
        if tok in FILL_OPS:
            if last_re is not None and re_start is not None:
                x, y, w, h, mtx = last_re
                rect = _transform_rect(mtx, x, y, w, h)
                pattern_fill = b"scn" in prev_bytes or b"SCN" in prev_bytes
                segments.append(
                    ContentSegment(
                        kind="fill",
                        start=re_start,
                        end=m.end(),
                        rect=rect,
                        pattern_fill=pattern_fill,
                    )
                )
            num_buf.clear()
            continue
        if _is_hex_token(tok):
            if in_text:
                glyph_count += _token_glyph_count(tok, four_digit_cids)
            num_buf.clear()
            continue
        # 数字
        try:
            num_buf.append(float(tok))
            continue
        except ValueError:
            pass
        # 其它操作符：不属于 cm/Tm/re/填充，直接丢弃数字缓冲
        num_buf.clear()

    return segments


def remove_segments(data: bytes, segments: List[ContentSegment]) -> bytes:
    """从内容流中删除指定片段。"""
    out = bytearray()
    prev = 0
    for seg in sorted(segments, key=lambda s: s.start):
        if seg.start < prev:
            continue  # 重叠片段去重
        out += data[prev : seg.start]
        prev = seg.end
    out += data[prev:]
    return bytes(out)


def page_content_bytes(page: fitz.Page, doc: fitz.Document) -> bytes:
    """合并页面全部内容流。"""
    parts = [doc.xref_stream(x) for x in page.get_contents()]
    return b"\n".join(p for p in parts if p)


def set_page_content(page: fitz.Page, doc: fitz.Document, data: bytes) -> None:
    """用新数据替换页面内容流（创建新流对象，不修改原有流）。"""
    xref = doc.get_new_xref()
    doc.update_object(xref, "<<>>")
    doc.update_stream(xref, data)
    page.set_contents(xref)


def _aligned_text_segments(
    page: fitz.Page,
    doc: fitz.Document,
) -> Optional[List[Tuple[ContentSegment, fitz.Rect]]]:
    """将内容流文本段与 texttrace 字形按顺序对齐，返回每段的真实渲染 bbox。

    内容流中文本段的绘制顺序与 get_texttrace() 提取的字形顺序一致，
    每个段的字形数量 = 其 hex 字形 token 对应的字形数，据此按顺序对齐，
    得到每个文本段的真实渲染 bbox（无需自行推导 CTM）。

    优先按 4 位 CID 解析；若字形总数与提取字符数不符则回退 2 位；
    仍不符或内容流含无法计数的文本时返回 None。
    """
    data = page_content_bytes(page, doc)
    try:
        trace = page.get_texttrace()
    except Exception:
        return None
    chars: List[Tuple[int, int, tuple, tuple]] = []
    for s in trace:
        for c in s["chars"]:
            if isinstance(c, (tuple, list)) and len(c) >= 4:
                chars.append(c)

    segments: Optional[List[ContentSegment]] = None
    for mode4 in (True, False):
        segs = [s for s in parse_segments(data, four_digit_cids=mode4) if s.kind == "text"]
        if any(s.glyph_count == 0 for s in segs):
            # 存在无法确定字形数量的文本段（如未使用 hex 编码）
            segments = None
            break
        if sum(s.glyph_count for s in segs) == len(chars):
            segments = segs
            break
    if segments is None:
        return None

    aligned: List[Tuple[ContentSegment, fitz.Rect]] = []
    cursor = 0
    for seg in segments:
        n = seg.glyph_count
        group = chars[cursor : cursor + n]
        cursor += n
        if not group:
            continue
        rects = [fitz.Rect(c[3]) for c in group]
        bbox = rects[0]
        for r in rects[1:]:
            bbox |= r
        aligned.append((seg, bbox))
    return aligned


def remove_text_in_region(page: fitz.Page, doc: fitz.Document, region: fitz.Rect) -> int:
    """内容流级删除 region 内的文本（redaction 无效时的后备方案）。

    按内容流文本段与 texttrace 字形的顺序对齐得到每段的真实渲染
    bbox，删除 bbox 与 region 相交的文本段。
    """
    region = fitz.Rect(region) & page.rect
    if region.is_empty:
        return 0
    aligned = _aligned_text_segments(page, doc)
    if aligned is None:
        return 0

    to_remove = [seg for seg, bbox in aligned if not (bbox & region).is_empty]
    if not to_remove:
        return 0
    data = page_content_bytes(page, doc)
    set_page_content(page, doc, remove_segments(data, to_remove))
    return len(to_remove)


def detect_full_page_pattern_fills(
    page: fitz.Page,
    doc: fitz.Document,
    min_ratio: float = 0.9,
) -> List[ContentSegment]:
    """检测覆盖整页的图案填充段（铺满整页的透明元素）。"""
    data = page_content_bytes(page, doc)
    segments = parse_segments(data)
    page_area = page.rect.get_area()
    return [
        s
        for s in segments
        if s.kind == "fill"
        and s.pattern_fill
        and s.rect is not None
        and s.rect.get_area() >= page_area * min_ratio
    ]


def remove_full_page_pattern_fills(page: fitz.Page, doc: fitz.Document, min_ratio: float = 0.9) -> int:
    """删除整页图案填充段。"""
    fills = detect_full_page_pattern_fills(page, doc, min_ratio)
    if not fills:
        return 0
    data = page_content_bytes(page, doc)
    set_page_content(page, doc, remove_segments(data, fills))
    return len(fills)
