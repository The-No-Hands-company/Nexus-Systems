/* ==========================================================================
   Nexus Systems Vision Board — Blueprint Core Logic
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  // Elements
  const viewport = document.getElementById('viewport');
  const canvas = document.getElementById('canvas');
  const svgLayer = document.getElementById('svg-layer');
  const connectionsGroup = document.getElementById('connections-group');
  const rerouteLinesGroup = document.getElementById('reroute-lines-group');
  const nodesLayer = document.getElementById('nodes-layer');
  const commentsLayer = document.getElementById('comment-boxes-layer');
  const rerouteLayer = document.getElementById('reroute-nodes-layer');
  const tempCable = document.getElementById('temp-cable');
  
  // Toolbar Buttons
  const btnSave = document.getElementById('btn-save');
  const btnAutoArrange = document.getElementById('btn-auto-arrange');
  const btnAddComment = document.getElementById('btn-add-comment');
  const btnHelp = document.getElementById('btn-help');
  const btnCloseHelp = document.getElementById('btn-close-modal');
  const helpModal = document.getElementById('help-modal');
  const searchInput = document.getElementById('search-input');
  
  // Sidebar elements
  const libraryApps = document.getElementById('library-apps');
  const librarySearch = document.getElementById('library-search');
  const libraryCount = document.getElementById('library-count');
  const categoryChips = document.getElementById('category-chips');
  const sidebarRight = document.getElementById('sidebar-right');
  const configContent = document.getElementById('config-content');
  const btnCloseRight = document.getElementById('btn-close-right');
  
  // Zoom displays
  const zoomPercentage = document.getElementById('zoom-percentage');
  const btnZoomIn = document.getElementById('zoom-in');
  const btnZoomOut = document.getElementById('zoom-out');
  const btnZoomReset = document.getElementById('zoom-reset');

  // Application State
  let ecosystemData = {};
  let systemsMap = new Map(); // id -> system details
  let categories = [];
  
  // Layout State (Saved to disk)
  let layout = {
    nodePositions: {},     // id -> { x, y }
    customConnections: [],  // [ { from, to, type } ]
    hiddenConnections: [],  // [ "fromId->toId" ]
    commentBoxes: [],       // [ { id, title, x, y, w, h, color } ]
    rerouteNodes: []        // [ { id, x, y, from, to } ]
  };

  // Viewport State
  let panX = 4000; // Center inside the 12000x12000 canvas
  let panY = 4000;
  let zoom = 1.0;
  let isPanning = false;
  let startPanX = 0;
  let startPanY = 0;
  
  // Interaction State
  let selectedElement = null; // node, comment, reroute, connection, etc.
  let selectedElementType = null; // 'node', 'comment', 'reroute', 'connection'
  let draggingNode = null;
  let dragOffset = { x: 0, y: 0 };
  let activePort = null; // { nodeId, type, pinEl, isInput }
  let isDrawingCable = false;
  
  // Comment Box resizing
  let resizingComment = null;
  let resizeStartCoords = { w: 0, h: 0, x: 0, y: 0 };

  // Category Color Map (used dynamically if not set in stylesheet)
  const categoryColors = {
    'core-platform': 'var(--cat-core)',
    'communication': 'var(--cat-communication)',
    'business-finance': 'var(--cat-business)',
    'productivity': 'var(--cat-productivity)',
    'creative-media': 'var(--cat-creative)',
    'games-arcade': 'var(--cat-gaming)',
    'ai-ml': 'var(--cat-ai)',
    'health-fitness': 'var(--cat-health)',
  };

  // Set up viewport center on load
  const rect = viewport.getBoundingClientRect();
  panX = (12000 - rect.width) / -2;
  panY = (12000 - rect.height) / -2;
  updateTransform();

  // ==========================================================================
  // API Integration (Load & Save)
  // ==========================================================================

  async function loadData() {
    try {
      const res = await fetch('/api/data');
      if (!res.ok) throw new Error('Failed to fetch data');
      const data = await res.json();
      
      ecosystemData = data.ecosystemData;
      layout = data.layout;
      
      // Initialize layout structures if missing
      if (!layout.nodePositions) layout.nodePositions = {};
      if (!layout.customConnections) layout.customConnections = [];
      if (!layout.hiddenConnections) layout.hiddenConnections = [];
      if (!layout.commentBoxes) layout.commentBoxes = [];
      if (!layout.rerouteNodes) layout.rerouteNodes = [];
      
      // Index systems
      systemsMap.clear();
      categories = [];
      
      if (ecosystemData.ecosystems && ecosystemData.ecosystems[0]) {
        const eco = ecosystemData.ecosystems[0];
        eco.categories.forEach(cat => {
          categories.push({ id: cat.id, name: cat.name, color: cat.color });
          cat.systems.forEach(sys => {
            sys.categoryId = cat.id;
            sys.categoryColor = cat.color || categoryColors[cat.id] || 'var(--cat-default)';
            systemsMap.set(sys.id, sys);
          });
        });
      }
      
      // Update sidebar category filters
      renderCategoryChips();
      
      // Render components
      renderBoard();
      renderLibrary();
      
      // Set initial zoom/pan to center nexus-cloud
      if (layout.nodePositions['nexus-cloud']) {
        centerOnNode('nexus-cloud');
      } else {
        autoArrangeTree();
      }
      
    } catch (err) {
      console.error(err);
      alert('Error connecting to the Vision Board Python server. Make sure server.py is running on port 9900!');
      document.getElementById('connection-status').textContent = 'Disconnected';
      document.getElementById('connection-status').className = 'offline';
    }
  }

  async function saveLayout() {
    try {
      const res = await fetch('/api/layout', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(layout)
      });
      if (!res.ok) throw new Error('Save failed');
      const result = await res.json();
      if (result.status === 'ok') {
        showHUDMessage('✓ Board Layout Saved to disk');
      } else {
        throw new Error(result.message || 'Unknown error');
      }
    } catch (err) {
      console.error(err);
      alert('Error saving layout: ' + err.message);
    }
  }

  function showHUDMessage(msg) {
    let msgEl = document.createElement('div');
    msgEl.className = 'canvas-hud';
    msgEl.style.position = 'absolute';
    msgEl.style.top = '12px';
    msgEl.style.left = '50%';
    msgEl.style.transform = 'translateX(-50%)';
    msgEl.style.backgroundColor = 'rgba(16, 185, 129, 0.9)';
    msgEl.style.color = '#fff';
    msgEl.style.border = 'none';
    msgEl.style.zIndex = '999';
    msgEl.innerHTML = `<span style="font-weight:600">${msg}</span>`;
    viewport.appendChild(msgEl);
    setTimeout(() => msgEl.remove(), 2500);
  }

  // ==========================================================================
  // Panning and Zooming
  // ==========================================================================

  function updateTransform() {
    canvas.style.transform = `translate(${panX}px, ${panY}px) scale(${zoom})`;
    zoomPercentage.textContent = `${Math.round(zoom * 100)}%`;
  }

  viewport.addEventListener('mousedown', (e) => {
    // Check if middle click, right click, or spacebar is pressed for panning
    const isPanningButton = e.button === 1 || e.button === 2 || e.shiftKey;
    const isSpacebarPan = e.code === 'Space';
    
    if (isPanningButton || e.target === viewport || e.target === canvas || e.target === svgLayer) {
      isPanning = true;
      startPanX = e.clientX - panX;
      startPanY = e.clientY - panY;
      
      // Deselect element if clicking canvas background
      if (e.button === 0 && e.target === svgLayer) {
        deselectAll();
      }
      
      e.preventDefault();
    }
  });

  window.addEventListener('mousemove', (e) => {
    if (isPanning) {
      panX = e.clientX - startPanX;
      panY = e.clientY - startPanY;
      updateTransform();
      return;
    }

    if (draggingNode) {
      // Calculate new position in canvas coordinates
      const currentZoom = zoom;
      const x = Math.round(((e.clientX - panX) / currentZoom) - dragOffset.x);
      const y = Math.round(((e.clientY - panY) / currentZoom) - dragOffset.y);
      
      // Snap to grid (10px)
      const snapX = Math.round(x / 10) * 10;
      const snapY = Math.round(y / 10) * 10;

      if (selectedElementType === 'node') {
        const nodeId = draggingNode.dataset.id;
        layout.nodePositions[nodeId] = { x: snapX, y: snapY };
        draggingNode.style.left = `${snapX}px`;
        draggingNode.style.top = `${snapY}px`;
        
        // If the node is inside a comment box, or if a comment box is selected, we move them.
        // Wait, standard comment box logic is handled below when dragging a comment box itself.
      } else if (selectedElementType === 'comment') {
        const commentId = draggingNode.dataset.id;
        const commentIdx = layout.commentBoxes.findIndex(c => c.id === commentId);
        if (commentIdx !== -1) {
          const comment = layout.commentBoxes[commentIdx];
          const dx = snapX - comment.x;
          const dy = snapY - comment.y;
          
          // Move comment box
          comment.x = snapX;
          comment.y = snapY;
          draggingNode.style.left = `${snapX}px`;
          draggingNode.style.top = `${snapY}px`;
          
          // Move all enclosed nodes by the same delta!
          const enclosedNodes = getNodesInCommentBox(comment);
          enclosedNodes.forEach(nodeId => {
            if (!layout.nodePositions[nodeId]) layout.nodePositions[nodeId] = { x: 0, y: 0 };
            layout.nodePositions[nodeId].x += dx;
            layout.nodePositions[nodeId].y += dy;
            const nodeEl = document.querySelector(`.node-container[data-id="${nodeId}"]`);
            if (nodeEl) {
              nodeEl.style.left = `${layout.nodePositions[nodeId].x}px`;
              nodeEl.style.top = `${layout.nodePositions[nodeId].y}px`;
            }
          });
        }
      } else if (selectedElementType === 'reroute') {
        const rerouteId = draggingNode.dataset.id;
        const rerouteIdx = layout.rerouteNodes.findIndex(r => r.id === rerouteId);
        if (rerouteIdx !== -1) {
          layout.rerouteNodes[rerouteIdx].x = snapX;
          layout.rerouteNodes[rerouteIdx].y = snapY;
          draggingNode.style.left = `${snapX}px`;
          draggingNode.style.top = `${snapY}px`;
        }
      }

      // Redraw cables
      drawCables();
      return;
    }

    if (resizingComment) {
      const currentZoom = zoom;
      const mouseCanvasX = (e.clientX - panX) / currentZoom;
      const mouseCanvasY = (e.clientY - panY) / currentZoom;
      
      const dw = Math.round(mouseCanvasX - resizeStartCoords.x);
      const dh = Math.round(mouseCanvasY - resizeStartCoords.y);
      
      const newW = Math.max(150, resizeStartCoords.w + dw);
      const newH = Math.max(100, resizeStartCoords.h + dh);
      
      const commentId = resizingComment.dataset.id;
      const commentIdx = layout.commentBoxes.findIndex(c => c.id === commentId);
      if (commentIdx !== -1) {
        layout.commentBoxes[commentIdx].w = newW;
        layout.commentBoxes[commentIdx].h = newH;
        resizingComment.style.width = `${newW}px`;
        resizingComment.style.height = `${newH}px`;
      }
      return;
    }

    if (isDrawingCable && activePort) {
      // Draw temporary cable
      const rect = viewport.getBoundingClientRect();
      const mouseCanvasX = (e.clientX - panX) / zoom;
      const mouseCanvasY = (e.clientY - panY) / zoom;
      
      const pinCoords = getPinCanvasCoords(activePort.nodeId, activePort.isInput, activePort.pinEl);
      
      let p1 = { x: pinCoords.x, y: pinCoords.y };
      let p2 = { x: mouseCanvasX, y: mouseCanvasY };
      
      if (activePort.isInput) {
        // Dragging out from input (Depends on)
        drawBezierPath(tempCable, p2, p1);
      } else {
        // Dragging out from output (Integrates)
        drawBezierPath(tempCable, p1, p2);
      }
    }
  });

  window.addEventListener('mouseup', (e) => {
    isPanning = false;
    draggingNode = null;
    resizingComment = null;
    
    if (isDrawingCable) {
      isDrawingCable = false;
      tempCable.style.display = 'none';
      
      // Check if dropped on a port
      const targetPin = e.target.closest('.port-pin');
      if (targetPin) {
        const portEl = targetPin.closest('.port');
        const targetNodeId = portEl.dataset.nodeId;
        const targetIsInput = portEl.classList.contains('port-input');
        
        if (targetNodeId !== activePort.nodeId && targetIsInput !== activePort.isInput) {
          // Establish connection!
          // Output connects to Input.
          const fromNode = activePort.isInput ? targetNodeId : activePort.nodeId;
          const toNode = activePort.isInput ? activePort.nodeId : targetNodeId;
          
          addCustomConnection(fromNode, toNode);
        }
      }
      activePort = null;
    }
  });

  // Zoom on scroll
  viewport.addEventListener('wheel', (e) => {
    e.preventDefault();
    const rect = viewport.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;
    
    // Mouse in canvas coordinates
    const canvasX = (mouseX - panX) / zoom;
    const canvasY = (mouseY - panY) / zoom;
    
    const zoomFactor = 1.1;
    let newZoom = zoom;
    if (e.deltaY < 0) {
      newZoom = Math.min(3.0, zoom * zoomFactor);
    } else {
      newZoom = Math.max(0.15, zoom / zoomFactor);
    }
    
    // Adjust pan to keep mouse centered on same canvas spot
    panX = mouseX - canvasX * newZoom;
    panY = mouseY - canvasY * newZoom;
    zoom = newZoom;
    
    updateTransform();
  }, { passive: false });

  // Zoom Button Listeners
  btnZoomIn.onclick = () => {
    const rect = viewport.getBoundingClientRect();
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    const canvasX = (cx - panX) / zoom;
    const canvasY = (cy - panY) / zoom;
    zoom = Math.min(3.0, zoom * 1.2);
    panX = cx - canvasX * zoom;
    panY = cy - canvasY * zoom;
    updateTransform();
  };

  btnZoomOut.onclick = () => {
    const rect = viewport.getBoundingClientRect();
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    const canvasX = (cx - panX) / zoom;
    const canvasY = (cy - panY) / zoom;
    zoom = Math.max(0.15, zoom / 1.2);
    panX = cx - canvasX * zoom;
    panY = cy - canvasY * zoom;
    updateTransform();
  };

  btnZoomReset.onclick = () => {
    const rect = viewport.getBoundingClientRect();
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    // Find center of current nodes
    let nodeCount = 0;
    let sumX = 0, sumY = 0;
    for (let id in layout.nodePositions) {
      sumX += layout.nodePositions[id].x;
      sumY += layout.nodePositions[id].y;
      nodeCount++;
    }
    
    zoom = 1.0;
    if (nodeCount > 0) {
      panX = cx - (sumX / nodeCount) - 125; // center of node width
      panY = cy - (sumY / nodeCount) - 60;  // center of node height
    } else {
      panX = (12000 - rect.width) / -2;
      panY = (12000 - rect.height) / -2;
    }
    updateTransform();
  };

  // Prevent right click context menu on canvas so we can use right-drag
  viewport.addEventListener('contextmenu', e => e.preventDefault());

  // ==========================================================================
  // Layout math & Auto-arranger (Hierarchy Tree)
  // ==========================================================================

  function getPinCanvasCoords(nodeId, isInput, pinEl) {
    const nodeEl = document.querySelector(`.node-container[data-id="${nodeId}"]`);
    if (!nodeEl) return { x: 0, y: 0 };
    
    const pos = layout.nodePositions[nodeId] || { x: 0, y: 0 };
    const pinRect = pinEl.getBoundingClientRect();
    const nodeRect = nodeEl.getBoundingClientRect();
    
    // Compute offset relative to node top-left
    const dx = (pinRect.left + pinRect.width / 2 - nodeRect.left) / zoom;
    const dy = (pinRect.top + pinRect.height / 2 - nodeRect.top) / zoom;
    
    return {
      x: pos.x + dx,
      y: pos.y + dy
    };
  }

  function getNodesInCommentBox(comment) {
    const nodesInside = [];
    const cx1 = comment.x;
    const cy1 = comment.y;
    const cx2 = comment.x + comment.w;
    const cy2 = comment.y + comment.h;
    
    for (let id in layout.nodePositions) {
      const pos = layout.nodePositions[id];
      // Node is 250px wide, about 150px tall
      if (pos.x >= cx1 && pos.x + 250 <= cx2 && pos.y >= cy1 && pos.y + 120 <= cy2) {
        nodesInside.push(id);
      }
    }
    return nodesInside;
  }

  function centerOnNode(nodeId) {
    const pos = layout.nodePositions[nodeId];
    if (!pos) return;
    
    const rect = viewport.getBoundingClientRect();
    panX = (rect.width / 2) - pos.x - 125;
    panY = (rect.height / 2) - pos.y - 60;
    zoom = 1.0;
    updateTransform();
  }

  // Hierarchical auto arrange algorithm (root is nexus-cloud)
  function autoArrangeTree() {
    const rootId = 'nexus-cloud';
    if (!systemsMap.has(rootId)) return;
    
    const visited = new Set();
    const layers = []; // [ Set of node ids ]
    
    // Simple BFS to find layers
    let queue = [rootId];
    visited.add(rootId);
    
    while (queue.length > 0) {
      const currentLayer = [...queue];
      layers.push(currentLayer);
      
      const nextQueue = [];
      currentLayer.forEach(nodeId => {
        const sys = systemsMap.get(nodeId);
        if (!sys) return;
        
        // Find connections (dependencies and integrates)
        const connections = [...sys.dependencies, ...sys.integrates];
        
        // Add custom connections if any
        layout.customConnections.forEach(conn => {
          if (conn.from === nodeId && !visited.has(conn.to)) {
            nextQueue.push(conn.to);
            visited.add(conn.to);
          }
          if (conn.to === nodeId && !visited.has(conn.from)) {
            nextQueue.push(conn.from);
            visited.add(conn.from);
          }
        });

        connections.forEach(connId => {
          const targetId = connId.toLowerCase();
          if (systemsMap.has(targetId) && !visited.has(targetId)) {
            nextQueue.push(targetId);
            visited.add(targetId);
          }
        });
      });
      queue = nextQueue;
    }
    
    // Add remaining systems not visited by BFS (unconnected) to a bottom layer
    const unconnected = [];
    systemsMap.forEach((sys, id) => {
      if (!visited.has(id)) {
        unconnected.push(id);
        visited.add(id);
      }
    });
    if (unconnected.length > 0) {
      layers.push(unconnected);
    }
    
    // Layout nodes starting from canvas center coordinate (6000, 3000)
    const startX = 6000;
    const startY = 3000;
    const verticalGap = 350;
    const horizontalGap = 320;
    
    layers.forEach((layerNodes, layerIdx) => {
      const layerWidth = (layerNodes.length - 1) * horizontalGap;
      const layerLeft = startX - (layerWidth / 2);
      const y = startY + (layerIdx * verticalGap);
      
      layerNodes.forEach((nodeId, idx) => {
        const x = layerLeft + (idx * horizontalGap);
        layout.nodePositions[nodeId] = { x, y };
      });
    });
    
    // Refresh board
    renderBoard();
    centerOnNode(rootId);
    showHUDMessage('Auto-arranged Tree Layout');
  }

  btnAutoArrange.onclick = () => {
    if (confirm('Auto-arranging will reset all manually positioned nodes. Comment boxes and reroute nodes will be cleared to re-align the ecosystem tree. Do you want to proceed?')) {
      layout.commentBoxes = [];
      layout.rerouteNodes = [];
      autoArrangeTree();
    }
  };

  // ==========================================================================
  // Render Board elements (Nodes, Comment Boxes, Cables)
  // ==========================================================================

  function renderBoard() {
    // Clear layers
    nodesLayer.innerHTML = '';
    commentsLayer.innerHTML = '';
    rerouteLayer.innerHTML = '';
    
    // Render Comment Boxes
    layout.commentBoxes.forEach(comment => {
      renderCommentBoxElement(comment);
    });

    // Render Nodes
    systemsMap.forEach((sys, id) => {
      const pos = layout.nodePositions[id];
      if (pos) {
        renderNodeElement(sys, pos);
      }
    });
    
    // Render Reroute Dots
    layout.rerouteNodes.forEach(dot => {
      renderRerouteElement(dot);
    });

    // Render Cables
    drawCables();
  }

  function renderNodeElement(sys, pos) {
    const node = document.createElement('div');
    node.className = 'node-container';
    node.dataset.id = sys.id;
    node.style.left = `${pos.x}px`;
    node.style.top = `${pos.y}px`;
    
    // Highlight category color border if category matched
    const catColor = sys.categoryColor;
    
    node.innerHTML = `
      <div class="node-header" style="background: linear-gradient(180deg, rgba(255, 255, 255, 0.15) 0%, rgba(255, 255, 255, 0) 100%), ${catColor}">
        <div class="node-header-icon">
          <svg viewBox="0 0 24 24" width="12" height="12" fill="none" stroke="currentColor" stroke-width="2.5">
            <rect x="3" y="3" width="7" height="9"></rect>
            <rect x="14" y="3" width="7" height="5"></rect>
            <rect x="14" y="12" width="7" height="9"></rect>
            <rect x="3" y="16" width="7" height="5"></rect>
          </svg>
        </div>
        <div class="node-header-title">${sys.name}</div>
        <div class="node-header-status ${sys.status}">${sys.status}</div>
      </div>
      <div class="node-body">
        <div class="node-desc">${sys.description || 'No description provided.'}</div>
        <div class="node-meta">
          <span class="tech-tag">${sys.tech || 'N/A'}</span>
          <span>${sys.port ? `:${sys.port}` : 'no port'}</span>
        </div>
        <div class="node-ports">
          <div class="port port-input" data-node-id="${sys.id}">
            <div class="port-pin"></div>
            <span>Depends</span>
          </div>
          <div class="port port-output" data-node-id="${sys.id}">
            <span>Integrates</span>
            <div class="port-pin"></div>
          </div>
        </div>
      </div>
    `;

    // Select behavior
    node.addEventListener('mousedown', (e) => {
      if (e.target.closest('.port') || e.target.closest('.btn-close')) return;
      
      e.stopPropagation();
      deselectAll();
      selectedElement = node;
      selectedElementType = 'node';
      node.classList.add('selected');
      
      draggingNode = node;
      const currentZoom = zoom;
      dragOffset.x = ((e.clientX - panX) / currentZoom) - pos.x;
      dragOffset.y = ((e.clientY - panY) / currentZoom) - pos.y;
      
      showNodeDetails(sys.id);
    });

    // Pin Dragging logic
    const portPins = node.querySelectorAll('.port');
    portPins.forEach(port => {
      const pinEl = port.querySelector('.port-pin');
      const isInput = port.classList.contains('port-input');
      
      port.addEventListener('mousedown', (e) => {
        e.stopPropagation();
        e.preventDefault();
        
        isDrawingCable = true;
        activePort = {
          nodeId: sys.id,
          type: isInput ? 'input' : 'output',
          pinEl: pinEl,
          isInput: isInput
        };
        
        tempCable.style.display = 'block';
      });
    });

    // Hover path highlighting
    node.addEventListener('mouseenter', () => {
      if (!isDrawingCable && !draggingNode) {
        highlightNodePaths(sys.id);
      }
    });

    node.addEventListener('mouseleave', () => {
      if (!isDrawingCable && !draggingNode) {
        resetPathHighlights();
      }
    });

    nodesLayer.appendChild(node);
  }

  function renderCommentBoxElement(comment) {
    const box = document.createElement('div');
    box.className = 'comment-box';
    box.dataset.id = comment.id;
    box.style.left = `${comment.x}px`;
    box.style.top = `${comment.y}px`;
    box.style.width = `${comment.w}px`;
    box.style.height = `${comment.h}px`;
    box.style.borderColor = comment.color || 'rgba(255, 255, 255, 0.15)';
    box.style.backgroundColor = comment.color ? comment.color.replace(')', ', 0.05)').replace('rgb', 'rgba') : 'rgba(255, 255, 255, 0.01)';
    
    box.innerHTML = `
      <div class="comment-header" style="background-color: ${comment.color ? comment.color.replace(')', ', 0.15)').replace('rgb', 'rgba') : 'rgba(255,255,255,0.05)'}">
        <span class="comment-title">${comment.title}</span>
      </div>
      <div class="comment-resize-handle"></div>
    `;

    // Panning/dragging comment box
    const header = box.querySelector('.comment-header');
    header.addEventListener('mousedown', (e) => {
      e.stopPropagation();
      deselectAll();
      selectedElement = box;
      selectedElementType = 'comment';
      box.classList.add('selected');
      
      draggingNode = box;
      const currentZoom = zoom;
      dragOffset.x = ((e.clientX - panX) / currentZoom) - comment.x;
      dragOffset.y = ((e.clientY - panY) / currentZoom) - comment.y;
      
      showCommentDetails(comment.id);
    });

    // Double click header to rename
    header.addEventListener('dblclick', (e) => {
      e.stopPropagation();
      const newTitle = prompt('Rename Comment Box:', comment.title);
      if (newTitle !== null && newTitle.trim() !== '') {
        comment.title = newTitle.trim();
        box.querySelector('.comment-title').textContent = comment.title;
      }
    });

    // Resize comment box
    const handle = box.querySelector('.comment-resize-handle');
    handle.addEventListener('mousedown', (e) => {
      e.stopPropagation();
      e.preventDefault();
      resizingComment = box;
      const currentZoom = zoom;
      resizeStartCoords.x = (e.clientX - panX) / currentZoom;
      resizeStartCoords.y = (e.clientY - panY) / currentZoom;
      resizeStartCoords.w = comment.w;
      resizeStartCoords.h = comment.h;
    });

    commentsLayer.appendChild(box);
  }

  function renderRerouteElement(dot) {
    const el = document.createElement('div');
    el.className = 'reroute-node';
    el.dataset.id = dot.id;
    el.style.left = `${dot.x}px`;
    el.style.top = `${dot.y}px`;
    
    el.addEventListener('mousedown', (e) => {
      e.stopPropagation();
      deselectAll();
      selectedElement = el;
      selectedElementType = 'reroute';
      el.classList.add('selected');
      
      draggingNode = el;
      const currentZoom = zoom;
      dragOffset.x = ((e.clientX - panX) / currentZoom) - dot.x;
      dragOffset.y = ((e.clientY - panY) / currentZoom) - dot.y;
    });

    rerouteLayer.appendChild(el);
  }

  // ==========================================================================
  // Connection Cabling (Drawing Curves)
  // ==========================================================================

  function drawBezierPath(pathEl, p1, p2) {
    const dx = Math.abs(p2.x - p1.x);
    const offset = Math.max(80, dx * 0.5);
    
    const c1x = p1.x + offset;
    const c1y = p1.y;
    const c2x = p2.x - offset;
    const c2y = p2.y;
    
    pathEl.setAttribute('d', `M ${p1.x} ${p1.y} C ${c1x} ${c1y}, ${c2x} ${c2y}, ${p2.x} ${p2.y}`);
  }

  function drawCables() {
    connectionsGroup.innerHTML = '';
    rerouteLinesGroup.innerHTML = '';
    
    const connectionsToDraw = [];
    
    // 1. Traverse default dependencies & integrates from ecosystem-data.json
    systemsMap.forEach((sys, nodeId) => {
      // Inputs: Dependencies
      sys.dependencies.forEach(depId => {
        const fromId = depId.toLowerCase();
        const connectionKey = `${fromId}->${nodeId}`;
        
        if (systemsMap.has(fromId) && layout.nodePositions[fromId] && !layout.hiddenConnections.includes(connectionKey)) {
          connectionsToDraw.push({
            from: fromId,
            to: nodeId,
            type: 'dependency',
            key: connectionKey
          });
        }
      });

      // Outputs: Integrates
      sys.integrates.forEach(intId => {
        const targetId = intId.toLowerCase();
        const connectionKey = `${nodeId}->${targetId}`;
        
        if (systemsMap.has(targetId) && layout.nodePositions[targetId] && !layout.hiddenConnections.includes(connectionKey)) {
          connectionsToDraw.push({
            from: nodeId,
            to: targetId,
            type: 'integration',
            key: connectionKey
          });
        }
      });
    });

    // 2. Add Custom Connections
    layout.customConnections.forEach((conn, index) => {
      const connectionKey = `custom_${index}_${conn.from}->${conn.to}`;
      if (layout.nodePositions[conn.from] && layout.nodePositions[conn.to]) {
        connectionsToDraw.push({
          from: conn.from,
          to: conn.to,
          type: 'custom',
          key: connectionKey,
          customIdx: index
        });
      }
    });

    // Draw the cables (resolving reroute dots if any)
    connectionsToDraw.forEach(conn => {
      // Find if there's reroute dots chained between conn.from and conn.to
      const chain = getRerouteChain(conn.from, conn.to);
      
      if (chain.length === 0) {
        // Direct link
        drawSingleCable(conn.from, conn.to, conn.type, conn.key, null, null, conn.customIdx);
      } else {
        // Multi-segmented link via Reroute Dots!
        // Segment 1: fromNode -> dot1
        drawSingleCable(conn.from, chain[0].id, conn.type, `${conn.key}_seg0`, null, chain[0], conn.customIdx);
        
        // Segments 2..N-1: dot_i -> dot_i+1
        for (let i = 0; i < chain.length - 1; i++) {
          drawSingleCable(chain[i].id, chain[i+1].id, conn.type, `${conn.key}_seg${i+1}`, chain[i], chain[i+1], conn.customIdx);
        }
        
        // Segment N: dot_last -> toNode
        drawSingleCable(chain[chain.length - 1].id, conn.to, conn.type, `${conn.key}_seg_last`, chain[chain.length - 1], null, conn.customIdx);
      }
    });
  }

  function getRerouteChain(fromId, toId) {
    // Find all reroutes that link fromId to toId
    // Simple filter and chain sorting
    const dots = layout.rerouteNodes.filter(r => r.from === fromId && r.to === toId);
    // Sort them if multiple (by distance or position, simple sort for now)
    return dots.sort((a,b) => a.x - b.x);
  }

  function drawSingleCable(fromId, toId, type, key, fromDot = null, toDot = null, customIdx = null) {
    let p1, p2;
    
    // Find p1 coordinates
    if (fromDot) {
      p1 = { x: fromDot.x, y: fromDot.y };
    } else {
      const pinOut = document.querySelector(`.node-container[data-id="${fromId}"] .port-output .port-pin`);
      if (pinOut) p1 = getPinCanvasCoords(fromId, false, pinOut);
      else return; // Nodes might not be fully loaded/rendered yet
    }

    // Find p2 coordinates
    if (toDot) {
      p2 = { x: toDot.x, y: toDot.y };
    } else {
      const pinIn = document.querySelector(`.node-container[data-id="${toId}"] .port-input .port-pin`);
      if (pinIn) p2 = getPinCanvasCoords(toId, true, pinIn);
      else return;
    }

    // Create Path elements
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.className.baseVal = `cable ${type}`;
    path.dataset.key = key;
    path.dataset.from = fromId;
    path.dataset.to = toId;
    path.dataset.type = type;
    if (customIdx !== null) path.dataset.customIdx = customIdx;
    
    // Colors
    let color = '#555';
    if (type === 'dependency') color = '#ef4444'; // Red
    else if (type === 'integration') color = '#3b82f6'; // Blue
    else if (type === 'custom') color = '#10b981'; // Green
    
    path.setAttribute('stroke', color);
    path.setAttribute('fill', 'none');
    
    drawBezierPath(path, p1, p2);
    
    // Add Flowing ants overlay if active
    const flowPath = path.cloneNode(true);
    flowPath.className.baseVal = `cable-flow`;
    flowPath.setAttribute('stroke', '#ffffff');
    flowPath.setAttribute('stroke-width', '1.2px');
    flowPath.setAttribute('opacity', '0.2');
    
    // Interaction
    path.addEventListener('mousedown', (e) => {
      e.stopPropagation();
      deselectAll();
      
      selectedElement = path;
      selectedElementType = 'connection';
      path.classList.add('selected');
      
      showConnectionDetails(fromId, toId, type, key, customIdx);
    });

    // Double-click cable to insert a reroute node!
    path.addEventListener('dblclick', (e) => {
      e.stopPropagation();
      
      // Get click coords in canvas space
      const currentZoom = zoom;
      const rect = viewport.getBoundingClientRect();
      const clickX = Math.round(((e.clientX - panX) / currentZoom));
      const clickY = Math.round(((e.clientY - panY) / currentZoom));
      
      // Add reroute node
      const newDot = {
        id: `rr_${Date.now()}`,
        x: clickX,
        y: clickY,
        from: fromId,
        to: toId
      };
      layout.rerouteNodes.push(newDot);
      
      renderBoard();
    });

    connectionsGroup.appendChild(path);
    connectionsGroup.appendChild(flowPath);
  }

  // Highlight full data flow path (upstream and downstream)
  function highlightNodePaths(nodeId) {
    const cables = document.querySelectorAll('.cable, .cable-flow');
    const nodes = document.querySelectorAll('.node-container');
    
    const activeNodes = new Set([nodeId]);
    const activeCables = new Set();
    
    // Simple BFS/DFS to find all connected paths (up and down)
    // 1. Direct dependencies (upstream)
    const sys = systemsMap.get(nodeId);
    if (sys) {
      sys.dependencies.forEach(d => {
        activeNodes.add(d.toLowerCase());
      });
      sys.integrates.forEach(i => {
        activeNodes.add(i.toLowerCase());
      });
    }
    
    // 2. Custom connections
    layout.customConnections.forEach(conn => {
      if (conn.from === nodeId) { activeNodes.add(conn.to); }
      if (conn.to === nodeId) { activeNodes.add(conn.from); }
    });

    // Match cables
    cables.forEach(c => {
      const fromNode = c.dataset.from;
      const toNode = c.dataset.to;
      
      if (fromNode === nodeId || toNode === nodeId || 
          (activeNodes.has(fromNode) && activeNodes.has(toNode))) {
        c.classList.add('path-active');
        activeCables.add(c);
      } else {
        c.style.opacity = '0.08';
      }
    });

    // Dim out inactive nodes
    nodes.forEach(n => {
      const id = n.dataset.id;
      if (activeNodes.has(id)) {
        n.style.opacity = '1.0';
        n.style.boxShadow = '0 0 15px rgba(255,255,255,0.1)';
      } else {
        n.style.opacity = '0.2';
      }
    });
  }

  function resetPathHighlights() {
    const cables = document.querySelectorAll('.cable, .cable-flow');
    const nodes = document.querySelectorAll('.node-container');
    
    cables.forEach(c => {
      c.classList.remove('path-active');
      c.style.opacity = '';
    });
    
    nodes.forEach(n => {
      n.style.opacity = '';
      n.style.boxShadow = '';
    });
  }

  // ==========================================================================
  // Sidebar App Library Drawer
  // ==========================================================================

  function renderLibrary() {
    libraryApps.innerHTML = '';
    
    let count = 0;
    systemsMap.forEach((sys, id) => {
      // Check if it's already on the board
      const isOnBoard = layout.nodePositions[id] !== undefined;
      
      // Apply filters
      const searchVal = librarySearch.value.toLowerCase().trim();
      const activeChip = categoryChips.querySelector('.chip.active');
      const activeCategory = activeChip ? activeChip.dataset.id : 'all';
      
      const matchesSearch = sys.name.toLowerCase().includes(searchVal) || sys.description.toLowerCase().includes(searchVal);
      const matchesCategory = activeCategory === 'all' || sys.categoryId === activeCategory;
      
      if (matchesSearch && matchesCategory) {
        count++;
        const item = document.createElement('div');
        item.className = 'app-list-item';
        item.dataset.id = sys.id;
        
        item.innerHTML = `
          <div class="app-info">
            <div class="app-title">${sys.name}</div>
            <div class="app-sub">${sys.tech} · ${sys.status}</div>
          </div>
          ${isOnBoard ? `
            <span style="font-size:10px; color:var(--fg-dark)">✓ Board</span>
          ` : `
            <button class="btn-add-node" title="Add node to center of board">+</button>
          `}
        `;
        
        // Add by clicking button
        const addBtn = item.querySelector('.btn-add-node');
        if (addBtn) {
          addBtn.onclick = (e) => {
            e.stopPropagation();
            addNodeToBoard(sys.id);
          };
        }
        
        // Drag item onto board
        item.setAttribute('draggable', 'true');
        item.addEventListener('dragstart', (e) => {
          e.dataTransfer.setData('text/plain', sys.id);
        });

        libraryApps.appendChild(item);
      }
    });

    libraryCount.textContent = count;
  }

  function renderCategoryChips() {
    categoryChips.innerHTML = '<span class="chip active" data-id="all">All</span>';
    categories.forEach(cat => {
      categoryChips.innerHTML += `<span class="chip" data-id="${cat.id}">${cat.name}</span>`;
    });
    
    // Add Chip Click handlers
    const chips = categoryChips.querySelectorAll('.chip');
    chips.forEach(chip => {
      chip.onclick = () => {
        chips.forEach(c => c.classList.remove('active'));
        chip.classList.add('active');
        renderLibrary();
      };
    });
  }

  librarySearch.oninput = () => renderLibrary();

  // Drag and Drop from library onto canvas
  viewport.addEventListener('dragover', (e) => {
    e.preventDefault();
  });

  viewport.addEventListener('drop', (e) => {
    e.preventDefault();
    const systemId = e.dataTransfer.getData('text/plain');
    if (systemId && systemsMap.has(systemId)) {
      // Calculate drops coords in canvas coordinates
      const currentZoom = zoom;
      const rect = viewport.getBoundingClientRect();
      const dropX = Math.round(((e.clientX - panX) / currentZoom) - 125);
      const dropY = Math.round(((e.clientY - panY) / currentZoom) - 60);
      
      addNodeToBoard(systemId, dropX, dropY);
    }
  });

  function addNodeToBoard(systemId, x = null, y = null) {
    if (layout.nodePositions[systemId]) {
      // Already on board, focus it
      centerOnNode(systemId);
      return;
    }

    if (x === null || y === null) {
      // Place at current viewport center
      const rect = viewport.getBoundingClientRect();
      x = Math.round(((-panX + rect.width / 2) / zoom) - 125);
      y = Math.round(((-panY + rect.height / 2) / zoom) - 60);
    }

    // Add position
    layout.nodePositions[systemId] = { x, y };
    
    // Refresh board & sidebar
    renderBoard();
    renderLibrary();
    
    // Highlight and focus new node
    const nodeEl = document.querySelector(`.node-container[data-id="${systemId}"]`);
    if (nodeEl) {
      deselectAll();
      selectedElement = nodeEl;
      selectedElementType = 'node';
      nodeEl.classList.add('selected');
      showNodeDetails(systemId);
    }
  }

  // ==========================================================================
  // Connections and Connection Modifiers
  // ==========================================================================

  function addCustomConnection(fromNode, toNode) {
    // Check if connection already exists
    const duplicate = layout.customConnections.some(conn => conn.from === fromNode && conn.to === toNode);
    const reverse = layout.customConnections.some(conn => conn.from === toNode && conn.to === fromNode);
    
    if (duplicate || reverse) {
      showHUDMessage('Cable connection already exists');
      return;
    }
    
    // Check if it was a hidden default connection, if so unhide it
    const defaultKey = `${fromNode}->${toNode}`;
    const hiddenIdx = layout.hiddenConnections.indexOf(defaultKey);
    if (hiddenIdx !== -1) {
      layout.hiddenConnections.splice(hiddenIdx, 1);
      drawCables();
      showHUDMessage('Default connection restored');
      return;
    }

    // Add to custom connections
    layout.customConnections.push({
      from: fromNode,
      to: toNode,
      type: 'custom'
    });
    
    drawCables();
    showHUDMessage('Custom connection added');
  }

  // ==========================================================================
  // Config Panels & Detail card on right side
  // ==========================================================================

  function deselectAll() {
    if (selectedElement) {
      selectedElement.classList.remove('selected');
    }
    selectedElement = null;
    selectedElementType = null;
    sidebarRight.classList.add('collapsed');
    resetPathHighlights();
  }

  function showNodeDetails(nodeId) {
    const sys = systemsMap.get(nodeId);
    if (!sys) return;
    
    sidebarRight.classList.remove('collapsed');
    
    configContent.innerHTML = `
      <div class="config-section">
        <h3>System Details</h3>
        <div class="config-row">
          <label>Name</label>
          <span class="val" style="font-weight:700">${sys.name}</span>
        </div>
        <div class="config-row">
          <label>Status</label>
          <span class="val node-header-status ${sys.status}">${sys.status}</span>
        </div>
        <div class="config-row">
          <label>Technology</label>
          <span class="val">${sys.tech}</span>
        </div>
        <div class="config-row">
          <label>Port</label>
          <span class="val">${sys.port || 'No port binded'}</span>
        </div>
        <div class="config-row">
          <label>Category</label>
          <span class="val" style="color: ${sys.categoryColor}">${sys.categoryId}</span>
        </div>
      </div>
      
      <div class="config-section">
        <h3>Description</h3>
        <p style="font-size: 11px; line-height:1.5; color:#999;">${sys.description}</p>
      </div>

      <div class="config-section">
        <h3>Custom Overrides</h3>
        <div class="config-row">
          <label>Custom Color Block</label>
          <input type="color" id="node-custom-color" value="${sys.categoryColor.startsWith('#') ? sys.categoryColor : '#6b7280'}">
        </div>
        <div class="config-row" style="margin-top:8px">
          <label>Custom Notes</label>
          <textarea id="node-custom-notes" class="notes-input" placeholder="Add layout notes, plans, ports, or todo list for this system..."></textarea>
        </div>
      </div>

      <button class="btn btn-delete-node" id="btn-remove-node-board">Remove Node from Board</button>
    `;
    
    // Load Custom Notes from layout if exists
    const notesInput = document.getElementById('node-custom-notes');
    const colorInput = document.getElementById('node-custom-color');
    
    if (layout.nodePositions[nodeId] && layout.nodePositions[nodeId].notes) {
      notesInput.value = layout.nodePositions[nodeId].notes;
    }
    
    // Notes input save handler
    notesInput.addEventListener('input', () => {
      if (layout.nodePositions[nodeId]) {
        layout.nodePositions[nodeId].notes = notesInput.value;
      }
    });

    // Custom Color block handler
    colorInput.addEventListener('input', () => {
      sys.categoryColor = colorInput.value;
      const headerEl = document.querySelector(`.node-container[data-id="${nodeId}"] .node-header`);
      if (headerEl) {
        headerEl.style.backgroundColor = colorInput.value;
      }
    });

    // Remove from board
    document.getElementById('btn-remove-node-board').onclick = () => {
      if (nodeId === 'nexus-cloud') {
        alert('Nexus-Cloud is the monorepo root control plane and cannot be removed from the vision board!');
        return;
      }
      if (confirm(`Are you sure you want to remove ${sys.name} from the vision board? This will remove its node and reset its position coordinates.`)) {
        // Delete position
        delete layout.nodePositions[nodeId];
        
        // Remove custom connections involving this node
        layout.customConnections = layout.customConnections.filter(c => c.from !== nodeId && c.to !== nodeId);
        
        // Remove reroute dots involving this node
        layout.rerouteNodes = layout.rerouteNodes.filter(r => r.from !== nodeId && r.to !== nodeId);
        
        deselectAll();
        renderBoard();
        renderLibrary();
      }
    };
  }

  function showCommentDetails(commentId) {
    const commentIdx = layout.commentBoxes.findIndex(c => c.id === commentId);
    if (commentIdx === -1) return;
    
    const comment = layout.commentBoxes[commentIdx];
    sidebarRight.classList.remove('collapsed');
    
    configContent.innerHTML = `
      <div class="config-section">
        <h3>Comment Box Details</h3>
        <div class="config-row">
          <label>Title</label>
          <input type="text" id="comment-title-input" value="${comment.title}">
        </div>
        <div class="config-row" style="margin-top: 8px;">
          <label>Box Tint Color</label>
          <select id="comment-color-select">
            <option value="rgba(99, 102, 241, 0.15)" ${comment.color === 'rgba(99, 102, 241, 0.15)' ? 'selected' : ''}>Core Platform (Blue)</option>
            <option value="rgba(16, 185, 129, 0.15)" ${comment.color === 'rgba(16, 185, 129, 0.15)' ? 'selected' : ''}>Communication (Green)</option>
            <option value="rgba(245, 158, 11, 0.15)" ${comment.color === 'rgba(245, 158, 11, 0.15)' ? 'selected' : ''}>Business / Database (Amber)</option>
            <option value="rgba(239, 68, 68, 0.15)" ${comment.color === 'rgba(239, 68, 68, 0.15)' ? 'selected' : ''}>Creative (Red)</option>
            <option value="rgba(139, 92, 246, 0.15)" ${comment.color === 'rgba(139, 92, 246, 0.15)' ? 'selected' : ''}>Gaming (Purple)</option>
            <option value="rgba(6, 182, 212, 0.15)" ${comment.color === 'rgba(6, 182, 212, 0.15)' ? 'selected' : ''}>AI Engine (Cyan)</option>
          </select>
        </div>
      </div>
      
      <button class="btn btn-delete-node" id="btn-delete-comment">Delete Comment Box</button>
    `;
    
    // Rename box handler
    document.getElementById('comment-title-input').addEventListener('input', (e) => {
      comment.title = e.target.value;
      const boxEl = document.querySelector(`.comment-box[data-id="${commentId}"]`);
      if (boxEl) {
        boxEl.querySelector('.comment-title').textContent = comment.title;
      }
    });

    // Color select box handler
    document.getElementById('comment-color-select').addEventListener('change', (e) => {
      comment.color = e.target.value;
      const boxEl = document.querySelector(`.comment-box[data-id="${commentId}"]`);
      if (boxEl) {
        boxEl.style.borderColor = comment.color.replace('0.15', '0.4');
        boxEl.style.backgroundColor = comment.color.replace('0.15', '0.04');
        boxEl.querySelector('.comment-header').style.backgroundColor = comment.color;
      }
    });

    // Delete comment box
    document.getElementById('btn-delete-comment').onclick = () => {
      layout.commentBoxes.splice(commentIdx, 1);
      deselectAll();
      renderBoard();
    };
  }

  function showConnectionDetails(fromId, toId, type, key, customIdx) {
    sidebarRight.classList.remove('collapsed');
    
    const fromSys = systemsMap.get(fromId);
    const toSys = systemsMap.get(toId);
    
    configContent.innerHTML = `
      <div class="config-section">
        <h3>Cable Properties</h3>
        <div class="config-row">
          <label>Source node</label>
          <span class="val">${fromSys ? fromSys.name : fromId}</span>
        </div>
        <div class="config-row">
          <label>Destination node</label>
          <span class="val">${toSys ? toSys.name : toId}</span>
        </div>
        <div class="config-row">
          <label>Link Type</label>
          <span class="val" style="text-transform: capitalize; color: ${type === 'dependency' ? '#ef4444' : type==='integration' ? '#3b82f6' : '#10b981'}">${type}</span>
        </div>
      </div>
      
      <button class="btn btn-delete-node" id="btn-delete-cable">Sever Connection Cable</button>
    `;
    
    document.getElementById('btn-delete-cable').onclick = () => {
      if (confirm('Are you sure you want to sever this connection thread?')) {
        if (type === 'custom') {
          // Remove from custom list
          layout.customConnections.splice(customIdx, 1);
        } else {
          // Hide default connection by adding to hiddenConnections
          const defaultKey = `${fromId}->${toId}`;
          layout.hiddenConnections.push(defaultKey);
        }
        
        deselectAll();
        renderBoard();
        showHUDMessage('Cable connection severed');
      }
    };
  }

  // Close Config panel on click 'x'
  btnCloseRight.onclick = () => deselectAll();

  // ==========================================================================
  // Adding comment boxes
  // ==========================================================================

  btnAddComment.onclick = () => {
    // Add box to center of viewport
    const rect = viewport.getBoundingClientRect();
    const cx = Math.round(((-panX + rect.width / 2) / zoom) - 150);
    const cy = Math.round(((-panY + rect.height / 2) / zoom) - 100);
    
    const newComment = {
      id: `comment_${Date.now()}`,
      title: 'GROUPING COMMENT',
      x: cx,
      y: cy,
      w: 350,
      h: 200,
      color: 'rgba(99, 102, 241, 0.15)'
    };
    
    layout.commentBoxes.push(newComment);
    renderBoard();
    
    // Select the new comment box
    const commentEl = document.querySelector(`.comment-box[data-id="${newComment.id}"]`);
    if (commentEl) {
      deselectAll();
      selectedElement = commentEl;
      selectedElementType = 'comment';
      commentEl.classList.add('selected');
      showCommentDetails(newComment.id);
    }
  };

  // ==========================================================================
  // Search bar locator on board
  // ==========================================================================

  searchInput.addEventListener('input', () => {
    const val = searchInput.value.toLowerCase().trim();
    const nodes = document.querySelectorAll('.node-container');
    
    if (val === '') {
      nodes.forEach(n => n.style.opacity = '1.0');
      return;
    }
    
    nodes.forEach(n => {
      const id = n.dataset.id;
      const sys = systemsMap.get(id);
      if (sys && sys.name.toLowerCase().includes(val)) {
        n.style.opacity = '1.0';
        n.style.boxShadow = '0 0 15px var(--accent-glow)';
      } else {
        n.style.opacity = '0.15';
        n.style.boxShadow = '';
      }
    });
  });

  searchInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      const val = searchInput.value.toLowerCase().trim();
      const matchId = Array.from(systemsMap.keys()).find(id => {
        const sys = systemsMap.get(id);
        return sys && sys.name.toLowerCase().includes(val) && layout.nodePositions[id] !== undefined;
      });
      if (matchId) {
        centerOnNode(matchId);
        deselectAll();
        const nodeEl = document.querySelector(`.node-container[data-id="${matchId}"]`);
        if (nodeEl) {
          selectedElement = nodeEl;
          selectedElementType = 'node';
          nodeEl.classList.add('selected');
          showNodeDetails(matchId);
        }
      }
    }
  });

  // ==========================================================================
  // Keyboard Shortcuts (Delete to remove)
  // ==========================================================================

  window.addEventListener('keydown', (e) => {
    // Check if user is typing in input or textarea
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

    if (e.key === 'Delete' || e.key === 'Backspace') {
      if (selectedElement && selectedElementType) {
        e.preventDefault();
        
        if (selectedElementType === 'node') {
          const id = selectedElement.dataset.id;
          if (id === 'nexus-cloud') {
            alert('Nexus-Cloud is the root control plane and cannot be removed!');
            return;
          }
          if (confirm(`Remove node ${systemsMap.get(id).name} from board?`)) {
            delete layout.nodePositions[id];
            layout.customConnections = layout.customConnections.filter(c => c.from !== id && c.to !== id);
            layout.rerouteNodes = layout.rerouteNodes.filter(r => r.from !== id && r.to !== id);
            deselectAll();
            renderBoard();
            renderLibrary();
          }
        } else if (selectedElementType === 'comment') {
          const id = selectedElement.dataset.id;
          layout.commentBoxes = layout.commentBoxes.filter(c => c.id !== id);
          deselectAll();
          renderBoard();
        } else if (selectedElementType === 'reroute') {
          const id = selectedElement.dataset.id;
          layout.rerouteNodes = layout.rerouteNodes.filter(r => r.id !== id);
          deselectAll();
          renderBoard();
        } else if (selectedElementType === 'connection') {
          const type = selectedElement.dataset.type;
          const from = selectedElement.dataset.from;
          const to = selectedElement.dataset.to;
          
          if (type === 'custom') {
            const idx = parseInt(selectedElement.dataset.customIdx);
            layout.customConnections.splice(idx, 1);
          } else {
            const key = `${from}->${to}`;
            layout.hiddenConnections.push(key);
          }
          
          deselectAll();
          renderBoard();
          showHUDMessage('Cable severed');
        }
      }
    }
    
    if (e.key === 'Escape') {
      deselectAll();
      resetPathHighlights();
      // Reset zoom to 1:1 at current center
      btnZoomReset.click();
    }
  });

  // Help Modal Handlers
  btnHelp.onclick = () => helpModal.classList.add('active');
  btnCloseHelp.onclick = () => helpModal.classList.remove('active');
  helpModal.onclick = () => helpModal.classList.remove('active');

  // Save Button Handler
  btnSave.onclick = () => saveLayout();

  // Load Layout & Registry on init
  loadData();
});
