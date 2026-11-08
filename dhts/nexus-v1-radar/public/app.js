let currentData = null;
let activeCategory = "all";
let searchFilter = "";

async function fetchAudit() {
  const grid = document.getElementById("appsGrid");
  grid.innerHTML = '<div class="loading-state">Scanning platform services and checking live ports...</div>';

  try {
    const res = await fetch("/api/summary");
    if (!res.ok) throw new Error("Failed to load audit summary");
    currentData = await res.json();
    renderAll();
  } catch (err) {
    grid.innerHTML = `<div class="loading-state" style="color: #f87171;">Error loading audit data: ${err.message}</div>`;
  }
}

function renderAll() {
  if (!currentData) return;

  // 1. KPI Stats
  document.getElementById("statTotalApps").textContent = currentData.totalApps;
  document.getElementById("statAvgScore").textContent = `${currentData.averageScore}%`;

  let avgStage = "Beta Stage";
  if (currentData.averageScore >= 80) avgStage = "Release Candidate";
  else if (currentData.averageScore < 60) avgStage = "Alpha Stage";
  document.getElementById("statAvgStage").textContent = `Ecosystem Overall • ${avgStage}`;

  // Breakdown badges
  const b = currentData.stagesCount;
  const breakdownEl = document.getElementById("statBreakdown");
  breakdownEl.innerHTML = `
    <span class="breakdown-badge" style="color: var(--cyan)">Alpha: ${b.alpha}</span>
    <span class="breakdown-badge" style="color: var(--yellow)">Beta: ${b.beta}</span>
    <span class="breakdown-badge" style="color: var(--green)">RC: ${b.rc}</span>
    <span class="breakdown-badge" style="color: var(--acid)">v1.0: ${b.v1}</span>
  `;

  document.getElementById("lastScannedAt").textContent = `Last Scan: ${new Date(currentData.scannedAt).toLocaleTimeString()}`;

  // 2. Render Apps Grid
  renderGrid();
}

function renderGrid() {
  const grid = document.getElementById("appsGrid");
  if (!currentData) return;

  const filtered = currentData.results.filter((item) => {
    const matchesCat = activeCategory === "all" || item.app.category === activeCategory;
    const matchesSearch =
      !searchFilter ||
      item.app.name.toLowerCase().includes(searchFilter) ||
      item.app.id.toLowerCase().includes(searchFilter) ||
      item.app.port.toString().includes(searchFilter) ||
      item.app.category.toLowerCase().includes(searchFilter);
    return matchesCat && matchesSearch;
  });

  if (filtered.length === 0) {
    grid.innerHTML = '<div class="loading-state">No platform services match your search filter.</div>';
    return;
  }

  grid.innerHTML = filtered
    .map((item) => {
      const stageClass = "stage-" + item.stage.replace(/\s+/g, "");
      const topMissing = item.missingFeatures[0]?.name || "Polish & testing";

      return `
      <div class="app-card" onclick="openModal('${item.app.id}')">
        <div>
          <div class="app-card-top">
            <div>
              <div class="app-card-title">${item.app.name}</div>
              <div class="app-card-meta">:${item.app.port} • ${item.app.category.toUpperCase()} • ${item.live.portListening ? "● ONLINE" : "○ OFFLINE"}</div>
            </div>
            <span class="stage-badge ${stageClass}">${item.stage}</span>
          </div>
          <p class="app-card-desc">${item.app.description}</p>
        </div>

        <div class="progress-section">
          <div class="progress-header">
            <span>v1.0 READINESS</span>
            <span><strong>${item.totalScore}%</strong></span>
          </div>
          <div class="progress-bar-bg">
            <div class="progress-bar-fill" style="width: ${item.totalScore}%"></div>
          </div>
          <div class="app-card-features">
            <span>Features: ${item.completedFeatures.length}/${item.app.features.length}</span>
            <span class="missing-pill" title="Next: ${topMissing}">Next: ${topMissing}</span>
          </div>
        </div>
      </div>
    `;
    })
    .join("");
}

window.openModal = function (appId) {
  if (!currentData) return;
  const item = currentData.results.find((r) => r.app.id === appId);
  if (!item) return;

  document.getElementById("modalCategory").textContent = item.app.category.toUpperCase();
  document.getElementById("modalTitle").textContent = item.app.name;
  document.getElementById("modalSub").textContent = `${item.app.publicUrl} (Port :${item.app.port})`;

  const modalBody = document.getElementById("modalBody");
  modalBody.innerHTML = `
    <!-- Top Readiness Metric -->
    <div style="background: rgba(226, 248, 78, 0.05); border: 1px solid rgba(226, 248, 78, 0.25); padding: 18px; margin-bottom: 24px;">
      <div style="display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 8px;">
        <span style="font-family: var(--font-mono); font-size: 11px; text-transform: uppercase; color: var(--text-dim);">Current Maturity Stage</span>
        <span style="font-family: var(--font-mono); font-size: 13px; font-weight: 700; color: var(--acid);">${item.stage} (${item.totalScore}%)</span>
      </div>
      <div class="progress-bar-bg" style="height: 8px;">
        <div class="progress-bar-fill" style="width: ${item.totalScore}%"></div>
      </div>
      <div style="display: flex; justify-content: space-between; font-family: var(--font-mono); font-size: 10px; color: var(--text-muted); margin-top: 8px;">
        <span>Port :${item.app.port} ${item.live.portListening ? "● Listening" : "○ Down"}</span>
        <span>HTTP Health: ${item.live.httpHealthy ? "● 200 OK" : "○ Offline"}</span>
      </div>
    </div>

    <!-- 6 Pillar Breakdown -->
    <div class="modal-section-title">Pillar Score Breakdown</div>
    <div class="pillars-grid">
      <div class="pillar-box">
        <div class="pillar-box-label">Data & Persistence (20 pts)</div>
        <div class="pillar-box-score">${item.pillars.data.score}/20 <span style="font-size: 11px; font-weight: 400; color: var(--text-muted);">(${item.pillars.data.label})</span></div>
      </div>
      <div class="pillar-box">
        <div class="pillar-box-label">UI & UX (25 pts)</div>
        <div class="pillar-box-score">${item.pillars.ui.score}/25 <span style="font-size: 11px; font-weight: 400; color: var(--text-muted);">(${item.pillars.ui.label})</span></div>
      </div>
      <div class="pillar-box">
        <div class="pillar-box-label">Core Features (25 pts)</div>
        <div class="pillar-box-score">${item.pillars.features.score}/25 <span style="font-size: 11px; font-weight: 400; color: var(--text-muted);">(${item.pillars.features.label})</span></div>
      </div>
      <div class="pillar-box">
        <div class="pillar-box-label">API & Protocols (10 pts)</div>
        <div class="pillar-box-score">${item.pillars.api.score}/10 <span style="font-size: 11px; font-weight: 400; color: var(--text-muted);">(${item.pillars.api.label})</span></div>
      </div>
      <div class="pillar-box">
        <div class="pillar-box-label">Quality & Tests (10 pts)</div>
        <div class="pillar-box-score">${item.pillars.quality.score}/10 <span style="font-size: 11px; font-weight: 400; color: var(--text-muted);">(${item.pillars.quality.label})</span></div>
      </div>
      <div class="pillar-box">
        <div class="pillar-box-label">Platform Gate (10 pts)</div>
        <div class="pillar-box-score">${item.pillars.platform.score}/10 <span style="font-size: 11px; font-weight: 400; color: var(--text-muted);">(${item.pillars.platform.label})</span></div>
      </div>
    </div>

    <!-- v1.0 Feature Checklist -->
    <div class="modal-section-title">v1.0 Feature Checklist (${item.completedFeatures.length}/${item.app.features.length} Done)</div>
    <div class="checklist-group">
      ${item.completedFeatures
        .map(
          (f) => `
        <div class="checklist-item">
          <span class="checklist-icon done">✔</span>
          <div class="checklist-content">
            <h4>${f.name}</h4>
            <p>${f.description}</p>
          </div>
        </div>
      `
        )
        .join("")}

      ${item.missingFeatures
        .map(
          (f) => `
        <div class="checklist-item">
          <span class="checklist-icon todo">✖</span>
          <div class="checklist-content">
            <h4>${f.name}</h4>
            <p>${f.description}</p>
          </div>
        </div>
      `
        )
        .join("")}
    </div>

    <!-- Actionable Roadmap -->
    <div class="modal-section-title">Priority Roadmap to Reach v1.0</div>
    <ol class="roadmap-list">
      ${item.app.roadmap
        .map(
          (step) => `
        <li class="roadmap-item">
          <span>${step}</span>
        </li>
      `
        )
        .join("")}
    </ol>
  `;

  document.getElementById("modalBackdrop").classList.add("open");
};

function closeModal() {
  document.getElementById("modalBackdrop").classList.remove("open");
}

// Event Listeners
document.getElementById("modalClose").addEventListener("click", closeModal);
document.getElementById("modalBackdrop").addEventListener("click", (e) => {
  if (e.target.id === "modalBackdrop") closeModal();
});
document.addEventListener("keydown", (e) => {
  if (e.key === "Escape") closeModal();
});

document.getElementById("rescanBtn").addEventListener("click", fetchAudit);

document.getElementById("searchInput").addEventListener("input", (e) => {
  searchFilter = e.target.value.toLowerCase().trim();
  renderGrid();
});

document.querySelectorAll("#categoryFilter .pill").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll("#categoryFilter .pill").forEach((p) => p.classList.remove("active"));
    btn.classList.add("active");
    activeCategory = btn.getAttribute("data-category");
    renderGrid();
  });
});

// Initial load
fetchAudit();
