import { describe, expect, it } from "vitest";
import * as BABYLON from "@babylonjs/core";
import { ARPGInputSystem, type ARPGInputCommand } from "../src/systems/arpg-input-system";
import { ARPGCombatInteractionSystem } from "../src/systems/arpg-combat-interaction-system";
import { ARPGCameraInteractionSystem } from "../src/systems/arpg-camera-interaction-system";
import { ARPGTerrainInteractionSystem } from "../src/systems/arpg-terrain-interaction-system";
import { ARPGTargetingSystem } from "../src/systems/arpg-targeting-system";
import { ARPGCombatSystem } from "../src/systems/arpg-combat-system";
import { TerrainEditor } from "../src/tools/terrain-editor";
import type { EnemyRuntimeState } from "../src/systems/arpg-runtime-state";

function processCommands(options: {
  commands: ARPGInputCommand[];
  inputSystem: ARPGInputSystem;
  terrainSystem: ARPGTerrainInteractionSystem;
  terrainEditor: TerrainEditor | null;
  targetingSystem: ARPGTargetingSystem;
  cameraInteractionSystem: ARPGCameraInteractionSystem;
  combatInteractionSystem: ARPGCombatInteractionSystem;
  combatSystem: ARPGCombatSystem;
  playerMesh: BABYLON.Mesh | null;
  enemies: EnemyRuntimeState[];
  activeLockTarget: BABYLON.Mesh | null;
}): { activeLockTarget: BABYLON.Mesh | null; statusMessages: string[]; attackCooldown: number; enemies: EnemyRuntimeState[] } {
  const {
    commands,
    inputSystem,
    terrainSystem,
    terrainEditor,
    targetingSystem,
    cameraInteractionSystem,
    combatInteractionSystem,
    combatSystem,
    playerMesh,
  
  } = options;

  inputSystem.queueCommands(commands);
  let enemies = options.enemies;
  let attackCooldown = 0;
  let activeLockTarget = targetingSystem.updateLifecycle({
    playerMesh,
    enemies,
    activeLockTarget: options.activeLockTarget,
  });
  const statusMessages: string[] = [];

  for (const command of inputSystem.consumeCommands()) {
    const terrainResult = terrainSystem.handleCommand(command, terrainEditor);
    if (terrainResult.handled) {
      if (terrainResult.statusMessage) {
        statusMessages.push(terrainResult.statusMessage);
      }
      continue;
    }

    const targetingResult = targetingSystem.handleCommand(command, {
      playerMesh,
      enemies,
      activeLockTarget,
    });
    if (targetingResult.handled) {
      activeLockTarget = targetingResult.activeLockTarget;
      if (targetingResult.statusMessage) {
        statusMessages.push(targetingResult.statusMessage);
      }
      continue;
    }

    const combatResult = combatInteractionSystem.handleCommand(command, {
      playerMesh,
      attackCooldown,
      enemies,
      facingDirection: new BABYLON.Vector3(0, 0, 1),
      activeLockTarget,
      cameraMode: "third-person",
      enemyDamage: 12,
      combatSystem,
      onEnemyKilled: () => {},
    });
    if (combatResult.handled) {
      attackCooldown = combatResult.attackCooldown;
      enemies = combatResult.enemies;
      activeLockTarget = combatResult.activeLockTarget;
      if (combatResult.statusMessage) {
        statusMessages.push(combatResult.statusMessage);
      }
      continue;
    }

    const cameraResult = cameraInteractionSystem.handleCommand(command, {
      cameraZoomDistance: inputSystem.getState().cameraZoomDistance,
      setCameraZoomDistance: (value) => inputSystem.setCameraZoomDistance(value),
    });
    if (cameraResult.handled) {
      continue;
    }
  }

  return { activeLockTarget, statusMessages, attackCooldown, enemies };
}

describe("arpg command integration", () => {
  it("consumes terrain commands in order and leaves terrain state consistent", () => {
    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const ground = BABYLON.MeshBuilder.CreateGround(
      "ground",
      { width: 10, height: 10, subdivisions: 4, updatable: true },
      scene
    ) as BABYLON.GroundMesh;

    const inputSystem = new ARPGInputSystem();
    const terrainSystem = new ARPGTerrainInteractionSystem();
    const terrainEditor = new TerrainEditor(scene, ground);
    const targetingSystem = new ARPGTargetingSystem();
    const cameraInteractionSystem = new ARPGCameraInteractionSystem();
    const combatInteractionSystem = new ARPGCombatInteractionSystem();
    const combatSystem = new ARPGCombatSystem();

    const result = processCommands({
      commands: ["toggleTerrain", "setTerrainLower", "toggleTerrain"],
      inputSystem,
      terrainSystem,
      terrainEditor,
      targetingSystem,
      cameraInteractionSystem,
      combatInteractionSystem,
      combatSystem,
      playerMesh: null,
      enemies: [],
      activeLockTarget: null,
    });

    expect(result.statusMessages).toEqual([
      "Terrain editor enabled (drag mouse to sculpt)",
      "Terrain editor disabled",
    ]);
    expect(terrainEditor.isEnabled()).toBe(false);
    expect(terrainEditor.getBrush().mode).toBe("lower");

    scene.dispose();
    engine.dispose();
  });

  it("preserves lock and zoom command order across a mixed command stream", () => {
    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const player = BABYLON.MeshBuilder.CreateBox("player", { size: 1 }, scene);
    const enemyNear = BABYLON.MeshBuilder.CreateBox("enemy_near", { size: 1 }, scene);
    const enemyFar = BABYLON.MeshBuilder.CreateBox("enemy_far", { size: 1 }, scene);
    player.position.set(0, 0, 0);
    enemyNear.position.set(4, 0, 0);
    enemyFar.position.set(15, 0, 0);

    const enemies: EnemyRuntimeState[] = [
      { mesh: enemyFar, health: 10, maxHealth: 10 },
      { mesh: enemyNear, health: 10, maxHealth: 10 },
    ];

    const inputSystem = new ARPGInputSystem({ initialZoomDistance: 7 });
    const terrainSystem = new ARPGTerrainInteractionSystem();
    const targetingSystem = new ARPGTargetingSystem();
    const cameraInteractionSystem = new ARPGCameraInteractionSystem();
    const combatInteractionSystem = new ARPGCombatInteractionSystem();
    const combatSystem = new ARPGCombatSystem();

    const result = processCommands({
      commands: ["toggleLock", "cycleCameraPreset", "clearLock", "toggleLock"],
      inputSystem,
      terrainSystem,
      terrainEditor: null,
      targetingSystem,
      cameraInteractionSystem,
      combatInteractionSystem,
      combatSystem,
      playerMesh: player,
      enemies,
      activeLockTarget: null,
    });

    expect(result.statusMessages).toEqual([
      "Locked on enemy_near",
      "Locked on enemy_near",
    ]);
    expect(result.activeLockTarget).toBe(enemyNear);
    expect(inputSystem.getState().cameraZoomDistance).toBe(22);

    scene.dispose();
    engine.dispose();
  });

  it("routes melee through the combat interaction system and updates cooldown", () => {
    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const player = BABYLON.MeshBuilder.CreateBox("player", { size: 1 }, scene);
    const enemy = BABYLON.MeshBuilder.CreateBox("enemy", { size: 1 }, scene);
    player.position.set(0, 0, 0);
    enemy.position.set(1.5, 0, 0);

    const inputSystem = new ARPGInputSystem();
    const terrainSystem = new ARPGTerrainInteractionSystem();
    const targetingSystem = new ARPGTargetingSystem();
    const cameraInteractionSystem = new ARPGCameraInteractionSystem();
    const combatInteractionSystem = new ARPGCombatInteractionSystem();
    const combatSystem = new ARPGCombatSystem();

    const result = processCommands({
      commands: ["melee"],
      inputSystem,
      terrainSystem,
      terrainEditor: null,
      targetingSystem,
      cameraInteractionSystem,
      combatInteractionSystem,
      combatSystem,
      playerMesh: player,
      enemies: [{ mesh: enemy, health: 10, maxHealth: 10 }],
      activeLockTarget: enemy,
    });

    expect(result.attackCooldown).toBeGreaterThan(0);
    expect(result.enemies).toHaveLength(0);
    expect(result.activeLockTarget).toBeNull();
    expect(result.statusMessages).toEqual(["Melee hit: 1 enemy defeated"]);

    scene.dispose();
    engine.dispose();
  });

  it("clears a lock target when lifecycle rules invalidate it", () => {
    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const player = BABYLON.MeshBuilder.CreateBox("player", { size: 1 }, scene);
    const enemy = BABYLON.MeshBuilder.CreateBox("enemy", { size: 1 }, scene);
    player.position.set(0, 0, 0);
    enemy.position.set(2, 0, 0);

    const targetingSystem = new ARPGTargetingSystem();
    let activeLockTarget = targetingSystem.updateLifecycle({
      playerMesh: player,
      enemies: [{ mesh: enemy, health: 10, maxHealth: 10 }],
      activeLockTarget: enemy,
    });
    expect(activeLockTarget).toBe(enemy);

    enemy.setEnabled(false);
    activeLockTarget = targetingSystem.updateLifecycle({
      playerMesh: player,
      enemies: [{ mesh: enemy, health: 10, maxHealth: 10 }],
      activeLockTarget,
    });
    expect(activeLockTarget).toBeNull();

    scene.dispose();
    engine.dispose();
  });
});