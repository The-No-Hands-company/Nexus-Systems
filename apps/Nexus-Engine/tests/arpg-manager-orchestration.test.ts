import { describe, expect, it } from "vitest";
import * as BABYLON from "@babylonjs/core";
import { ARPGGameManager } from "../src/game/arpg-manager";
import type { ARPGInputCommand, ARPGInputState } from "../src/systems/arpg-input-system";
import type { EnemyRuntimeState } from "../src/systems/arpg-runtime-state";

describe("arpg manager orchestration", () => {
  it("runs targeting, command dispatch, movement, and camera in update order", () => {
    const order: string[] = [];

    const inputState: ARPGInputState = {
      keys: {},
      orbitYaw: 0,
      orbitPitch: 0.45,
      isOrbiting: false,
      cameraZoomDistance: 7,
    };

    const queuedCommands: ARPGInputCommand[] = ["melee"];

    const inputSystem = {
      bindInput: () => {},
      resetForPause: () => {},
      getState: () => inputState,
      consumeCommands: () => queuedCommands.splice(0, queuedCommands.length),
      setCameraZoomDistance: (value: number) => {
        inputState.cameraZoomDistance = value;
      },
    };

    const cooldownSystem = {
      tickCooldown: (current: number) => current,
    };

    const movementSystem = {
      update: (input: {
        facingDirection: BABYLON.Vector3;
        characterPosition: { x: number; y: number; z: number };
      }) => {
        order.push("movement.update");
        input.characterPosition.x = 1;
        input.characterPosition.y = 2;
        input.characterPosition.z = 3;
        return input.facingDirection;
      },
    };

    const cameraSystem = {
      update: (input: { activeLockTarget: BABYLON.Mesh | null }) => {
        order.push("camera.update");
        return {
          cameraMode: "third-person" as const,
          activeLockTarget: input.activeLockTarget,
        };
      },
    };

    const cameraInteractionSystem = {
      handleCommand: (command: ARPGInputCommand, context: {
        cameraZoomDistance: number;
        setCameraZoomDistance: (value: number) => void;
      }) => {
        order.push(`cameraInteraction.handleCommand:${command}`);
        if (command !== "cycleCameraPreset") {
          return { handled: false, cameraMode: null };
        }
        context.setCameraZoomDistance(22);
        return { handled: true, cameraMode: "isometric" as const };
      },
    };

    const combatSystem = {
      performMeleeAttack: (input: { activeLockTarget: BABYLON.Mesh | null; enemies: EnemyRuntimeState[] }) => {
        order.push("combat.performMeleeAttack");
        return {
          attackCooldown: 0.42,
          enemies: input.enemies,
          activeLockTarget: input.activeLockTarget,
          hits: 0,
          kills: 0,
        };
      },
    };

    const combatInteractionSystem = {
      handleCommand: (command: ARPGInputCommand, context: {
        attackCooldown: number;
        enemies: EnemyRuntimeState[];
        activeLockTarget: BABYLON.Mesh | null;
      }) => {
        order.push(`combatInteraction.handleCommand:${command}`);
        if (command !== "melee") {
          return {
            handled: false,
            attackCooldown: context.attackCooldown,
            enemies: context.enemies,
            activeLockTarget: context.activeLockTarget,
            hits: 0,
            kills: 0,
            statusMessage: null,
          };
        }

        const result = combatSystem.performMeleeAttack(context);
        return {
          handled: true,
          attackCooldown: result.attackCooldown,
          enemies: result.enemies,
          activeLockTarget: result.activeLockTarget,
          hits: result.hits,
          kills: result.kills,
          statusMessage: null,
        };
      },
    };

    const enemyAiSystem = {
      update: () => {
        order.push("enemyAi.update");
      },
    };

    const lootSystem = {
      collectNearbyLoot: (loot: EnemyRuntimeState[]) => {
        order.push("loot.collectNearbyLoot");
        return loot;
      },
    };

    const hudSystem = {
      update: () => {
        order.push("hud.update");
      },
      setStatus: () => {
        order.push("hud.setStatus");
      },
    };

    const audioSystem = {
      queuePlayback: () => {},
      stopVoice: () => false,
      stopVoicesByAssetId: () => 0,
      ensureLoopPlaying: () => true,
      duckBus: () => {},
      setBusGain: () => {},
      getBusGain: () => 1,
      applySnapshot: () => true,
      update: () => ({ processedEvents: 0, activeVoices: 0 }),
      getSnapshot: () => ({
        frame: 0,
        queuedEvents: 0,
        activeVoices: 0,
        activeSnapshotId: "default",
        targetSnapshotId: null,
        busGains: { master: 1, music: 1, sfx: 1, ui: 1 },
        effectiveBusGains: { master: 1, music: 1, sfx: 1, ui: 1 },
      }),
    };

    const terrainInteractionSystem = {
      handleCommand: (command: ARPGInputCommand, _terrainEditor: unknown) => {
        order.push(`terrain.handleCommand:${command}`);
        return { handled: false, statusMessage: null };
      },
    };

    const targetingSystem = {
      updateLifecycle: (context: { activeLockTarget: BABYLON.Mesh | null }) => {
        order.push("targeting.updateLifecycle");
        return context.activeLockTarget;
      },
      handleCommand: (command: ARPGInputCommand, context: {
        activeLockTarget: BABYLON.Mesh | null;
      }) => {
        order.push(`targeting.handleCommand:${command}`);
        return {
          handled: false,
          activeLockTarget: context.activeLockTarget,
          statusMessage: null,
        };
      },
    };

    const manager = new ARPGGameManager({
      inputSystem,
      cooldownSystem,
      movementSystem,
      cameraSystem,
      cameraInteractionSystem,
      combatSystem,
      combatInteractionSystem,
      enemyAiSystem,
      lootSystem,
      hudSystem,
      terrainInteractionSystem,
      targetingSystem,
      audioSystem,
    });

    manager["camera"] = {} as never;
    manager["playerMesh"] = {} as never;
    manager["isActive"] = true;

    const fakeEngine = {
      getBabylonEngine: () => ({ getFps: () => 120 }),
    };

    manager.update(0.016, fakeEngine as never);

    expect(order).toEqual([
      "targeting.updateLifecycle",
      "terrain.handleCommand:melee",
      "targeting.handleCommand:melee",
      "combatInteraction.handleCommand:melee",
      "combat.performMeleeAttack",
      "movement.update",
      "camera.update",
      "enemyAi.update",
      "loot.collectNearbyLoot",
      "hud.update",
    ]);

    expect(manager.getGameState().character.position).toEqual({ x: 1, y: 2, z: 3 });
    expect(manager.getRuntimeState().attackCooldown).toBe(0.42);
  });

  it("processes lock, melee, and camera preset in one frame with deterministic sequencing", () => {
    const order: string[] = [];

    const inputState: ARPGInputState = {
      keys: {},
      orbitYaw: 0,
      orbitPitch: 0.45,
      isOrbiting: false,
      cameraZoomDistance: 7,
    };

    const queuedCommands: ARPGInputCommand[] = ["toggleLock", "melee", "cycleCameraPreset"];

    const inputSystem = {
      bindInput: () => {},
      resetForPause: () => {},
      getState: () => inputState,
      consumeCommands: () => queuedCommands.splice(0, queuedCommands.length),
      setCameraZoomDistance: (value: number) => {
        order.push(`input.setCameraZoomDistance:${value}`);
        inputState.cameraZoomDistance = value;
      },
    };

    const cooldownSystem = {
      tickCooldown: (current: number) => current,
    };

    const movementSystem = {
      update: (input: {
        facingDirection: BABYLON.Vector3;
        characterPosition: { x: number; y: number; z: number };
      }) => {
        order.push("movement.update");
        input.characterPosition.x = 4;
        input.characterPosition.y = 5;
        input.characterPosition.z = 6;
        return input.facingDirection;
      },
    };

    const cameraSystem = {
      update: (input: { activeLockTarget: BABYLON.Mesh | null }) => {
        order.push("camera.update");
        return {
          cameraMode: "isometric" as const,
          activeLockTarget: input.activeLockTarget,
        };
      },
    };

    const combatSystem = {
      performMeleeAttack: (input: { activeLockTarget: BABYLON.Mesh | null; enemies: EnemyRuntimeState[] }) => {
        order.push("combat.performMeleeAttack");
        return {
          attackCooldown: 0.52,
          enemies: input.enemies,
          activeLockTarget: input.activeLockTarget,
          hits: 0,
          kills: 0,
        };
      },
    };

    const combatInteractionSystem = {
      handleCommand: (command: ARPGInputCommand, context: {
        attackCooldown: number;
        enemies: EnemyRuntimeState[];
        activeLockTarget: BABYLON.Mesh | null;
      }) => {
        order.push(`combatInteraction.handleCommand:${command}`);
        if (command !== "melee") {
          return {
            handled: false,
            attackCooldown: context.attackCooldown,
            enemies: context.enemies,
            activeLockTarget: context.activeLockTarget,
            hits: 0,
            kills: 0,
            statusMessage: null,
          };
        }

        const result = combatSystem.performMeleeAttack(context);
        return {
          handled: true,
          attackCooldown: result.attackCooldown,
          enemies: result.enemies,
          activeLockTarget: result.activeLockTarget,
          hits: result.hits,
          kills: result.kills,
          statusMessage: null,
        };
      },
    };

    const cameraInteractionSystem = {
      handleCommand: (command: ARPGInputCommand, context: {
        cameraZoomDistance: number;
        setCameraZoomDistance: (value: number) => void;
      }) => {
        order.push(`cameraInteraction.handleCommand:${command}`);
        if (command !== "cycleCameraPreset") {
          return { handled: false, cameraMode: null };
        }
        expect(context.cameraZoomDistance).toBe(7);
        context.setCameraZoomDistance(22);
        return { handled: true, cameraMode: "isometric" as const };
      },
    };

    const enemyAiSystem = {
      update: () => {
        order.push("enemyAi.update");
      },
    };

    const lootSystem = {
      collectNearbyLoot: (loot: EnemyRuntimeState[]) => {
        order.push("loot.collectNearbyLoot");
        return loot;
      },
    };

    const hudSystem = {
      update: () => {
        order.push("hud.update");
      },
      setStatus: () => {
        order.push("hud.setStatus");
      },
    };

    const audioSystem = {
      queuePlayback: () => {},
      stopVoice: () => false,
      stopVoicesByAssetId: () => 0,
      ensureLoopPlaying: () => true,
      duckBus: () => {},
      setBusGain: () => {},
      getBusGain: () => 1,
      applySnapshot: () => true,
      update: () => ({ processedEvents: 0, activeVoices: 0 }),
      getSnapshot: () => ({
        frame: 0,
        queuedEvents: 0,
        activeVoices: 0,
        activeSnapshotId: "default",
        targetSnapshotId: null,
        busGains: { master: 1, music: 1, sfx: 1, ui: 1 },
        effectiveBusGains: { master: 1, music: 1, sfx: 1, ui: 1 },
      }),
    };

    const terrainInteractionSystem = {
      handleCommand: (command: ARPGInputCommand, _terrainEditor: unknown) => {
        order.push(`terrain.handleCommand:${command}`);
        return { handled: false, statusMessage: null };
      },
    };

    const targetingSystem = {
      updateLifecycle: (context: { activeLockTarget: BABYLON.Mesh | null }) => {
        order.push("targeting.updateLifecycle");
        return context.activeLockTarget;
      },
      handleCommand: (command: ARPGInputCommand, context: {
        activeLockTarget: BABYLON.Mesh | null;
      }) => {
        order.push(`targeting.handleCommand:${command}`);
        if (command !== "toggleLock") {
          return {
            handled: false,
            activeLockTarget: context.activeLockTarget,
            statusMessage: null,
          };
        }
        return {
          handled: true,
          activeLockTarget: context.activeLockTarget,
          statusMessage: "Locked on dummy",
        };
      },
    };

    const manager = new ARPGGameManager({
      inputSystem,
      cooldownSystem,
      movementSystem,
      cameraSystem,
      cameraInteractionSystem,
      combatSystem,
      combatInteractionSystem,
      enemyAiSystem,
      lootSystem,
      hudSystem,
      terrainInteractionSystem,
      targetingSystem,
      audioSystem,
    });

    manager["camera"] = {} as never;
    manager["playerMesh"] = {} as never;
    manager["isActive"] = true;

    const fakeEngine = {
      getBabylonEngine: () => ({ getFps: () => 120 }),
    };

    manager.update(0.016, fakeEngine as never);

    expect(order).toEqual([
      "targeting.updateLifecycle",
      "terrain.handleCommand:toggleLock",
      "targeting.handleCommand:toggleLock",
      "hud.setStatus",
      "terrain.handleCommand:melee",
      "targeting.handleCommand:melee",
      "combatInteraction.handleCommand:melee",
      "combat.performMeleeAttack",
      "terrain.handleCommand:cycleCameraPreset",
      "targeting.handleCommand:cycleCameraPreset",
      "combatInteraction.handleCommand:cycleCameraPreset",
      "cameraInteraction.handleCommand:cycleCameraPreset",
      "input.setCameraZoomDistance:22",
      "movement.update",
      "camera.update",
      "enemyAi.update",
      "loot.collectNearbyLoot",
      "hud.update",
    ]);

    expect(inputState.cameraZoomDistance).toBe(22);
    expect(manager.getRuntimeState().attackCooldown).toBe(0.52);
    expect(manager.getGameState().character.position).toEqual({ x: 4, y: 5, z: 6 });
  });
});