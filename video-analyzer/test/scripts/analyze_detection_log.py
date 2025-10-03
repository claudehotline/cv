#!/usr/bin/env python3
"""检测日志统计工具。

从 VideoAnalyzer 标准输出或日志文件中解析
“Analysis complete - source: <id>, request: <seq>, detections: <n>”格式
的记录，计算每路视频源的检测框统计信息。
"""

from __future__ import annotations

import argparse
import math
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import DefaultDict, Iterable, Iterator, List, Sequence
import re


@dataclass
class DetectionRecord:
    source: str
    request_id: int
    detections: int


LINE_PATTERN = re.compile(
    r"Analysis complete - source: (?P<source>[^,]+), request: (?P<request>\d+), detections: (?P<count>\d+)"
)


def iter_records(path: Path, prefix: str | None, max_count: int | None) -> Iterator[DetectionRecord]:
    """逐行解析日志文件。"""

    if not path.exists():
        raise FileNotFoundError(f"日志文件不存在: {path}")

    with path.open("r", encoding="utf-8", errors="ignore") as handle:
        for raw in handle:
            segments = raw.strip().split("\r")
            for segment in segments:
                match = LINE_PATTERN.search(segment)
                if not match:
                    continue
                source = match.group("source").strip()
                if prefix and not source.startswith(prefix):
                    continue

                detections = int(match.group("count"))
                if max_count is not None and detections > max_count:
                    # 过滤格式异常或脏数据
                    continue

                yield DetectionRecord(
                    source=source,
                    request_id=int(match.group("request")),
                    detections=detections,
                )


def percentile(values: Sequence[int], ratio: float) -> float:
    if not values:
        return 0.0
    sorted_values = sorted(values)
    pos = (len(sorted_values) - 1) * ratio
    lower = math.floor(pos)
    upper = math.ceil(pos)
    if lower == upper:
        return float(sorted_values[int(pos)])
    base = sorted_values[lower]
    diff = sorted_values[upper] - base
    return base + diff * (pos - lower)


def build_summary(records: Iterable[DetectionRecord], threshold: int) -> tuple[List[str], List[DetectionRecord]]:
    grouped: DefaultDict[str, List[int]] = defaultdict(list)
    entries: List[DetectionRecord] = []

    for record in records:
        grouped[record.source].append(record.detections)
        entries.append(record)

    lines: List[str] = []
    for source in sorted(grouped):
        values = grouped[source]
        total = len(values)
        zero = sum(1 for v in values if v == 0)
        high = sum(1 for v in values if v >= threshold)
        avg = sum(values) / total if total else 0.0
        p50 = percentile(values, 0.5)
        p95 = percentile(values, 0.95)
        line = (
            f"{source}: frames={total}, "
            f"avg={avg:.2f}, median={p50:.2f}, p95={p95:.2f}, "
            f"zero={zero} ({zero/total*100:.1f}%), "
            f">={threshold}={high} ({high/total*100:.2f}%), "
            f"min={min(values)}, max={max(values)}"
        )
        lines.append(line)

    return lines, entries


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="统计检测日志中的目标框数量分布")
    parser.add_argument("--log", type=Path, default=Path("va-output.log"), help="待分析的日志路径")
    parser.add_argument(
        "--source-prefix",
        default=None,
        help="仅统计指定前缀的视频源，默认不过滤",
    )
    parser.add_argument(
        "--threshold",
        type=int,
        default=15,
        help="判定为高检测数的阈值，默认 15",
    )
    parser.add_argument(
        "--max-count",
        type=int,
        default=200,
        help="忽略超过该检测数的记录，用于过滤脏数据",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=10,
        help="输出检测数最高的前 N 帧，设置为 0 可跳过",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv or sys.argv[1:])

    try:
        records = list(iter_records(args.log, args.source_prefix, args.max_count))
    except FileNotFoundError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    summary_lines, entries = build_summary(records, args.threshold)
    if not summary_lines:
        print("未解析到任何检测记录，请检查日志或过滤条件。")
        return 0

    print("=== 每路视频源汇总 ===")
    for line in summary_lines:
        print(line)

    if args.top > 0 and entries:
        entries.sort(key=lambda item: item.detections, reverse=True)
        print(f"\n=== 检测数最高的前 {args.top} 帧 ===")
        for record in entries[: args.top]:
            print(
                f"source={record.source}, request={record.request_id}, detections={record.detections}"
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
