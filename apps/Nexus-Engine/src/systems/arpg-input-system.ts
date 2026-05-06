import * as BABYLON from "@babylonjs/core";

export type ARPGInputCommand =
  | "melee"
  | "toggleTerrain"
  | "setTerrainRaise"
  | "setTerrainLower"
  | "toggleLock"
  | "cycleCameraPreset"
  | "clearLock";

export interface ARPGInputState {
  keys: Record<string, boolean>;
  orbitYaw: number;
  orbitPitch: number;
  isOrbiting: boolean;
  cameraZoomDistance: number;
}

export interface ARPGInputSystemConfig {
  initialOrbitYaw?: number;
  initialOrbitPitch?: number;
  initialZoomDistance?: number;
  minZoomDistance?: number;
  maxZoomDistance?: number;
}

export function commandFromKey(key: string): ARPGInputCommand | null {
  const normalized = key.toLowerCase();
  if (normalized === "f") return "melee";
  if (normalized === "t") return "toggleTerrain";
  if (normalized === "1") return "setTerrainRaise";
  if (normalized === "2") return "setTerrainLower";
  if (normalized === "l") return "toggleLock";
  if (normalized === "c") return "cycleCameraPreset";
  if (key === "Escape") return "clearLock";
  return null;
}

export function applyZoomDelta(
  current: number,
  deltaY: number,
  minZoomDistance: number,
  maxZoomDistance: number
): number {
  return BABYLON.Scalar.Clamp(
    current + deltaY * 0.015,
    minZoomDistance,
    maxZoomDistance
  );
}

export class ARPGInputSystem {
  private readonly minZoomDistance: number;
  private readonly maxZoomDistance: number;
  private state: ARPGInputState;
  private commands: ARPGInputCommand[] = [];
  private isBound = false;

  constructor(config: ARPGInputSystemConfig = {}) {
    this.minZoomDistance = config.minZoomDistance ?? 0.25;
    this.maxZoomDistance = config.maxZoomDistance ?? 36;
    this.state = {
      keys: {},
      orbitYaw: config.initialOrbitYaw ?? Math.PI / 4,
      orbitPitch: config.initialOrbitPitch ?? 0.45,
      isOrbiting: false,
      cameraZoomDistance: config.initialZoomDistance ?? 7,
    };
  }

  bindInput(isActive: () => boolean): void {
    if (this.isBound) return;
    this.isBound = true;

    window.addEventListener("keydown", (event) => {
      if (!isActive()) return;
      this.state.keys[event.key.toLowerCase()] = true;
      const command = commandFromKey(event.key);
      if (command) {
        this.commands.push(command);
      }
    });

    window.addEventListener("keyup", (event) => {
      if (!isActive()) return;
      this.state.keys[event.key.toLowerCase()] = false;
    });

    window.addEventListener(
      "wheel",
      (event) => {
        if (!isActive()) return;
        this.state.cameraZoomDistance = applyZoomDelta(
          this.state.cameraZoomDistance,
          event.deltaY,
          this.minZoomDistance,
          this.maxZoomDistance
        );
        event.preventDefault();
      },
      { passive: false }
    );

    window.addEventListener("pointerdown", (event) => {
      if (!isActive()) return;
      if (event.button === 1 || event.button === 2) {
        this.state.isOrbiting = true;
      }
    });

    window.addEventListener("pointerup", (event) => {
      if (!isActive()) return;
      if (event.button === 1 || event.button === 2) {
        this.state.isOrbiting = false;
      }
    });

    window.addEventListener("mousemove", (event) => {
      if (!isActive() || !this.state.isOrbiting) return;
      this.state.orbitYaw += event.movementX * 0.004;
      this.state.orbitPitch -= event.movementY * 0.003;
      this.state.orbitPitch = BABYLON.Scalar.Clamp(this.state.orbitPitch, 0.08, 1.35);
    });

    window.addEventListener("contextmenu", (event) => {
      event.preventDefault();
    });
  }

  consumeCommands(): ARPGInputCommand[] {
    const next = this.commands;
    this.commands = [];
    return next;
  }

  queueCommands(commands: ARPGInputCommand[]): void {
    this.commands.push(...commands);
  }

  setCameraZoomDistance(value: number): void {
    this.state.cameraZoomDistance = BABYLON.Scalar.Clamp(
      value,
      this.minZoomDistance,
      this.maxZoomDistance
    );
  }

  getState(): Readonly<ARPGInputState> {
    return this.state;
  }

  resetForPause(): void {
    this.state.keys = {};
    this.state.isOrbiting = false;
    this.commands = [];
  }
}
