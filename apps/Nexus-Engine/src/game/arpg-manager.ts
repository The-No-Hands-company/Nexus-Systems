import * as BABYLON from "@babylonjs/core";
import { GameEngine, GameSystem } from "@engine/core";
import {
  Character,
  CharacterStats,
  GameState,
  Item,
  ItemRarity,
  ItemType,
  Skill,
} from "@game-types/arpg";
import {
  ARPGCameraSystem,
  type ARPGCameraMode,
} from "@systems/arpg-camera-system";
import { ARPGCameraInteractionSystem } from "@systems/arpg-camera-interaction-system";
import { ARPGCooldownSystem } from "@systems/arpg-cooldown-system";
import { ARPGCombatSystem } from "@systems/arpg-combat-system";
import { ARPGCombatInteractionSystem } from "@systems/arpg-combat-interaction-system";
import { ARPGEnemyAISystem } from "@systems/arpg-enemy-ai-system";
import { ARPGLootSystem } from "@systems/arpg-loot-system";
import { ARPGMovementSystem } from "@systems/arpg-movement-system";
import {
  createARPGRuntimeState,
  type EnemyRuntimeState,
  type LootRuntimeState,
} from "@systems/arpg-runtime-state";
import { ARPGInputSystem, type ARPGInputCommand } from "@systems/arpg-input-system";
import { ARPGHudSystem } from "@systems/arpg-hud-system";
import { ARPGTerrainInteractionSystem } from "@systems/arpg-terrain-interaction-system";
import { ARPGTargetingSystem } from "@systems/arpg-targeting-system";
import { StubAudioEngineSystem, type IAudioEngine } from "@systems/audio-engine-system";
import { ARPGAudioFeedbackSystem } from "@systems/arpg-audio-feedback-system";
import { BabylonAudioBackendAdapter } from "@systems/babylon-audio-backend";
import { TerrainEditor } from "@tools/terrain-editor";
import { CharacterRiggingAssistant } from "@tools/character-rigging";

type CooldownSystemLike = Pick<ARPGCooldownSystem, "tickCooldown">;
type MovementSystemLike = Pick<ARPGMovementSystem, "update">;
type CameraSystemLike = Pick<ARPGCameraSystem, "update">;
type CameraInteractionSystemLike = Pick<ARPGCameraInteractionSystem, "handleCommand">;
type CombatSystemLike = Pick<ARPGCombatSystem, "performMeleeAttack">;
type CombatInteractionSystemLike = Pick<ARPGCombatInteractionSystem, "handleCommand">;
type EnemyAiSystemLike = Pick<ARPGEnemyAISystem, "update">;
type LootSystemLike = Pick<ARPGLootSystem, "collectNearbyLoot">;
type InputSystemLike = Pick<ARPGInputSystem, "bindInput" | "resetForPause" | "getState" | "consumeCommands" | "setCameraZoomDistance">;
type HudSystemLike = Pick<ARPGHudSystem, "update" | "setStatus">;
type TerrainInteractionSystemLike = Pick<ARPGTerrainInteractionSystem, "handleCommand">;
type TargetingSystemLike = Pick<ARPGTargetingSystem, "updateLifecycle" | "handleCommand">;
type AudioSystemLike = Pick<IAudioEngine, "queuePlayback" | "stopVoice" | "stopVoicesByAssetId" | "ensureLoopPlaying" | "duckBus" | "setBusGain" | "getBusGain" | "applySnapshot" | "update" | "getSnapshot">;

type AudioControlBus = "master" | "music" | "sfx" | "ui";
type MusicModeOverride = "auto" | "menu" | "ambient" | "cinematic" | "combat";

interface ARPGManagerDependencies {
  cooldownSystem: CooldownSystemLike;
  movementSystem: MovementSystemLike;
  cameraSystem: CameraSystemLike;
  cameraInteractionSystem: CameraInteractionSystemLike;
  combatSystem: CombatSystemLike;
  combatInteractionSystem: CombatInteractionSystemLike;
  enemyAiSystem: EnemyAiSystemLike;
  lootSystem: LootSystemLike;
  inputSystem: InputSystemLike;
  hudSystem: HudSystemLike;
  terrainInteractionSystem: TerrainInteractionSystemLike;
  targetingSystem: TargetingSystemLike;
  audioSystem: AudioSystemLike;
}

/**
 * ARPG Game Manager - controls all ARPG gameplay logic
 */
export class ARPGGameManager implements GameSystem {
  name = "ARPGGameManager";
  private cameraMode: ARPGCameraMode = "third-person";
  private gameState: GameState;
  private scene: BABYLON.Scene | null = null;
  private camera: BABYLON.FreeCamera | null = null;
  private playerMesh: BABYLON.Mesh | null = null;
  private terrain: BABYLON.GroundMesh | null = null;
  private terrainEditor: TerrainEditor | null = null;
  private rigAssistant = new CharacterRiggingAssistant();
  private runtime = createARPGRuntimeState();
  private readonly movementSpeed = 11;
  private readonly enemyAggroRange = 16;
  private readonly enemyStopRange = 1.7;
  private readonly enemyDamage = 12;
  private activeLockTarget: BABYLON.Mesh | null = null;
  private facingDirection = new BABYLON.Vector3(0, 0, 1);
  private cooldownSystem: CooldownSystemLike;
  private movementSystem: MovementSystemLike;
  private cameraSystem: CameraSystemLike;
  private cameraInteractionSystem: CameraInteractionSystemLike;
  private combatSystem: CombatSystemLike;
  private combatInteractionSystem: CombatInteractionSystemLike;
  private enemyAiSystem: EnemyAiSystemLike;
  private lootSystem: LootSystemLike;
  private inputSystem: InputSystemLike;
  private hudSystem: HudSystemLike;
  private terrainInteractionSystem: TerrainInteractionSystemLike;
  private targetingSystem: TargetingSystemLike;
  private audioSystem: AudioSystemLike;
  private audioFeedbackSystem: ARPGAudioFeedbackSystem;
  private footstepDistanceAccumulator = 0;
  private readonly footstepStepDistance = 1.8;
  private currentMusicLoopAssetId: string | null = null;
  private musicModeOverride: MusicModeOverride = "auto";
  /** True only in Play mode — gates all input and game logic. */
  private isActive = false;

  constructor(deps: Partial<ARPGManagerDependencies> = {}) {
    this.gameState = this.createNewGame();
    this.cooldownSystem = deps.cooldownSystem ?? new ARPGCooldownSystem();
    this.movementSystem = deps.movementSystem ?? new ARPGMovementSystem();
    this.cameraSystem = deps.cameraSystem ?? new ARPGCameraSystem();
    this.cameraInteractionSystem = deps.cameraInteractionSystem ?? new ARPGCameraInteractionSystem();
    this.combatSystem = deps.combatSystem ?? new ARPGCombatSystem();
    this.combatInteractionSystem = deps.combatInteractionSystem ?? new ARPGCombatInteractionSystem();
    this.enemyAiSystem = deps.enemyAiSystem ?? new ARPGEnemyAISystem();
    this.lootSystem = deps.lootSystem ?? new ARPGLootSystem();
    this.inputSystem = deps.inputSystem ?? new ARPGInputSystem();
    this.hudSystem = deps.hudSystem ?? new ARPGHudSystem();
    this.terrainInteractionSystem = deps.terrainInteractionSystem ?? new ARPGTerrainInteractionSystem();
    this.targetingSystem = deps.targetingSystem ?? new ARPGTargetingSystem();
    this.audioSystem = deps.audioSystem ?? new StubAudioEngineSystem();
    this.audioFeedbackSystem = new ARPGAudioFeedbackSystem(this.audioSystem);
  }

  onRegister(engine: GameEngine): void {
    this.scene = engine.getScene();
    if (!this.scene) {
      console.error("[ARPGGameManager] No scene available");
      return;
    }

    this.bindInput();
    this.setupScene();
    this.createPlayer();
    this.spawnStarterEnemies();
    this.validateRigPreset();
    this.syncMusicLoopState();
    this.audioSystem.update(1 / 60);
  }

  onStart(_engine: GameEngine): void {
    this.isActive = true;
    this.syncMusicLoopState();
    this.audioSystem.update(1 / 60);
    // Make sure game camera is active when playing
    if (this.camera && this.scene) {
      this.scene.activeCamera = this.camera;
    }
  }

  onStop(_engine: GameEngine): void {
    this.isActive = false;
    this.inputSystem.resetForPause();
    this.syncMusicLoopState();
    this.audioSystem.update(1 / 60);
  }

  /** Expose the game camera so main.ts can switch back to it on Play. */
  getGameCamera(): BABYLON.FreeCamera | null {
    return this.camera;
  }

  tickEditorAudio(deltaTime: number): void {
    if (this.isActive) return;
    this.syncMusicLoopState();
    this.audioSystem.update(deltaTime);
  }

  setAudioBusGain(bus: AudioControlBus, gain: number): void {
    this.audioSystem.setBusGain(bus, gain);
  }

  previewAudioDuck(bus: AudioControlBus, targetGain = 0.55): void {
    this.audioSystem.duckBus(bus, targetGain, 0.04, 0.1, 0.22);
  }

  setMusicModeOverride(mode: MusicModeOverride): void {
    this.musicModeOverride = mode;
    this.syncMusicLoopState();
    this.audioSystem.update(1 / 60);
  }

  getAudioDebugState(): {
    activeLoopAssetId: string | null;
    musicModeOverride: MusicModeOverride;
    resolvedMusicMode: Exclude<MusicModeOverride, "auto">;
    snapshot: ReturnType<AudioSystemLike["getSnapshot"]>;
  } {
    return {
      activeLoopAssetId: this.currentMusicLoopAssetId,
      musicModeOverride: this.musicModeOverride,
      resolvedMusicMode: this.resolveMusicMode(),
      snapshot: this.audioSystem.getSnapshot(),
    };
  }

  update(deltaTime: number, engine: GameEngine): void {
    if (!this.isActive) return;
    this.activeLockTarget = this.targetingSystem.updateLifecycle({
      playerMesh: this.playerMesh,
      enemies: this.runtime.enemies,
      activeLockTarget: this.activeLockTarget,
    });
    this.handleInputCommands(this.inputSystem.consumeCommands());
    this.syncMusicLoopState();
    this.audioSystem.update(deltaTime);
    const inputState = this.inputSystem.getState();

    this.gameState.playtime += deltaTime;
    this.runtime.attackCooldown = this.cooldownSystem.tickCooldown(this.runtime.attackCooldown, deltaTime);

    const movementStartPosition = this.playerMesh?.position?.clone?.() ?? null;
    this.facingDirection = this.movementSystem.update({
      playerMesh: this.playerMesh,
      keys: inputState.keys,
      orbitYaw: inputState.orbitYaw,
      deltaTime,
      movementSpeed: this.movementSpeed,
      activeLockTarget: this.activeLockTarget,
      facingDirection: this.facingDirection,
      characterPosition: this.gameState.character.position,
    });
    if (movementStartPosition && this.playerMesh?.position) {
      const movedDistance = BABYLON.Vector3.Distance(movementStartPosition, this.playerMesh.position);
      this.footstepDistanceAccumulator += movedDistance;
      while (this.footstepDistanceAccumulator >= this.footstepStepDistance) {
        this.audioFeedbackSystem.onFootstep();
        this.footstepDistanceAccumulator -= this.footstepStepDistance;
      }
    }
    const cameraResult = this.cameraSystem.update({
      camera: this.camera,
      playerMesh: this.playerMesh,
      deltaTime,
      orbitYaw: inputState.orbitYaw,
      cameraZoomDistance: inputState.cameraZoomDistance,
      activeLockTarget: this.activeLockTarget,
    });
    this.cameraMode = cameraResult.cameraMode;
    this.activeLockTarget = cameraResult.activeLockTarget;
    this.enemyAiSystem.update(this.runtime.enemies, this.playerMesh, deltaTime, {
      aggroRange: this.enemyAggroRange,
      stopRange: this.enemyStopRange,
      speed: 4,
    });
    this.runtime.loot = this.lootSystem.collectNearbyLoot(
      this.runtime.loot,
      this.playerMesh,
      this.gameState.character,
      1.6,
      (itemName) => {
        this.hudSystem.setStatus(`Looted: ${itemName}`);
        this.audioFeedbackSystem.onLootPickup();
      }
    );
    this.hudSystem.update({
      playtimeSeconds: this.gameState.playtime,
      fps: engine.getBabylonEngine().getFps(),
      cameraMode: this.cameraMode,
      cameraZoomDistance: inputState.cameraZoomDistance,
      lockTargetName: this.activeLockTarget?.name ?? null,
    });
  }

  private bindInput(): void {
    this.inputSystem.bindInput(() => this.isActive);
  }

  private setupScene(): void {
    if (!this.scene) return;

    // Add lighting
    const ambientLight = new BABYLON.HemisphericLight(
      "ambient",
      new BABYLON.Vector3(1, 1, 1),
      this.scene
    );
    ambientLight.intensity = 0.7;

    const directionalLight = new BABYLON.PointLight(
      "directional",
      new BABYLON.Vector3(10, 20, 10),
      this.scene
    );
    directionalLight.intensity = 0.8;

    // Setup camera (isometric ARPG angle)
    this.camera = new BABYLON.FreeCamera(
      "camera",
      new BABYLON.Vector3(0, 24, -24),
      this.scene
    );
    this.camera.attachControl(this.scene.getEngine().getRenderingCanvas(), true);
    this.camera.rotation = new BABYLON.Vector3(0.8, 0, 0);
    this.camera.inertia = 0.8;

    // Add ground
    this.terrain = BABYLON.MeshBuilder.CreateGround(
      "ground",
      { width: 120, height: 120, subdivisions: 120, updatable: true },
      this.scene
    ) as BABYLON.GroundMesh;
    this.terrain.material = new BABYLON.StandardMaterial("groundMat", this.scene);
    (this.terrain.material as BABYLON.StandardMaterial).diffuseColor = new BABYLON.Color3(
      0.2,
      0.4,
      0.2
    );

    this.terrainEditor = new TerrainEditor(this.scene, this.terrain);
    this.terrainEditor.setEnabled(false);

    if (this.audioSystem instanceof StubAudioEngineSystem) {
      this.audioSystem.attachBackendAdapter(
        new BabylonAudioBackendAdapter({ scene: this.scene })
      );
    }

    console.log("[ARPGGameManager] Scene setup complete");
  }

  private createPlayer(): void {
    if (!this.scene) return;

    // Create player mesh placeholder
    this.playerMesh = BABYLON.MeshBuilder.CreateCapsule(
      "player",
      { height: 2, radius: 0.5 },
      this.scene
    );
    this.playerMesh.position = new BABYLON.Vector3(0, 1, 0);

    const playerMat = new BABYLON.StandardMaterial("playerMat", this.scene);
    playerMat.diffuseColor = new BABYLON.Color3(1, 0.5, 0);
    this.playerMesh.material = playerMat;

    console.log("[ARPGGameManager] Player created");
  }

  private spawnStarterEnemies(): void {
    if (!this.scene) return;

    const enemyMat = new BABYLON.StandardMaterial("enemyMat", this.scene);
    enemyMat.diffuseColor = new BABYLON.Color3(0.8, 0.1, 0.1);

    const spawnPoints = [
      new BABYLON.Vector3(6, 1, 6),
      new BABYLON.Vector3(-8, 1, 4),
      new BABYLON.Vector3(10, 1, -7),
      new BABYLON.Vector3(-6, 1, -10),
    ];

    spawnPoints.forEach((point, index) => {
      const mesh = BABYLON.MeshBuilder.CreateBox(`enemy_${index}`, { size: 1.6 }, this.scene!);
      mesh.position = point;
      mesh.material = enemyMat;
      this.runtime.enemies.push({ mesh, health: 35, maxHealth: 35 });
    });
  }

  private handleInputCommands(commands: ARPGInputCommand[]): void {
    for (const command of commands) {
      const terrainResult = this.terrainInteractionSystem.handleCommand(command, this.terrainEditor);
      if (terrainResult.handled) {
        this.audioFeedbackSystem.onUiNavigate();
        if (terrainResult.statusMessage) {
          this.hudSystem.setStatus(terrainResult.statusMessage);
        }
        continue;
      }

      const targetingResult = this.targetingSystem.handleCommand(command, {
        playerMesh: this.playerMesh,
        enemies: this.runtime.enemies,
        activeLockTarget: this.activeLockTarget,
      });
      if (targetingResult.handled) {
        this.audioFeedbackSystem.onUiNavigate();
        this.activeLockTarget = targetingResult.activeLockTarget;
        if (targetingResult.statusMessage) {
          this.hudSystem.setStatus(targetingResult.statusMessage);
        }
        continue;
      }

      const combatResult = this.combatInteractionSystem.handleCommand(command, {
        playerMesh: this.playerMesh,
        attackCooldown: this.runtime.attackCooldown,
        enemies: this.runtime.enemies,
        facingDirection: this.facingDirection,
        activeLockTarget: this.activeLockTarget,
        cameraMode: this.cameraMode,
        enemyDamage: this.enemyDamage,
        combatSystem: this.combatSystem,
        onEnemyKilled: (position) => this.handleEnemyKilled(position),
      });
      if (combatResult.handled) {
        this.runtime.attackCooldown = combatResult.attackCooldown;
        this.runtime.enemies = combatResult.enemies;
        this.activeLockTarget = combatResult.activeLockTarget;
        if (combatResult.hits > 0) {
          this.audioFeedbackSystem.onMeleeHit();
        }
        if (combatResult.statusMessage) {
          this.hudSystem.setStatus(combatResult.statusMessage);
        }
        continue;
      }

      const cameraResult = this.cameraInteractionSystem.handleCommand(command, {
        cameraZoomDistance: this.inputSystem.getState().cameraZoomDistance,
        setCameraZoomDistance: (value) => this.inputSystem.setCameraZoomDistance(value),
      });
      if (cameraResult.handled) {
        this.audioFeedbackSystem.onUiConfirm();
        if (cameraResult.cameraMode) {
          this.cameraMode = cameraResult.cameraMode;
        }
        continue;
      }
    }
  }

  private dropLoot(position: BABYLON.Vector3): void {
    if (!this.scene) return;

    const lootMesh = BABYLON.MeshBuilder.CreateSphere(`loot_${Date.now()}`, { diameter: 0.6 }, this.scene);
    lootMesh.position = position.add(new BABYLON.Vector3(0, 0.4, 0));

    const lootMat = new BABYLON.StandardMaterial(`lootMat_${Date.now()}`, this.scene);
    lootMat.diffuseColor = new BABYLON.Color3(0.15, 0.55, 1.0);
    lootMesh.material = lootMat;

    const item: Item = {
      id: `itm_${Date.now()}`,
      name: "Rustforged Axe",
      type: ItemType.Weapon,
      rarity: ItemRarity.Uncommon,
      level: this.gameState.character.level,
      stats: { damage: 8 },
      description: "A rough but deadly axe.",
    };

    this.runtime.loot.push({ mesh: lootMesh, item });
  }

  private handleEnemyKilled(position: BABYLON.Vector3): void {
    this.audioFeedbackSystem.onEnemyDeath(position);
    this.dropLoot(position);
  }

  private syncMusicLoopState(): void {
    const desiredLoopAssetId = this.getLoopAssetForMusicMode(this.resolveMusicMode());
    if (this.currentMusicLoopAssetId === desiredLoopAssetId) {
      this.audioSystem.ensureLoopPlaying(desiredLoopAssetId);
      return;
    }

    if (this.currentMusicLoopAssetId) {
      this.audioSystem.stopVoicesByAssetId(this.currentMusicLoopAssetId);
    }

    this.audioSystem.ensureLoopPlaying(desiredLoopAssetId);
    this.currentMusicLoopAssetId = desiredLoopAssetId;
  }

  private resolveMusicMode(): Exclude<MusicModeOverride, "auto"> {
    if (this.musicModeOverride !== "auto") {
      return this.musicModeOverride;
    }

    if (!this.isActive) {
      return "menu";
    }

    if (this.activeLockTarget !== null || this.runtime.attackCooldown > 0) {
      return "combat";
    }

    if (this.cameraMode === "isometric") {
      return "cinematic";
    }

    return "ambient";
  }

  private getLoopAssetForMusicMode(mode: Exclude<MusicModeOverride, "auto">): string {
    if (mode === "menu") return "menu-loop";
    if (mode === "combat") return "combat-loop";
    if (mode === "cinematic") return "cinematic-loop";
    return "ambient-loop";
  }

  getRuntimeState(): {
    enemies: EnemyRuntimeState[];
    loot: LootRuntimeState[];
    attackCooldown: number;
  } {
    return this.runtime;
  }

  private validateRigPreset(): void {
    const sampleRig = [
      "Hips",
      "Spine",
      "Head",
      "LeftArm",
      "RightArm",
      "LeftLeg",
      "RightLeg",
    ];
    const result = this.rigAssistant.validateAndMap(sampleRig);
    if (!result.valid) {
      this.hudSystem.setStatus("Rig warnings detected in sample mapping");
    }
  }

  private createNewGame(): GameState {
    const baseStats: CharacterStats = {
      health: 100,
      maxHealth: 100,
      mana: 50,
      maxMana: 50,
      strength: 10,
      dexterity: 10,
      intelligence: 10,
      vitality: 10,
      attackSpeed: 1.0,
      castSpeed: 1.0,
      movementSpeed: 5.0,
      armor: 5,
      dodge: 0.05,
      critChance: 0.05,
      critMultiplier: 1.5,
    };

    const character: Character = {
      id: "player_" + Date.now(),
      name: "Wanderer",
      level: 1,
      experience: 0,
      stats: baseStats,
      inventory: [],
      equipment: {},
      skills: this.getStartingSkills(),
      position: { x: 0, y: 1, z: 0 },
    };

    return {
      character,
      currentDungeon: null,
      gameDifficulty: 1,
      playtime: 0,
      gold: 0,
      achievements: [],
    };
  }

  private getStartingSkills(): Skill[] {
    return [
      {
        id: "skill_slash",
        name: "Slash",
        description: "Basic melee attack",
        cooldown: 0.5,
        manaCost: 0,
        damage: 10,
        range: 2,
        icon: "slash",
      },
      {
        id: "skill_dash",
        name: "Dash",
        description: "Quick dash to dodge enemies",
        cooldown: 3,
        manaCost: 10,
        range: 5,
        icon: "dash",
      },
    ];
  }

  getGameState(): GameState {
    return this.gameState;
  }

  getPlayerMesh(): BABYLON.Mesh | null {
    return this.playerMesh;
  }
}
