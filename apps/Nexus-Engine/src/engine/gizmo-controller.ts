import * as BABYLON from "@babylonjs/core";

export type GizmoMode = "translate" | "rotate" | "scale";

/**
 * EditorGizmos — wraps Babylon's GizmoManager.
 * Attach to any transform node to show translate / rotate / scale handles.
 * Keyboard shortcuts (W/E/R) are wired in main.ts.
 */
export class EditorGizmos {
  private manager: BABYLON.GizmoManager;
  private mode: GizmoMode = "translate";
  private enabled = true;

  constructor(scene: BABYLON.Scene) {
    this.manager = new BABYLON.GizmoManager(scene);
    this.manager.usePointerToAttachGizmos = false;
    this.setMode("translate");
  }

  attachTo(node: BABYLON.TransformNode | null): void {
    if (this.enabled) {
      this.manager.attachToNode(node);
    }
  }

  setMode(mode: GizmoMode): void {
    this.mode = mode;
    this.manager.positionGizmoEnabled = this.enabled && mode === "translate";
    this.manager.rotationGizmoEnabled = this.enabled && mode === "rotate";
    this.manager.scaleGizmoEnabled = this.enabled && mode === "scale";
    this.manager.boundingBoxGizmoEnabled = false;
  }

  getMode(): GizmoMode {
    return this.mode;
  }

  /** Enable in editor mode, disable when game is running. */
  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
    if (!enabled) {
      this.manager.positionGizmoEnabled = false;
      this.manager.rotationGizmoEnabled = false;
      this.manager.scaleGizmoEnabled = false;
      this.manager.boundingBoxGizmoEnabled = false;
      this.manager.attachToNode(null);
    } else {
      this.setMode(this.mode);
    }
  }

  dispose(): void {
    this.manager.dispose();
  }
}
