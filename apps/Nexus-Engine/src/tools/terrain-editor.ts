import * as BABYLON from "@babylonjs/core";

export interface TerrainBrushSettings {
  radius: number;
  strength: number;
  mode: "raise" | "lower" | "flatten";
}

/**
 * Runtime terrain sculpting helper for prototyping when no authored terrain exists.
 */
export class TerrainEditor {
  private scene: BABYLON.Scene;
  private terrain: BABYLON.GroundMesh;
  private brush: TerrainBrushSettings = { radius: 2.5, strength: 0.25, mode: "raise" };
  private enabled = false;
  private pointerDown = false;

  constructor(scene: BABYLON.Scene, terrain: BABYLON.GroundMesh) {
    this.scene = scene;
    this.terrain = terrain;
    this.bindInput();
  }

  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
  }

  toggleEnabled(): boolean {
    this.enabled = !this.enabled;
    return this.enabled;
  }

  setBrush(brush: Partial<TerrainBrushSettings>): void {
    this.brush = { ...this.brush, ...brush };
  }

  isEnabled(): boolean {
    return this.enabled;
  }

  getBrush(): Readonly<TerrainBrushSettings> {
    return this.brush;
  }

  private bindInput(): void {
    this.scene.onPointerObservable.add((pointerInfo) => {
      if (!this.enabled) return;

      if (pointerInfo.type === BABYLON.PointerEventTypes.POINTERDOWN) {
        this.pointerDown = true;
      }

      if (pointerInfo.type === BABYLON.PointerEventTypes.POINTERUP) {
        this.pointerDown = false;
      }

      if (
        pointerInfo.type === BABYLON.PointerEventTypes.POINTERMOVE &&
        this.pointerDown &&
        this.scene.pointerX >= 0
      ) {
        const pick = this.scene.pick(this.scene.pointerX, this.scene.pointerY, (mesh) => mesh === this.terrain);
        if (pick?.pickedPoint) {
          this.applyBrush(pick.pickedPoint);
        }
      }
    });
  }

  private applyBrush(center: BABYLON.Vector3): void {
    const positions = this.terrain.getVerticesData(BABYLON.VertexBuffer.PositionKind);
    if (!positions) return;

    const world = this.terrain.computeWorldMatrix(true);
    const inv = world.clone().invert();
    const localCenter = BABYLON.Vector3.TransformCoordinates(center, inv);

    for (let i = 0; i < positions.length; i += 3) {
      const vx = positions[i];
      const vy = positions[i + 1];
      const vz = positions[i + 2];

      const dx = vx - localCenter.x;
      const dz = vz - localCenter.z;
      const dist = Math.sqrt(dx * dx + dz * dz);

      if (dist > this.brush.radius) continue;

      const falloff = 1 - dist / this.brush.radius;
      const delta = this.brush.strength * falloff;

      if (this.brush.mode === "raise") {
        positions[i + 1] = vy + delta;
      } else if (this.brush.mode === "lower") {
        positions[i + 1] = vy - delta;
      } else {
        positions[i + 1] = vy + (0 - vy) * 0.1 * falloff;
      }
    }

    this.terrain.updateVerticesData(BABYLON.VertexBuffer.PositionKind, positions);
    this.terrain.refreshBoundingInfo();
  }
}
