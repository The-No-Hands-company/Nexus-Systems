import * as BABYLON from "@babylonjs/core";

/**
 * EditorCamera — ArcRotateCamera tuned for scene-view editing.
 * LMB: orbit  |  RMB: pan  |  Scroll: zoom
 */
export class EditorCamera {
  private camera: BABYLON.ArcRotateCamera;

  constructor(scene: BABYLON.Scene) {
    this.camera = new BABYLON.ArcRotateCamera(
      "__editorCamera",
      -Math.PI / 2,
      Math.PI / 3.5,
      18,
      BABYLON.Vector3.Zero(),
      scene
    );

    const canvas = scene.getEngine().getRenderingCanvas();
    this.camera.attachControl(canvas, true);

    this.camera.lowerRadiusLimit = 0.5;
    this.camera.upperRadiusLimit = 300;
    this.camera.wheelPrecision = 3;
    this.camera.pinchPrecision = 50;
    this.camera.panningSensibility = 50;
    this.camera.angularSensibilityX = 500;
    this.camera.angularSensibilityY = 500;
    // Reserve LMB for object picking / box-select in editor mode.
    const pointerInput = this.camera.inputs.attached
      .pointers as unknown as { buttons?: number[] } | undefined;
    if (pointerInput) {
      pointerInput.buttons = [1, 2];
    }
    this.camera.inertia = 0.4;
    this.camera.panningInertia = 0.4;
    this.camera.panningDistanceLimit = 300;
  }

  /** Make this the active scene camera and re-attach controls. */
  activate(scene: BABYLON.Scene): void {
    const canvas = scene.getEngine().getRenderingCanvas();
    scene.activeCamera = this.camera;
    this.camera.attachControl(canvas, true);
  }

  /** Detach controls so the game camera can take over. */
  deactivate(): void {
    this.camera.detachControl();
  }

  /** Orbit and zoom to frame a mesh. */
  focusOn(mesh: BABYLON.AbstractMesh): void {
    this.camera.target = mesh.getAbsolutePosition().clone();
    const r = mesh.getBoundingInfo().boundingSphere.radiusWorld;
    this.camera.radius = Math.max(r * 3.5, 5);
  }

  getCamera(): BABYLON.ArcRotateCamera {
    return this.camera;
  }

  dispose(): void {
    this.camera.detachControl();
    this.camera.dispose();
  }
}
