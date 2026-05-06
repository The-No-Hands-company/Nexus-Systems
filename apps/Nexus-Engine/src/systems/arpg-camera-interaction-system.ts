import {
  cycleCameraDistancePreset,
  getCameraModeFromZoom,
  type ARPGCameraMode,
} from "@systems/arpg-camera-system";
import type { ARPGInputCommand } from "@systems/arpg-input-system";

export interface ARPGCameraInteractionContext {
  cameraZoomDistance: number;
  setCameraZoomDistance: (value: number) => void;
}

export interface ARPGCameraInteractionResult {
  handled: boolean;
  cameraMode: ARPGCameraMode | null;
}

export function isCameraInteractionCommand(command: ARPGInputCommand): boolean {
  return command === "cycleCameraPreset";
}

export class ARPGCameraInteractionSystem {
  handleCommand(
    command: ARPGInputCommand,
    context: ARPGCameraInteractionContext
  ): ARPGCameraInteractionResult {
    if (!isCameraInteractionCommand(command)) {
      return { handled: false, cameraMode: null };
    }

    const nextZoom = cycleCameraDistancePreset(context.cameraZoomDistance);
    context.setCameraZoomDistance(nextZoom);

    return {
      handled: true,
      cameraMode: getCameraModeFromZoom(nextZoom),
    };
  }
}