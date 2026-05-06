import { describe, expect, it } from "vitest";
import * as BABYLON from "@babylonjs/core";
import {
  cycleCameraDistancePreset,
  getCameraModeFromZoom,
} from "../src/systems/arpg-camera-system";
import {
  ARPGCombatSystem,
  getCombatTuning,
} from "../src/systems/arpg-combat-system";

describe("arpg camera extracted helpers", () => {
  it("resolves camera mode from zoom breakpoints", () => {
    expect(getCameraModeFromZoom(0.8)).toBe("first-person");
    expect(getCameraModeFromZoom(6.5)).toBe("third-person");
    expect(getCameraModeFromZoom(20)).toBe("isometric");
  });

  it("cycles zoom presets in expected order", () => {
    expect(cycleCameraDistancePreset(0.9)).toBe(6.5);
    expect(cycleCameraDistancePreset(6.5)).toBe(22);
    expect(cycleCameraDistancePreset(22)).toBe(0.6);
  });
});

describe("arpg combat extracted system", () => {
  it("returns unchanged state while attack is on cooldown", () => {
    const system = new ARPGCombatSystem();
    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const player = BABYLON.MeshBuilder.CreateBox("player", { size: 1 }, scene);
    const enemyMesh = BABYLON.MeshBuilder.CreateBox("enemy", { size: 1 }, scene);

    const result = system.performMeleeAttack({
      playerMesh: player,
      attackCooldown: 0.2,
      enemies: [{ mesh: enemyMesh, health: 10, maxHealth: 10 }],
      facingDirection: new BABYLON.Vector3(0, 0, 1),
      activeLockTarget: null,
      cameraMode: "third-person",
      enemyDamage: 12,
      onEnemyKilled: () => {},
    });

    expect(result.attackCooldown).toBe(0.2);
    expect(result.enemies).toHaveLength(1);
    expect(result.kills).toBe(0);

    scene.dispose();
    engine.dispose();
  });

  it("provides tuning values by camera mode", () => {
    expect(getCombatTuning("first-person", 12).range).toBe(2.1);
    expect(getCombatTuning("third-person", 12).range).toBe(3.0);
    expect(getCombatTuning("isometric", 12).range).toBe(4.5);
  });
});
