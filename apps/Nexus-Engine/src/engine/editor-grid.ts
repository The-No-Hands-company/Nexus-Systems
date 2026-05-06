import * as BABYLON from "@babylonjs/core";
import { GridMaterial } from "@babylonjs/materials";

/**
 * EditorGrid — large ground plane with GridMaterial.
 * Gives spatial reference in the editor viewport.
 * Major lines every 5 units, minor every 1 unit.
 */
export class EditorGrid {
  private mesh: BABYLON.Mesh;

  constructor(scene: BABYLON.Scene) {
    this.mesh = BABYLON.MeshBuilder.CreateGround(
      "__editorGrid",
      { width: 500, height: 500 },
      scene
    );
    this.mesh.isPickable = false;
    this.mesh.receiveShadows = false;

    const mat = new GridMaterial("__editorGridMat", scene);
    mat.majorUnitFrequency = 5;
    mat.minorUnitVisibility = 0.45;
    mat.gridRatio = 1;
    mat.backFaceCulling = false;
    mat.mainColor = new BABYLON.Color3(0.05, 0.05, 0.07);
    mat.lineColor = new BABYLON.Color3(0.25, 0.38, 0.48);
    mat.opacity = 0.9;
    this.mesh.material = mat;

    // Slightly below zero to avoid z-fighting with scene geometry at y=0
    this.mesh.position.y = -0.02;
  }

  setVisible(visible: boolean): void {
    this.mesh.setEnabled(visible);
  }

  dispose(): void {
    this.mesh.material?.dispose();
    this.mesh.dispose();
  }
}
