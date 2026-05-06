import * as BABYLON from "@babylonjs/core";

export interface EngineConfig {
  canvas: HTMLCanvasElement;
  width: number;
  height: number;
  antialiasing: boolean;
  renderingMode: "webgl" | "webgpu";
}

export class GameEngine {
  private babylonEngine: BABYLON.Engine;
  private scenes: Map<string, BABYLON.Scene> = new Map();
  private currentScene: BABYLON.Scene | null = null;
  private systems: GameSystem[] = [];
  private deltaTime: number = 0;
  private lastFrameTime: number = 0;
  /** Whether the game logic is running (Play mode). The render loop always runs. */
  private isPlaying: boolean = false;
  private loopStarted: boolean = false;

  constructor(config: EngineConfig) {
    // Create Babylon.js engine
    this.babylonEngine = new BABYLON.Engine(
      config.canvas,
      config.antialiasing,
      {
        preserveDrawingBuffer: true,
        powerPreference: "high-performance",
      }
    );

    // Handle window resize
    window.addEventListener("resize", () => {
      this.babylonEngine.resize();
    });

    console.log(
      `[Engine] Initialized with ${config.width}x${config.height} canvas`
    );
  }

  /**
   * Create a new scene
   */
  createScene(name: string, options?: BABYLON.SceneOptions): BABYLON.Scene {
    const scene = new BABYLON.Scene(this.babylonEngine, options);
    this.scenes.set(name, scene);
    console.log(`[Engine] Created scene: ${name}`);
    return scene;
  }

  /**
   * Switch to a scene
   */
  switchScene(name: string): boolean {
    const scene = this.scenes.get(name);
    if (!scene) {
      console.warn(`[Engine] Scene not found: ${name}`);
      return false;
    }
    this.currentScene = scene;
    console.log(`[Engine] Switched to scene: ${name}`);
    return true;
  }

  /**
   * Get current active scene
   */
  getScene(): BABYLON.Scene | null {
    return this.currentScene;
  }

  /**
   * Get specific scene by name
   */
  getSceneByName(name: string): BABYLON.Scene | undefined {
    return this.scenes.get(name);
  }

  /**
   * Register a game system (for update loops, etc.)
   */
  registerSystem(system: GameSystem): void {
    this.systems.push(system);
    system.onRegister(this);
  }

  /**
   * Start the always-on render loop (editor loop).
   * Call this once after scene setup. The loop renders every frame
   * regardless of play state, but only calls system.update() when playing.
   */
  startEditorLoop(): void {
    if (this.loopStarted) return;
    if (!this.currentScene) {
      console.error("[Engine] No scene set. Call switchScene() first.");
      return;
    }

    this.loopStarted = true;
    this.lastFrameTime = performance.now();

    this.babylonEngine.runRenderLoop(() => {
      const now = performance.now();
      this.deltaTime = (now - this.lastFrameTime) / 1000;
      this.lastFrameTime = now;

      // Only run game logic when in Play mode
      if (this.isPlaying) {
        for (const system of this.systems) {
          system.update(this.deltaTime, this);
        }
      }

      if (this.currentScene) {
        this.currentScene.render();
      }
    });

    console.log("[Engine] Editor render loop started");
  }

  /**
   * Enter Play mode — game systems start updating.
   */
  start(): void {
    if (this.isPlaying) return;
    this.isPlaying = true;
    for (const system of this.systems) {
      system.onStart?.(this);
    }
    console.log("[Engine] Play mode started");
  }

  /**
   * Exit Play mode — game systems stop updating, render loop continues.
   */
  stop(): void {
    if (!this.isPlaying) return;
    this.isPlaying = false;
    for (const system of this.systems) {
      system.onStop?.(this);
    }
    console.log("[Engine] Play mode stopped");
  }

  getIsPlaying(): boolean {
    return this.isPlaying;
  }

  /**
   * Get delta time since last frame (in seconds)
   */
  getDeltaTime(): number {
    return this.deltaTime;
  }

  /**
   * Get Babylon.js engine instance (for advanced usage)
   */
  getBabylonEngine(): BABYLON.Engine {
    return this.babylonEngine;
  }

  /**
   * Dispose and cleanup
   */
  dispose(): void {
    for (const [name, scene] of this.scenes) {
      scene.dispose();
    }
    this.scenes.clear();
    this.systems = [];
    this.babylonEngine.dispose();
    console.log("[Engine] Engine disposed");
  }
}

/**
 * Base interface for game systems
 */
export interface GameSystem {
  name: string;
  onRegister(engine: GameEngine): void;
  update(deltaTime: number, engine: GameEngine): void;
  /** Called when Play is pressed. */
  onStart?(engine: GameEngine): void;
  /** Called when Pause/Stop is pressed. */
  onStop?(engine: GameEngine): void;
}
