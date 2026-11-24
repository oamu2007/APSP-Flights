"""Plot the APSP benchmark CSVs as a standalone SVG."""

import argparse
import csv
import math
import statistics
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Timing:
    graph: str
    nodes: int
    median: float
    minimum: float
    maximum: float
    samples: tuple[float, ...]


def read_timings(directory: Path) -> list[Timing]:
    samples_by_graph = defaultdict(list)
    with (directory / "benchmark_trials.csv").open(encoding="utf-8", newline="") as file:
        for row in csv.DictReader(file):
            key = (row["graph"], int(row["nodes"]))
            samples_by_graph[key].append((int(row["trial"]), float(row["runtime_ms"])))

    timings = []
    with (directory / "benchmark_summary.csv").open(encoding="utf-8", newline="") as file:
        for row in csv.DictReader(file):
            graph, nodes = row["graph"], int(row["nodes"])
            samples = sorted(samples_by_graph.pop((graph, nodes), []))
            trial_ids, runtimes = zip(*samples) if samples else ((), ())
            count = int(row["trials"])
            if not runtimes or trial_ids != tuple(range(1, count + 1)):
                raise ValueError(f"Trial numbers or count do not match for {graph} at {nodes} nodes")
            if any(not math.isfinite(runtime) or runtime <= 0 for runtime in runtimes):
                raise ValueError(f"Expected positive finite timings for {graph} at {nodes} nodes")
            median, minimum, maximum = (float(row[key]) for key in ("median_ms", "min_ms", "max_ms"))
            for recorded, calculated in zip((median, minimum, maximum),
                                            (statistics.median(runtimes), min(runtimes), max(runtimes))):
                if not math.isclose(recorded, calculated, rel_tol=0, abs_tol=1e-9):
                    raise ValueError(f"Summary and trials disagree for {graph} at {nodes} nodes")
            timings.append(Timing(graph, nodes, median, minimum, maximum, runtimes))
    if samples_by_graph:
        raise ValueError("The trial CSV contains graphs missing from the summary CSV")
    return sorted(timings, key=lambda timing: (timing.graph, timing.nodes))


def marker(graph: str, x: float, y: float) -> str:
    if graph == "synthetic":
        return f'<circle class="synthetic-median" cx="{x:.2f}" cy="{y:.2f}" r="4.5"/>'
    points = f"{x:.2f},{y - 8:.2f} {x + 8:.2f},{y:.2f} {x:.2f},{y + 8:.2f} {x - 8:.2f},{y:.2f}"
    return f'<polygon class="historical-median" points="{points}"/>'


def render(timings: list[Timing]) -> str:
    synthetic = [timing for timing in timings if timing.graph == "synthetic"]
    historical = [timing for timing in timings if timing.graph == "historical"]
    reference = next((timing for timing in synthetic if timing.nodes == 150), None)
    if (len(synthetic) < 2 or [timing.nodes for timing in historical] != [150] or reference is None
            or any(timing.nodes <= 0 for timing in timings)
            or len(synthetic) + len(historical) != len(timings)):
        raise ValueError("Expected one historical 150-node graph and multiple synthetic sizes including 150")

    sizes = sorted({timing.nodes for timing in timings})
    cubic = [reference.median * (size / reference.nodes) ** 3 for size in sizes]
    log_x_min, log_x_max = math.log10(sizes[0]), math.log10(sizes[-1])
    y_min = min(*cubic, *(timing.minimum for timing in timings))
    y_max = max(*cubic, *(timing.maximum for timing in timings))
    log_y_min, log_y_max = math.log10(y_min) - 0.12, math.log10(y_max) + 0.12
    left, right, top, bottom = 96, 944, 108, 460

    def x_for(nodes: int) -> float:
        return left + 20 + (right - left - 40) * (math.log10(nodes) - log_x_min) / (log_x_max - log_x_min)

    def y_for(runtime: float) -> float:
        return bottom - (bottom - top) * (math.log10(runtime) - log_y_min) / (log_y_max - log_y_min)

    svg = ['''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 620" width="1000" height="620"
     role="img" aria-labelledby="title description">
<title id="title">Floyd–Warshall runtime by graph size</title>
<desc id="description">Log–log chart of synthetic and historical graph benchmark trials,
with medians, minimum and maximum trial times, and a cubic reference anchored at 150 synthetic nodes.</desc>
<style>
  text { font: 15px 'Segoe UI', Arial, sans-serif; fill: #23364a; }
  .title { font-size: 29px; font-weight: 700; fill: #10243b; }
  .label { font-size: 16px; font-weight: 600; }
  .note { font-size: 13px; fill: #42566a; }
  .grid { stroke: #dce5ec; }
  .axis { stroke: #61778c; }
  .cubic { fill: none; stroke: #6b587d; stroke-width: 2; stroke-dasharray: 7 5; }
  .synthetic-line { fill: none; stroke: #166f8b; stroke-width: 2; }
  .synthetic-range { fill: none; stroke: #166f8b; stroke-width: 1.5; }
  .historical-range { fill: none; stroke: #a3481e; stroke-width: 1.5; }
  .synthetic-trial { fill: #166f8b; opacity: .55; }
  .historical-trial { fill: #a3481e; opacity: .55; }
  .synthetic-median { fill: #166f8b; stroke: white; stroke-width: 1.4; }
  .historical-median { fill: none; stroke: #a3481e; stroke-width: 2.2; }
</style>
<rect width="1000" height="620" fill="white"/>
<text class="title" x="96" y="51">Floyd–Warshall runtime by graph size</text>
<text x="96" y="77">Cubic reference: synthetic 150-node median × (n / 150)³</text>''']

    for decade in range(math.floor(log_y_min), math.ceil(log_y_max) + 1):
        for factor in (1, 2, 5):
            tick = factor * 10.0 ** decade
            y = y_for(tick)
            if top <= y <= bottom:
                svg.append(f'<path class="grid" d="M {left} {y:.2f} H {right}"/>')
                svg.append(f'<text x="{left - 9}" y="{y + 5:.2f}" text-anchor="end">{tick:g}</text>')
    for size in sizes:
        x = x_for(size)
        svg.append(f'<path class="grid" d="M {x:.2f} {top} V {bottom}"/>')
        svg.append(f'<text x="{x:.2f}" y="{bottom + 23}" text-anchor="middle">{size}</text>')
    svg.append(f'<path class="axis" fill="none" d="M {left} {top} V {bottom} H {right}"/>')
    svg.append(f'<text class="label" x="{(left + right) / 2}" y="515" text-anchor="middle">Nodes (log scale)</text>')
    svg.append(f'<text class="label" transform="translate(29 {(top + bottom) / 2}) rotate(-90)" '
               'text-anchor="middle">Runtime (ms, log scale)</text>')

    cubic_points = " ".join(f"{x_for(size):.2f},{y_for(value):.2f}" for size, value in zip(sizes, cubic))
    synthetic_points = " ".join(f"{x_for(p.nodes):.2f},{y_for(p.median):.2f}" for p in synthetic)
    svg.append(f'<polyline class="cubic" points="{cubic_points}"/>')
    svg.append(f'<polyline class="synthetic-line" points="{synthetic_points}"/>')

    for timing in synthetic + historical:
        x = x_for(timing.nodes)
        for index, runtime in enumerate(timing.samples):
            jitter = 2 * (index - (len(timing.samples) - 1) / 2)
            svg.append(f'<circle class="{timing.graph}-trial" cx="{x + jitter:.2f}" cy="{y_for(runtime):.2f}" r="2"/>')
        low, high = y_for(timing.minimum), y_for(timing.maximum)
        cap = 7 if timing.graph == "historical" else 4
        svg.append(f'<path class="{timing.graph}-range" '
                   f'd="M {x:.2f} {high:.2f} V {low:.2f} M {x - cap:.2f} {high:.2f} h {2 * cap} '
                   f'M {x - cap:.2f} {low:.2f} h {2 * cap}"/>')
    for timing in synthetic + historical:
        svg.append(marker(timing.graph, x_for(timing.nodes), y_for(timing.median)))

    svg.extend([
        '<path class="synthetic-line" d="M 96 554 h 26"/>',
        marker("synthetic", 109, 554),
        '<text x="130" y="559">Synthetic median</text>',
        marker("historical", 288, 554),
        '<text x="306" y="559">Historical median</text>',
        '<path class="cubic" d="M 471 554 h 31"/>',
        '<text x="510" y="559">Cubic reference</text>',
        '<path class="synthetic-range" d="M 670 544 v 20 M 665 544 h 10 M 665 564 h 10"/>',
        '<text x="685" y="559">Min–max</text>',
        '<circle class="synthetic-trial" cx="803" cy="552" r="2"/>',
        '<circle class="historical-trial" cx="811" cy="556" r="2"/>',
        '<text x="822" y="559">Individual trials</text>',
        '<text class="note" x="96" y="592">Individual trials are offset horizontally for visibility.</text>',
        '</svg>',
    ])
    return "\n".join(svg) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_directory", type=Path, help="directory containing the benchmark CSVs")
    args = parser.parse_args()
    try:
        timings = read_timings(args.output_directory)
        (args.output_directory / "runtime.svg").write_text(render(timings), encoding="utf-8")
    except (KeyError, OSError, ValueError) as error:
        parser.exit(1, f"Plot failed: {error}\n")


if __name__ == "__main__":
    main()
