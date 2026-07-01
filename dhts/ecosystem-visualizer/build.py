#!/usr/bin/env python3
"""
Build the Ecosystem Visualizer HTML from ecosystem-data.json + app.js.
Bakes data + JS + CSS into a single self-contained file.

Usage:
  python3 build.py          # rebuild index.html
  python3 build.py --watch  # rebuild on file changes (requires watchdog)
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parent
DATA_PATH = ROOT / "ecosystem-data.json"
JS_PATH = ROOT / "app.js"
OUT_PATH = ROOT / "index.html"

# ═══════════════════════════════════════════════════════
#  HTML Template
# ═══════════════════════════════════════════════════════

HTML_TEMPLATE = r"""<!doctype html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover" />
  <title>Nexus Systems — Ecosystem Visualizer v3</title>
  <style>
    *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
    html,body{height:100%}
    html{-webkit-text-size-adjust:100%}
    body{
      -webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale;
      font-family:"IBM Plex Sans","Segoe UI",system-ui,sans-serif;
      background:#060a0e;color:#dce8e4;
      display:grid;grid-template-rows:auto auto auto auto 1fr;overflow:hidden;
    }
    /* Scrollbar — dark, visible */
    ::-webkit-scrollbar{width:6px;height:6px}
    ::-webkit-scrollbar-track{background:rgba(255,255,255,0.02)}
    ::-webkit-scrollbar-thumb{background:rgba(255,255,255,0.08);border-radius:3px}
    ::-webkit-scrollbar-thumb:hover{background:rgba(255,255,255,0.14)}
    ::-webkit-scrollbar-corner{background:transparent}
    :root{color-scheme:dark}
    button,a,[role="button"],[tabindex],.chip,input,select,textarea{
      touch-action:manipulation;-webkit-tap-highlight-color:transparent;
    }

    /* ── Header ── */
    header{
      padding:10px 18px;border-bottom:1px solid rgba(255,255,255,0.05);
      background:rgba(6,10,14,0.94);backdrop-filter:blur(16px);
      display:flex;gap:14px;align-items:center;justify-content:space-between;
      flex-wrap:wrap;z-index:20;
    }
    .header-left{display:flex;align-items:center;gap:10px}
    .logo{
      width:32px;height:32px;border-radius:8px;
      background:linear-gradient(135deg,#6366f1 0%,#14b8a6 100%);
      display:flex;align-items:center;justify-content:center;
      font-weight:900;font-size:15px;color:#fff;flex-shrink:0;
    }
    .title-group{display:flex;flex-direction:column;min-width:0}
    .title{
      font-size:15px;font-weight:700;
      background:linear-gradient(135deg,#a5b4fc 0%,#5eead4 100%);
      -webkit-background-clip:text;-webkit-text-fill-color:transparent;
      background-clip:text;
    }
    .subtitle{font-size:9px;color:#475569;text-transform:uppercase;letter-spacing:1.2px}
    .header-right{display:flex;gap:16px;align-items:center;flex-wrap:wrap}
    .header-stats{display:flex;gap:14px;font-size:11px;color:#64748b;flex-wrap:wrap}
    .stat{display:flex;align-items:center;gap:5px;cursor:pointer;transition:opacity 150ms}
    .stat:hover{opacity:0.8}
    .stat-dot{width:7px;height:7px;border-radius:999px;flex-shrink:0}
    .stat-mini{font-size:10px;cursor:pointer;transition:opacity 120ms}
    .stat-mini:hover{opacity:0.7}
    .org-badge{
      font-size:9px;padding:3px 8px;border-radius:999px;
      border:1px solid rgba(255,255,255,0.08);color:#64748b;
      white-space:nowrap;
    }
    .graph-sync{font-size:10px;color:#64748b;display:flex;align-items:center;gap:4px;white-space:nowrap}

    /* ── Insights bar ── */
    .insights-bar{
      padding:6px 18px;border-bottom:1px solid rgba(255,255,255,0.03);
      background:rgba(8,12,16,0.7);font-size:10px;color:#475569;
      display:flex;gap:24px;align-items:center;flex-wrap:wrap;overflow:hidden;
    }
    .insight-row{display:flex;align-items:center;gap:6px;white-space:nowrap}
    .insight-label{color:#334155;text-transform:uppercase;letter-spacing:0.5px;font-size:9px}

    /* ── Focus Banner ── */
    .focus-banner{
      display:none;align-items:center;gap:10px;
      padding:8px 18px;background:rgba(99,102,241,0.12);
      border-bottom:1px solid rgba(99,102,241,0.2);
      font-size:12px;color:#a5b4fc;
    }
    .focus-banner.visible{display:flex}
    .focus-banner .focus-pill{
      background:rgba(99,102,241,0.2);padding:3px 10px;border-radius:999px;
      font-weight:700;color:#c7d2fe;
    }
    .focus-banner button{
      margin-left:auto;padding:4px 10px;border-radius:6px;
      border:1px solid rgba(99,102,241,0.3);background:rgba(99,102,241,0.1);
      color:#a5b4fc;cursor:pointer;font-size:11px;transition:all 150ms;
    }
    .focus-banner button:hover{background:rgba(99,102,241,0.2)}

    /* ── Toolbar ── */
    .toolbar{
      padding:8px 18px;border-bottom:1px solid rgba(255,255,255,0.04);
      background:rgba(8,12,16,0.85);display:flex;gap:10px;align-items:center;
    }
    .view-toggle{display:flex;border-radius:8px;overflow:hidden;border:1px solid rgba(255,255,255,0.08)}
    .view-btn{
      padding:5px 12px;font-size:11px;cursor:pointer;
      background:rgba(255,255,255,0.02);color:#64748b;
      border:none;border-right:1px solid rgba(255,255,255,0.06);
      transition:all 150ms;white-space:nowrap;
    }
    .view-btn:last-child{border-right:none}
    .view-btn.active{background:rgba(99,102,241,0.15);color:#a5b4fc}
    .view-btn:hover:not(.active){background:rgba(255,255,255,0.04)}

    /* ── Main ── */
    main{
      display:grid;grid-template-columns:1fr 380px;min-height:0;overflow:hidden;
    }
    .view-panel{position:relative;width:100%;height:100%}
    .view-panel.hidden{display:none}

    /* ── Graph ── */
    .graph-wrap{
      background:radial-gradient(ellipse at 40% 30%,#0f172a 0%,#060a0e 65%);
      overflow:hidden;
    }
    #graph{width:100%;height:100%}
    .state-overlay{
      position:absolute;inset:0;display:none;
      align-items:center;justify-content:center;padding:24px;
      background:rgba(6,10,14,0.75);text-align:center;
      color:#475569;font-size:14px;z-index:10;
    }
    .state-overlay.visible{display:flex}

    /* ── List View ── */
    .list-wrap{overflow-y:auto;padding:8px;height:100%}
    .list-header{
      font-size:11px;color:#475569;padding:8px 12px;
      text-transform:uppercase;letter-spacing:0.8px;
      border-bottom:1px solid rgba(255,255,255,0.04);margin-bottom:4px;
    }
    .list-row{
      display:flex;align-items:center;gap:10px;padding:8px 12px;
      border-radius:6px;cursor:pointer;transition:background 120ms;font-size:12px;
    }
    .list-row:hover{background:rgba(255,255,255,0.03)}
    .list-dot{width:8px;height:8px;border-radius:999px;flex-shrink:0}
    .list-name{color:#e2e8f0;font-weight:600;flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .list-tech{color:#475569;font-size:11px;white-space:nowrap}
    .list-status{font-size:9px;font-weight:700;text-transform:uppercase;white-space:nowrap}

    /* ── Roadmap View ── */
    .roadmap-wrap{overflow-y:auto;padding:12px 18px;height:100%}
    /* Tree view */
    .tree-wrap{overflow:auto;padding:0;height:100%;background:var(--bg)}
    .tree-root{margin:0;padding:0}
    .tree-node{margin:0;border-left:2px solid transparent;user-select:none}
    .tree-node-header{display:flex;align-items:center;gap:6px;padding:6px 12px;cursor:pointer;border-radius:4px;margin:1px 0;transition:background .12s}
    .tree-node-header:hover{background:var(--bg2)}
    .tree-node-header.selected{background:var(--accent-bg);outline:1px solid var(--accent)}
    .tree-toggle{width:18px;height:18px;display:flex;align-items:center;justify-content:center;font-size:10px;color:var(--fg2);flex-shrink:0;transition:transform .15s}
    .tree-toggle.expanded{transform:rotate(90deg)}
    .tree-toggle.empty{visibility:hidden}
    .tree-dot{width:10px;height:10px;border-radius:50%;flex-shrink:0}
    .tree-label{font-size:13px;font-weight:500;color:var(--fg);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
    .tree-tech{font-size:11px;color:var(--fg2);flex-shrink:0}
    .tree-status{font-size:11px;font-weight:600;flex-shrink:0}
    .tree-count{font-size:10px;color:var(--fg2);background:var(--bg2);padding:1px 6px;border-radius:10px;flex-shrink:0}
    .tree-children{padding-left:22px;overflow:hidden;transition:max-height .2s ease}
    .tree-children.collapsed{max-height:0!important}
    .tree-category-label{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.5px;padding:8px 12px 4px;color:var(--fg2);display:flex;align-items:center;gap:6px}
    .tree-category-label .cat-swatch{width:8px;height:8px;border-radius:2px;flex-shrink:0}
    .roadmap-header{padding:12px 0 16px;border-bottom:1px solid rgba(255,255,255,0.04);margin-bottom:12px}
    .roadmap-title{font-size:16px;font-weight:700;color:#e2e8f0}
    .roadmap-sub{display:block;font-size:10px;color:#475569;margin-top:4px}
    .roadmap-category{margin-bottom:16px;padding:12px;border-radius:10px;background:rgba(255,255,255,0.015);border:1px solid rgba(255,255,255,0.04)}
    .roadmap-cat-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}
    .roadmap-cat-name{font-size:13px;font-weight:700;color:#cbd5e1;display:flex;align-items:center;gap:8px}
    .roadmap-cat-pct{font-size:11px;color:#64748b}
    .progress-bar{height:4px;border-radius:2px;background:rgba(255,255,255,0.04);margin-bottom:10px;overflow:hidden}
    .progress-fill{height:100%;border-radius:2px;transition:width 400ms ease}
    .roadmap-systems{display:flex;flex-wrap:wrap;gap:4px}
    .roadmap-sys{
      display:inline-flex;align-items:center;gap:5px;padding:4px 10px;
      border-radius:999px;background:rgba(255,255,255,0.02);
      border:1px solid rgba(255,255,255,0.05);cursor:pointer;
      font-size:10px;transition:all 120ms;
    }
    .roadmap-sys:hover{background:rgba(255,255,255,0.05);border-color:rgba(255,255,255,0.12)}
    .roadmap-sys-dot{width:6px;height:6px;border-radius:999px;flex-shrink:0}
    .roadmap-sys-name{color:#e2e8f0;font-weight:600}
    .roadmap-sys-tech{color:#475569;font-size:9px}
    .roadmap-sys-status{font-size:8px;font-weight:700;text-transform:uppercase}

    /* ── Matrix View ── */
    .matrix-wrap{display:flex;flex-direction:column;overflow:hidden;height:100%;padding:8px 12px}
    .matrix-scroll{overflow:auto;flex:1;position:relative}
    .matrix-legend{display:flex;align-items:center;gap:16px;padding:8px 0;border-top:1px solid rgba(255,255,255,0.04);font-size:9px;color:#475569;flex-shrink:0}
    .matrix-legend span{display:flex;align-items:center;gap:4px}
    .matrix-legend .dot{width:8px;height:8px;border-radius:2px;flex-shrink:0}
    .matrix-table{border-collapse:collapse;font-size:9px}
    .matrix-table th,.matrix-table td{padding:0;text-align:center;position:relative}
    .matrix-table th{padding:2px 4px;position:sticky;top:0;z-index:2;background:var(--bg)}
    .matrix-table th:first-child{position:sticky;left:0;z-index:3}
    .matrix-table .row-label,.matrix-table .col-label{
      writing-mode:horizontal-tb;font-size:9px;white-space:nowrap;max-width:100px;
      overflow:hidden;text-overflow:ellipsis;cursor:pointer;color:#94a3b8;padding:2px 6px;
    }
    .matrix-table .row-label:hover,.matrix-table .col-label:hover{color:#e2e8f0}
    .matrix-table .col-label{writing-mode:vertical-rl;text-orientation:mixed;max-height:80px;min-width:20px;padding:4px 2px}
    .matrix-cell{width:14px;height:14px;border-radius:2px;cursor:pointer;transition:all 100ms}
    .matrix-cell:hover{transform:scale(1.5);z-index:1;box-shadow:0 0 6px rgba(255,255,255,0.15)}
    .matrix-cell.empty{background:rgba(255,255,255,0.015)}
    .matrix-cell.integrates{background:rgba(99,102,241,0.5)}
    .matrix-cell.depends{background:rgba(245,158,11,0.5)}
    .matrix-cell.connects{background:rgba(34,197,94,0.5)}
    .matrix-cell.both{background:rgba(236,72,153,0.5)}
    .matrix-cell.self{background:rgba(255,255,255,0.04)}

    /* ── Sidebar ── */
    aside{
      border-left:1px solid rgba(255,255,255,0.05);
      background:rgba(8,12,16,0.96);display:flex;flex-direction:column;overflow:hidden;
    }
    .sidebar-top{
      padding:12px;display:grid;gap:8px;
      border-bottom:1px solid rgba(255,255,255,0.05);
      overflow-y:auto;flex-shrink:0;max-height:50%;
    }
    .sidebar-bottom{flex:1;overflow-y:auto;padding:12px}

    .text-input{
      width:100%;border:1px solid rgba(255,255,255,0.08);
      border-radius:8px;background:rgba(255,255,255,0.02);
      color:#e2e8f0;padding:7px 10px;font-size:12px;
      outline:none;transition:border-color 200ms;
    }
    .text-input:focus{border-color:rgba(99,102,241,0.5)}
    .text-input::placeholder{color:#334155}

    .section-label{
      font-size:9px;color:#334155;text-transform:uppercase;
      letter-spacing:1px;font-weight:700;
    }
    .chip-grid{display:flex;flex-wrap:wrap;gap:5px}
    .chip{
      display:inline-flex;align-items:center;gap:5px;
      border:1px solid rgba(255,255,255,0.06);border-radius:999px;
      padding:3px 8px;background:rgba(255,255,255,0.015);
      font-size:10px;color:#94a3b8;cursor:pointer;
      transition:all 150ms;user-select:none;
    }
    .chip:hover{background:rgba(255,255,255,0.04)}
    .chip input{accent-color:#6366f1;width:12px;height:12px}
    .chip-dot{width:7px;height:7px;border-radius:999px;flex-shrink:0}

    .ghost-btn{
      border:1px solid rgba(255,255,255,0.08);
      background:rgba(255,255,255,0.02);color:#94a3b8;
      border-radius:8px;padding:5px 10px;cursor:pointer;
      font-size:10px;width:fit-content;transition:all 150ms;
    }
    .ghost-btn:hover{background:rgba(255,255,255,0.06);color:#cbd5e1}

    /* ── Detail Card ── */
    .empty-detail{text-align:center;padding:28px 12px;color:#334155}
    .empty-detail-icon{font-size:32px;margin-bottom:8px}
    .empty-detail-text{font-size:13px;color:#475569}
    .empty-detail-sub{font-size:11px;margin-top:4px;color:#334155}

    .detail-header{margin-bottom:10px}
    .detail-name{
      font-size:17px;font-weight:700;color:#f1f5f9;
      display:flex;align-items:center;gap:8px;margin-bottom:6px;
    }
    .status-dot-big{width:10px;height:10px;border-radius:999px;flex-shrink:0}
    .detail-badges{display:flex;gap:6px;flex-wrap:wrap}
    .status-badge{font-size:9px;padding:2px 7px;border-radius:999px;font-weight:700;text-transform:uppercase}
    .deploy-badge{
      font-size:9px;padding:2px 7px;border-radius:999px;
      background:rgba(255,255,255,0.04);color:#64748b;
      border:1px solid rgba(255,255,255,0.06);
    }
    .detail-desc{font-size:12px;color:#94a3b8;line-height:1.6;margin-bottom:12px}

    .detail-actions{display:flex;gap:6px;margin-bottom:12px;flex-wrap:wrap}
    .action-btn{
      font-size:10px;padding:5px 10px;border-radius:6px;cursor:pointer;
      border:1px solid rgba(255,255,255,0.08);background:rgba(255,255,255,0.02);
      color:#94a3b8;transition:all 150ms;white-space:nowrap;
    }
    .action-btn:hover{background:rgba(99,102,241,0.1);border-color:rgba(99,102,241,0.3);color:#a5b4fc}

    .detail-grid{
      display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px;
      padding:10px;border-radius:8px;background:rgba(255,255,255,0.015);
      border:1px solid rgba(255,255,255,0.04);
    }
    .detail-item{}
    .detail-label{font-size:9px;color:#334155;text-transform:uppercase;letter-spacing:0.6px;margin-bottom:2px}
    .detail-value{font-size:12px;color:#cbd5e1;word-break:break-word}

    .detail-section{margin-bottom:10px}
    .detail-section-title{
      font-size:9px;color:#475569;text-transform:uppercase;
      letter-spacing:0.6px;margin-bottom:5px;font-weight:700;
    }
    .chip-wrap{display:flex;flex-wrap:wrap;gap:4px}
    .link-chip{
      font-size:10px;padding:2px 7px;border-radius:999px;
      cursor:pointer;transition:all 120ms;border:1px solid transparent;
    }
    .link-chip:hover{opacity:0.85}
    .dep-chip{background:rgba(239,68,68,0.1);color:#f87171;border-color:rgba(239,68,68,0.2)}
    .int-chip{background:rgba(99,102,241,0.1);color:#a5b4fc;border-color:rgba(99,102,241,0.2)}
    .used-chip{background:rgba(16,185,129,0.1);color:#34d399;border-color:rgba(16,185,129,0.2)}
    .link-chip.unresolved{opacity:0.5}
    .link-chip.meta{background:rgba(245,158,11,0.1);color:#fbbf24;border-color:rgba(245,158,11,0.2);cursor:default}
    .muted{color:#334155;font-size:10px}

    /* ── Dependency Tree ── */
    .dep-tree{padding-left:8px;font-size:11px}
    .dep-tree.collapsed{max-height:0;overflow:hidden}
    .dep-tree:not(.collapsed){max-height:600px;overflow-y:auto}
    .tree-item{padding:3px 0;padding-left:12px;border-left:1px solid rgba(255,255,255,0.06)}
    .tree-item.unresolved{color:#475569;font-style:italic}
    .tree-dot{display:inline-block;width:6px;height:6px;border-radius:999px;margin-right:5px;vertical-align:middle}
    .tree-link{color:#94a3b8;cursor:pointer;transition:color 120ms}
    .tree-link:hover{color:#e2e8f0}
    .tree-status{font-size:8px;font-weight:700;text-transform:uppercase;margin-left:5px}
    .tree-children{margin-top:2px}
    .tree-item.muted{color:#334155}

    /* ── Legend ── */
    .legend-section{
      margin-top:auto;padding:12px;
      border-top:1px solid rgba(255,255,255,0.05);
    }

    /* ── Mobile Bottom Sheet ── */
    @media(max-width:980px){
      body{grid-template-rows:auto auto auto auto 1fr}
      main{
        grid-template-columns:1fr;grid-template-rows:1fr;
        position:relative;
      }
      aside{
        position:fixed;bottom:0;left:0;right:0;z-index:30;
        max-height:55vh;border-left:none;border-top:2px solid rgba(255,255,255,0.08);
        border-radius:16px 16px 0 0;box-shadow:0 -4px 24px rgba(0,0,0,0.4);
        transform:translateY(0);transition:transform 250ms ease;
      }
      aside.collapsed{transform:translateY(calc(100% - 44px))}
      .mobile-handle{
        display:flex;justify-content:center;padding:8px 0 4px;cursor:pointer;
      }
      .mobile-handle-bar{width:32px;height:3px;border-radius:2px;background:rgba(255,255,255,0.15)}
      .header-stats{font-size:10px;gap:8px}
      .insights-bar{gap:12px}
      .toolbar{gap:6px}
      .view-btn{font-size:10px;padding:5px 8px}
    }
    @media(min-width:981px){
      .mobile-handle{display:none}
    }

    /* ── Help Overlay ── */
    .help-overlay{
      position:fixed;inset:0;z-index:100;display:none;
      align-items:center;justify-content:center;
      background:rgba(6,10,14,0.88);backdrop-filter:blur(6px);
    }
    .help-overlay.visible{display:flex}
    .help-card{
      background:rgba(15,23,42,0.98);border:1px solid rgba(255,255,255,0.08);
      border-radius:16px;padding:28px 32px;max-width:520px;width:90vw;
      box-shadow:0 16px 48px rgba(0,0,0,0.5);max-height:85vh;overflow-y:auto;
    }
    .help-card h2{
      font-size:18px;color:#e2e8f0;margin-bottom:4px;
      display:flex;align-items:center;gap:8px;
    }
    .help-card .help-sub{font-size:10px;color:#475569;margin-bottom:18px;text-transform:uppercase;letter-spacing:1px}
    .help-section{margin-bottom:14px}
    .help-section h3{font-size:11px;color:#64748b;text-transform:uppercase;letter-spacing:0.6px;margin-bottom:6px}
    .help-row{
      display:flex;align-items:center;gap:10px;padding:5px 0;
      font-size:12px;color:#94a3b8;border-bottom:1px solid rgba(255,255,255,0.02);
    }
    .help-row kbd{
      display:inline-block;min-width:28px;text-align:center;
      background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.1);
      border-radius:5px;padding:2px 7px;font-size:11px;font-family:inherit;
      color:#cbd5e1;white-space:nowrap;
    }
    .help-row .help-desc{flex:1;min-width:0}
    .help-close{
      position:absolute;top:8px;right:12px;
      background:none;border:none;color:#475569;cursor:pointer;
      font-size:18px;padding:4px 8px;border-radius:6px;transition:all 150ms;
    }
    .help-close:hover{color:#e2e8f0;background:rgba(255,255,255,0.04)}
    .help-btn{
      margin-left:auto;padding:3px 9px;border-radius:6px;
      border:1px solid rgba(255,255,255,0.08);background:rgba(255,255,255,0.02);
      color:#64748b;cursor:pointer;font-size:12px;font-weight:700;
      transition:all 150ms;white-space:nowrap;
    }
    .help-btn:hover{background:rgba(99,102,241,0.12);border-color:rgba(99,102,241,0.3);color:#a5b4fc}
  </style>
  <script src="https://unpkg.com/vis-network/standalone/umd/vis-network.min.js"></script>
</head>
<body>
  <header>
    <div class="header-left">
      <div class="logo">N</div>
      <div class="title-group">
        <div class="title">Nexus Systems Ecosystem</div>
        <div class="subtitle">The No Hands Company</div>
      </div>
    </div>
    <div class="header-right">
      <div class="header-stats">
        <span class="stat" title="Native (Rust/C++)"><span class="stat-dot" style="background:#8b5cf6"></span> <span id="nativeCount">0</span></span>
        <span class="stat" title="Built & tested"><span class="stat-dot" style="background:#10b981"></span> <span id="builtCount">0</span></span>
        <span class="stat" title="Scaffolded / WIP"><span class="stat-dot" style="background:#f59e0b"></span> <span id="scaffoldedCount">0</span></span>
        <span class="stat" title="Planned"><span class="stat-dot" style="background:#475569"></span> <span id="shellCount">0</span></span>
        <span class="stat" style="font-weight:700;color:#94a3b8">· <span id="totalCount">0</span></span>
      </div>
      <span class="graph-sync" id="graphStatus">📄 …</span>
      <span class="org-badge">🖐️ The No Hands Company</span>
    </div>
  </header>

  <div class="insights-bar" id="insightsData">
    <span style="color:#334155">Loading insights…</span>
  </div>

  <div class="focus-banner" id="focusBanner">
    🔍 Focus: <span class="focus-pill" id="focusLabel"></span>
    <span style="font-size:10px;color:#64748b">— showing only direct connections. Double-click a node to focus, Esc to exit.</span>
    <button id="exitFocus">✕ Exit Focus</button>
  </div>

  <div class="toolbar">
    <div class="view-toggle">
      <button class="view-btn active" id="btnGraphView">🔗 Graph</button>
      <button class="view-btn" id="btnListView">📋 List</button>
      <button class="view-btn" id="btnRoadmapView">📅 Roadmap</button>
      <button class="view-btn" id="btnTreeView">🌳 Tree</button>
      <button class="view-btn" id="btnMatrixView">🔲 Matrix</button>
    </div>
    <span style="font-size:10px;color:#334155">
      <span id="visibleNodes">0</span> nodes · <span id="visibleEdges">0</span> edges
    </span>
    <span style="font-size:9px;color:#334155;margin-left:auto">
      <kbd style="background:rgba(255,255,255,0.04);padding:1px 5px;border-radius:4px">Ctrl+K</kbd> search
      <kbd style="background:rgba(255,255,255,0.04);padding:1px 5px;border-radius:4px">←→</kbd> nav
      <kbd style="background:rgba(255,255,255,0.04);padding:1px 5px;border-radius:4px">Enter</kbd> focus
      <kbd style="background:rgba(255,255,255,0.04);padding:1px 5px;border-radius:4px">Esc</kbd> clear
      <button class="help-btn" id="btnHelp" title="Keyboard shortcuts & navigation">?</button>
    </span>
  </div>

  <!-- Help Overlay -->
  <div class="help-overlay" id="helpOverlay">
    <div class="help-card" onclick="event.stopPropagation()">
      <h2>⌨️ How to Use the Visualizer</h2>
      <div class="help-sub">Keyboard shortcuts & navigation guide</div>
      <div class="help-section">
        <h3>🔍 Search & Select</h3>
        <div class="help-row"><kbd>Ctrl+K</kbd><span class="help-desc">Focus the search bar</span></div>
        <div class="help-row"><kbd>Click</kbd><span class="help-desc">Select a system node to see details</span></div>
        <div class="help-row"><kbd>Tab</kbd><span class="help-desc">Cycle forward through visible systems</span></div>
      </div>
      <div class="help-section">
        <h3>🔗 Navigation</h3>
        <div class="help-row"><kbd>DblClick</kbd><span class="help-desc">Focus mode — show only a system & its connections</span></div>
        <div class="help-row"><kbd>← ↑ → ↓</kbd><span class="help-desc">Jump between connected nodes (when a system is selected)</span></div>
        <div class="help-row"><kbd>Enter</kbd><span class="help-desc">Enter Focus mode for the selected system</span></div>
        <div class="help-row"><kbd>Esc</kbd><span class="help-desc">Exit Focus mode / clear selection / close this help</span></div>
      </div>
      <div class="help-section">
        <h3>📋 Views</h3>
        <div class="help-row"><kbd>🔗</kbd><span class="help-desc"><b>Graph</b> — interactive network of all systems. Drag, zoom, click.</span></div>
        <div class="help-row"><kbd>📋</kbd><span class="help-desc"><b>List</b> — searchable, sortable table. Scroll to see all systems.</span></div>
        <div class="help-row"><kbd>📅</kbd><span class="help-desc"><b>Roadmap</b> — systems grouped by category with progress bars. Scroll per category.</span></div>
        <div class="help-row"><kbd>🌳</kbd><span class="help-desc"><b>Tree</b> — hierarchy view with Nexus-Cloud as root. Collapse/expand categories.</span></div>
        <div class="help-row"><kbd>🔲</kbd><span class="help-desc"><b>Matrix</b> — dependency adjacency matrix. See every connection at a glance.</span></div>
      </div>
      <div class="help-section">
        <h3>🖱️ Mouse</h3>
        <div class="help-row"><kbd>Scroll</kbd><span class="help-desc">Zoom in/out of the graph. In List/Roadmap: scroll the page.</span></div>
        <div class="help-row"><kbd>Drag</kbd><span class="help-desc">Pan the graph. Drag background to move around.</span></div>
        <div class="help-row"><kbd>RClick</kbd><span class="help-desc">Right-click a node in the graph for more options</span></div>
      </div>
    </div>
  </div>

  <main>
    <!-- Graph View -->
    <div class="view-panel graph-wrap" id="graphWrap">
      <div id="graph"></div>
      <div id="emptyState" class="state-overlay">No systems match the current filters.</div>
    </div>

    <!-- List View -->
    <div class="view-panel list-wrap hidden" id="listWrap">
      <div class="list-header" id="listCount">—</div>
      <div id="listView"></div>
    </div>

    <!-- Roadmap View -->
    <div class="view-panel roadmap-wrap hidden" id="roadmapWrap"></div>

    <!-- Tree (Hierarchy) View -->
    <div class="view-panel tree-wrap hidden" id="treeWrap"></div>

    <!-- Matrix (Dependency) View -->
    <div class="view-panel matrix-wrap hidden" id="matrixWrap">
      <div class="matrix-scroll" id="matrixScroll"></div>
      <div class="matrix-legend" id="matrixLegend"></div>
    </div>

    <!-- Sidebar -->
    <aside id="sidebar">
      <div class="mobile-handle" id="mobileHandle" title="Tap to expand/collapse">
        <div class="mobile-handle-bar"></div>
      </div>
      <div class="sidebar-top" id="sidebarTop">
        <div class="section-label">🔍 Search</div>
        <input id="searchInput" class="text-input" type="search" placeholder="Search 88+ systems…" autocomplete="off" />

        <div class="section-label">Status</div>
        <div class="chip-grid" id="statusFilters"></div>

        <div class="section-label">Categories</div>
        <div class="chip-grid" id="categoryFilters"></div>

        <button id="resetFilters" class="ghost-btn" type="button">↺ Reset All</button>
      </div>

      <div class="sidebar-bottom">
        <div class="section-label" style="margin-bottom:6px">📋 System Details</div>
        <div id="detailCard">
          <div class="empty-detail">
            <div class="empty-detail-icon">👆</div>
            <div class="empty-detail-text">Click a node to see details</div>
            <div class="empty-detail-sub">Double-click to focus · Arrow keys to navigate</div>
          </div>
        </div>
      </div>

      <div class="legend-section">
        <div class="section-label" style="margin-bottom:6px">Legend</div>
        <div style="display:grid;gap:4px;font-size:10px;color:#475569">
          <div style="display:flex;align-items:center;gap:6px"><span style="width:8px;height:8px;border-radius:999px;background:#8b5cf6"></span> Native (Rust/C++)</div>
          <div style="display:flex;align-items:center;gap:6px"><span style="width:8px;height:8px;border-radius:999px;background:#10b981"></span> Built (working)</div>
          <div style="display:flex;align-items:center;gap:6px"><span style="width:8px;height:8px;border-radius:999px;background:#f59e0b"></span> Scaffolded (WIP)</div>
          <div style="display:flex;align-items:center;gap:6px"><span style="width:8px;height:8px;border-radius:999px;background:#475569"></span> Shell (planned)</div>
          <div style="display:flex;align-items:center;gap:6px;margin-top:4px"><span style="width:8px;height:2px;background:#ef4444"></span> Depends on</div>
          <div style="display:flex;align-items:center;gap:6px"><span style="width:8px;height:2px;background:#6366f1;border-top:1px dashed #6366f1"></span> Integrates with</div>
        </div>
      </div>
    </aside>
  </main>

  <script>
    // ═══════════════════════════════════════════
    // Embedded Data (baked at build time)
    // ═══════════════════════════════════════════
    const EMBEDDED_DATA = __DATA_JSON__;

    // ═══════════════════════════════════════════
    // Application (baked at build time)
    // ═══════════════════════════════════════════
    __APP_JS__
  </script>
</body>
</html>"""


# ═══════════════════════════════════════════════════════
#  Build
# ═══════════════════════════════════════════════════════

def build() -> int:
    """Read data + JS, bake into HTML, write output. Returns 0 on success."""
    try:
        data_json = DATA_PATH.read_text(encoding="utf-8")
        app_js = JS_PATH.read_text(encoding="utf-8")
    except FileNotFoundError as e:
        print(f"✗ Missing file: {e}", file=sys.stderr)
        return 1

    # Validate JSON
    try:
        data = json.loads(data_json)
    except json.JSONDecodeError as e:
        print(f"✗ Invalid JSON in {DATA_PATH}: {e}", file=sys.stderr)
        return 1

    # Update timestamp
    data["generatedAt"] = datetime.now(timezone.utc).isoformat()
    data_json = json.dumps(data, ensure_ascii=False, indent=2)

    # Bake
    html = HTML_TEMPLATE.replace("__DATA_JSON__", data_json)
    html = html.replace("__APP_JS__", app_js)

    OUT_PATH.write_text(html, encoding="utf-8")
    size_kb = OUT_PATH.stat().st_size / 1024
    sys_count = sum(len(cat["systems"]) for eco in data["ecosystems"] for cat in eco["categories"])
    print(f"✓ Built {OUT_PATH.name} — {size_kb:.0f} KB, {sys_count} systems, {len(data['ecosystems'][0]['categories'])} categories")
    return 0


def watch():
    """Rebuild whenever data or JS changes."""
    try:
        from watchdog.observers import Observer
        from watchdog.events import FileSystemEventHandler
    except ImportError:
        print("✗ Install watchdog for --watch: pip install watchdog", file=sys.stderr)
        return 1

    class Handler(FileSystemEventHandler):
        def on_modified(self, event):
            if event.src_path.endswith(('.json', '.js')):
                print(f"\n→ Change detected in {Path(event.src_path).name}")
                build()

    observer = Observer()
    observer.schedule(Handler(), str(ROOT), recursive=False)
    observer.start()
    print(f"👀 Watching {ROOT} for changes to *.json, *.js — Ctrl+C to stop")
    build()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        observer.stop()
    observer.join()
    return 0


if __name__ == "__main__":
    if "--watch" in sys.argv:
        sys.exit(watch())
    else:
        sys.exit(build())
