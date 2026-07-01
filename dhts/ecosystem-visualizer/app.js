// ═══════════════════════════════════════════════════════════════
// Nexus Systems Ecosystem Visualizer v3
// The No Hands Company — Free. Open Source. Forever.
// ═══════════════════════════════════════════════════════════════

// ── Constants ──
const STATUS_COLORS = {
  native:     { bg: '#8b5cf6', border: '#a78bfa', text: '#ede9fe', label: 'NATIVE' },
  built:      { bg: '#10b981', border: '#34d399', text: '#ecfdf5', label: 'BUILT' },
  scaffolded: { bg: '#f59e0b', border: '#fbbf24', text: '#fefce8', label: 'WIP' },
  shell:      { bg: '#475569', border: '#64748b', text: '#f1f5f9', label: 'PLANNED' }
};

const DEPLOYMENT_LABELS = {
  'both':        '⚡ Self + Cloud',
  'self-hosted': '🏠 Self-Hosted',
  'cloud':       '☁️ Cloud Only',
  'infra':       '🔧 Infrastructure',
  'package':     '📦 Package'
};

// ── State ──
const state = {
  data: null,
  systemIndex: new Map(),       // id → system (deduplicated)
  reverseDeps: new Map(),       // id → [{id, name, status}]
  categoryList: [],
  visibleStatuses: new Set(Object.keys(STATUS_COLORS)),
  visibleCategories: new Set(),
  search: '',
  searchDebounceTimer: null,
  selectedSystemId: null,
  viewMode: 'graph',            // 'graph' | 'list' | 'roadmap' | 'tree'
  savedGraphOptions: null,       // saved physics/layout for graph→tree→graph restore
  focusMode: false,
  focusRootId: null,
  graphData: null,              // ecosystem-graph cross-reference
};

let nodesDataSet, edgesDataSet, network;

// ═══════════════════════════════════════════════════════
//  Data Indexing
// ═══════════════════════════════════════════════════════

function buildSystemIndex(data) {
  const index = new Map();
  for (const eco of data.ecosystems) {
    for (const cat of eco.categories) {
      for (const sys of cat.systems) {
        const existing = index.get(sys.id);
        if (!existing) {
          index.set(sys.id, {
            ...sys,
            categories: [cat.id],
            categoryNames: [cat.name],
            categoryColors: [cat.color]
          });
        } else {
          if (!existing.categories.includes(cat.id)) {
            existing.categories.push(cat.id);
            existing.categoryNames.push(cat.name);
            existing.categoryColors.push(cat.color);
          }
        }
      }
    }
  }
  return index;
}

function buildReverseDeps(systemIndex) {
  const rev = new Map();
  for (const [id, sys] of systemIndex) {
    for (const dep of sys.dependencies) {
      const target = findSystemByNameOrId(dep, systemIndex);
      const targetId = target ? target.id : dep.toLowerCase().replace(/\s+/g, '-');
      if (!rev.has(targetId)) rev.set(targetId, []);
      if (!rev.get(targetId).some(r => r.id === id)) {
        rev.get(targetId).push({ id, name: sys.name, status: sys.status });
      }
    }
  }
  return rev;
}

function findSystemByNameOrId(nameOrId, systemIndex) {
  if (!nameOrId || nameOrId === 'ALL_SYSTEMS' || nameOrId === 'ALL_TS_SYSTEMS' || nameOrId === 'ALL_AI_CONSUMERS') return null;
  const slug = nameOrId.toLowerCase().replace(/\s+/g, '-');
  if (systemIndex.has(slug)) return systemIndex.get(slug);
  return [...systemIndex.values()].find(s =>
    s.name.toLowerCase() === nameOrId.toLowerCase() || s.id === slug
  );
}

// ═══════════════════════════════════════════════════════
//  Focus Mode
// ═══════════════════════════════════════════════════════

function enterFocusMode(sysId) {
  const sys = state.systemIndex.get(sysId);
  if (!sys) return;

  state.focusMode = true;
  state.focusRootId = sysId;
  document.getElementById('focusBanner').classList.add('visible');
  document.getElementById('focusLabel').textContent = sys.name;
  updateHash();

  _renderFocusGraph(sysId);
}

function exitFocusMode() {
  state.focusMode = false;
  state.focusRootId = null;
  document.getElementById('focusBanner').classList.remove('visible');
  updateHash();
  applyFilters();
}

function _renderFocusGraph(sysId) {
  const sys = state.systemIndex.get(sysId);
  if (!sys) return;

  const neighborIds = new Set([sysId]);

  // Dependencies (sys depends on these)
  for (const depName of sys.dependencies || []) {
    const t = findSystemByNameOrId(depName, state.systemIndex);
    if (t) neighborIds.add(t.id);
  }

  // Integrations
  for (const intName of sys.integrates || []) {
    const t = findSystemByNameOrId(intName, state.systemIndex);
    if (t) neighborIds.add(t.id);
  }

  // Reverse deps (systems that depend on this)
  for (const rev of state.reverseDeps.get(sysId) || []) {
    neighborIds.add(rev.id);
  }

  // Also: systems that integrate with this (search all systems' integrates lists)
  for (const [, other] of state.systemIndex) {
    if (neighborIds.has(other.id)) continue;
    for (const intName of other.integrates || []) {
      const t = findSystemByNameOrId(intName, state.systemIndex);
      if (t && t.id === sysId) {
        neighborIds.add(other.id);
        break;
      }
    }
  }

  nodesDataSet.clear();
  edgesDataSet.clear();

  const nodes = [];
  for (const nid of neighborIds) {
    const s = state.systemIndex.get(nid);
    if (s) nodes.push(mkNode(s, s.categories?.[0]));
  }
  nodesDataSet.add(nodes);

  // Build edges only between visible nodes
  const edges = [];
  const addedPairs = new Set();
  for (const nid of neighborIds) {
    const s = state.systemIndex.get(nid);
    if (!s) continue;
    for (const depName of s.dependencies || []) {
      const t = findSystemByNameOrId(depName, state.systemIndex);
      if (t && neighborIds.has(t.id)) {
        const key = `${nid}->${t.id}`;
        if (!addedPairs.has(key)) {
          addedPairs.add(key);
          edges.push(mkEdge(nid, t.id, 'dependency', '#ef4444'));
        }
      }
    }
    for (const intName of s.integrates || []) {
      const t = findSystemByNameOrId(intName, state.systemIndex);
      if (t && neighborIds.has(t.id)) {
        const key = `${nid}--${t.id}`;
        if (!addedPairs.has(key)) {
          addedPairs.add(key);
          edges.push(mkEdge(nid, t.id, 'integration', '#6366f1'));
        }
      }
    }
  }
  edgesDataSet.add(edges);

  document.getElementById('visibleNodes').textContent = nodes.length;
  document.getElementById('visibleEdges').textContent = edges.length;
  document.getElementById('emptyState').classList.remove('visible');

  if (network) {
    network.selectNodes([sysId]);
    setTimeout(() => network.fit({ animation: { duration: 400, easingFunction: 'easeInOutQuad' } }), 50);
  }
}

// ═══════════════════════════════════════════════════════
//  Graph Construction
// ═══════════════════════════════════════════════════════

function mkNode(sys, primaryCategory) {
  const colors = STATUS_COLORS[sys.status] || STATUS_COLORS.shell;
  const depCount = sys.dependencies?.length || 0;
  const intCount = sys.integrates?.length || 0;
  const revCount = (state.reverseDeps.get(sys.id) || []).length;
  const totalEdges = depCount + intCount + revCount;
  const size = Math.min(10 + totalEdges * 2, 34);

  const label = sys.name.replace('Nexus-', '');

  return {
    id: sys.id,
    label: label,
    group: primaryCategory || sys.categories?.[0] || 'other',
    color: {
      background: colors.bg,
      border: colors.border,
      highlight: { background: colors.border, border: '#fff' },
      hover: { background: colors.border, border: '#fff' }
    },
    font: {
      color: '#e2e8f0',
      size: Math.max(10, Math.min(label.length > 10 ? 9 : 11, 14)),
      face: 'system-ui, sans-serif',
      strokeWidth: 2,
      strokeColor: '#080c0f'
    },
    shape: sys.status === 'native' ? 'diamond' : 'dot',
    size: size,
    borderWidth: sys.status === 'native' ? 3 : sys.status === 'built' ? 2 : 1,
    data: sys
  };
}

function mkEdge(sourceId, targetId, relation, color) {
  return {
    from: sourceId,
    to: targetId,
    color: { color: color, opacity: 0.5, highlight: color },
    arrows: { to: { enabled: true, scaleFactor: 0.5 } },
    width: 0.8,
    smooth: { type: 'continuous', roundness: 0.3 },
    dashes: relation === 'integration',
    title: relation === 'dependency' ? 'depends on' : 'integrates with'
  };
}

function buildEdges(systemIndex, visibleIds) {
  const edges = [];
  const addedPairs = new Set();

  for (const sysId of visibleIds) {
    const sys = systemIndex.get(sysId);
    if (!sys) continue;

    for (const depName of sys.dependencies || []) {
      const target = findSystemByNameOrId(depName, systemIndex);
      if (target && visibleIds.has(target.id)) {
        const key = `${sysId}->${target.id}`;
        if (!addedPairs.has(key)) {
          addedPairs.add(key);
          edges.push(mkEdge(sysId, target.id, 'dependency', '#ef4444'));
        }
      }
    }

    for (const intName of sys.integrates || []) {
      const target = findSystemByNameOrId(intName, systemIndex);
      if (target && visibleIds.has(target.id)) {
        const key = `${sysId}--${target.id}`;
        if (!addedPairs.has(key)) {
          addedPairs.add(key);
          edges.push(mkEdge(sysId, target.id, 'integration', '#6366f1'));
        }
      }
    }
  }

  return edges;
}

// ═══════════════════════════════════════════════════════
//  Filtering
// ═══════════════════════════════════════════════════════

function getVisibleSystems() {
  const results = [];
  for (const [id, sys] of state.systemIndex) {
    if (!state.visibleStatuses.has(sys.status)) continue;

    const hasVisibleCategory = sys.categories.some(c => state.visibleCategories.has(c));
    if (state.visibleCategories.size > 0 && !hasVisibleCategory) continue;

    if (state.search) {
      const q = state.search.toLowerCase();
      const matches =
        sys.name.toLowerCase().includes(q) ||
        sys.id.toLowerCase().includes(q) ||
        (sys.description || '').toLowerCase().includes(q) ||
        (sys.tech || '').toLowerCase().includes(q);
      if (!matches) continue;
    }

    results.push(sys);
  }
  return results;
}

function applyFilters() {
  if (!state.systemIndex.size) return;

  // If in focus mode, use focus rendering instead
  if (state.focusMode && state.focusRootId) {
    _renderFocusGraph(state.focusRootId);
    return;
  }

  const visible = getVisibleSystems();
  const visibleIds = new Set(visible.map(s => s.id));

  nodesDataSet.clear();
  edgesDataSet.clear();

  const nodes = visible.map(s => mkNode(s, s.categories?.[0]));
  nodesDataSet.add(nodes);

  const edges = buildEdges(state.systemIndex, visibleIds);
  edgesDataSet.add(edges);

  document.getElementById('visibleNodes').textContent = visible.length;
  document.getElementById('visibleEdges').textContent = edges.length;

  if (state.viewMode === 'list') renderListView(visible);
  if (state.viewMode === 'roadmap') renderRoadmapView();
  if (state.viewMode === 'tree') renderTreeView();
  if (state.viewMode === 'matrix') renderMatrixView();

  if (visible.length === 0) {
    document.getElementById('emptyState').classList.add('visible');
  } else {
    document.getElementById('emptyState').classList.remove('visible');
    if (network && state.viewMode === 'graph') {
      setTimeout(() => network.fit({ animation: { duration: 300, easingFunction: 'easeInOutQuad' } }), 50);
    }
  }
}

// ═══════════════════════════════════════════════════════
//  URL Hash Routing
// ═══════════════════════════════════════════════════════

function updateHash() {
  if (state.focusMode && state.focusRootId) {
    window.location.hash = 'focus=' + state.focusRootId;
  } else if (state.selectedSystemId) {
    window.location.hash = state.selectedSystemId;
  } else {
    // Don't clear hash if we're just rendering — only clear on explicit deselect
  }
}

function applyHash() {
  const hash = window.location.hash.replace('#', '');
  if (!hash) return;

  if (hash.startsWith('focus=')) {
    const sysId = hash.replace('focus=', '');
    if (state.systemIndex.has(sysId)) {
      enterFocusMode(sysId);
    }
  } else if (state.systemIndex.has(hash)) {
    selectSystem(hash);
  }
}

// ═══════════════════════════════════════════════════════
//  Sidebar Detail
// ═══════════════════════════════════════════════════════

function selectSystem(sysId) {
  state.selectedSystemId = sysId;
  renderDetail(sysId);
  updateHash();
  if (network && sysId) {
    network.selectNodes([sysId]);
    network.focus(sysId, { scale: 1.5, animation: { duration: 400, easingFunction: 'easeInOutQuad' } });
  }
}

function renderDetail(sysId) {
  const card = document.getElementById('detailCard');

  if (!sysId) {
    card.innerHTML = `
      <div class="empty-detail">
        <div class="empty-detail-icon">👆</div>
        <div class="empty-detail-text">Click a node to see details</div>
        <div class="empty-detail-sub">Double-click to focus · Arrow keys to navigate</div>
      </div>`;
    return;
  }

  const sys = state.systemIndex.get(sysId);
  if (!sys) return;

  state.selectedSystemId = sysId;
  const colors = STATUS_COLORS[sys.status] || STATUS_COLORS.shell;
  const deployLabel = DEPLOYMENT_LABELS[sys.deployment] || sys.deployment || '—';

  // Dependencies
  const depLinks = (sys.dependencies || []).map(d => {
    const target = findSystemByNameOrId(d, state.systemIndex);
    return target
      ? `<span class="link-chip dep-chip" data-id="${target.id}" onclick="event.stopPropagation(); selectSystem('${target.id}')">⬇ ${target.name}</span>`
      : `<span class="link-chip dep-chip unresolved">⬇ ${d}</span>`;
  }).join('') || '<span class="muted">none</span>';

  // Integrations
  const intLinks = (sys.integrates || []).map(i => {
    const target = findSystemByNameOrId(i, state.systemIndex);
    if (target) {
      return `<span class="link-chip int-chip" data-id="${target.id}" onclick="event.stopPropagation(); selectSystem('${target.id}')">↔ ${target.name}</span>`;
    }
    if (i === 'ALL_SYSTEMS' || i === 'ALL_TS_SYSTEMS' || i === 'ALL_AI_CONSUMERS') {
      return `<span class="link-chip int-chip meta">${i}</span>`;
    }
    return `<span class="link-chip int-chip unresolved">↔ ${i}</span>`;
  }).join('') || '<span class="muted">none</span>';

  // Used by
  const usedBy = (state.reverseDeps.get(sysId) || []);
  const usedByLinks = usedBy.length > 0
    ? usedBy.map(u => `<span class="link-chip used-chip" data-id="${u.id}" onclick="event.stopPropagation(); selectSystem('${u.id}')">⬆ ${u.name}</span>`).join('')
    : '<span class="muted">none</span>';

  // Docs coverage from ecosystem-graph
  let docCoverage = '—';
  if (state.graphData) {
    const docNode = state.graphData.nodes.find(n =>
      n.type === 'system' && n.label.toLowerCase() === sys.name.toLowerCase()
    );
    docCoverage = docNode
      ? `✅ Documented (${(docNode.sources || []).length} files)`
      : '⚠️ Not documented';
  }

  // Dependency tree (recursive)
  const depTree = _buildDepTree(sysId, new Set(), 0);

  card.innerHTML = `
    <div class="detail-header">
      <div class="detail-name">
        <span class="status-dot-big" style="background:${colors.bg};box-shadow:0 0 8px ${colors.bg}"></span>
        ${sys.name}
      </div>
      <div class="detail-badges">
        <span class="status-badge" style="background:${colors.bg};color:${colors.text}">${colors.label}</span>
        <span class="deploy-badge">${deployLabel}</span>
      </div>
    </div>

    <div class="detail-desc">${sys.description || 'No description yet.'}</div>

    <div class="detail-actions">
      <button class="action-btn" onclick="enterFocusMode('${sysId}')" title="Show only this system and its direct connections">🔍 Focus</button>
      <button class="action-btn" onclick="copySystemMarkdown('${sysId}')" title="Copy system details as Markdown">📋 Copy MD</button>
      <button class="action-btn" onclick="window.open('https://github.com/The-No-Hands-Company/${sys.name}', '_blank')" title="Open GitHub repo">🐙 GitHub</button>
    </div>

    <div class="detail-grid">
      <div class="detail-item">
        <div class="detail-label">Tech Stack</div>
        <div class="detail-value">${sys.tech || '—'}</div>
      </div>
      <div class="detail-item">
        <div class="detail-label">Port</div>
        <div class="detail-value">${sys.port || '—'}</div>
      </div>
      <div class="detail-item">
        <div class="detail-label">Categories</div>
        <div class="detail-value">${(sys.categoryNames || []).join(', ') || '—'}</div>
      </div>
      <div class="detail-item">
        <div class="detail-label">Docs</div>
        <div class="detail-value">${docCoverage}</div>
      </div>
    </div>

    <div class="detail-section">
      <div class="detail-section-title">⬇ Dependencies (${(sys.dependencies || []).length})</div>
      <div class="chip-wrap">${depLinks}</div>
    </div>

    <div class="detail-section">
      <div class="detail-section-title">↔ Integrates With (${(sys.integrates || []).length})</div>
      <div class="chip-wrap">${intLinks}</div>
    </div>

    <div class="detail-section">
      <div class="detail-section-title">⬆ Used By (${usedBy.length})</div>
      <div class="chip-wrap">${usedByLinks}</div>
    </div>

    <div class="detail-section">
      <div class="detail-section-title" style="cursor:pointer" onclick="document.getElementById('depTree').classList.toggle('collapsed')">
        🌳 Dependency Tree
        <span style="font-size:9px;color:#475569;margin-left:4px">(click to expand)</span>
      </div>
      <div id="depTree" class="dep-tree collapsed">${depTree}</div>
    </div>
  `;
}

function _buildDepTree(sysId, visited, depth) {
  if (visited.has(sysId) || depth > 3) return '';
  visited.add(sysId);

  const sys = state.systemIndex.get(sysId);
  const deps = sys?.dependencies || [];
  const children = deps.map(d => {
    const target = findSystemByNameOrId(d, state.systemIndex);
    if (!target) return `<div class="tree-item unresolved">${d}</div>`;
    const colors = STATUS_COLORS[target.status] || STATUS_COLORS.shell;
    const subTree = _buildDepTree(target.id, new Set(visited), depth + 1);
    return `
      <div class="tree-item">
        <span class="tree-dot" style="background:${colors.bg}"></span>
        <span class="tree-link" onclick="event.stopPropagation(); selectSystem('${target.id}')">${target.name}</span>
        <span class="tree-status" style="color:${colors.border}">${colors.label}</span>
        ${subTree ? `<div class="tree-children">${subTree}</div>` : ''}
      </div>`;
  }).join('');

  if (!children && depth === 0) return '<div class="tree-item muted">No dependencies</div>';
  return children;
}

function copySystemMarkdown(sysId) {
  const sys = state.systemIndex.get(sysId);
  if (!sys) return;
  const colors = STATUS_COLORS[sys.status] || STATUS_COLORS.shell;
  const md = [
    `# ${sys.name}`,
    `- **Status**: ${colors.label}`,
    `- **Tech**: ${sys.tech || '—'}`,
    `- **Port**: ${sys.port || '—'}`,
    `- **Deployment**: ${DEPLOYMENT_LABELS[sys.deployment] || sys.deployment}`,
    `- **Description**: ${sys.description || '—'}`,
    `- **Dependencies**: ${(sys.dependencies || []).join(', ') || 'none'}`,
    `- **Integrates with**: ${(sys.integrates || []).join(', ') || 'none'}`,
    `- **GitHub**: https://github.com/The-No-Hands-Company/${sys.name}`,
  ].join('\n');

  navigator.clipboard.writeText(md).then(() => {
    const btn = document.querySelector('.action-btn');
    if (btn) { const orig = btn.textContent; btn.textContent = '✅ Copied!'; setTimeout(() => btn.textContent = orig, 1500); }
  }).catch(() => alert('Copy failed — here is the markdown:\n\n' + md));
}

// ═══════════════════════════════════════════════════════
//  List View
// ═══════════════════════════════════════════════════════

function renderListView(systems) {
  const container = document.getElementById('listView');
  if (!container) return;

  container.innerHTML = systems.map(s => {
    const colors = STATUS_COLORS[s.status] || STATUS_COLORS.shell;
    return `
      <div class="list-row" data-id="${s.id}" onclick="selectSystem('${s.id}'); switchView('graph')">
        <span class="list-dot" style="background:${colors.bg}"></span>
        <span class="list-name">${s.name}</span>
        <span class="list-tech">${s.tech || ''}</span>
        <span class="list-status" style="color:${colors.border}">${colors.label}</span>
      </div>`;
  }).join('');

  document.getElementById('listCount').textContent = `${systems.length} systems`;
}

// ═══════════════════════════════════════════════════════
//  Roadmap View
// ═══════════════════════════════════════════════════════

function renderRoadmapView() {
  const container = document.getElementById('roadmapWrap');
  if (!container) return;

  // Group systems by category, respecting current filters
  const visible = getVisibleSystems();
  const byCategory = new Map();

  for (const sys of visible) {
    const catId = sys.categories?.[0] || 'other';
    const catName = sys.categoryNames?.[0] || 'Other';
    if (!byCategory.has(catId)) {
      byCategory.set(catId, { id: catId, name: catName, color: sys.categoryColors?.[0] || '#475569', systems: [] });
    }
    byCategory.get(catId).systems.push(sys);
  }

  // Sort categories: least complete first (more shells = higher priority)
  const sorted = [...byCategory.values()].sort((a, b) => {
    const aPct = a.systems.filter(s => s.status === 'built' || s.status === 'native').length / a.systems.length;
    const bPct = b.systems.filter(s => s.status === 'built' || s.status === 'native').length / b.systems.length;
    return aPct - bPct;
  });

  container.innerHTML = `
    <div class="roadmap-header">
      <span class="roadmap-title">📅 Development Roadmap</span>
      <span class="roadmap-sub">${visible.length} systems · categories sorted by completion (least → most)</span>
    </div>
    ${sorted.map(cat => {
      const built = cat.systems.filter(s => s.status === 'built' || s.status === 'native').length;
      const pct = Math.round((built / cat.systems.length) * 100);
      const statusOrder = { shell: 0, scaffolded: 1, built: 2, native: 3 };
      const sortedSystems = [...cat.systems].sort((a, b) => statusOrder[a.status] - statusOrder[b.status]);

      return `
        <div class="roadmap-category">
          <div class="roadmap-cat-header">
            <span class="roadmap-cat-name">
              <span class="chip-dot" style="background:${cat.color}"></span> ${cat.name}
            </span>
            <span class="roadmap-cat-pct">${built}/${cat.systems.length} · ${pct}%</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" style="width:${pct}%;background:${cat.color}"></div>
          </div>
          <div class="roadmap-systems">
            ${sortedSystems.map(s => {
              const c = STATUS_COLORS[s.status] || STATUS_COLORS.shell;
              return `
                <div class="roadmap-sys" data-id="${s.id}" onclick="selectSystem('${s.id}'); switchView('graph')">
                  <span class="roadmap-sys-dot" style="background:${c.bg}"></span>
                  <span class="roadmap-sys-name">${s.name}</span>
                  <span class="roadmap-sys-tech">${s.tech || ''}</span>
                  <span class="roadmap-sys-status" style="color:${c.border}">${c.label}</span>
                </div>`;
            }).join('')}
          </div>
        </div>`;
    }).join('')}
  `;
}

// ═══════════════════════════════════════════════════════
//  Dependency Matrix View — adjacency matrix of all visible systems
// ═══════════════════════════════════════════════════════

function renderMatrixView() {
  const scroll = document.getElementById('matrixScroll');
  const legend = document.getElementById('matrixLegend');
  if (!scroll || !legend) return;

  const visible = getVisibleSystems();
  if (visible.length === 0) { scroll.innerHTML = '<div class="roadmap-header"><span class="roadmap-sub">No systems match filters.</span></div>'; legend.innerHTML = ''; return; }

  // Build relationship lookup from system dependencies/integrates
  const visibleIds = new Set(visible.map(s => s.id));
  const relMap = {};
  for (const sys of visible) {
    relMap[sys.id] = { integrates: new Set(), depends: new Set(), connects: new Set() };
  }
  for (const sys of visible) {
    // Dependencies (sys depends on these)
    for (const depName of sys.dependencies || []) {
      const target = findSystemByNameOrId(depName, state.systemIndex);
      if (target && visibleIds.has(target.id)) {
        relMap[sys.id].depends.add(target.id);
      }
    }
    // Integrations (sys integrates with these)
    for (const intName of sys.integrates || []) {
      const target = findSystemByNameOrId(intName, state.systemIndex);
      if (target && visibleIds.has(target.id)) {
        relMap[sys.id].integrates.add(target.id);
      }
    }
  }

  // Sort by category then name
  const byCat = {};
  for (const sys of visible) {
    const cat = sys.categories?.[0] || 'other';
    if (!byCat[cat]) byCat[cat] = [];
    byCat[cat].push(sys);
  }
  const sorted = [];
  for (const [catId, systems] of Object.entries(byCat)) {
    systems.sort((a, b) => a.name.localeCompare(b.name));
    sorted.push(...systems);
  }

  const COLORS = {
    built: '#22c55e', native: '#3b82f6', scaffolded: '#f59e0b',
    shell: '#ef4444', planned: '#6b7280', unknown: '#475569'
  };

  function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }

  // Build table
  let html = '<div class="roadmap-header"><span class="roadmap-title">🔲 Dependency Matrix</span>';
  html += `<span class="roadmap-sub">${sorted.length} systems — hover cells to highlight · click to select</span></div>`;
  html += '<table class="matrix-table"><thead><tr><th></th>';

  // Column headers
  for (const sys of sorted) {
    const c = COLORS[sys.status] || COLORS.unknown;
    html += `<th><div class="col-label" title="${esc(sys.name)}" style="color:${c}" onclick="selectSystem('${esc(sys.id)}'); switchView('graph')">${esc(sys.name)}</div></th>`;
  }
  html += '</tr></thead><tbody>';

  // Rows
  for (let ri = 0; ri < sorted.length; ri++) {
    const rowSys = sorted[ri];
    const c = COLORS[rowSys.status] || COLORS.unknown;
    html += '<tr>';
    html += `<td><div class="row-label" title="${esc(rowSys.name)}" style="color:${c}" onclick="selectSystem('${esc(rowSys.id)}'); switchView('graph')">${esc(rowSys.name)}</div></td>`;

    for (let ci = 0; ci < sorted.length; ci++) {
      const colSys = sorted[ci];
      if (ri === ci) {
        html += `<td><div class="matrix-cell self" title="${esc(rowSys.name)}"></div></td>`;
      } else {
        const r = relMap[rowSys.id];
        const hasIntegrates = r.integrates.has(colSys.id);
        const hasDepends = r.depends.has(colSys.id);
        const hasConnects = r.connects.has(colSys.id);
        let cls = 'empty', title = 'No connection';
        if (hasIntegrates && hasDepends) { cls = 'both'; title = `${rowSys.name} ↔ ${colSys.name} (integrates + depends)`; }
        else if (hasIntegrates) { cls = 'integrates'; title = `${rowSys.name} integrates ${colSys.name}`; }
        else if (hasDepends) { cls = 'depends'; title = `${rowSys.name} depends on ${colSys.name}`; }
        else if (hasConnects) { cls = 'connects'; title = `${rowSys.name} ↔ ${colSys.name}`; }
        html += `<td><div class="matrix-cell ${cls}" title="${esc(title)}" onclick="selectSystem('${esc(colSys.id)}'); switchView('graph')"></div></td>`;
      }
    }
    html += '</tr>';
  }

  html += '</tbody></table>';
  scroll.innerHTML = html;

  legend.innerHTML = `
    <span><span class="dot" style="background:rgba(99,102,241,0.5)"></span> Integrates</span>
    <span><span class="dot" style="background:rgba(245,158,11,0.5)"></span> Depends on</span>
    <span><span class="dot" style="background:rgba(34,197,94,0.5)"></span> Connected</span>
    <span><span class="dot" style="background:rgba(236,72,153,0.5)"></span> Both</span>
    <span><span class="dot" style="background:rgba(255,255,255,0.04)"></span> Self</span>
    <span style="margin-left:auto">${sorted.length}×${sorted.length} matrix</span>`;
}

// ═══════════════════════════════════════════════════════
//  Hierarchy Tree View — Nexus-Cloud root → categories → systems
// ═══════════════════════════════════════════════════════

function renderTreeView() {
  const container = document.getElementById('treeWrap');
  if (!container) return;

  const visible = getVisibleSystems();
  const COLORS = {
    built: '#22c55e', native: '#3b82f6', scaffolded: '#f59e0b',
    shell: '#ef4444', planned: '#6b7280', unknown: '#475569'
  };
  const statusOrder = { native: 0, built: 1, scaffolded: 2, shell: 3, planned: 4, unknown: 5 };

  // Group by category
  const byCategory = new Map();
  for (const sys of visible) {
    const catId = sys.categories?.[0] || 'other';
    const catName = sys.categoryNames?.[0] || 'Other';
    const catColor = sys.categoryColors?.[0] || '#475569';
    if (!byCategory.has(catId)) {
      byCategory.set(catId, { id: catId, name: catName, color: catColor, systems: [] });
    }
    byCategory.get(catId).systems.push(sys);
  }

  const sortedCats = [...byCategory.values()].sort((a, b) => a.name.localeCompare(b.name));
  const totalSystems = visible.length;
  const builtCount = visible.filter(s => s.status === 'built' || s.status === 'native').length;

  function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;'); }

  let html = '';
  html += `<div class="roadmap-header">
    <span class="roadmap-title">🌳 Hierarchy Tree</span>
    <span class="roadmap-sub">Nexus-Cloud → ${sortedCats.length} categories → ${totalSystems} systems · ${builtCount} built/native</span>
  </div>`;

  // Root node
  html += `<div class="tree-root">`;
  html += `<div class="tree-node" style="border-left:2px solid #6366f1;padding:0">
    <div class="tree-node-header" style="padding:10px 16px;background:rgba(99,102,241,0.08);border-radius:8px;margin:4px 0">
      <span class="tree-toggle expanded" onclick="toggleTreeNode(event, this)">▶</span>
      <span class="tree-dot" style="background:linear-gradient(135deg,#6366f1,#14b8a6)"></span>
      <span class="tree-label" style="font-weight:700;font-size:15px;color:#a5b4fc">Nexus-Cloud</span>
      <span class="tree-tech" style="color:#6366f1">☁️ Platform Hub</span>
      <span class="tree-count">${totalSystems} apps</span>
    </div>
    <div class="tree-children" style="max-height:99999px">`;

  // Categories
  for (const cat of sortedCats) {
    const sysList = [...cat.systems].sort((a, b) => statusOrder[a.status] - statusOrder[b.status]);
    const catBuilt = sysList.filter(s => s.status === 'built' || s.status === 'native').length;

    html += `<div class="tree-category-label">
      <span class="cat-swatch" style="background:${cat.color}"></span>${esc(cat.name)}
      <span style="font-weight:400;font-size:9px;color:#475569">${catBuilt}/${sysList.length}</span>
    </div>`;

    html += `<div class="tree-node" style="border-left:2px solid ${cat.color}22">
      <div class="tree-node-header" onclick="toggleTreeNode(event, this)">
        <span class="tree-toggle expanded">▶</span>
        <span class="tree-dot" style="background:${cat.color}"></span>
        <span class="tree-label" style="font-weight:600">${esc(cat.name)}</span>
        <span class="tree-count">${sysList.length}</span>
      </div>
      <div class="tree-children" style="max-height:99999px">`;

    for (const sys of sysList) {
      const c = COLORS[sys.status] || COLORS.unknown;
      html += `<div class="tree-node">
        <div class="tree-node-header" data-id="${esc(sys.id)}" onclick="event.stopPropagation(); selectSystem('${esc(sys.id)}'); switchView('graph')">
          <span class="tree-toggle empty">▶</span>
          <span class="tree-dot" style="background:${c}"></span>
          <span class="tree-label">${esc(sys.name)}</span>
          <span class="tree-tech">${esc(sys.tech || '')}</span>
          <span class="tree-status" style="color:${c}">●</span>
        </div>
      </div>`;
    }

    html += `</div></div>`; // close category children + node
  }

  html += `</div></div></div>`; // close root children + root node + tree-root
  container.innerHTML = html;
}

function toggleTreeNode(event, header) {
  const toggle = header.querySelector('.tree-toggle');
  const children = header.parentElement.querySelector('.tree-children');
  if (children) {
    toggle.classList.toggle('expanded');
    children.classList.toggle('collapsed');
  }
}

// ═══════════════════════════════════════════════════════
//  View Switching
// ═══════════════════════════════════════════════════════

function switchView(mode) {
  state.viewMode = mode;
  document.getElementById('graphWrap').classList.toggle('hidden', mode !== 'graph');
  document.getElementById('listWrap').classList.toggle('hidden', mode !== 'list');
  document.getElementById('roadmapWrap').classList.toggle('hidden', mode !== 'roadmap');
  document.getElementById('treeWrap').classList.toggle('hidden', mode !== 'tree');
  document.getElementById('matrixWrap').classList.toggle('hidden', mode !== 'matrix');
  document.getElementById('btnGraphView').classList.toggle('active', mode === 'graph');
  document.getElementById('btnListView').classList.toggle('active', mode === 'list');
  document.getElementById('btnRoadmapView').classList.toggle('active', mode === 'roadmap');
  document.getElementById('btnTreeView').classList.toggle('active', mode === 'tree');
  document.getElementById('btnMatrixView').classList.toggle('active', mode === 'matrix');

  if (mode === 'list') renderListView(getVisibleSystems());
  if (mode === 'roadmap') renderRoadmapView();
  if (mode === 'tree') renderTreeView();
  if (mode === 'matrix') renderMatrixView();
  if (mode === 'graph' && network) {
    setTimeout(() => network.fit({ animation: { duration: 300 } }), 50);
  }
}

// ═══════════════════════════════════════════════════════
//  Controls
// ═══════════════════════════════════════════════════════

function buildControls() {
  // Status toggles
  const statusContainer = document.getElementById('statusFilters');
  for (const [status, colors] of Object.entries(STATUS_COLORS)) {
    const chip = document.createElement('label');
    chip.className = 'chip';
    chip.style.borderColor = colors.border;
    chip.innerHTML = `
      <span class="chip-dot" style="background:${colors.bg}"></span>
      <input type="checkbox" checked data-kind="status" data-value="${status}">
      <span>${colors.label}</span>`;
    chip.querySelector('input').addEventListener('change', (e) => {
      if (e.target.checked) state.visibleStatuses.add(status);
      else state.visibleStatuses.delete(status);
      applyFilters();
    });
    statusContainer.appendChild(chip);
  }

  // Category toggles
  const catContainer = document.getElementById('categoryFilters');
  for (const cat of state.categoryList) {
    const chip = document.createElement('label');
    chip.className = 'chip';
    chip.style.borderColor = cat.color + '66';
    chip.innerHTML = `
      <span class="chip-dot" style="background:${cat.color}"></span>
      <input type="checkbox" data-kind="category" data-value="${cat.id}">
      <span>${cat.name}</span>`;
    chip.querySelector('input').addEventListener('change', (e) => {
      if (e.target.checked) state.visibleCategories.add(cat.id);
      else state.visibleCategories.delete(cat.id);
      applyFilters();
    });
    catContainer.appendChild(chip);
  }

  // Search with debounce (250ms)
  document.getElementById('searchInput').addEventListener('input', (e) => {
    state.search = e.target.value.trim();
    if (state.searchDebounceTimer) clearTimeout(state.searchDebounceTimer);
    state.searchDebounceTimer = setTimeout(() => applyFilters(), 250);
  });

  // Reset
  document.getElementById('resetFilters').addEventListener('click', () => {
    state.visibleStatuses = new Set(Object.keys(STATUS_COLORS));
    state.visibleCategories = new Set();
    state.search = '';
    state.focusMode = false;
    state.focusRootId = null;
    document.getElementById('focusBanner').classList.remove('visible');
    document.getElementById('searchInput').value = '';
    document.querySelectorAll('#statusFilters input, #categoryFilters input').forEach(i => i.checked = true);
    window.location.hash = '';
    applyFilters();
    renderDetail(null);
  });

  // View toggles
  document.getElementById('btnGraphView').addEventListener('click', () => switchView('graph'));
  document.getElementById('btnListView').addEventListener('click', () => switchView('list'));
  document.getElementById('btnRoadmapView').addEventListener('click', () => switchView('roadmap'));
  document.getElementById('btnTreeView').addEventListener('click', () => switchView('tree'));
  document.getElementById('btnMatrixView').addEventListener('click', () => switchView('matrix'));

  // Focus banner
  document.getElementById('exitFocus').addEventListener('click', exitFocusMode);

  // Help overlay
  document.getElementById('btnHelp').addEventListener('click', toggleHelp);
  document.getElementById('helpOverlay').addEventListener('click', (e) => {
    if (e.target === document.getElementById('helpOverlay')) toggleHelp();
  });

  // Keyboard shortcuts
  document.addEventListener('keydown', (e) => {
    // Don't capture when typing in input
    if (e.target.tagName === 'INPUT') return;

    // ? toggles help
    if (e.key === '?') {
      e.preventDefault();
      toggleHelp();
      return;
    }

    if (e.key === 'Escape') {
      // Close help first if open
      if (document.getElementById('helpOverlay').classList.contains('visible')) {
        toggleHelp();
        return;
      }
      if (state.focusMode) { exitFocusMode(); return; }
      renderDetail(null);
      state.selectedSystemId = null;
      window.location.hash = '';
      if (network) network.unselectAll();
    }

    if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
      e.preventDefault();
      document.getElementById('searchInput').focus();
    }

    // Arrow key navigation between connected nodes
    if (state.selectedSystemId && network && ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.key)) {
      e.preventDefault();
      const neighbors = _getConnectedNodes(state.selectedSystemId);
      if (neighbors.length > 0) {
        const idx = (e.key === 'ArrowRight' || e.key === 'ArrowDown') ? 0 : neighbors.length - 1;
        selectSystem(neighbors[idx]);
      }
    }

    // Enter to focus on selected
    if (e.key === 'Enter' && state.selectedSystemId && !state.focusMode) {
      e.preventDefault();
      enterFocusMode(state.selectedSystemId);
    }

    // Tab to cycle through visible nodes
    if (e.key === 'Tab' && !e.shiftKey) {
      e.preventDefault();
      const visible = getVisibleSystems();
      if (visible.length === 0) return;
      const currentIdx = visible.findIndex(s => s.id === state.selectedSystemId);
      const nextIdx = (currentIdx + 1) % visible.length;
      selectSystem(visible[nextIdx].id);
    }
  });
}

function _getConnectedNodes(sysId) {
  const sys = state.systemIndex.get(sysId);
  if (!sys) return [];

  const ids = new Set();

  for (const dep of sys.dependencies || []) {
    const t = findSystemByNameOrId(dep, state.systemIndex);
    if (t) ids.add(t.id);
  }
  for (const int of sys.integrates || []) {
    const t = findSystemByNameOrId(int, state.systemIndex);
    if (t) ids.add(t.id);
  }
  for (const rev of state.reverseDeps.get(sysId) || []) {
    ids.add(rev.id);
  }

  return [...ids];
}

// ═══════════════════════════════════════════════════════
//  Help Overlay
// ═══════════════════════════════════════════════════════

function toggleHelp() {
  const overlay = document.getElementById('helpOverlay');
  overlay.classList.toggle('visible');
}

// ═══════════════════════════════════════════════════════
//  Ecosystem-Graph Integration
// ═══════════════════════════════════════════════════════

async function loadEcosystemGraph() {
  try {
    const resp = await fetch('../ecosystem-graph/graph.json');
    if (resp.ok) {
      state.graphData = await resp.json();
      const docSystems = state.graphData.nodes.filter(n => n.type === 'system');
      document.getElementById('graphStatus').textContent =
        docSystems.length > 0
          ? `📄 ${docSystems.length} documented`
          : '📄 0 documented';
      document.getElementById('graphStatus').style.color =
        docSystems.length > 0 ? '#34d399' : '#f59e0b';
    } else {
      document.getElementById('graphStatus').textContent = '📄 no graph.json';
    }
  } catch {
    document.getElementById('graphStatus').textContent = '📄 unavailable';
  }
}

// ═══════════════════════════════════════════════════════
//  Init
// ═══════════════════════════════════════════════════════

function initStats() {
  let native = 0, built = 0, scaffolded = 0, shell = 0;
  const orphans = []; // systems with no dependencies and nothing depends on them
  const hubs = [];    // systems with most total connections

  for (const [id, sys] of state.systemIndex) {
    switch (sys.status) { case 'native': native++; break; case 'built': built++; break; case 'scaffolded': scaffolded++; break; case 'shell': shell++; break; }

    const depCount = sys.dependencies?.length || 0;
    const intCount = sys.integrates?.length || 0;
    const revCount = (state.reverseDeps.get(id) || []).length;
    const total = depCount + intCount + revCount;

    if (total === 0) orphans.push(sys.name);
    hubs.push({ name: sys.name, id, total, status: sys.status });
  }

  document.getElementById('nativeCount').textContent = native;
  document.getElementById('builtCount').textContent = built;
  document.getElementById('scaffoldedCount').textContent = scaffolded;
  document.getElementById('shellCount').textContent = shell;
  document.getElementById('totalCount').textContent = state.systemIndex.size;

  // Top hubs
  hubs.sort((a, b) => b.total - a.total);
  const topHubs = hubs.slice(0, 5).map(h => {
    const c = STATUS_COLORS[h.status] || STATUS_COLORS.shell;
    return `<span class="stat-mini" style="color:${c.border};cursor:pointer" onclick="selectSystem('${h.id}')" title="${h.total} connections">${h.name}</span>`;
  }).join(' · ');

  document.getElementById('insightsData').innerHTML = `
    <div class="insight-row"><span class="insight-label">🔗 Top Hub</span> <span>${topHubs || '—'}</span></div>
    <div class="insight-row"><span class="insight-label">🕳️ Orphans</span> <span>${orphans.length} systems</span></div>
    <div class="insight-row"><span class="insight-label">📊 Completion</span> <span>${Math.round(((native + built) / state.systemIndex.size) * 100)}% built</span></div>
  `;
}

function initNetwork() {
  const container = document.getElementById('graph');
  nodesDataSet = new vis.DataSet([]);
  edgesDataSet = new vis.DataSet([]);

  network = new vis.Network(
    container,
    { nodes: nodesDataSet, edges: edgesDataSet },
    {
      autoResize: true,
      interaction: {
        hover: true,
        tooltipDelay: 200,
        navigationButtons: true,
        keyboard: { enabled: true, bindToWindow: false },
        zoomView: true,
        dragView: true
      },
      physics: {
        enabled: true,
        solver: 'forceAtlas2Based',
        forceAtlas2Based: {
          gravitationalConstant: -50,
          centralGravity: 0.01,
          springLength: 180,
          springConstant: 0.08,
          damping: 0.4,
          avoidOverlap: 0.5
        },
        stabilization: { iterations: 200, fit: true }
      },
      layout: { improvedLayout: true },
      edges: {
        smooth: { type: 'continuous' },
        width: 0.8,
        selectionWidth: 2
      },
      groups: {}
    }
  );

  network.on('click', (params) => {
    if (params.nodes.length > 0) {
      selectSystem(params.nodes[0]);
    }
  });

  network.on('doubleClick', (params) => {
    if (params.nodes.length > 0) {
      enterFocusMode(params.nodes[0]);
    }
  });

  network.on('stabilizationIterationsDone', () => {
    network.fit({ animation: { duration: 400, easingFunction: 'easeInOutQuad' } });
  });

  // Handle window resize
  window.addEventListener('resize', () => {
    if (network) network.redraw();
  });
}

function boot() {
  if (typeof EMBEDDED_DATA === 'undefined') {
    console.error('EMBEDDED_DATA not found — data not baked into HTML?');
    return;
  }

  state.data = EMBEDDED_DATA;

  // Build index
  state.systemIndex = buildSystemIndex(state.data);
  state.reverseDeps = buildReverseDeps(state.systemIndex);

  // Collect categories
  const catMap = new Map();
  for (const eco of state.data.ecosystems) {
    for (const cat of eco.categories) {
      catMap.set(cat.id, { id: cat.id, name: cat.name, color: cat.color });
    }
  }
  state.categoryList = [...catMap.values()];

  // Init
  initStats();
  initNetwork();
  buildControls();
  applyFilters();
  loadEcosystemGraph();

  // Apply URL hash after everything is ready
  setTimeout(applyHash, 600);

  // Listen for hash changes
  window.addEventListener('hashchange', applyHash);
}

// Start when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', boot);
} else {
  boot();
}
