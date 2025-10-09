#!/usr/bin/env python3
"""
Generate glyph index ranges for a subset of Unicode codepoints in a font.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Iterable, List, Sequence, Tuple

from fontTools.ttLib import TTFont


def parse_range_token(token: str) -> Tuple[int, int]:
    """
    Parse a single range token into an inclusive [start, end] tuple.
    Accepts forms like "0020-007E" or "00A0".
    """
    token = token.strip()
    if not token:
        raise ValueError("Empty range token.")

    if "-" in token:
        start_str, end_str = token.split("-", 1)
    else:
        start_str = end_str = token

    try:
        start = int(start_str, 16)
        end = int(end_str, 16)
    except ValueError as exc:
        raise ValueError(f"Invalid hex value in range '{token}'.") from exc

    if start > end:
        raise ValueError(f"Start greater than end in range '{token}'.")

    return start, end


def load_codepoint_ranges(tokens: Iterable[str]) -> List[Tuple[int, int]]:
    return [parse_range_token(token) for token in tokens if token.strip()]


def iter_codepoints(range_pairs: Sequence[Tuple[int, int]]) -> Iterable[int]:
    for start, end in range_pairs:
        for value in range(start, end + 1):
            yield value


def read_ranges_from_file(path: Path) -> List[str]:
    """
    Read Unicode range tokens from a text file.

    Supports the new line-delimited format (one token or range per line),
    while remaining backward compatible with comma-separated lists. Inline
    comments starting with '#' are ignored. Blank lines are skipped.
    """
    tokens: List[str] = []
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        # Strip inline comments and surrounding whitespace
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        # Allow either a single token per line or comma-separated on a line
        for chunk in line.replace(",", " ").split():
            if chunk:
                tokens.append(chunk)
    return tokens


def collect_glyph_indices(font_path: Path, codepoints: Iterable[int]) -> Tuple[List[int], List[int]]:
    font = TTFont(font_path)
    cmap = font.getBestCmap()
    glyph_order = font.getGlyphOrder()
    glyph_to_index = {name: idx for idx, name in enumerate(glyph_order)}

    glyph_indices: List[int] = []
    missing: List[int] = []

    for codepoint in sorted(set(codepoints)):
        glyph_name = cmap.get(codepoint)
        if glyph_name is None:
            missing.append(codepoint)
            continue

        glyph_index = glyph_to_index.get(glyph_name)
        if glyph_index is None:
            missing.append(codepoint)
            continue

        glyph_indices.append(glyph_index)

    font.close()
    glyph_indices.sort()
    return glyph_indices, missing


def compress_indices(indices: Sequence[int]) -> List[Tuple[int, int]]:
    if not indices:
        return []

    compressed: List[Tuple[int, int]] = []
    start = prev = indices[0]

    for value in indices[1:]:
        if value == prev + 1:
            prev = value
            continue
        compressed.append((start, prev))
        start = prev = value

    compressed.append((start, prev))
    return compressed


def format_range(start: int, end: int) -> str:
    return f"[0x{start:02X}, 0x{end:02X}]"


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Emit glyph index ranges for the given Unicode ranges."
    )
    parser.add_argument(
        "font",
        type=Path,
        help="Path to a TTF/OTF font file.",
    )
    parser.add_argument(
        "ranges",
        nargs="*",
        help="Unicode hex ranges like 0020-007E or single values like 00A0.",
    )
    parser.add_argument(
        "--ranges-file",
        type=Path,
        help=(
            "Optional path to a text file containing line-delimited "
            "or comma-separated Unicode ranges (supports '#' comments)."
        ),
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional path to write the glyph index ranges. Defaults to stdout.",
    )
    parser.add_argument(
        "--include-missing",
        action="store_true",
        help="If set, log missing codepoints to stderr.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)

    tokens: List[str] = []
    if args.ranges_file:
        if not args.ranges_file.exists():
            raise FileNotFoundError(f"Ranges file not found: {args.ranges_file}")
        tokens.extend(read_ranges_from_file(args.ranges_file))

    tokens.extend(args.ranges)

    if not tokens:
        raise ValueError("No Unicode ranges provided.")

    range_pairs = load_codepoint_ranges(tokens)
    codepoints = list(iter_codepoints(range_pairs))

    glyph_indices, missing = collect_glyph_indices(args.font, codepoints)
    compressed = compress_indices(glyph_indices)
    lines = [format_range(start, end) for start, end in compressed]
    output_text = "\n".join(lines)

    if args.output:
        args.output.write_text(output_text + ("\n" if output_text else ""), encoding="utf-8")
    else:
        print(output_text)

    if missing and args.include_missing:
        missing_hex = ", ".join(f"0x{cp:04X}" for cp in missing)
        print(f"Missing glyphs for codepoints: {missing_hex}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
