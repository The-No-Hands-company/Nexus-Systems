import * as BABYLON from "@babylonjs/core";

export type ARPGCameraMode = "first-person" | "third-person" | "isometric";

export function getCameraModeFromZoom(zoom: number): ARPGCameraMode {
  if (zoom <= 1.2) return "first-person";
  if (zoom <= 10) return "third-person";
  return "isometric";
}

export function cycleCameraDistancePreset(zoom: number): number {
  if (zoom <= 1.2) return 6.5;
  if (zoom <= 10) return 22;
  return 0.6;
}

export interface ARPGCameraUpdateInput {
  camera: BABYLON.FreeCamera | null;
  playerMesh: BABYLON.Mesh | null;
  deltaTime: number;
  orbitYaw: number;
  cameraZoomDistance: number;
  activeLockTarget: BABYLON.Mesh | null;
}

export interface ARPGCameraUpdateOutput {
  cameraMode: ARPGCameraMode;
  activeLockTarget: BABYLON.Mesh | null;
}

export class ARPGCameraSystem {
  update(input: ARPGCameraUpdateInput): ARPGCameraUpdateOutput {
    const {
      camera,
      playerMesh,
      deltaTime,
      orbitYaw,
      cameraZoomDistance,
      activeLockTarget,
    } = input;

    const cameraMode = getCameraModeFromZoom(cameraZoomDistance);
    if (!camera || !playerMesh) {
      return { cameraMode, activeLockTarget };
    }

    let desired: BABYLON.Vector3;
    let target: BABYLON.Vector3;

    const forward = new BABYLON.Vector3(
      Math.sin(orbitYaw),
      0,
      Math.cos(orbitYaw)
    ).normalize();

    if (cameraMode === "first-person") {
      const eyeHeight = new BABYLON.Vector3(0, 1.6, 0);
      desired = playerMesh.position.add(eyeHeight).add(forward.scale(-0.05));
      target = desired.add(forward.scale(8));
    } else {
      const behind = forward.scale(-cameraZoomDistance);
      const cameraHeight =
        cameraMode === "isometric"
          ? Math.max(8, cameraZoomDistance * 0.9 + 2)
          : 2.2 + cameraZoomDistance * 0.2;
      desired = playerMesh.position.add(behind).add(new BABYLON.Vector3(0, cameraHeight, 0));

      if (activeLockTarget) {
        target = BABYLON.Vector3.Lerp(
          playerMesh.position,
          activeLockTarget.position,
          0.45
        ).add(new BABYLON.Vector3(0, 1.2, 0));
      } else {
        target = playerMesh.position.add(new BABYLON.Vector3(0, 1.5, 0));
      }
    }

    camera.position = BABYLON.Vector3.Lerp(camera.position, desired, Math.min(1, deltaTime * 5));
    camera.setTarget(target);

    playerMesh.isVisible = cameraMode !== "first-person";

    return {
      cameraMode,
      activeLockTarget,
    };
  }
}
