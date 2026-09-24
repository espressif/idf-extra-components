#!/usr/bin/env python3
"""Compare machine-readable SPI NAND FatFs throughput logs."""

import argparse
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import TypedDict


JsonScalar = str | int | float | bool | None
ResultKey = tuple[str, int]
TAG_PATTERN = re.compile(r"\b(RUN|RESULT)\s+(\{.*\})\s*$")
# ESP-IDF log colour codes (CONFIG_LOG_COLORS), e.g. "\x1b[0;32m" ... "\x1b[0m"
ANSI_ESCAPE_PATTERN = re.compile(r"\x1b\[[0-9;]*m")


class RunRecord(TypedDict, total=False):
    path_cache: bool
    page_reg_cache: bool
    meta_cache: bool
    meta_slots: int
    precondition: bool
    precond_percent: int
    tries: int
    chip_kb: int | float
    file_kb: int | float
    cluster_kb: int | float


class ResultRecord(TypedDict, total=False):
    phase: str
    chunk: int
    tries: int
    write_avg_kbps: int | float
    write_min_kbps: int | float
    write_max_kbps: int | float
    read_avg_kbps: int | float
    read_min_kbps: int | float
    read_max_kbps: int | float
    write_wa: int | float
    read_wa: int | float
    w_reads: int
    w_programs: int
    w_copies: int
    w_erases: int
    w_meta_hits: int
    w_meta_misses: int
    r_reads: int
    r_meta_hits: int
    r_meta_misses: int
    skipped: bool


@dataclass(frozen=True, slots=True)
class Capture:
    path: Path
    run: RunRecord
    results: dict[ResultKey, ResultRecord]
    result_order: tuple[ResultKey, ...]


class LogParseError(Exception):
    """Report an invalid or incomplete throughput capture."""


def parse_json_record(path: Path, line_number: int, body: str) -> dict[str, JsonScalar]:
    """Parse one JSON object from a tagged log line."""
    try:
        value = json.loads(body)
    except json.JSONDecodeError as error:
        raise LogParseError(
            f"{path}:{line_number}: invalid JSON after log tag: {error.msg}"
        ) from error
    if not isinstance(value, dict):
        raise LogParseError(f"{path}:{line_number}: tagged record must be a JSON object")
    return value


def parse_capture(path: Path) -> Capture:
    """Extract the RUN record and keyed RESULT records from one capture."""
    run: RunRecord | None = None
    results: dict[ResultKey, ResultRecord] = {}
    result_order: list[ResultKey] = []

    try:
        with path.open(encoding="utf-8", errors="replace") as log_file:
            for line_number, line in enumerate(log_file, start=1):
                match = TAG_PATTERN.search(ANSI_ESCAPE_PATTERN.sub("", line))
                if match is None:
                    continue
                tag, body = match.groups()
                record = parse_json_record(path, line_number, body)
                if tag == "RUN":
                    if run is not None:
                        raise LogParseError(f"{path}:{line_number}: multiple RUN lines found")
                    run = record
                    continue

                phase = record.get("phase")
                chunk = record.get("chunk")
                if not isinstance(phase, str) or not isinstance(chunk, int):
                    raise LogParseError(
                        f"{path}:{line_number}: RESULT requires string phase and integer chunk"
                    )
                key = (phase, chunk)
                if key in results:
                    raise LogParseError(
                        f"{path}:{line_number}: duplicate RESULT for phase={phase!r}, chunk={chunk}"
                    )
                results[key] = record
                result_order.append(key)
    except OSError as error:
        raise LogParseError(f"{path}: cannot read log: {error.strerror}") from error

    if run is None:
        raise LogParseError(f"{path}: no RUN line found")
    if not results:
        raise LogParseError(f"{path}: no RESULT lines found")
    return Capture(path=path, run=run, results=results, result_order=tuple(result_order))


THROUGHPUT_FIELDS = (("write_avg_kbps", "Write"), ("read_avg_kbps", "Read"))
AMPLIFICATION_FIELDS = (("write_wa", "Write amp"), ("read_wa", "Read amp"))
# (column heading, count field) for the physical operations table.
PHYSICAL_COUNT_FIELDS = (
    ("Write reads", "w_reads"),
    ("Programs", "w_programs"),
    ("Copies", "w_copies"),
    ("Erases", "w_erases"),
)
# (column heading, hits field, misses field) for metadata cache hit rates.
HIT_RATE_FIELDS = (
    ("Write meta hit %", "w_meta_hits", "w_meta_misses"),
    ("Read meta hit %", "r_meta_hits", "r_meta_misses"),
)


def escape_cell(text: str) -> str:
    """Escape Markdown table separators."""
    return text.replace("|", "\\|")


def capture_label(index: int) -> str:
    """Return the short column label for a capture: A, B, ..., Z, AA, ..."""
    label = ""
    index += 1
    while index > 0:
        index, remainder = divmod(index - 1, 26)
        label = chr(ord("A") + remainder) + label
    return label


def format_config_value(value: JsonScalar) -> str:
    """Format a RUN value for Markdown."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "—"
    return escape_cell(str(value))


def metric_value(record: ResultRecord | None, field: str) -> float | None:
    """Return a usable numeric metric, or None for missing/skipped/invalid cells."""
    if record is None or record.get("skipped") is True:
        return None
    value = record.get(field)
    if not isinstance(value, int | float) or isinstance(value, bool):
        return None
    return float(value)


def format_metric(record: ResultRecord | None, field: str, precision: int) -> str:
    """Render a result metric, skip, or missing capture cell."""
    if record is None:
        return "no data"
    if record.get("skipped") is True:
        return "skipped"
    value = metric_value(record, field)
    return "no data" if value is None else f"{value:.{precision}f}"


def ratio(baseline: ResultRecord | None, candidate: ResultRecord | None, field: str) -> float | None:
    """Return candidate/baseline for one metric, or None if not comparable."""
    baseline_value = metric_value(baseline, field)
    candidate_value = metric_value(candidate, field)
    if baseline_value is None or candidate_value is None or baseline_value <= 0 or candidate_value <= 0:
        return None
    return candidate_value / baseline_value


def format_percent(value: float | None) -> str:
    """Format a candidate/baseline ratio as a signed percentage."""
    return "" if value is None else f"{(value - 1.0) * 100.0:+.1f}%"


def cell_name(key: ResultKey) -> str:
    """Return a compact 'phase/chunk' label."""
    return escape_cell(f"{key[0]}/{key[1]}")


def split_run_keys(captures: list[Capture]) -> tuple[list[str], list[str]]:
    """Split RUN keys into (differing, shared), keeping first-seen order."""
    ordered_keys: list[str] = []
    for capture in captures:
        for key in capture.run:
            if key not in ordered_keys:
                ordered_keys.append(key)
    differing: list[str] = []
    shared: list[str] = []
    for key in ordered_keys:
        values = {json.dumps(capture.run.get(key), sort_keys=True) for capture in captures}
        (differing if len(values) > 1 else shared).append(key)
    return differing, shared


def merged_result_order(captures: list[Capture]) -> list[ResultKey]:
    """Keep baseline order, then append cells seen only in later captures."""
    ordered: list[ResultKey] = []
    for capture in captures:
        for key in capture.result_order:
            if key not in ordered:
                ordered.append(key)
    return ordered


def table_row(cells: list[str]) -> str:
    """Render one Markdown table row."""
    return "| " + " | ".join(cells) + " |"


def render_header(captures: list[Capture], labels: list[str]) -> list[str]:
    """Render the capture legend, differing RUN settings and shared settings."""
    lines = ["# Throughput comparison", ""]
    lines.append(
        " · ".join(
            f"**{label}** = {escape_cell(capture.path.name)}" + (" (baseline)" if index == 0 else "")
            for index, (label, capture) in enumerate(zip(labels, captures))
        )
    )
    lines.append("")
    differing_keys, shared_keys = split_run_keys(captures)
    if differing_keys:
        lines.append(table_row(["Config", *labels]))
        lines.append("|---|" + "---|" * len(captures))
        for key in differing_keys:
            lines.append(table_row([escape_cell(key), *(format_config_value(c.run.get(key)) for c in captures)]))
    else:
        lines.append("All captures have identical RUN configuration.")
    lines.append("")
    if shared_keys:
        shared = ", ".join(f"{key}={format_config_value(captures[0].run.get(key))}" for key in shared_keys)
        lines.extend([f"Same in all: {shared}", ""])
    return lines


def geometric_mean(values: list[float]) -> float:
    """Return the geometric mean of positive values."""
    return math.exp(sum(math.log(value) for value in values) / len(values))


def median(values: list[float]) -> float:
    """Return the median of a non-empty list."""
    ordered = sorted(values)
    middle = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[middle]
    return (ordered[middle - 1] + ordered[middle]) / 2.0


def render_summary(captures: list[Capture], labels: list[str], keys: list[ResultKey]) -> list[str]:
    """Render per-candidate geomean deltas plus worst/best throughput cells."""
    baseline = captures[0]
    candidates = list(zip(labels[1:], captures[1:]))
    lines = ["## Summary", ""]
    lines.append(table_row(["Metric", *(f"{label} vs {labels[0]}" for label, _ in candidates)]))
    lines.append("|---|" + "---|" * len(candidates))
    extremes: list[str] = []

    for field, name in THROUGHPUT_FIELDS:
        cells = [f"{name} kB/s (geomean)"]
        for label, capture in candidates:
            ratios = {
                key: value
                for key in keys
                if (value := ratio(baseline.results.get(key), capture.results.get(key), field)) is not None
            }
            if not ratios:
                cells.append("no data")
                continue
            summary = f"**{format_percent(geometric_mean(list(ratios.values())))}**"
            if len(ratios) < len(keys):
                summary += f" ({len(ratios)}/{len(keys)} cells)"
            cells.append(summary)
            worst = min(ratios, key=ratios.__getitem__)
            best = max(ratios, key=ratios.__getitem__)
            extremes.append(
                f"- {name}, {label} vs {labels[0]}: worst {format_percent(ratios[worst])} "
                f"{cell_name(worst)}, best {format_percent(ratios[best])} {cell_name(best)}"
            )
        lines.append(table_row(cells))

    if has_physical_ops(captures):
        cells = ["Write reads (geomean)"]
        for _, capture in candidates:
            ratios = [
                value
                for key in keys
                if (value := ratio(baseline.results.get(key), capture.results.get(key), "w_reads")) is not None
            ]
            cells.append(format_percent(geometric_mean(ratios)) if ratios else "no data")
        lines.append(table_row(cells))

    for field, name in AMPLIFICATION_FIELDS:
        cells = [f"{name} (median)"]
        baseline_values = [v for key in keys if (v := metric_value(baseline.results.get(key), field)) is not None]
        for _, capture in candidates:
            values = [v for key in keys if (v := metric_value(capture.results.get(key), field)) is not None]
            if not baseline_values or not values:
                cells.append("no data")
            else:
                cells.append(f"{median(baseline_values):.2f} → {median(values):.2f}")
        lines.append(table_row(cells))

    lines.append("")
    lines.extend(extremes)
    lines.append("")
    return lines


def render_throughput(captures: list[Capture], labels: list[str], keys: list[ResultKey]) -> list[str]:
    """Render mean write/read kB/s per cell with deltas against the baseline."""
    headings = ["Phase", "Chunk"]
    for _, name in THROUGHPUT_FIELDS:
        for index, label in enumerate(labels):
            headings.append(f"{name} {label}")
            if index > 0:
                headings.append(f"Δ {label}")
    lines = ["## Throughput (kB/s)", "", table_row(headings)]
    lines.append("|---|---:|" + "---:|" * (len(headings) - 2))

    previous_phase = None
    for key in keys:
        phase, chunk = key
        cells = [escape_cell(phase) if phase != previous_phase else "", str(chunk)]
        previous_phase = phase
        baseline_record = captures[0].results.get(key)
        for field, _ in THROUGHPUT_FIELDS:
            for index, capture in enumerate(captures):
                record = capture.results.get(key)
                cells.append(format_metric(record, field, 0))
                if index > 0:
                    cells.append(format_percent(ratio(baseline_record, record, field)))
        lines.append(table_row(cells))
    lines.append("")
    return lines


def render_amplification(captures: list[Capture], labels: list[str], keys: list[ResultKey]) -> list[str]:
    """Render write/read amplification per cell as 'A → B → ...' chains."""
    arrow_labels = " → ".join(labels)
    headings = ["Phase", "Chunk", *(f"{name} {arrow_labels}" for _, name in AMPLIFICATION_FIELDS)]
    lines = ["## Amplification", "", table_row(headings), "|---|---:|---|---|"]
    previous_phase = None
    for key in keys:
        phase, chunk = key
        cells = [escape_cell(phase) if phase != previous_phase else "", str(chunk)]
        previous_phase = phase
        for field, _ in AMPLIFICATION_FIELDS:
            cells.append(" → ".join(format_metric(c.results.get(key), field, 2) for c in captures))
        lines.append(table_row(cells))
    lines.append("")
    return lines


def format_hit_rate(record: ResultRecord | None, hits_field: str, misses_field: str) -> str:
    """Render a metadata cache hit percentage from hit and miss counts."""
    if record is None:
        return "no data"
    if record.get("skipped") is True:
        return "skipped"
    hits = metric_value(record, hits_field)
    misses = metric_value(record, misses_field)
    if hits is None or misses is None:
        return "no data"
    total = hits + misses
    return "—" if total == 0 else f"{hits * 100.0 / total:.1f}"


def has_physical_ops(captures: list[Capture]) -> bool:
    """Return True if any capture has raw physical operation counters."""
    return any("w_reads" in record for capture in captures for record in capture.results.values())


def render_physical_ops(captures: list[Capture], labels: list[str], keys: list[ResultKey]) -> list[str]:
    """Render per-cell physical operation counts and cache hit rates."""
    if not has_physical_ops(captures):
        return []
    arrow_labels = " → ".join(labels)
    headings = [
        "Phase",
        "Chunk",
        *(heading for heading, _ in PHYSICAL_COUNT_FIELDS[:1]),
        *(heading for heading, _, _ in HIT_RATE_FIELDS[:1]),
        *(heading for heading, _ in PHYSICAL_COUNT_FIELDS[1:]),
        "Read reads",
        *(heading for heading, _, _ in HIT_RATE_FIELDS[1:]),
    ]
    lines = [
        "## Physical operations",
        "",
        f"Counts per cell, summed over all tries ({arrow_labels}).",
        "",
        table_row(headings),
        "|---|---:|" + "---|" * (len(headings) - 2),
    ]
    write_hits, read_hits = HIT_RATE_FIELDS
    previous_phase = None
    for key in keys:
        phase, chunk = key
        cells = [escape_cell(phase) if phase != previous_phase else "", str(chunk)]
        previous_phase = phase
        records = [capture.results.get(key) for capture in captures]
        cells.append(" → ".join(format_metric(r, "w_reads", 0) for r in records))
        cells.append(" → ".join(format_hit_rate(r, write_hits[1], write_hits[2]) for r in records))
        for _, field in PHYSICAL_COUNT_FIELDS[1:]:
            cells.append(" → ".join(format_metric(r, field, 0) for r in records))
        cells.append(" → ".join(format_metric(r, "r_reads", 0) for r in records))
        cells.append(" → ".join(format_hit_rate(r, read_hits[1], read_hits[2]) for r in records))
        lines.append(table_row(cells))
    lines.append("")
    return lines


def render_markdown(captures: list[Capture]) -> str:
    """Render the legend, summary, throughput and amplification sections."""
    labels = [capture_label(index) for index in range(len(captures))]
    keys = merged_result_order(captures)
    lines = [
        *render_header(captures, labels),
        *render_summary(captures, labels, keys),
        *render_throughput(captures, labels, keys),
        *render_amplification(captures, labels, keys),
        *render_physical_ops(captures, labels, keys),
    ]
    return "\n".join(lines).rstrip("\n") + "\n"


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path, help="serial log files; first file is the baseline")
    parser.add_argument("--output", type=Path, help="write Markdown to this file instead of stdout")
    args = parser.parse_args()
    if len(args.logs) < 2:
        parser.error("at least two log files are required")
    return args


def main() -> int:
    """Run the comparison command."""
    args = parse_args()
    try:
        captures = [parse_capture(path) for path in args.logs]
        markdown = render_markdown(captures)
        if args.output is None:
            sys.stdout.write(markdown)
        else:
            args.output.write_text(markdown, encoding="utf-8")
    except (LogParseError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
