let currentData = null;
let activeCategory = "all";
let searchFilter = "";
let isFetching = false;

async function fetchAudit(silent = false) {
  if (isFetching) return;
  isFetching = true;
  const grid = document.getElementById("appsGrid");
  if (!silent && !currentData) {
    grid.innerHTML = '<div class="loading-state">Probing ecosystem daemons & public endpoints in real-time...</div>';
  }

  try {
    const res = await fetch("/api/summary");
    if (!res.ok) throw new Error("Failed to load live audit summary");
    currentData = await res.json();
    renderAll();
  } catch (err) {
    if (!silent) {
      grid.innerHTML = `<div class="loading-state" style="color: #f87171;">Error loading live audit data: ${err.message}</div>`;
    }
  } finally {
    isFetching = false;
  }
}

function renderAll() {
  if (!currentData) return;

  // 1. KPI Stats
  document.getElementById("statTotalApps").textContent = currentData.totalApps;

  const healthRatioEl = document.getElementById("statHealthRatio");
  const healthSubEl = document.getElementById("statHealthSub");
  if (healthRatioEl) {
    healthRatioEl.textContent = `${currentData.operationalApps}/${currentData.totalApps} Live`;
    healthRatioEl.style.color = currentData.degradedApps === 0 ? "#4ade80" : "#ef4444";
  }
  if (healthSubEl) {
    healthSubEl.textContent =
      currentData.degradedApps === 0
        ? "All services healthy & reachable"
        : `${currentData.degradedApps} service(s) offline or 502`;
  }

  document.getElementById("statAvgScore").textContent = `${currentData.averageScore}%`;

  let avgStage = "Beta Stage";
  if (currentData.averageScore >= 80) avgStage = "Release Candidate";
  else if (currentData.averageScore < 60) avgStage = "Alpha Stage";
  document.getElementById("statAvgStage").textContent = `Ecosystem Overall • ${avgStage}`;

  // Breakdown badges
  const b = currentData.stagesCount;
  const breakdownEl = document.getElementById("statBreakdown");
  breakdownEl.innerHTML = `
    ${b.degraded > 0 ? `<span class="breakdown-badge" style="color: #ef4444; font-weight: 700; border: 1px solid #ef4444; padding: 2px 6px;">Degraded: ${b.degraded}</span>` : ""}
    <span class="breakdown-badge" style="color: var(--cyan)">Alpha: ${b.alpha}</span>
    <span class="breakdown-badge" style="color: var(--yellow)">Beta: ${b.beta}</span>
    <span class="breakdown-badge" style="color: var(--green)">RC: ${b.rc}</span>
    <span class="breakdown-badge" style="color: var(--acid)">v1.0: ${b.v1}</span>
  `;

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
      const stageClass = "stage-" + item.stage.replace(/[^a-zA-Z0-9]/g, "");
      const topMissing = item.live.degradedReason || item.missingFeatures[0]?.name || "Polish & testing";

      const livePill = item.live.isOperational
        ? `<span class="live-pill live-pill-up">🟢 ${item.live.publicStatusText} (${item.live.publicLatencyMs}ms)</span>`
        : `<span class="live-pill live-pill-down">🔴 ${item.live.degradedReason || "OFFLINE / 502"}</span>`;

      return `
      <div class="app-card ${!item.live.isOperational ? "app-card-degraded" : ""}" onclick="openModal('${item.app.id}')">
        <div>
          <div class="app-card-top">
            <div>
              <div class="app-card-title">${item.app.name}</div>
              <div class="app-card-meta">:${item.app.port} • ${item.app.category.toUpperCase()}</div>
            </div>
            <span class="stage-badge ${stageClass}">${item.stage}</span>
          </div>
          <div style="margin: 6px 0 10px;">
            ${livePill}
          </div>
          <p class="app-card-desc">${item.app.description}</p>
        </div>

        <div class="progress-section">
          <div class="progress-header">
            <span>v1.0 READINESS</span>
            <span><strong>${item.totalScore}%</strong> ${item.rawScore !== item.totalScore ? `<small style="color: #ef4444;">(Capped from ${item.rawScore}%)</small>` : ""}</span>
          </div>
          <div class="progress-bar-bg">
            <div class="progress-bar-fill" style="width: ${item.totalScore}%; background: ${!item.live.isOperational ? "#ef4444" : ""}"></div>
          </div>
          <div class="app-card-features">
            <span>Features: ${item.completedFeatures.length}/${item.app.features.length}</span>
            <span class="missing-pill" title="${topMissing}">${topMissing.slice(0, 32)}</span>
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
        <span style="font-family: var(--font-mono); font-size: 11px; text-transform: uppercase; color: var(--text-dim);">Live Ground-Truth Maturity</span>
        <span style="font-family: var(--font-mono); font-size: 13px; font-weight: 700; color: ${item.live.isOperational ? "var(--acid)" : "#ef4444"};">${item.stage} (${item.totalScore}%)</span>
      </div>
      <div class="progress-bar-bg" style="height: 8px;">
        <div class="progress-bar-fill" style="width: ${item.totalScore}%; background: ${!item.live.isOperational ? "#ef4444" : ""}"></div>
      </div>
      ${
        item.live.degradedReason
          ? `<div style="background: rgba(239,68,68,0.15); border: 1px solid rgba(239,68,68,0.4); padding: 8px 12px; margin-top: 12px; font-family: var(--font-mono); font-size: 11px; color: #fca5a5;">
              ⚠️ <strong>DEGRADED RULE APPLIED:</strong> ${item.live.degradedReason}. Score hard-capped at ${item.totalScore}% (raw score: ${item.rawScore}%).
            </div>`
          : ""
      }
      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px; font-family: var(--font-mono); font-size: 11px; color: var(--text-muted); margin-top: 12px; border-top: 1px solid var(--border-subtle); padding-top: 10px;">
        <div>Local Daemon :${item.app.port}: <strong style="color: ${item.live.portListening ? "#4ade80" : "#ef4444"};">${item.live.portListening ? "● Listening" : "○ Down"}</strong> (${item.live.localLatencyMs}ms)</div>
        <div>Public Gateway: <strong style="color: ${item.live.publicReachable ? "#4ade80" : "#ef4444"};">${item.live.publicStatusText}</strong> (${item.live.publicLatencyMs}ms)</div>
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
        <div class="pillar-box-label">Runtime Gateway (10 pts)</div>
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

document.getElementById("rescanBtn").addEventListener("click", () => fetchAudit(false));

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

// Initial load + 5-second automatic ground-truth polling
fetchAudit(false);
setInterval(() => {
  fetchAudit(true);
}, 5000);
