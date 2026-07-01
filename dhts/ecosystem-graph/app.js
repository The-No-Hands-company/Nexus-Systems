const NODE_TYPE_ORDER = ["system", "capability", "surface", "document"];
const RELATION_ORDER = ["depends_on", "integrates_with", "owns", "exposes", "documented_by"];

const TYPE_COLOR = {
  system: "#5cc9ff",
  capability: "#87f0ba",
  surface: "#f7d67b",
  document: "#cfb6ff",
};

const RELATION_COLOR = {
  depends_on: "#ff9f6d",
  integrates_with: "#8fd8ff",
  owns: "#8fffd0",
  exposes: "#ffe190",
  documented_by: "#d5c2ff",
};

const state = {
  graph: null,
  visibleNodeTypes: new Set(NODE_TYPE_ORDER),
  visibleRelations: new Set(RELATION_ORDER),
  search: "",
  searchTimer: null,
};

let nodesDataSet;
let edgesDataSet;
let network;

function mkNode(n) {
  return {
    id: n.id,
    label: n.label,
    group: n.type,
    color: TYPE_COLOR[n.type] || "#bbbbbb",
    font: { color: "#eaf7f2", size: 14 },
    shape: n.type === "document" ? "box" : "dot",
    size: n.type === "system" ? 18 : 12,
    data: n,
  };
}

function mkEdge(e) {
  return {
    id: e.id,
    from: e.source,
    to: e.target,
    label: e.relation,
    color: RELATION_COLOR[e.relation] || "#9aa",
    arrows: "to",
    font: { color: "#b6cdc5", size: 10, align: "top" },
    smooth: { type: "dynamic" },
    data: e,
  };
}

function setEmptyState(message, visible) {
  const emptyState = document.getElementById("emptyState");
  emptyState.textContent = message;
  emptyState.classList.toggle("visible", visible);
}

function renderSelection(node) {
  document.getElementById("selectedLabel").textContent = node?.label || "Select a node";
  document.getElementById("selectedType").textContent = node?.type || "-";

  const sources = node?.sources?.length ? node.sources.join(", ") : "-";
  document.getElementById("selectedSources").textContent = sources;

  const links = document.getElementById("docLinks");
  links.innerHTML = "";
  if (!node?.sources?.length) {
    return;
  }

  node.sources.forEach((source) => {
    const link = document.createElement("a");
    link.className = "doc-link";
    link.href = `/${source}`;
    link.target = "_blank";
    link.rel = "noopener noreferrer";
    link.textContent = `Open ${source}`;
    links.appendChild(link);
  });
}

function normalizeSearch(value) {
  return value.trim().toLowerCase();
}

function nodeMatchesSearch(node, searchText) {
  if (!searchText) {
    return true;
  }

  return (
    node.label.toLowerCase().includes(searchText) ||
    node.id.toLowerCase().includes(searchText) ||
    node.sources.some((source) => source.toLowerCase().includes(searchText))
  );
}

function applyFilters() {
  if (!state.graph) {
    return;
  }

  const filteredBaseNodes = state.graph.nodes.filter(
    (node) => state.visibleNodeTypes.has(node.type) && nodeMatchesSearch(node, state.search),
  );
  const visibleNodeIds = new Set(filteredBaseNodes.map((node) => node.id));

  const filteredEdges = state.graph.edges.filter(
    (edge) =>
      state.visibleRelations.has(edge.relation) &&
      visibleNodeIds.has(edge.source) &&
      visibleNodeIds.has(edge.target),
  );

  nodesDataSet.clear();
  edgesDataSet.clear();
  nodesDataSet.add(filteredBaseNodes.map(mkNode));
  edgesDataSet.add(filteredEdges.map(mkEdge));

  document.getElementById("counts").textContent = `Nodes: ${filteredBaseNodes.length} | Edges: ${filteredEdges.length}`;

  if (filteredBaseNodes.length === 0) {
    setEmptyState("No nodes match the current filter set.", true);
  } else {
    setEmptyState("", false);
    network.fit({ animation: { duration: 220, easingFunction: "easeInOutQuad" } });
  }
}

function createToggleChip(kind, value, checked, onChange) {
  const label = document.createElement("label");
  label.className = "chip";

  const input = document.createElement("input");
  input.type = "checkbox";
  input.checked = checked;
  input.dataset.kind = kind;
  input.dataset.value = value;
  input.addEventListener("change", onChange);

  const text = document.createElement("span");
  text.textContent = value;

  label.appendChild(input);
  label.appendChild(text);
  return label;
}

function buildControls() {
  const nodeTypeFilters = document.getElementById("nodeTypeFilters");
  const relationFilters = document.getElementById("relationFilters");

  NODE_TYPE_ORDER.forEach((nodeType) => {
    nodeTypeFilters.appendChild(
      createToggleChip("nodeType", nodeType, true, (event) => {
        if (event.target.checked) {
          state.visibleNodeTypes.add(nodeType);
        } else {
          state.visibleNodeTypes.delete(nodeType);
        }
        applyFilters();
      }),
    );
  });

  RELATION_ORDER.forEach((relation) => {
    relationFilters.appendChild(
      createToggleChip("relation", relation, true, (event) => {
        if (event.target.checked) {
          state.visibleRelations.add(relation);
        } else {
          state.visibleRelations.delete(relation);
        }
        applyFilters();
      }),
    );
  });

  const searchInput = document.getElementById("searchInput");
  searchInput.addEventListener("input", (event) => {
    state.search = normalizeSearch(event.target.value);
    if (state.searchTimer) clearTimeout(state.searchTimer);
    state.searchTimer = setTimeout(() => applyFilters(), 200);
  });

  document.getElementById("resetFilters").addEventListener("click", () => {
    state.visibleNodeTypes = new Set(NODE_TYPE_ORDER);
    state.visibleRelations = new Set(RELATION_ORDER);
    state.search = "";

    document.querySelectorAll("#nodeTypeFilters input, #relationFilters input").forEach((input) => {
      input.checked = true;
    });
    searchInput.value = "";

    applyFilters();
    renderSelection(null);
  });
}

function assertGraphContract(graph) {
  const topLevelKeys = ["schemaVersion", "generatedAt", "nodeCount", "edgeCount", "nodes", "edges"];
  for (const key of topLevelKeys) {
    if (!(key in graph)) {
      throw new Error(`graph.json missing required key '${key}'`);
    }
  }

  if (graph.schemaVersion !== "1.0.0") {
    throw new Error(`Unsupported graph schemaVersion '${graph.schemaVersion}'`);
  }

  if (!Array.isArray(graph.nodes) || !Array.isArray(graph.edges)) {
    throw new Error("graph.json has invalid node or edge arrays");
  }
}

async function loadGraph() {
  const response = await fetch("graph.json", { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`Failed to load graph.json (${response.status})`);
  }

  const graph = await response.json();
  assertGraphContract(graph);
  return graph;
}

function initNetwork(graph) {
  const container = document.getElementById("graph");
  nodesDataSet = new vis.DataSet(graph.nodes.map(mkNode));
  edgesDataSet = new vis.DataSet(graph.edges.map(mkEdge));

  network = new vis.Network(
    container,
    { nodes: nodesDataSet, edges: edgesDataSet },
    {
      autoResize: true,
      interaction: {
        hover: true,
        navigationButtons: true,
        keyboard: true,
      },
      physics: {
        enabled: true,
        stabilization: { iterations: 300 },
        barnesHut: {
          gravitationalConstant: -9000,
          springLength: 120,
          springConstant: 0.02,
          damping: 0.14,
        },
      },
      edges: {
        width: 1.2,
        selectionWidth: 2.4,
      },
    },
  );

  network.on("click", (params) => {
    if (!params.nodes.length) {
      renderSelection(null);
      return;
    }

    const id = params.nodes[0];
    const node = nodesDataSet.get(id)?.data;
    renderSelection(node || null);
  });
}

async function boot() {
  try {
    buildControls();

    state.graph = await loadGraph();
    document.getElementById("generatedAt").textContent = `Generated: ${state.graph.generatedAt}`;
    document.getElementById("counts").textContent = `Nodes: ${state.graph.nodeCount} | Edges: ${state.graph.edgeCount}`;

    initNetwork(state.graph);
    applyFilters();
  } catch (error) {
    renderSelection(null);
    setEmptyState(`Error loading graph: ${error.message}`, true);
    document.getElementById("selectedLabel").textContent = `Error: ${error.message}`;
    console.error(error);
  }
}

boot();
