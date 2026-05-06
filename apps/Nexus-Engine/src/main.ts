import { GameEngine } from "@engine/core";
import { EditorCamera } from "@engine/editor-camera";
import { EditorGrid } from "@engine/editor-grid";
import { EditorGizmos, GizmoMode } from "@engine/gizmo-controller";
import { ARPGGameManager } from "@game/arpg-manager";
import * as BABYLON from "@babylonjs/core";
import { SceneLoader } from "@babylonjs/core/Loading/sceneLoader";
import { FilesInputStore } from "@babylonjs/core/Misc/filesInputStore";
import "@babylonjs/loaders/glTF";
import { buildHierarchyRows, buildUniqueObjectName, collectDescendantIds } from "@engine/editor-scene-utils";
import { PrefabManager, type PrefabDefinition } from "@engine/prefab-manager";

interface SceneObjectLayout {
  name: string;
  position: [number, number, number];
  rotation: [number, number, number];
  scaling: [number, number, number];
  enabled: boolean;
  visible: boolean;
  pickable: boolean;
  parentName?: string;
  nodeKind?: string;
}

interface SceneLayoutFile {
  version: number;
  timestamp: string;
  objects: SceneObjectLayout[];
}

type EditorNode = BABYLON.TransformNode;

async function main() {
  // Setup canvas
  const canvasEl = document.getElementById("gameCanvas");
  if (!(canvasEl instanceof HTMLCanvasElement)) {
    console.error("Canvas element not found!");
    return;
  }
  const canvas = canvasEl;

  // Initialize engine
  const engine = new GameEngine({
    canvas,
    width: window.innerWidth,
    height: window.innerHeight,
    antialiasing: true,
    renderingMode: "webgl",
  });

  // Create game scene
  engine.createScene("main");

  // Switch to game scene
  engine.switchScene("main");

  const scene = engine.getScene();
  if (!scene) {
    console.error("Scene not found after switchScene");
    return;
  }

  // Register ARPG game manager
  const arpgManager = new ARPGGameManager();
  engine.registerSystem(arpgManager);

  // --- Editor subsystems ---
  const editorCamera = new EditorCamera(scene);
  editorCamera.activate(scene);

  const editorGrid = new EditorGrid(scene);
  editorGrid.setVisible(true);

  const editorGizmos = new EditorGizmos(scene);
  const selectionHighlight = new BABYLON.HighlightLayer("__selectionHighlight", scene);

  // Start the always-on render loop (renders even before Play is pressed)
  engine.startEditorLoop();

  const playBtn = document.getElementById("enginePlay");
  const pauseBtn = document.getElementById("enginePause");
  const resetBtn = document.getElementById("engineReset");
  const duplicateSelectionBtn = document.getElementById("duplicateSelection");
  const deleteSelectionBtn = document.getElementById("deleteSelection");
  const importAssetsBtn = document.getElementById("importAssets");
  const importAssetsInput = document.getElementById("importAssetsInput");
  const saveSceneBtn = document.getElementById("engineSaveScene");
  const loadSceneBtn = document.getElementById("engineLoadScene");
  const loadSceneInput = document.getElementById("engineLoadInput");
  const statusEl = document.getElementById("status");

  const objectListEl = document.getElementById("sceneObjectList");
  const hierarchySearchEl = document.getElementById("hierarchySearch") as HTMLInputElement | null;
  const hierarchyMetaEl = document.getElementById("hierarchyMeta");
  const selectedObjectEl = document.getElementById("selectedObject");
  const selectionRectEl = document.getElementById("selectionRect");
  const objectNameInputEl = document.getElementById("objectNameInput") as HTMLInputElement | null;
  const objectEnabledEl = document.getElementById("objectEnabled") as HTMLInputElement | null;
  const objectVisibleEl = document.getElementById("objectVisible") as HTMLInputElement | null;
  const objectPickableEl = document.getElementById("objectPickable") as HTMLInputElement | null;
  const objectPrefabLabelEl = document.getElementById("objectPrefabLabel");
  const objectParentSelectEl = document.getElementById("objectParentSelect") as HTMLSelectElement | null;
  const audioMusicModeEl = document.getElementById("audioMusicMode") as HTMLSelectElement | null;
  const audioActiveLoopEl = document.getElementById("audioActiveLoop");
  const audioSnapshotStateEl = document.getElementById("audioSnapshotState");
  const audioBusMasterEl = document.getElementById("audioBusMaster") as HTMLInputElement | null;
  const audioBusMasterValEl = document.getElementById("audioBusMasterVal");
  const audioBusMusicEl = document.getElementById("audioBusMusic") as HTMLInputElement | null;
  const audioBusMusicValEl = document.getElementById("audioBusMusicVal");
  const audioBusSfxEl = document.getElementById("audioBusSfx") as HTMLInputElement | null;
  const audioBusSfxValEl = document.getElementById("audioBusSfxVal");
  const audioBusUiEl = document.getElementById("audioBusUi") as HTMLInputElement | null;
  const audioBusUiValEl = document.getElementById("audioBusUiVal");
  const audioDuckMusicBtn = document.getElementById("audioDuckMusicBtn");
  const audioResetMixerBtn = document.getElementById("audioResetMixerBtn");
  const posXEl = document.getElementById("posX") as HTMLInputElement | null;
  const posYEl = document.getElementById("posY") as HTMLInputElement | null;
  const posZEl = document.getElementById("posZ") as HTMLInputElement | null;
  const rotXEl = document.getElementById("rotX") as HTMLInputElement | null;
  const rotYEl = document.getElementById("rotY") as HTMLInputElement | null;
  const rotZEl = document.getElementById("rotZ") as HTMLInputElement | null;
  const sclXEl = document.getElementById("sclX") as HTMLInputElement | null;
  const sclYEl = document.getElementById("sclY") as HTMLInputElement | null;
  const sclZEl = document.getElementById("sclZ") as HTMLInputElement | null;

  // Material editor refs
  const matAlbedoEl = document.getElementById("matAlbedo") as HTMLInputElement | null;
  const matEmissiveEl = document.getElementById("matEmissive") as HTMLInputElement | null;
  const matEmissiveIntEl = document.getElementById("matEmissiveInt") as HTMLInputElement | null;
  const matRoughnessEl = document.getElementById("matRoughness") as HTMLInputElement | null;
  const matMetallicEl = document.getElementById("matMetallic") as HTMLInputElement | null;
  const matRoughnessValEl = document.getElementById("matRoughnessVal");
  const matMetallicValEl = document.getElementById("matMetallicVal");
  const matPbrSectionEl = document.getElementById("matPbrSection");
  const matUpgradePbrBtn = document.getElementById("matUpgradePbr");
  const matNewMatBtn = document.getElementById("matNewMat");

  // Prefab panel refs
  const prefabNameInputEl = document.getElementById("prefabNameInput") as HTMLInputElement | null;
  const savePrefabBtn = document.getElementById("savePrefabBtn");
  const exportPrefabBtn = document.getElementById("exportPrefabBtn");
  const importPrefabBtn = document.getElementById("importPrefabBtn");
  const importPrefabInput = document.getElementById("importPrefabInput");
  const prefabListEl = document.getElementById("prefabList");
  const createEmptyNodeBtn = document.getElementById("createEmptyNode");
  const createFolderNodeBtn = document.getElementById("createFolderNode");

  let isRunning = false;
  let selectedMesh: EditorNode | null = null;
  const selectedMeshes = new Set<EditorNode>();
  let currentGizmoMode: GizmoMode = "translate";

  let isPointerDown = false;
  let isBoxSelecting = false;
  let pointerStartX = 0;
  let pointerStartY = 0;
  let hierarchyQuery = "";

  const prefabManager = new PrefabManager();
  let lastSavedPrefab: PrefabDefinition | null = null;
  let hierarchyDraggedNodeId: number | null = null;
  const collapsedHierarchyNodeIds = new Set<number>();

  const setGizmoMode = (mode: GizmoMode) => {
    currentGizmoMode = mode;
    editorGizmos.setMode(mode);
    // Update button active states
    document.getElementById("gizmoTranslate")?.classList.toggle("active", mode === "translate");
    document.getElementById("gizmoRotate")?.classList.toggle("active", mode === "rotate");
    document.getElementById("gizmoScale")?.classList.toggle("active", mode === "scale");
  };

  // W/E/R shortcuts for gizmo modes (only in editor mode)
  window.addEventListener("keydown", (e) => {
    if (isRunning) return; // game handles its own keys

    if (e.key === "Delete") {
      e.preventDefault();
      const toDelete = Array.from(selectedMeshes);
      if (toDelete.length > 0) {
        for (const mesh of toDelete) {
          mesh.dispose();
        }
        clearSelection();
        setStatus(`Deleted ${toDelete.length} object(s)`);
      }
      return;
    }

    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "d") {
      e.preventDefault();
      const toDuplicate = Array.from(selectedMeshes);
      if (toDuplicate.length > 0) {
        const clones: EditorNode[] = [];
        for (const mesh of toDuplicate) {
          const clone = mesh.clone(`${mesh.name}_copy_${Date.now()}`, null);
          if (!clone) continue;
          clone.metadata = { ...(mesh.metadata ?? {}) };
          clone.position = mesh.position.add(new BABYLON.Vector3(0.8, 0, 0.8));
          clones.push(clone);
        }
        if (clones.length > 0) {
          setSelectionFromMeshes(clones, false);
          setStatus(`Duplicated ${clones.length} object(s)`);
        }
      }
      return;
    }

    if (e.key.toLowerCase() === "w") setGizmoMode("translate");
    if (e.key.toLowerCase() === "e") setGizmoMode("rotate");
    if (e.key.toLowerCase() === "r") setGizmoMode("scale");
    if (e.key.toLowerCase() === "f" && selectedMesh instanceof BABYLON.AbstractMesh) {
      editorCamera.focusOn(selectedMesh);
    }
  });

  // FPS updater for editor mode
  const fpsEl = document.getElementById("fps");
  const fpsInterval = window.setInterval(() => {
    if (fpsEl) fpsEl.textContent = `FPS: ${Math.round(engine.getBabylonEngine().getFps())}`;
  }, 500);

  setGizmoMode("translate");

  const setStatus = (message: string) => {
    if (statusEl) {
      statusEl.textContent = `Status: ${message}`;
    }
  };

  const syncAudioMixerPanel = () => {
    const audioState = arpgManager.getAudioDebugState();
    if (audioMusicModeEl) {
      audioMusicModeEl.value = audioState.musicModeOverride;
    }
    if (audioActiveLoopEl) {
      audioActiveLoopEl.textContent = audioState.activeLoopAssetId ?? "None";
    }
    if (audioSnapshotStateEl) {
      const targetSnapshotId = audioState.snapshot.targetSnapshotId;
      audioSnapshotStateEl.textContent = targetSnapshotId
        ? `${audioState.snapshot.activeSnapshotId} -> ${targetSnapshotId}`
        : audioState.snapshot.activeSnapshotId;
    }

    const controls: Array<[HTMLInputElement | null, HTMLElement | null, number]> = [
      [audioBusMasterEl, audioBusMasterValEl, audioState.snapshot.effectiveBusGains.master],
      [audioBusMusicEl, audioBusMusicValEl, audioState.snapshot.effectiveBusGains.music],
      [audioBusSfxEl, audioBusSfxValEl, audioState.snapshot.effectiveBusGains.sfx],
      [audioBusUiEl, audioBusUiValEl, audioState.snapshot.effectiveBusGains.ui],
    ];

    for (const [input, label, value] of controls) {
      if (input) input.value = value.toFixed(2);
      if (label) label.textContent = value.toFixed(2);
    }
  };

  const editorAudioInterval = window.setInterval(() => {
    if (!isRunning) {
      arpgManager.tickEditorAudio(1 / 20);
    }
    syncAudioMixerPanel();
  }, 50);

  const refreshSelectionHighlight = () => {
    selectionHighlight.removeAllMeshes();
    for (const mesh of selectedMeshes) {
      if (mesh instanceof BABYLON.Mesh) {
        selectionHighlight.addMesh(mesh, new BABYLON.Color3(0.2, 0.8, 1.0));
      }
    }
  };

  const syncMaterialEditor = () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh)) {
      if (matAlbedoEl) matAlbedoEl.value = "#a5a5a5";
      if (matEmissiveEl) matEmissiveEl.value = "#000000";
      if (matEmissiveIntEl) matEmissiveIntEl.value = "0";
      if (matPbrSectionEl) matPbrSectionEl.style.display = "none";
      return;
    }
    const mat = selectedMesh.material;
    if (!mat) return;
    if (mat instanceof BABYLON.PBRMaterial) {
      if (matAlbedoEl) matAlbedoEl.value = mat.albedoColor.toHexString().toLowerCase();
      if (matEmissiveEl) matEmissiveEl.value = mat.emissiveColor.toHexString().toLowerCase();
      if (matEmissiveIntEl) matEmissiveIntEl.value = mat.emissiveIntensity.toFixed(2);
      if (matRoughnessEl) matRoughnessEl.value = String(mat.roughness);
      if (matMetallicEl) matMetallicEl.value = String(mat.metallic);
      if (matRoughnessValEl) matRoughnessValEl.textContent = (mat.roughness ?? 0).toFixed(2);
      if (matMetallicValEl) matMetallicValEl.textContent = (mat.metallic ?? 0).toFixed(2);
      if (matPbrSectionEl) matPbrSectionEl.style.display = "";
    } else if (mat instanceof BABYLON.StandardMaterial) {
      if (matAlbedoEl) matAlbedoEl.value = mat.diffuseColor.toHexString().toLowerCase();
      if (matEmissiveEl) matEmissiveEl.value = mat.emissiveColor.toHexString().toLowerCase();
      if (matEmissiveIntEl) matEmissiveIntEl.value = "1";
      if (matPbrSectionEl) matPbrSectionEl.style.display = "none";
    }
  };

  const getPrefabNameForMesh = (mesh: EditorNode | null): string | null => {
    const prefabName = mesh?.metadata?.nexusPrefabName;
    return typeof prefabName === "string" && prefabName.length > 0 ? prefabName : null;
  };

  const getMeshRotation = (mesh: EditorNode): BABYLON.Vector3 => {
    if (mesh.rotationQuaternion) {
      return mesh.rotationQuaternion.toEulerAngles();
    }
    return mesh.rotation;
  };

  const isEditableSceneNode = (node: BABYLON.Node | null): node is EditorNode => {
    if (!(node instanceof BABYLON.TransformNode)) return false;
    if ((node as EditorNode).isDisposed()) return false;
    if (!node.name) return false;
    if (node.name.startsWith("__editor")) return false;
    if (node.name.toLowerCase().includes("gizmo")) return false;
    return true;
  };

  const getEditableSceneNodes = (): EditorNode[] => {
    const unique = new Map<number, EditorNode>();
    for (const node of scene.transformNodes) {
      if (isEditableSceneNode(node)) unique.set(node.uniqueId, node);
    }
    for (const mesh of scene.meshes) {
      if (isEditableSceneNode(mesh)) unique.set(mesh.uniqueId, mesh);
    }
    return Array.from(unique.values());
  };

  const getEditableMeshes = (): BABYLON.AbstractMesh[] => {
    return getEditableSceneNodes().filter((node): node is BABYLON.AbstractMesh => node instanceof BABYLON.AbstractMesh);
  };

  const syncTransformInputs = () => {
    if (!selectedMesh) {
      if (selectedObjectEl) selectedObjectEl.textContent = "Selected: none";
      if (objectNameInputEl) objectNameInputEl.value = "";
      if (objectEnabledEl) objectEnabledEl.checked = false;
      if (objectVisibleEl) {
        objectVisibleEl.checked = false;
        objectVisibleEl.disabled = true;
      }
      if (objectPickableEl) {
        objectPickableEl.checked = false;
        objectPickableEl.disabled = true;
      }
      if (objectPrefabLabelEl) objectPrefabLabelEl.textContent = "None";
      if (objectParentSelectEl) objectParentSelectEl.value = "";
      return;
    }

    if (selectedObjectEl) {
      const extra = selectedMeshes.size > 1 ? ` (+${selectedMeshes.size - 1})` : "";
      selectedObjectEl.textContent = `Selected: ${selectedMesh.name}${extra}`;
    }
    const rotation = getMeshRotation(selectedMesh);

    if (posXEl) posXEl.value = selectedMesh.position.x.toFixed(3);
    if (posYEl) posYEl.value = selectedMesh.position.y.toFixed(3);
    if (posZEl) posZEl.value = selectedMesh.position.z.toFixed(3);

    if (rotXEl) rotXEl.value = rotation.x.toFixed(3);
    if (rotYEl) rotYEl.value = rotation.y.toFixed(3);
    if (rotZEl) rotZEl.value = rotation.z.toFixed(3);

    if (sclXEl) sclXEl.value = selectedMesh.scaling.x.toFixed(3);
    if (sclYEl) sclYEl.value = selectedMesh.scaling.y.toFixed(3);
    if (sclZEl) sclZEl.value = selectedMesh.scaling.z.toFixed(3);

    if (objectNameInputEl) objectNameInputEl.value = selectedMesh.name;
    if (objectEnabledEl) objectEnabledEl.checked = selectedMesh.isEnabled();
    if (objectVisibleEl) {
      objectVisibleEl.disabled = !(selectedMesh instanceof BABYLON.AbstractMesh);
      objectVisibleEl.checked = selectedMesh instanceof BABYLON.AbstractMesh ? selectedMesh.isVisible : true;
    }
    if (objectPickableEl) {
      objectPickableEl.disabled = !(selectedMesh instanceof BABYLON.AbstractMesh);
      objectPickableEl.checked = selectedMesh instanceof BABYLON.AbstractMesh ? selectedMesh.isPickable : false;
    }
    if (objectPrefabLabelEl) objectPrefabLabelEl.textContent = getPrefabNameForMesh(selectedMesh) ?? "None";
    if (objectParentSelectEl) {
      objectParentSelectEl.value = selectedMesh.parent instanceof BABYLON.TransformNode ? String(selectedMesh.parent.uniqueId) : "";
    }

    syncMaterialEditor();
  };

  const clearSelection = () => {
    selectedMesh = null;
    selectedMeshes.clear();
    refreshSelectionHighlight();
    syncTransformInputs();
    renderSceneObjectList();
    if (!isRunning) {
      editorGizmos.attachTo(null);
    }
  };

  const setSingleSelection = (mesh: EditorNode | null) => {
    selectedMeshes.clear();
    selectedMesh = mesh;
    if (mesh) {
      selectedMeshes.add(mesh);
    }
    refreshSelectionHighlight();
    syncTransformInputs();
    renderSceneObjectList();
    if (!isRunning) {
      editorGizmos.attachTo(mesh);
    }
  };

  const toggleSelection = (mesh: EditorNode) => {
    if (selectedMeshes.has(mesh)) {
      selectedMeshes.delete(mesh);
      if (selectedMesh === mesh) {
        selectedMesh = selectedMeshes.values().next().value ?? null;
      }
    } else {
      selectedMeshes.add(mesh);
      selectedMesh = mesh;
    }

    if (selectedMeshes.size === 0) {
      selectedMesh = null;
    }

    refreshSelectionHighlight();
    syncTransformInputs();
    renderSceneObjectList();
    if (!isRunning) {
      editorGizmos.attachTo(selectedMesh);
    }
  };

  const setSelectionFromMeshes = (
    meshes: EditorNode[],
    additive: boolean
  ) => {
    if (!additive) {
      selectedMeshes.clear();
    }

    for (const mesh of meshes) {
      selectedMeshes.add(mesh);
    }

    selectedMesh = meshes.length > 0
      ? meshes[meshes.length - 1]
      : (selectedMeshes.values().next().value ?? null);

    refreshSelectionHighlight();
    syncTransformInputs();
    renderSceneObjectList();
    if (!isRunning) {
      editorGizmos.attachTo(selectedMesh);
    }
  };

  const selectMesh = (mesh: EditorNode | null) => {
    setSingleSelection(mesh);
  };

  const isSelectableSceneMesh = (mesh: BABYLON.AbstractMesh | null): mesh is BABYLON.AbstractMesh => {
    return isEditableSceneNode(mesh);
  };

  const importAssetFiles = async (files: File[]) => {
    if (files.length === 0) return;

    const rootFiles = files.filter((file) => /\.(glb|gltf)$/i.test(file.name));
    if (rootFiles.length === 0) {
      setStatus("Import failed: choose .glb or .gltf file");
      return;
    }

    FilesInputStore.FilesToLoad = {};
    for (const file of files) {
      FilesInputStore.FilesToLoad[file.name] = file;
      FilesInputStore.FilesToLoad[file.name.toLowerCase()] = file;
    }

    let totalImported = 0;
    for (const file of rootFiles) {
      try {
        setStatus(`Importing ${file.name}...`);
        const ext = file.name.toLowerCase().endsWith(".gltf") ? ".gltf" : ".glb";
        const result = await SceneLoader.ImportMeshAsync("", "file:", file, scene, undefined, ext);
        const imported = result.meshes.filter((mesh) => isSelectableSceneMesh(mesh));
        totalImported += imported.length;

        if (imported.length > 0) {
          setSelectionFromMeshes(imported, false);
          editorCamera.focusOn(imported[0]);
        }
      } catch (error) {
        console.error(error);
        setStatus(`Import failed: ${file.name}`);
      }
    }

    if (totalImported > 0) {
      setStatus(`Imported ${totalImported} mesh(es)`);
    }
    renderSceneObjectList();
  };

  const buildHierarchySnapshot = () => {
    return getEditableSceneNodes().map((node) => ({
      id: node.uniqueId,
      name: node.name,
      parentId: node.parent instanceof BABYLON.TransformNode ? node.parent.uniqueId : null,
      node,
    }));
  };

  const getNodeLabel = (node: EditorNode): string => {
    const kind = node.metadata?.nexusNodeKind;
    if (kind === "folder") return `Folder: ${node.name}`;
    if (kind === "empty") return `Empty: ${node.name}`;
    if (node instanceof BABYLON.Mesh) return node.name;
    return `Node: ${node.name}`;
  };

  const syncParentOptions = () => {
    if (!objectParentSelectEl) return;

    const allNodes = getEditableSceneNodes();
    const excludedIds = selectedMesh ? collectDescendantIds(
      allNodes.map((node) => ({
        id: node.uniqueId,
        name: node.name,
        parentId: node.parent instanceof BABYLON.TransformNode ? node.parent.uniqueId : null,
      })),
      selectedMesh.uniqueId
    ) : new Set<number>();

    objectParentSelectEl.innerHTML = "<option value=\"\">No parent</option>";
    for (const node of allNodes.sort((a, b) => a.name.localeCompare(b.name))) {
      if (!selectedMesh || node === selectedMesh) continue;
      if (excludedIds.has(node.uniqueId)) continue;

      const option = document.createElement("option");
      option.value = String(node.uniqueId);
      option.textContent = node.name;
      objectParentSelectEl.appendChild(option);
    }
  };

  const renderSceneObjectList = () => {
    if (!objectListEl) return;

    const allNodes = getEditableSceneNodes();
    const hierarchySnapshot = buildHierarchySnapshot();
    const rows = buildHierarchyRows(hierarchySnapshot, hierarchyQuery, collapsedHierarchyNodeIds);
    objectListEl.innerHTML = "";
    if (hierarchyMetaEl) {
      hierarchyMetaEl.textContent = hierarchyQuery
        ? `${rows.length} of ${allNodes.length} objects`
        : `${allNodes.length} objects`;
    }

    const childCountByParent = new Map<number | null, number>();
    for (const item of hierarchySnapshot) {
      childCountByParent.set(item.parentId, (childCountByParent.get(item.parentId) ?? 0) + 1);
    }

    const nodeById = new Map(hierarchySnapshot.map((item) => [item.id, item.node as EditorNode]));

    for (const row of rows) {
      const mesh = row.item.node as EditorNode;
      const hasChildren = (childCountByParent.get(mesh.uniqueId) ?? 0) > 0;
      const rowEl = document.createElement("div");
      rowEl.className = "scene-object-row";

      const toggleBtn = document.createElement("button");
      toggleBtn.className = `scene-tree-toggle${hasChildren ? "" : " is-empty"}`;
      toggleBtn.textContent = collapsedHierarchyNodeIds.has(mesh.uniqueId) ? "▸" : "▾";
      toggleBtn.addEventListener("click", (event) => {
        event.stopPropagation();
        if (!hasChildren) return;
        if (collapsedHierarchyNodeIds.has(mesh.uniqueId)) {
          collapsedHierarchyNodeIds.delete(mesh.uniqueId);
        } else {
          collapsedHierarchyNodeIds.add(mesh.uniqueId);
        }
        renderSceneObjectList();
      });
      rowEl.appendChild(toggleBtn);

      const btn = document.createElement("button");
      btn.className = "scene-object-btn";
      if (selectedMeshes.has(mesh)) {
        btn.classList.add("selected");
      }
      btn.textContent = getNodeLabel(mesh);
      btn.style.paddingLeft = `${8 + row.depth * 14}px`;
      btn.draggable = true;
      btn.addEventListener("dragstart", () => {
        hierarchyDraggedNodeId = mesh.uniqueId;
      });
      btn.addEventListener("dragend", () => {
        hierarchyDraggedNodeId = null;
      });
      btn.addEventListener("dragover", (event) => {
        if (hierarchyDraggedNodeId === null || hierarchyDraggedNodeId === mesh.uniqueId) return;
        event.preventDefault();
        btn.classList.add("drag-over");
      });
      btn.addEventListener("dragleave", () => {
        btn.classList.remove("drag-over");
      });
      btn.addEventListener("drop", (event) => {
        event.preventDefault();
        btn.classList.remove("drag-over");
        if (hierarchyDraggedNodeId === null) return;
        const dragged = nodeById.get(hierarchyDraggedNodeId) ?? null;
        if (!dragged || dragged === mesh) return;

        const descendants = collectDescendantIds(
          hierarchySnapshot.map((item) => ({ id: item.id, name: item.name, parentId: item.parentId })),
          dragged.uniqueId
        );
        if (descendants.has(mesh.uniqueId)) {
          setStatus("Invalid hierarchy move: cannot parent under a descendant");
          return;
        }

        dragged.parent = mesh;
        renderSceneObjectList();
        syncTransformInputs();
        setStatus(`Reparented ${dragged.name} to ${mesh.name}`);
      });
      btn.addEventListener("click", (event) => {
        if (event.shiftKey) {
          toggleSelection(mesh);
          return;
        }
        setSingleSelection(mesh);
      });
      rowEl.appendChild(btn);
      objectListEl.appendChild(rowEl);
    }

    if (selectedMesh && selectedMesh.isDisposed()) {
      clearSelection();
    }

    syncParentOptions();
  };

  const parseInput = (el: HTMLInputElement | null, fallback: number): number => {
    if (!el) return fallback;
    const parsed = Number(el.value);
    return Number.isFinite(parsed) ? parsed : fallback;
  };

  const applyTransformFromInputs = () => {
    if (!selectedMesh) return;

    selectedMesh.position.x = parseInput(posXEl, selectedMesh.position.x);
    selectedMesh.position.y = parseInput(posYEl, selectedMesh.position.y);
    selectedMesh.position.z = parseInput(posZEl, selectedMesh.position.z);

    selectedMesh.rotationQuaternion = null;
    selectedMesh.rotation.x = parseInput(rotXEl, selectedMesh.rotation.x);
    selectedMesh.rotation.y = parseInput(rotYEl, selectedMesh.rotation.y);
    selectedMesh.rotation.z = parseInput(rotZEl, selectedMesh.rotation.z);

    selectedMesh.scaling.x = parseInput(sclXEl, selectedMesh.scaling.x);
    selectedMesh.scaling.y = parseInput(sclYEl, selectedMesh.scaling.y);
    selectedMesh.scaling.z = parseInput(sclZEl, selectedMesh.scaling.z);
  };

  const transformInputs = [posXEl, posYEl, posZEl, rotXEl, rotYEl, rotZEl, sclXEl, sclYEl, sclZEl];
  for (const input of transformInputs) {
    input?.addEventListener("input", applyTransformFromInputs);
  }

  hierarchySearchEl?.addEventListener("input", () => {
    hierarchyQuery = hierarchySearchEl.value;
    renderSceneObjectList();
  });

  objectListEl?.addEventListener("dragover", (event) => {
    if (hierarchyDraggedNodeId === null) return;
    event.preventDefault();
  });

  objectListEl?.addEventListener("drop", (event) => {
    if (hierarchyDraggedNodeId === null) return;
    event.preventDefault();
    const dragged = getEditableSceneNodes().find((node) => node.uniqueId === hierarchyDraggedNodeId) ?? null;
    if (!dragged) return;
    dragged.parent = null;
    hierarchyDraggedNodeId = null;
    renderSceneObjectList();
    syncTransformInputs();
    setStatus(`Reparented ${dragged.name} to root`);
  });

  objectNameInputEl?.addEventListener("change", () => {
    if (!selectedMesh || !objectNameInputEl) return;
    const uniqueName = buildUniqueObjectName(
      objectNameInputEl.value,
      getEditableSceneNodes()
        .filter((mesh) => mesh !== selectedMesh)
        .map((mesh) => mesh.name)
    );
    selectedMesh.name = uniqueName;
    objectNameInputEl.value = uniqueName;
    renderSceneObjectList();
    syncTransformInputs();
    setStatus(`Renamed object to ${uniqueName}`);
  });

  objectEnabledEl?.addEventListener("change", () => {
    if (!selectedMesh || !objectEnabledEl) return;
    selectedMesh.setEnabled(objectEnabledEl.checked);
    renderSceneObjectList();
    setStatus(`${selectedMesh.name} ${objectEnabledEl.checked ? "enabled" : "disabled"}`);
  });

  objectVisibleEl?.addEventListener("change", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh) || !objectVisibleEl) return;
    selectedMesh.isVisible = objectVisibleEl.checked;
    renderSceneObjectList();
    setStatus(`${selectedMesh.name} ${objectVisibleEl.checked ? "visible" : "hidden"}`);
  });

  objectPickableEl?.addEventListener("change", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh) || !objectPickableEl) return;
    selectedMesh.isPickable = objectPickableEl.checked;
    setStatus(`${selectedMesh.name} ${objectPickableEl.checked ? "pickable" : "not pickable"}`);
  });

  objectParentSelectEl?.addEventListener("change", () => {
    if (!selectedMesh || !objectParentSelectEl) return;
    const parentId = Number(objectParentSelectEl.value);
    const nextParent = Number.isFinite(parentId)
      ? getEditableSceneNodes().find((node) => node.uniqueId === parentId) ?? null
      : null;
    selectedMesh.parent = nextParent;
    renderSceneObjectList();
    syncTransformInputs();
    setStatus(nextParent ? `Parent set to ${nextParent.name}` : "Parent cleared");
  });

  const hideSelectionRect = () => {
    if (!selectionRectEl) return;
    selectionRectEl.style.display = "none";
  };

  const showSelectionRect = (x1: number, y1: number, x2: number, y2: number) => {
    if (!selectionRectEl) return;
    const left = Math.min(x1, x2);
    const top = Math.min(y1, y2);
    const width = Math.abs(x2 - x1);
    const height = Math.abs(y2 - y1);

    selectionRectEl.style.display = "block";
    selectionRectEl.style.left = `${left}px`;
    selectionRectEl.style.top = `${top}px`;
    selectionRectEl.style.width = `${width}px`;
    selectionRectEl.style.height = `${height}px`;
  };

  const pickMeshesInRect = (
    x1: number,
    y1: number,
    x2: number,
    y2: number
  ): BABYLON.AbstractMesh[] => {
    const camera = scene.activeCamera;
    if (!camera) return [];

    const minX = Math.min(x1, x2);
    const maxX = Math.max(x1, x2);
    const minY = Math.min(y1, y2);
    const maxY = Math.max(y1, y2);
    const viewport = camera.viewport.toGlobal(
      engine.getBabylonEngine().getRenderWidth(),
      engine.getBabylonEngine().getRenderHeight()
    );

    const selected: BABYLON.AbstractMesh[] = [];
    for (const mesh of getEditableMeshes()) {
      if (!isSelectableSceneMesh(mesh)) continue;
      if (!mesh.isEnabled() || !mesh.isVisible) continue;

      const world = mesh.getBoundingInfo().boundingSphere.centerWorld;
      const screen = BABYLON.Vector3.Project(
        world,
        BABYLON.Matrix.Identity(),
        scene.getTransformMatrix(),
        viewport
      );

      if (screen.z < 0 || screen.z > 1) continue;
      if (screen.x >= minX && screen.x <= maxX && screen.y >= minY && screen.y <= maxY) {
        selected.push(mesh);
      }
    }

    return selected;
  };

  // Scene viewport picking: click to select, drag for box-select.
  canvas.addEventListener("pointerdown", (event) => {
    if (isRunning) return;
    if (event.button !== 0) return;

    isPointerDown = true;
    isBoxSelecting = false;
    pointerStartX = event.clientX;
    pointerStartY = event.clientY;
    hideSelectionRect();
  });

  canvas.addEventListener("pointermove", (event) => {
    if (isRunning || !isPointerDown) return;
    const dragDistance = Math.hypot(event.clientX - pointerStartX, event.clientY - pointerStartY);
    if (dragDistance < 6) return;

    isBoxSelecting = true;
    showSelectionRect(pointerStartX, pointerStartY, event.clientX, event.clientY);
  });

  canvas.addEventListener("dragover", (event) => {
    if (isRunning) return;
    event.preventDefault();
    if (event.dataTransfer) {
      event.dataTransfer.dropEffect = "copy";
    }
  });

  canvas.addEventListener("drop", async (event) => {
    if (isRunning) return;
    event.preventDefault();
    const dropped = event.dataTransfer?.files ? Array.from(event.dataTransfer.files) : [];
    await importAssetFiles(dropped);
  });

  canvas.addEventListener("pointerup", (event) => {
    if (isRunning) return;
    if (event.button !== 0) return;

    isPointerDown = false;

    if (isBoxSelecting) {
      const pickedMeshes = pickMeshesInRect(
        pointerStartX,
        pointerStartY,
        event.clientX,
        event.clientY
      );
      if (pickedMeshes.length > 0) {
        setSelectionFromMeshes(pickedMeshes, event.shiftKey);
      } else if (!event.shiftKey) {
        clearSelection();
      }
      isBoxSelecting = false;
      hideSelectionRect();
      return;
    }

    const pick = scene.pick(event.clientX, event.clientY, undefined, false, scene.activeCamera);
    const mesh = pick?.pickedMesh ?? null;

    if (isSelectableSceneMesh(mesh)) {
      if (event.shiftKey) {
        toggleSelection(mesh);
      } else {
        setSingleSelection(mesh);
      }
      return;
    }

    // Clicking empty space clears selection.
    if (!mesh && !event.shiftKey) {
      clearSelection();
    }
  });

  const createLayoutFile = (): SceneLayoutFile => {
    const objects: SceneObjectLayout[] = getEditableSceneNodes().map((mesh) => {
      const rot = getMeshRotation(mesh);
      return {
        name: mesh.name,
        position: [mesh.position.x, mesh.position.y, mesh.position.z],
        rotation: [rot.x, rot.y, rot.z],
        scaling: [mesh.scaling.x, mesh.scaling.y, mesh.scaling.z],
        enabled: mesh.isEnabled(),
        visible: mesh instanceof BABYLON.AbstractMesh ? mesh.isVisible : true,
        pickable: mesh instanceof BABYLON.AbstractMesh ? mesh.isPickable : false,
        parentName: mesh.parent?.name,
        nodeKind: typeof mesh.metadata?.nexusNodeKind === "string" ? mesh.metadata.nexusNodeKind : undefined,
      };
    });

    return {
      version: 1,
      timestamp: new Date().toISOString(),
      objects,
    };
  };

  const saveSceneLayout = () => {
    const layout = createLayoutFile();
    const payload = JSON.stringify(layout, null, 2);
    const blob = new Blob([payload], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `nexus-engine-scene-${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
    setStatus(`Scene saved (${layout.objects.length} objects)`);
  };

  const applySceneLayout = (layout: SceneLayoutFile) => {
    let applied = 0;

    for (const item of layout.objects) {
      const mesh = getEditableSceneNodes().find((node) => node.name === item.name) ?? null;
      if (!mesh) continue;

      mesh.position.set(item.position[0], item.position[1], item.position[2]);
      mesh.rotationQuaternion = null;
      mesh.rotation.set(item.rotation[0], item.rotation[1], item.rotation[2]);
      mesh.scaling.set(item.scaling[0], item.scaling[1], item.scaling[2]);
      mesh.setEnabled(item.enabled);
      if (mesh instanceof BABYLON.AbstractMesh) {
        mesh.isVisible = item.visible ?? true;
        mesh.isPickable = item.pickable ?? true;
      }
      mesh.metadata = {
        ...(mesh.metadata ?? {}),
        ...(item.nodeKind ? { nexusNodeKind: item.nodeKind } : {}),
      };
      applied += 1;
    }

    for (const item of layout.objects) {
      const mesh = getEditableSceneNodes().find((node) => node.name === item.name);
      if (!mesh) continue;
      const parent = item.parentName
        ? getEditableSceneNodes().find((node) => node.name === item.parentName) ?? null
        : null;
      mesh.parent = parent;
    }

    renderSceneObjectList();
    syncTransformInputs();
    setStatus(`Scene loaded (${applied}/${layout.objects.length} applied)`);
  };

  if (saveSceneBtn) {
    saveSceneBtn.addEventListener("click", saveSceneLayout);
  }

  if (loadSceneBtn && loadSceneInput instanceof HTMLInputElement) {
    loadSceneBtn.addEventListener("click", () => {
      loadSceneInput.click();
    });

    loadSceneInput.addEventListener("change", async () => {
      const file = loadSceneInput.files?.[0];
      if (!file) return;

      try {
        const raw = await file.text();
        const parsed = JSON.parse(raw) as SceneLayoutFile;
        if (!parsed.objects || !Array.isArray(parsed.objects)) {
          throw new Error("Invalid scene file format");
        }
        applySceneLayout(parsed);
      } catch (error) {
        setStatus("Failed to load scene JSON");
        console.error(error);
      } finally {
        loadSceneInput.value = "";
      }
    });
  }

  if (importAssetsBtn && importAssetsInput instanceof HTMLInputElement) {
    importAssetsBtn.addEventListener("click", () => {
      importAssetsInput.click();
    });

    importAssetsInput.addEventListener("change", async () => {
      const files = importAssetsInput.files ? Array.from(importAssetsInput.files) : [];
      await importAssetFiles(files);
      importAssetsInput.value = "";
    });
  }

  if (playBtn) {
    playBtn.addEventListener("click", () => {
      if (isRunning) return;
      // Switch from editor camera to game camera
      editorCamera.deactivate();
      editorGrid.setVisible(false);
      editorGizmos.setEnabled(false);
      engine.start(); // sets isPlaying = true, calls onStart on systems
      isRunning = true;
      setStatus("Running (Play mode)");
    });
  }

  if (pauseBtn) {
    pauseBtn.addEventListener("click", () => {
      if (!isRunning) return;
      engine.stop(); // sets isPlaying = false, calls onStop on systems
      isRunning = false;
      // Switch back to editor camera
      editorCamera.activate(scene);
      editorGrid.setVisible(true);
      editorGizmos.setEnabled(true);
      editorGizmos.attachTo(selectedMesh);
      setStatus("Paused (Editor mode)");
    });
  }

  if (resetBtn) {
    resetBtn.addEventListener("click", () => {
      window.location.reload();
    });
  }

  if (duplicateSelectionBtn) {
    duplicateSelectionBtn.addEventListener("click", () => {
      const toDuplicate = Array.from(selectedMeshes);
      if (toDuplicate.length === 0) return;

      const clones: EditorNode[] = [];
      for (const mesh of toDuplicate) {
        const clone = mesh.clone(`${mesh.name}_copy_${Date.now()}`, null);
        if (!clone) continue;
        clone.metadata = { ...(mesh.metadata ?? {}) };
        clone.position = mesh.position.add(new BABYLON.Vector3(0.8, 0, 0.8));
        clones.push(clone);
      }
      if (clones.length > 0) {
        setSelectionFromMeshes(clones, false);
        setStatus(`Duplicated ${clones.length} object(s)`);
      }
    });
  }

  if (deleteSelectionBtn) {
    deleteSelectionBtn.addEventListener("click", () => {
      const toDelete = Array.from(selectedMeshes);
      if (toDelete.length === 0) return;

      for (const mesh of toDelete) {
        mesh.dispose();
      }
      clearSelection();
      setStatus(`Deleted ${toDelete.length} object(s)`);
    });
  }

  // Gizmo mode toolbar buttons
  document.getElementById("gizmoTranslate")?.addEventListener("click", () => setGizmoMode("translate"));
  document.getElementById("gizmoRotate")?.addEventListener("click", () => setGizmoMode("rotate"));
  document.getElementById("gizmoScale")?.addEventListener("click", () => setGizmoMode("scale"));

  // Add Object panel
  let addCounter = 0;
  const createSceneNode = (kind: "empty" | "folder") => {
    const nameBase = kind === "folder" ? "Folder" : "Empty";
    const name = buildUniqueObjectName(nameBase, getEditableSceneNodes().map((node) => node.name));
    const node = new BABYLON.TransformNode(name, scene);
    node.metadata = {
      ...(node.metadata ?? {}),
      nexusNodeKind: kind,
    };
    if (selectedMesh) {
      node.parent = selectedMesh;
    } else {
      node.position.copyFrom(editorCamera.getCamera().target);
    }
    selectMesh(node);
    renderSceneObjectList();
    setStatus(`Created ${kind} node ${name}`);
  };

  const addObjectToScene = (type: string) => {
    const n = ++addCounter;
    let mesh: BABYLON.Mesh | null = null;
    // Spawn near editor camera target
    const pos = editorCamera.getCamera().target.clone();
    pos.y = type === "plane" ? 0 : 0.5;

    const mat = new BABYLON.StandardMaterial(`obj_mat_${n}`, scene);
    mat.diffuseColor = new BABYLON.Color3(0.65, 0.65, 0.65);

    switch (type) {
      case "box":
        mesh = BABYLON.MeshBuilder.CreateBox(`Cube_${n}`, { size: 1 }, scene);
        break;
      case "sphere":
        mesh = BABYLON.MeshBuilder.CreateSphere(`Sphere_${n}`, { diameter: 1 }, scene);
        break;
      case "capsule":
        mesh = BABYLON.MeshBuilder.CreateCapsule(`Capsule_${n}`, { height: 2, radius: 0.4 }, scene);
        pos.y = 1;
        break;
      case "cylinder":
        mesh = BABYLON.MeshBuilder.CreateCylinder(`Cylinder_${n}`, { height: 2, diameter: 1 }, scene);
        pos.y = 1;
        break;
      case "plane":
        mesh = BABYLON.MeshBuilder.CreateGround(`Plane_${n}`, { width: 2, height: 2 }, scene);
        break;
      case "pointlight": {
        // Visual placeholder sphere
        mesh = BABYLON.MeshBuilder.CreateSphere(`PointLight_${n}`, { diameter: 0.25 }, scene);
        const lMat = new BABYLON.StandardMaterial(`lightMat_${n}`, scene);
        lMat.diffuseColor = new BABYLON.Color3(1, 0.9, 0.4);
        lMat.emissiveColor = new BABYLON.Color3(1, 0.9, 0.4);
        mesh.material = lMat;
        pos.y = 2;
        const light = new BABYLON.PointLight(`PointLight_src_${n}`, pos.clone(), scene);
        light.intensity = 1.2;
        break;
      }
    }

    if (mesh) {
      if (type !== "pointlight") mesh.material = mat;
      mesh.position = pos;
      selectMesh(mesh);
    }
  };

  createEmptyNodeBtn?.addEventListener("click", () => createSceneNode("empty"));
  createFolderNodeBtn?.addEventListener("click", () => createSceneNode("folder"));

  document.querySelectorAll<HTMLButtonElement>(".add-obj-btn").forEach((btn) => {
    btn.addEventListener("click", () => {
      const type = btn.dataset.type;
      if (type) addObjectToScene(type);
    });
  });

  // ─── Prefab system ────────────────────────────────────────────────────────

  const renderPrefabList = () => {
    if (!prefabListEl) return;
    const all = prefabManager.getAll();
    prefabListEl.innerHTML = "";
    if (all.length === 0) {
      prefabListEl.innerHTML =
        '<div style="font-size:10px;color:#5a7a8a;padding:4px 0;">No prefabs saved yet</div>';
      return;
    }
    const syncPrefabInstances = (def: PrefabDefinition): number => {
      const prefabNodes = getEditableSceneNodes().filter(
        (node) => node.metadata?.nexusPrefabId === def.id
      );
      let updated = 0;
      for (const node of prefabNodes) {
        const objIndex = Number(node.metadata?.nexusPrefabObjectIndex);
        if (!Number.isFinite(objIndex)) continue;
        const objDef = def.objects[objIndex];
        if (!objDef) continue;

        node.rotationQuaternion = null;
        node.rotation.set(objDef.rotation[0], objDef.rotation[1], objDef.rotation[2]);
        node.scaling.set(objDef.scaling[0], objDef.scaling[1], objDef.scaling[2]);

        const rootX = Number(node.metadata?.nexusPrefabRootX);
        const rootY = Number(node.metadata?.nexusPrefabRootY);
        const rootZ = Number(node.metadata?.nexusPrefabRootZ);
        if (Number.isFinite(rootX) && Number.isFinite(rootY) && Number.isFinite(rootZ)) {
          node.position.set(rootX + objDef.position[0], rootY + objDef.position[1], rootZ + objDef.position[2]);
        }

        if (node instanceof BABYLON.AbstractMesh && objDef.materialColor && node.material instanceof BABYLON.StandardMaterial) {
          node.material.diffuseColor = new BABYLON.Color3(...objDef.materialColor);
        }
        updated += 1;
      }
      return updated;
    };

    for (const def of all) {
      const entry = document.createElement("div");
      entry.className = "prefab-entry";

      const nameSpan = document.createElement("span");
      nameSpan.className = "prefab-name";
      nameSpan.textContent = def.name;
      nameSpan.title = `${def.objects.length} object(s) ─ ${def.createdAt.split("T")[0]}`;
      entry.appendChild(nameSpan);

      const spawnBtn = document.createElement("button");
      spawnBtn.className = "prefab-btn";
      spawnBtn.textContent = "Spawn";
      spawnBtn.addEventListener("click", () => {
        const spawnPos = editorCamera.getCamera().target.clone();
        const spawned = prefabManager.spawnPrefab(def, scene, spawnPos);
        if (spawned.length > 0) {
          const instanceId = `inst_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`;
          for (let i = 0; i < spawned.length; i += 1) {
            const mesh = spawned[i];
            mesh.metadata = {
              ...(mesh.metadata ?? {}),
              nexusPrefabId: def.id,
              nexusPrefabName: def.name,
              nexusPrefabInstanceId: instanceId,
              nexusPrefabObjectIndex: i,
              nexusPrefabRootX: spawnPos.x,
              nexusPrefabRootY: spawnPos.y,
              nexusPrefabRootZ: spawnPos.z,
            };
          }
          setSelectionFromMeshes(spawned, false);
          editorCamera.focusOn(spawned[0]);
          setStatus(`Spawned "${def.name}" (${spawned.length} mesh(es))`);
        }
        renderSceneObjectList();
      });
      entry.appendChild(spawnBtn);

      const updateFromSelectionBtn = document.createElement("button");
      updateFromSelectionBtn.className = "prefab-btn";
      updateFromSelectionBtn.textContent = "Update";
      updateFromSelectionBtn.title = "Overwrite prefab with current selection";
      updateFromSelectionBtn.addEventListener("click", () => {
        const meshes = Array.from(selectedMeshes).filter(
          (node): node is BABYLON.AbstractMesh => node instanceof BABYLON.AbstractMesh
        );
        if (meshes.length === 0) {
          setStatus("Select mesh objects to update prefab");
          return;
        }
        const updatedDef = prefabManager.savePrefab(def.name, meshes);
        lastSavedPrefab = updatedDef;
        const synced = syncPrefabInstances(updatedDef);
        renderPrefabList();
        setStatus(`Prefab updated and synced ${synced} instance object(s)`);
      });
      entry.appendChild(updateFromSelectionBtn);

      const syncBtn = document.createElement("button");
      syncBtn.className = "prefab-btn";
      syncBtn.textContent = "Sync";
      syncBtn.title = "Apply prefab definition to placed instances";
      syncBtn.addEventListener("click", () => {
        const synced = syncPrefabInstances(def);
        setStatus(`Synced ${synced} instance object(s) for ${def.name}`);
      });
      entry.appendChild(syncBtn);

      const removeBtn = document.createElement("button");
      removeBtn.className = "prefab-btn";
      removeBtn.textContent = "✕";
      removeBtn.title = "Remove from library";
      removeBtn.addEventListener("click", () => {
        prefabManager.remove(def.name);
        renderPrefabList();
      });
      entry.appendChild(removeBtn);

      prefabListEl.appendChild(entry);
    }
  };

  savePrefabBtn?.addEventListener("click", () => {
    const name = prefabNameInputEl?.value.trim() || `Prefab_${Date.now()}`;
    const meshes = Array.from(selectedMeshes).filter(
      (node): node is BABYLON.AbstractMesh => node instanceof BABYLON.AbstractMesh
    );
    if (meshes.length === 0) {
      setStatus("Select objects first to save as prefab");
      return;
    }
    const def = prefabManager.savePrefab(name, meshes);
    lastSavedPrefab = def;
    if (prefabNameInputEl) prefabNameInputEl.value = "";
    renderPrefabList();
    setStatus(`Saved prefab "${def.name}" (${def.objects.length} object(s))`);
  });

  exportPrefabBtn?.addEventListener("click", () => {
    const target = lastSavedPrefab ?? prefabManager.getAll().at(-1);
    if (!target) {
      setStatus("No prefabs to export");
      return;
    }
    prefabManager.downloadPrefab(target);
  });

  importPrefabBtn?.addEventListener("click", () => {
    if (importPrefabInput instanceof HTMLInputElement) importPrefabInput.click();
  });

  if (importPrefabInput instanceof HTMLInputElement) {
    importPrefabInput.addEventListener("change", async () => {
      const file = importPrefabInput.files?.[0];
      if (!file) return;
      try {
        const raw = await file.text();
        const def = prefabManager.loadFromJSON(raw);
        renderPrefabList();
        setStatus(`Imported prefab "${def.name}"`);
      } catch {
        setStatus("Failed to import prefab JSON");
      } finally {
        importPrefabInput.value = "";
      }
    });
  }

  // ─── Material editor ─────────────────────────────────────────────────────

  matAlbedoEl?.addEventListener("input", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh) || !selectedMesh.material || !matAlbedoEl) return;
    const color = BABYLON.Color3.FromHexString(matAlbedoEl.value);
    if (selectedMesh.material instanceof BABYLON.PBRMaterial) {
      selectedMesh.material.albedoColor = color;
    } else if (selectedMesh.material instanceof BABYLON.StandardMaterial) {
      selectedMesh.material.diffuseColor = color;
    }
  });

  const applyEmissiveChange = () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh) || !selectedMesh.material) return;
    const emissive = matEmissiveEl
      ? BABYLON.Color3.FromHexString(matEmissiveEl.value)
      : new BABYLON.Color3(0, 0, 0);
    if (selectedMesh.material instanceof BABYLON.PBRMaterial) {
      selectedMesh.material.emissiveColor = emissive;
      if (matEmissiveIntEl) {
        selectedMesh.material.emissiveIntensity = parseFloat(matEmissiveIntEl.value) || 0;
      }
    } else if (selectedMesh.material instanceof BABYLON.StandardMaterial) {
      selectedMesh.material.emissiveColor = emissive;
    }
  };
  matEmissiveEl?.addEventListener("input", applyEmissiveChange);
  matEmissiveIntEl?.addEventListener("input", applyEmissiveChange);

  matRoughnessEl?.addEventListener("input", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh) || !(selectedMesh.material instanceof BABYLON.PBRMaterial) || !matRoughnessEl) return;
    selectedMesh.material.roughness = parseFloat(matRoughnessEl.value);
    if (matRoughnessValEl) matRoughnessValEl.textContent = selectedMesh.material.roughness.toFixed(2);
  });

  matMetallicEl?.addEventListener("input", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh) || !(selectedMesh.material instanceof BABYLON.PBRMaterial) || !matMetallicEl) return;
    selectedMesh.material.metallic = parseFloat(matMetallicEl.value);
    if (matMetallicValEl) matMetallicValEl.textContent = selectedMesh.material.metallic.toFixed(2);
  });

  matUpgradePbrBtn?.addEventListener("click", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh)) return;
    if (selectedMesh.material instanceof BABYLON.PBRMaterial) {
      setStatus("Already using PBR material");
      return;
    }
    const oldMat = selectedMesh.material;
    const pbrMat = new BABYLON.PBRMaterial(`${selectedMesh.name}_pbr`, scene);
    pbrMat.metallic = 0;
    pbrMat.roughness = 0.8;
    if (oldMat instanceof BABYLON.StandardMaterial) {
      pbrMat.albedoColor = oldMat.diffuseColor.clone();
      pbrMat.emissiveColor = oldMat.emissiveColor.clone();
    } else {
      pbrMat.albedoColor = new BABYLON.Color3(0.65, 0.65, 0.65);
    }
    selectedMesh.material = pbrMat;
    syncMaterialEditor();
    setStatus("Upgraded to PBR material");
  });

  matNewMatBtn?.addEventListener("click", () => {
    if (!(selectedMesh instanceof BABYLON.AbstractMesh)) return;
    const newMat = new BABYLON.StandardMaterial(`${selectedMesh.name}_std_mat`, scene);
    newMat.diffuseColor = new BABYLON.Color3(0.65, 0.65, 0.65);
    selectedMesh.material = newMat;
    syncMaterialEditor();
    setStatus("Assigned new standard material");
  });

  audioMusicModeEl?.addEventListener("change", () => {
    arpgManager.setMusicModeOverride(audioMusicModeEl.value as "auto" | "menu" | "ambient" | "cinematic" | "combat");
    syncAudioMixerPanel();
    setStatus(`Music mode: ${audioMusicModeEl.value}`);
  });

  const bindAudioBusControl = (
    input: HTMLInputElement | null,
    label: HTMLElement | null,
    bus: "master" | "music" | "sfx" | "ui"
  ) => {
    input?.addEventListener("input", () => {
      const value = Number(input.value);
      arpgManager.setAudioBusGain(bus, value);
      if (label) label.textContent = value.toFixed(2);
      syncAudioMixerPanel();
    });
  };

  bindAudioBusControl(audioBusMasterEl, audioBusMasterValEl, "master");
  bindAudioBusControl(audioBusMusicEl, audioBusMusicValEl, "music");
  bindAudioBusControl(audioBusSfxEl, audioBusSfxValEl, "sfx");
  bindAudioBusControl(audioBusUiEl, audioBusUiValEl, "ui");

  audioDuckMusicBtn?.addEventListener("click", () => {
    arpgManager.previewAudioDuck("music", 0.45);
    setStatus("Previewed music duck");
  });

  audioResetMixerBtn?.addEventListener("click", () => {
    arpgManager.setAudioBusGain("master", 1);
    arpgManager.setAudioBusGain("music", 1);
    arpgManager.setAudioBusGain("sfx", 1);
    arpgManager.setAudioBusGain("ui", 1);
    arpgManager.setMusicModeOverride("auto");
    syncAudioMixerPanel();
    setStatus("Audio mixer reset");
  });

  // Content browser tab switching
  document.querySelectorAll<HTMLButtonElement>(".cb-tab").forEach((tab) => {
    tab.addEventListener("click", () => {
      document.querySelectorAll(".cb-tab").forEach((t) => t.classList.remove("active"));
      document.querySelectorAll(".cb-panel").forEach((p) => p.classList.remove("active"));
      tab.classList.add("active");
      document.getElementById(`tab-${tab.dataset.tab}`)?.classList.add("active");
    });
  });

  renderSceneObjectList();
  if (getEditableSceneNodes().length > 0) {
    selectMesh(getEditableSceneNodes()[0]);
  }
  renderPrefabList();
  syncAudioMixerPanel();
  const objectListRefresh = window.setInterval(renderSceneObjectList, 800);

  setStatus("Editor ready — press Play to run game");

  console.log("[Main] Engine loaded in editor-ready state");

  // Handle window close
  window.addEventListener("beforeunload", () => {
    window.clearInterval(objectListRefresh);
    window.clearInterval(fpsInterval);
    window.clearInterval(editorAudioInterval);
    selectionHighlight.dispose();
    editorCamera.dispose();
    editorGrid.dispose();
    editorGizmos.dispose();
    engine.dispose();
  });
}

// Start when DOM is loaded
if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", main);
} else {
  main();
}
