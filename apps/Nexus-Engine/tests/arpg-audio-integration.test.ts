import { describe, expect, it } from "vitest";
import * as BABYLON from "@babylonjs/core";
import { ARPGGameManager } from "../src/game/arpg-manager";
import { StubAudioEngineSystem, type AudioBackendAdapter, type AudioEngineVoice } from "../src/systems/audio-engine-system";
import type { ARPGInputCommand, ARPGInputState } from "../src/systems/arpg-input-system";
import type { EnemyRuntimeState } from "../src/systems/arpg-runtime-state";

describe("arpg audio integration", () => {
  it("queues melee hit, enemy death, and ui confirm audio in one frame", () => {
    const startedVoices: AudioEngineVoice[] = [];
    const appliedSnapshots: string[] = [];

    const audioAdapter: AudioBackendAdapter = {
      startVoice: (voice) => startedVoices.push(voice),
      applySnapshot: (snapshotId) => appliedSnapshots.push(snapshotId),
    };
    const audioSystem = new StubAudioEngineSystem({ backendAdapter: audioAdapter });

    const inputState: ARPGInputState = {
      keys: {},
      orbitYaw: 0,
      orbitPitch: 0.45,
      isOrbiting: false,
      cameraZoomDistance: 7,
    };
    const queuedCommands: ARPGInputCommand[] = ["melee", "cycleCameraPreset"];

    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const playerMesh = BABYLON.MeshBuilder.CreateBox("player", { size: 1 }, scene);
    const enemyMesh = BABYLON.MeshBuilder.CreateBox("enemy", { size: 1 }, scene);
    const enemies: EnemyRuntimeState[] = [{ mesh: enemyMesh, health: 1, maxHealth: 1 }];

    const inputSystem = {
      bindInput: () => {},
      resetForPause: () => {},
      getState: () => inputState,
      consumeCommands: () => queuedCommands.splice(0, queuedCommands.length),
      setCameraZoomDistance: (value: number) => {
        inputState.cameraZoomDistance = value;
      },
    };

    const manager = new ARPGGameManager({
      inputSystem,
      cooldownSystem: { tickCooldown: (current: number) => current },
      movementSystem: {
        update: (input: { facingDirection: BABYLON.Vector3 }) => input.facingDirection,
      },
      cameraSystem: {
        update: (input: { activeLockTarget: BABYLON.Mesh | null }) => ({
          cameraMode: "isometric" as const,
          activeLockTarget: input.activeLockTarget,
        }),
      },
      cameraInteractionSystem: {
        handleCommand: (command: ARPGInputCommand, context: {
          setCameraZoomDistance: (value: number) => void;
        }) => {
          if (command !== "cycleCameraPreset") {
            return { handled: false, cameraMode: null };
          }
          context.setCameraZoomDistance(22);
          return { handled: true, cameraMode: "isometric" as const };
        },
      },
      combatSystem: {
        performMeleeAttack: (input: { enemies: EnemyRuntimeState[]; activeLockTarget: BABYLON.Mesh | null }) => ({
          attackCooldown: 0.42,
          enemies: [],
          activeLockTarget: null,
          hits: 1,
          kills: 1,
        }),
      },
      combatInteractionSystem: {
        handleCommand: (command: ARPGInputCommand, context: {
          attackCooldown: number;
          enemies: EnemyRuntimeState[];
          activeLockTarget: BABYLON.Mesh | null;
          onEnemyKilled: (position: BABYLON.Vector3) => void;
        }) => {
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

          context.onEnemyKilled(new BABYLON.Vector3(3, 1, 2));
          return {
            handled: true,
            attackCooldown: 0.42,
            enemies: [],
            activeLockTarget: null,
            hits: 1,
            kills: 1,
            statusMessage: "Melee hit: 1 enemy defeated",
          };
        },
      },
      enemyAiSystem: { update: () => {} },
      lootSystem: { collectNearbyLoot: (loot: EnemyRuntimeState[]) => loot },
      hudSystem: { update: () => {}, setStatus: () => {} },
      terrainInteractionSystem: { handleCommand: () => ({ handled: false, statusMessage: null }) },
      targetingSystem: {
        updateLifecycle: (context: { activeLockTarget: BABYLON.Mesh | null }) => context.activeLockTarget,
        handleCommand: (_command: ARPGInputCommand, context: { activeLockTarget: BABYLON.Mesh | null }) => ({
          handled: false,
          activeLockTarget: context.activeLockTarget,
          statusMessage: null,
        }),
      },
      audioSystem,
    });

    manager["playerMesh"] = playerMesh as never;
    manager["camera"] = {} as never;
    manager["scene"] = scene as never;
    manager["isActive"] = true;
    manager["runtime"].enemies = enemies;
    manager["activeLockTarget"] = enemyMesh as never;

    manager.update(0.016, { getBabylonEngine: () => ({ getFps: () => 120 }) } as never);

    expect(startedVoices.map((voice) => voice.assetId)).toEqual([
      "enemy-death",
      "melee-hit",
      "ui-confirm",
      "combat-loop",
    ]);
    expect(audioSystem.getSnapshot().targetSnapshotId).toBe("combat");
    expect(audioSystem.getSnapshot().activeSnapshotId).toBe("default");
    expect(audioSystem.getSnapshot().effectiveBusGains.music).toBeLessThan(1);
    expect(audioSystem.getSnapshot().effectiveBusGains.music).toBeGreaterThan(0.4);
    expect(appliedSnapshots).not.toContain("combat");
    expect(inputState.cameraZoomDistance).toBe(22);

    scene.dispose();
    engine.dispose();
  });

  it("emits ui navigation, footsteps, and loot pickup audio across update frames", () => {
    const startedVoices: AudioEngineVoice[] = [];

    const audioAdapter: AudioBackendAdapter = {
      startVoice: (voice) => startedVoices.push(voice),
    };
    const audioSystem = new StubAudioEngineSystem({ backendAdapter: audioAdapter });

    const inputState: ARPGInputState = {
      keys: { w: true },
      orbitYaw: 0,
      orbitPitch: 0.45,
      isOrbiting: false,
      cameraZoomDistance: 7,
    };
    const queuedCommands: ARPGInputCommand[] = ["toggleLock"];

    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const playerMesh = BABYLON.MeshBuilder.CreateBox("player", { size: 1 }, scene);

    const inputSystem = {
      bindInput: () => {},
      resetForPause: () => {},
      getState: () => inputState,
      consumeCommands: () => queuedCommands.splice(0, queuedCommands.length),
      setCameraZoomDistance: (_value: number) => {},
    };

    const manager = new ARPGGameManager({
      inputSystem,
      cooldownSystem: { tickCooldown: (current: number) => current },
      movementSystem: {
        update: (input: {
          facingDirection: BABYLON.Vector3;
          playerMesh: BABYLON.Mesh | null;
        }) => {
          input.playerMesh?.position.addInPlace(new BABYLON.Vector3(2, 0, 0));
          return input.facingDirection;
        },
      },
      cameraSystem: {
        update: (input: { activeLockTarget: BABYLON.Mesh | null }) => ({
          cameraMode: "third-person" as const,
          activeLockTarget: input.activeLockTarget,
        }),
      },
      cameraInteractionSystem: {
        handleCommand: () => ({ handled: false, cameraMode: null }),
      },
      combatSystem: {
        performMeleeAttack: () => ({
          attackCooldown: 0,
          enemies: [],
          activeLockTarget: null,
          hits: 0,
          kills: 0,
        }),
      },
      combatInteractionSystem: {
        handleCommand: (_command: ARPGInputCommand, context: {
          attackCooldown: number;
          enemies: EnemyRuntimeState[];
          activeLockTarget: BABYLON.Mesh | null;
        }) => ({
          handled: false,
          attackCooldown: context.attackCooldown,
          enemies: context.enemies,
          activeLockTarget: context.activeLockTarget,
          hits: 0,
          kills: 0,
          statusMessage: null,
        }),
      },
      enemyAiSystem: { update: () => {} },
      lootSystem: {
        collectNearbyLoot: (loot: EnemyRuntimeState[], _player: BABYLON.Mesh | null, _character: unknown, _range: number, onLooted: (itemName: string) => void) => {
          onLooted("Test Relic");
          return loot;
        },
      },
      hudSystem: { update: () => {}, setStatus: () => {} },
      terrainInteractionSystem: { handleCommand: () => ({ handled: false, statusMessage: null }) },
      targetingSystem: {
        updateLifecycle: (context: { activeLockTarget: BABYLON.Mesh | null }) => context.activeLockTarget,
        handleCommand: (command: ARPGInputCommand, context: { activeLockTarget: BABYLON.Mesh | null }) => {
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
            statusMessage: "Lock toggled",
          };
        },
      },
      audioSystem,
    });

    manager["playerMesh"] = playerMesh as never;
    manager["camera"] = {} as never;
    manager["scene"] = scene as never;
    manager["isActive"] = true;

    manager.update(0.016, { getBabylonEngine: () => ({ getFps: () => 120 }) } as never);
    manager.update(0.016, { getBabylonEngine: () => ({ getFps: () => 120 }) } as never);

    expect(startedVoices.map((voice) => voice.assetId)).toContain("ui-navigate");
    expect(startedVoices.map((voice) => voice.assetId)).toContain("ambient-loop");
    expect(startedVoices.map((voice) => voice.assetId)).toContain("footstep");
    expect(startedVoices.map((voice) => voice.assetId)).toContain("loot-pickup");

    scene.dispose();
    engine.dispose();
  });

  it("supports menu and cinematic music states outside the combat loop", () => {
    const startedVoices: AudioEngineVoice[] = [];
    const audioSystem = new StubAudioEngineSystem({
      backendAdapter: {
        startVoice: (voice) => startedVoices.push(voice),
      },
    });

    const manager = new ARPGGameManager({
      inputSystem: {
        bindInput: () => {},
        resetForPause: () => {},
        getState: () => ({
          keys: {},
          orbitYaw: 0,
          orbitPitch: 0.45,
          isOrbiting: false,
          cameraZoomDistance: 7,
        }),
        consumeCommands: () => [],
        setCameraZoomDistance: () => {},
      },
      cooldownSystem: { tickCooldown: (current: number) => current },
      movementSystem: { update: (input: { facingDirection: BABYLON.Vector3 }) => input.facingDirection },
      cameraSystem: {
        update: (input: { activeLockTarget: BABYLON.Mesh | null }) => ({
          cameraMode: "third-person" as const,
          activeLockTarget: input.activeLockTarget,
        }),
      },
      cameraInteractionSystem: { handleCommand: () => ({ handled: false, cameraMode: null }) },
      combatSystem: {
        performMeleeAttack: () => ({
          attackCooldown: 0,
          enemies: [],
          activeLockTarget: null,
          hits: 0,
          kills: 0,
        }),
      },
      combatInteractionSystem: {
        handleCommand: (_command: ARPGInputCommand, context: {
          attackCooldown: number;
          enemies: EnemyRuntimeState[];
          activeLockTarget: BABYLON.Mesh | null;
        }) => ({
          handled: false,
          attackCooldown: context.attackCooldown,
          enemies: context.enemies,
          activeLockTarget: context.activeLockTarget,
          hits: 0,
          kills: 0,
          statusMessage: null,
        }),
      },
      enemyAiSystem: { update: () => {} },
      lootSystem: { collectNearbyLoot: (loot: EnemyRuntimeState[]) => loot },
      hudSystem: { update: () => {}, setStatus: () => {} },
      terrainInteractionSystem: { handleCommand: () => ({ handled: false, statusMessage: null }) },
      targetingSystem: {
        updateLifecycle: (context: { activeLockTarget: BABYLON.Mesh | null }) => context.activeLockTarget,
        handleCommand: (_command: ARPGInputCommand, context: { activeLockTarget: BABYLON.Mesh | null }) => ({
          handled: false,
          activeLockTarget: context.activeLockTarget,
          statusMessage: null,
        }),
      },
      audioSystem,
    });

    manager.tickEditorAudio(1 / 20);
    manager.setMusicModeOverride("cinematic");

    expect(startedVoices.map((voice) => voice.assetId)).toEqual(["menu-loop", "cinematic-loop"]);
    expect(manager.getAudioDebugState().resolvedMusicMode).toBe("cinematic");
  });
});