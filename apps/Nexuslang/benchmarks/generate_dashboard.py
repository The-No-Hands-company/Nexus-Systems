"""
NLPL Performance Dashboard Generator
======================================

Reads perf-baseline.json and generates a self-contained HTML performance
dashboard with bar charts comparing NexusLang against C, Python, and Rust.

Usage:
    python benchmarks/generate_dashboard.py
    python benchmarks/generate_dashboard.py --input perf-baseline.json --output perf-dashboard.html
"""

import sys
import json
import argparse
from pathlib import Path
from datetime import datetime

PROJ_ROOT = Path(__file__).parent.parent
BENCH_DIR = PROJ_ROOT / "benchmarks"


# ---------------------------------------------------------------------------
# HTML template with embedded Chart.js (CDN)
# ---------------------------------------------------------------------------

HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>NLPL Performance Dashboard</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
  :root {{
    --bg: #0d1117;
    --surface: #161b22;
    --border: #30363d;
    --text: #c9d1d9;
    --text-muted: #8b949e;
    --accent: #58a6ff;
    --green: #3fb950;
    --yellow: #d29922;
    --red: #f85149;
    --orange: #d18616;
    --purple: #bc8cff;
    --cyan: #39d0d8;
  }}
  * {{ box-sizing: border-box; margin: 0; padding: 0; }}
  body {{
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    font-size: 14px;
    line-height: 1.5;
    padding: 24px;
    max-width: 1200px;
    margin: 0 auto;
  }}

  h1 {{ font-size: 28px; font-weight: 700; color: var(--accent); margin-bottom: 6px; }}
  h2 {{ font-size: 18px; font-weight: 600; color: var(--text); margin: 24px 0 12px; }}
  h3 {{ font-size: 15px; font-weight: 600; color: var(--text-muted); margin-bottom: 10px; }}

  .meta {{ color: var(--text-muted); font-size: 12px; margin-bottom: 24px; }}
  .meta span {{ margin-right: 16px; }}
  .meta .badge {{
    display: inline-block;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 4px;
    padding: 1px 6px;
    font-family: monospace;
    font-size: 11px;
  }}

  .grid {{
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(340px, 1fr));
    gap: 16px;
    margin-bottom: 24px;
  }}

  .card {{
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 16px;
  }}

  .stat-grid {{
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 10px;
    margin-bottom: 24px;
  }}
  .stat {{
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 14px 16px;
    text-align: center;
  }}
  .stat .value {{ font-size: 26px; font-weight: 700; color: var(--accent); }}
  .stat .label {{ font-size: 11px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; margin-top: 4px; }}

  .ratio-table {{
    width: 100%;
    border-collapse: collapse;
    margin-top: 8px;
    font-size: 13px;
  }}
  .ratio-table th {{
    text-align: left;
    color: var(--text-muted);
    font-weight: 500;
    padding: 6px 8px;
    border-bottom: 1px solid var(--border);
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }}
  .ratio-table td {{
    padding: 6px 8px;
    border-bottom: 1px solid var(--border);
    font-family: monospace;
  }}
  .ratio-table tr:last-child td {{ border-bottom: none; }}
  .tag {{
    display: inline-block;
    padding: 1px 6px;
    border-radius: 4px;
    font-size: 11px;
    font-weight: 600;
    font-family: monospace;
  }}
  .tag-nlpl {{ background: #1f3261; color: var(--accent); }}
  .tag-c    {{ background: #1f3322; color: var(--green); }}
  .tag-py   {{ background: #2a2010; color: var(--yellow); }}
  .tag-rs   {{ background: #2a1015; color: var(--orange); }}

  .chart-container {{ position: relative; height: 260px; }}

  .note {{
    background: #1c1f26;
    border: 1px solid #2d3748;
    border-left: 3px solid var(--accent);
    border-radius: 4px;
    padding: 12px 16px;
    font-size: 12px;
    color: var(--text-muted);
    line-height: 1.6;
    margin-top: 24px;
  }}
  .note strong {{ color: var(--text); }}
  footer {{
    margin-top: 32px;
    padding-top: 16px;
    border-top: 1px solid var(--border);
    color: var(--text-muted);
    font-size: 11px;
    text-align: center;
  }}
</style>
</head>
<body>

<h1>NLPL Performance Dashboard</h1>
<div class="meta">
  <span>Generated: <span class="badge">{date}</span></span>
  <span>Commit: <span class="badge">{git_commit}</span></span>
  <span>Python: <span class="badge">{python_version}</span></span>
  {gcc_badge}
  {rustc_badge}
  <span>Runs: <span class="badge">{runs}</span></span>
</div>

<!-- Top stats -->
<div class="stat-grid">
  <div class="stat">
    <div class="value">{dispatch_speedup}x</div>
    <div class="label">Dispatch Speedup</div>
  </div>
  {extra_stats}
</div>

<!-- Per-benchmark detail charts -->
<h2>Benchmark Timings (ms) — Log Scale</h2>
<div class="grid">
  {benchmark_cards}
</div>

<!-- Cross-language ratio table -->
<h2>NLPL vs Reference Languages (ratio — lower is better for NLPL)</h2>
<div class="card">
  <table class="ratio-table">
    <thead>
      <tr>
        <th>Benchmark</th>
        <th>NLPL O3 (ms)</th>
        <th>vs C -O3</th>
        <th>vs Rust</th>
        <th>vs Python</th>
        <th>O3 vs O0 speedup</th>
      </tr>
    </thead>
    <tbody>
      {ratio_rows}
    </tbody>
  </table>
</div>

<!-- NexusLang optimization levels chart -->
<h2>NLPL Optimization Levels (O0 -> O3)</h2>
<div class="card">
  <div class="chart-container">
    <canvas id="optLevelChart"></canvas>
  </div>
</div>

<div class="note">
  <strong>Interpreter vs Compiled Languages</strong><br>
  NexusLang currently runs as an interpreted language on top of CPython. The ratios above reflect
  interpreter overhead versus ahead-of-time compiled code (C, Rust) and Python's highly
  optimized CPython interpreter.<br><br>
  <strong>Dispatch table speedup:</strong> A static class-level dispatch table + per-instance
  bound method cache replaced per-node regex processing, delivering {dispatch_speedup}x faster
  AST node dispatch. This is an interpreter-level optimization, not an algorithmic one.<br><br>
  <strong>Next steps to reduce NexusLang/C ratio:</strong> LLVM native code generation backend
  (planned — Phase 3 compiler work) will eliminate interpreter overhead entirely, targeting
  &lt;5x C performance for compute-intensive workloads.
</div>

<footer>
  NexusLang Performance Dashboard &mdash; {date_short} &mdash; commit {git_commit}
</footer>

<script>
const COLORS = {{
  nlplO0:  'rgba(88,  166, 255, 0.6)',
  nlplO1:  'rgba(88,  166, 255, 0.75)',
  nlplO2:  'rgba(88,  166, 255, 0.88)',
  nlplO3:  'rgba(88,  166, 255, 1.0)',
  c:       'rgba(63,  185, 80,  1.0)',
  python:  'rgba(210, 153, 34,  1.0)',
  rust:    'rgba(209, 134, 22,  1.0)',
}};

Chart.defaults.color = '#8b949e';
Chart.defaults.borderColor = '#30363d';

function makeLogBarChart(canvasId, labels, datasets) {{
  const ctx = document.getElementById(canvasId);
  if (!ctx) return;
  new Chart(ctx, {{
    type: 'bar',
    data: {{ labels, datasets }},
    options: {{
      responsive: true,
      maintainAspectRatio: false,
      plugins: {{
        legend: {{ position: 'top', labels: {{ boxWidth: 12, font: {{ size: 11 }} }} }},
        tooltip: {{
          callbacks: {{
            label: (ctx) => ` ${{ctx.dataset.label}}: ${{ctx.raw != null ? ctx.raw.toFixed(6) : 'N/A'}} ms`
          }}
        }}
      }},
      scales: {{
        y: {{
          type: 'logarithmic',
          title: {{ display: true, text: 'Time (ms) — log scale' }},
          ticks: {{
            callback: (v) => v >= 1 ? v.toFixed(0) + 'ms' : v.toFixed(4) + 'ms'
          }}
        }},
        x: {{ ticks: {{ font: {{ size: 11 }} }} }}
      }}
    }}
  }});
}}

function makeLinearBarChart(canvasId, labels, datasets) {{
  const ctx = document.getElementById(canvasId);
  if (!ctx) return;
  new Chart(ctx, {{
    type: 'bar',
    data: {{ labels, datasets }},
    options: {{
      responsive: true,
      maintainAspectRatio: false,
      plugins: {{
        legend: {{ position: 'top', labels: {{ boxWidth: 12, font: {{ size: 11 }} }} }},
        tooltip: {{
          callbacks: {{
            label: (ctx) => ` ${{ctx.dataset.label}}: ${{ctx.raw != null ? ctx.raw.toFixed(3) : 'N/A'}} ms`
          }}
        }}
      }},
      scales: {{
        y: {{ title: {{ display: true, text: 'Time (ms)' }} }},
        x: {{ ticks: {{ font: {{ size: 11 }} }} }}
      }}
    }}
  }});
}}

{chart_js_calls}
</script>

</body>
</html>
"""


# ---------------------------------------------------------------------------
# Dashboard generation
# ---------------------------------------------------------------------------

def _ms_or_null(v):
    """Return float or None."""
    return v if (v is not None and isinstance(v, (int, float))) else None


def _fmt_ratio(v):
    if v is None:
        return "<td>N/A</td>"
    if v >= 10000:
        return f"<td style='color:#f85149'>{v:,.0f}x</td>"
    if v >= 1000:
        return f"<td style='color:#d29922'>{v:,.0f}x</td>"
    if v >= 100:
        return f"<td style='color:#d29922'>{v:.1f}x</td>"
    if v < 1:
        return f"<td style='color:#3fb950'>{v:.3f}x (faster)</td>"
    return f"<td>{v:.1f}x</td>"


def generate_dashboard(data: dict) -> str:
    meta = data.get("meta", {})
    baseline = data.get("baseline", {})
    dispatch_speedup = data.get("dispatch_speedup_x", "?")

    date_raw = meta.get("date", datetime.now().isoformat())
    try:
        date_fmt = datetime.fromisoformat(date_raw).strftime("%Y-%m-%d %H:%M")
        date_short = datetime.fromisoformat(date_raw).strftime("%Y-%m-%d")
    except Exception:
        date_fmt = date_raw
        date_short = date_raw[:10]

    gcc_ver = meta.get("gcc_version", "")
    rustc_ver = meta.get("rustc_version", "")
    gcc_badge = f'<span>GCC: <span class="badge">{gcc_ver.split()[2] if gcc_ver and "GCC" in gcc_ver else gcc_ver[:30]}</span></span>' if gcc_ver and gcc_ver != "skipped" else ""
    rustc_badge = f'<span>Rustc: <span class="badge">{rustc_ver.split()[1] if rustc_ver and len(rustc_ver.split()) > 1 else rustc_ver[:20]}</span></span>' if rustc_ver and rustc_ver != "skipped" else ""

    # Extra stats from benchmark data
    ratios_c = [r["nxl_vs_c_ratio"] for r in baseline.values() if r.get("nxl_vs_c_ratio")]
    ratios_py = [r["nxl_vs_python_ratio"] for r in baseline.values() if r.get("nxl_vs_python_ratio")]

    extra_stats_html = ""
    if ratios_c:
        avg_c = sum(ratios_c) / len(ratios_c)
        # Use geometric mean for orders-of-magnitude ratios
        import math
        gm_c = math.exp(sum(math.log(r) for r in ratios_c) / len(ratios_c))
        extra_stats_html += f'<div class="stat"><div class="value">{gm_c:,.0f}x</div><div class="label">vs C -O3 (geomean)</div></div>'
    if ratios_py:
        import math
        gm_py = math.exp(sum(math.log(r) for r in ratios_py) / len(ratios_py))
        extra_stats_html += f'<div class="stat"><div class="value">{gm_py:,.0f}x</div><div class="label">vs Python (geomean)</div></div>'

    # Per-benchmark chart cards
    bench_cards_html = ""
    chart_js_calls = ""
    bench_names_display = []
    nxl_o0_vals = []
    nxl_o1_vals = []
    nxl_o2_vals = []
    nxl_o3_vals = []

    for i, (bench_name, r) in enumerate(baseline.items()):
        display_name = bench_name.replace("_", " ").title()
        bench_names_display.append(display_name)
        canvas_id = f"chart_{i}"

        o0 = _ms_or_null(r.get("nxl_O0_ms"))
        o1 = _ms_or_null(r.get("nxl_O1_ms"))
        o2 = _ms_or_null(r.get("nxl_O2_ms"))
        o3 = _ms_or_null(r.get("nxl_O3_ms"))
        c  = _ms_or_null(r.get("c_o3_ms"))
        py = _ms_or_null(r.get("python_ms"))
        rs = _ms_or_null(r.get("rust_release_ms"))

        nxl_o0_vals.append(o0)
        nxl_o1_vals.append(o1)
        nxl_o2_vals.append(o2)
        nxl_o3_vals.append(o3)

        langs_labels = []
        langs_data = []
        langs_colors = []
        lang_datasets = []

        for label, val, color in [
            ("NLPL O0", o0, "COLORS.nlplO0"),
            ("NLPL O1", o1, "COLORS.nlplO1"),
            ("NLPL O2", o2, "COLORS.nlplO2"),
            ("NLPL O3", o3, "COLORS.nlplO3"),
            ("C -O3",   c,  "COLORS.c"),
            ("Python",  py, "COLORS.python"),
            ("Rust",    rs, "COLORS.rust"),
        ]:
            if val is not None:
                langs_labels.append(label)
                langs_data.append(val)
                langs_colors.append(color)

        labels_js = json.dumps(langs_labels)
        data_js = json.dumps(langs_data)
        colors_js = "[" + ", ".join(langs_colors) + "]"

        chart_js_calls += f"""
makeLogBarChart('{canvas_id}',
  {labels_js},
  [{{
    label: 'Time (ms)',
    data: {data_js},
    backgroundColor: {colors_js},
    borderWidth: 0,
    borderRadius: 3,
  }}]
);
"""

        desc = r.get("description", "")
        bench_cards_html += f"""
<div class="card">
  <h3>{display_name}</h3>
  <div style="font-size:11px; color:var(--text-muted); margin-bottom:8px;">{desc}</div>
  <div class="chart-container">
    <canvas id="{canvas_id}"></canvas>
  </div>
</div>
"""

    # Optimization level comparison chart (NLPL only, grouped by benchmark)
    opt_datasets_js = ""
    for label, vals, color_key in [
        ("O0 (no opts)", nxl_o0_vals, "nlplO0"),
        ("O1", nxl_o1_vals, "nlplO1"),
        ("O2", nxl_o2_vals, "nlplO2"),
        ("O3 (aggressive)", nxl_o3_vals, "nlplO3"),
    ]:
        opt_datasets_js += f"""
  {{
    label: {json.dumps(label)},
    data: {json.dumps(vals)},
    backgroundColor: COLORS.{color_key},
    borderWidth: 0,
    borderRadius: 3,
  }},"""

    chart_js_calls += f"""
makeLinearBarChart('optLevelChart',
  {json.dumps(bench_names_display)},
  [{opt_datasets_js}]
);
"""

    # Ratio table rows
    ratio_rows_html = ""
    for bench_name, r in baseline.items():
        display = bench_name.replace("_", " ").title()
        o3 = _ms_or_null(r.get("nxl_O3_ms"))
        o3_fmt = f"{o3:.3f}" if o3 else "N/A"
        speedup = r.get("nxl_O3_vs_O0_speedup")
        speedup_fmt = f"{speedup:.2f}x" if speedup else "N/A"

        ratio_rows_html += f"""
<tr>
  <td><strong>{display}</strong></td>
  <td style="font-family:monospace">{o3_fmt}</td>
  {_fmt_ratio(r.get("nxl_vs_c_ratio"))}
  {_fmt_ratio(r.get("nxl_vs_rust_ratio"))}
  {_fmt_ratio(r.get("nxl_vs_python_ratio"))}
  <td style="color:var(--green)">{speedup_fmt}</td>
</tr>"""

    html = HTML_TEMPLATE.format(
        date=date_fmt,
        date_short=date_short,
        git_commit=meta.get("git_commit", "unknown"),
        python_version=meta.get("python_version", "?"),
        gcc_badge=gcc_badge,
        rustc_badge=rustc_badge,
        runs=meta.get("runs_per_measurement", "?"),
        dispatch_speedup=dispatch_speedup,
        extra_stats=extra_stats_html,
        benchmark_cards=bench_cards_html,
        chart_js_calls=chart_js_calls,
        ratio_rows=ratio_rows_html,
    )
    return html


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Generate NexusLang performance dashboard HTML")
    parser.add_argument("--input", default=str(BENCH_DIR / "perf-baseline.json"),
                        help="Input perf-baseline.json path")
    parser.add_argument("--output", default=str(BENCH_DIR / "perf-dashboard.html"),
                        help="Output HTML file path")
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: Input file not found: {input_path}", file=sys.stderr)
        print("Run: PYTHONPATH=src python benchmarks/run_perf_baseline.py --verbose", file=sys.stderr)
        return 1

    with open(input_path) as f:
        data = json.load(f)

    html = generate_dashboard(data)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w") as f:
        f.write(html)

    print(f"Dashboard generated: {output_path}")

    # Print quick summary
    baseline = data.get("baseline", {})
    print()
    print("Quick summary:")
    for bench_name, r in baseline.items():
        ratio_c = r.get("nxl_vs_c_ratio")
        ratio_py = r.get("nxl_vs_python_ratio")
        o3 = r.get("nxl_O3_ms")
        parts = [f"  {bench_name}: NexusLang O3={o3:.1f}ms" if o3 else f"  {bench_name}:"]
        if ratio_c:
            parts.append(f"  C-ratio={ratio_c:,.0f}x")
        if ratio_py:
            parts.append(f"  Python-ratio={ratio_py:.0f}x")
        print("".join(parts))

    return 0


if __name__ == "__main__":
    sys.exit(main())
