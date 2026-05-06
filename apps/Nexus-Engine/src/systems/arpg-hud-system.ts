import type { ARPGCameraMode } from "@systems/arpg-camera-system";

export interface ARPGHudSnapshot {
  playtimeSeconds: number;
  fps: number;
  cameraMode: ARPGCameraMode;
  cameraZoomDistance: number;
  lockTargetName: string | null;
}

export function getCameraModeLabel(mode: ARPGCameraMode): string {
  if (mode === "first-person") return "First-person";
  if (mode === "third-person") return "Third-person";
  return "Isometric";
}

export class ARPGHudSystem {
  private uiStatusEl: HTMLElement | null = null;
  private uiPlaytimeEl: HTMLElement | null = null;

  update(snapshot: ARPGHudSnapshot): void {
    if (!this.uiPlaytimeEl) {
      this.uiPlaytimeEl = document.getElementById("playtime");
    }

    if (this.uiPlaytimeEl) {
      this.uiPlaytimeEl.textContent = `Playtime: ${Math.floor(snapshot.playtimeSeconds)}s`;
    }

    const fpsEl = document.getElementById("fps");
    if (fpsEl) {
      fpsEl.textContent = `FPS: ${Math.round(snapshot.fps)}`;
    }

    const cameraModeEl = document.getElementById("cameraMode");
    if (cameraModeEl) {
      cameraModeEl.textContent = `Camera: ${getCameraModeLabel(snapshot.cameraMode)} (${snapshot.cameraZoomDistance.toFixed(1)}m)`;
    }

    const lockOnEl = document.getElementById("lockOn");
    if (lockOnEl) {
      lockOnEl.textContent = snapshot.lockTargetName
        ? `Lock-on: ${snapshot.lockTargetName}`
        : "Lock-on: none";
    }
  }

  setStatus(message: string): void {
    if (!this.uiStatusEl) {
      this.uiStatusEl = document.getElementById("status");
    }

    if (this.uiStatusEl) {
      this.uiStatusEl.textContent = `Status: ${message}`;
    }
  }
}
