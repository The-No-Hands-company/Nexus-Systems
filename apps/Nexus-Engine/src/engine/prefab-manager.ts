import * as BABYLON from "@babylonjs/core";

export interface PrefabObject {
  name: string;
  /** Position relative to the group's centroid. */
  position: [number, number, number];
  rotation: [number, number, number];
  scaling: [number, number, number];
  /** Extracted diffuse/albedo color for reconstruction. */
  materialColor?: [number, number, number];
}

export interface PrefabDefinition {
  version: 1;
  id: string;
  name: string;
  createdAt: string;
  /** World-space centroid of the original selection. */
  pivot: [number, number, number];
  objects: PrefabObject[];
}

export class PrefabManager {
  private _library = new Map<string, PrefabDefinition>();

  /** Snapshot the current selection into a named prefab and store it. */
  savePrefab(name: string, meshes: BABYLON.AbstractMesh[]): PrefabDefinition {
    const valid = meshes.filter((m) => !m.isDisposed());
    const existing = this._library.get(name);

    // Compute centroid so all positions are stored relative to it.
    let cx = 0, cy = 0, cz = 0;
    for (const m of valid) {
      cx += m.position.x;
      cy += m.position.y;
      cz += m.position.z;
    }
    if (valid.length > 0) {
      cx /= valid.length;
      cy /= valid.length;
      cz /= valid.length;
    }

    const objects: PrefabObject[] = valid.map((mesh) => {
      const rot = mesh.rotationQuaternion
        ? mesh.rotationQuaternion.toEulerAngles()
        : mesh.rotation;

      let materialColor: [number, number, number] | undefined;
      if (mesh.material instanceof BABYLON.StandardMaterial) {
        const c = mesh.material.diffuseColor;
        materialColor = [c.r, c.g, c.b];
      } else if (mesh.material instanceof BABYLON.PBRMaterial) {
        const c = mesh.material.albedoColor;
        materialColor = [c.r, c.g, c.b];
      }

      return {
        name: mesh.name,
        position: [mesh.position.x - cx, mesh.position.y - cy, mesh.position.z - cz],
        rotation: [rot.x, rot.y, rot.z],
        scaling: [mesh.scaling.x, mesh.scaling.y, mesh.scaling.z],
        materialColor,
      };
    });

    const def: PrefabDefinition = {
      version: 1,
      id: existing?.id ?? `prefab_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`,
      name,
      createdAt: existing?.createdAt ?? new Date().toISOString(),
      pivot: [cx, cy, cz],
      objects,
    };

    this._library.set(name, def);
    return def;
  }

  /**
   * Spawn a prefab into the scene.
   * For each object it tries to clone the original mesh by name; if that mesh
   * is gone a box placeholder is created instead.
   */
  spawnPrefab(
    def: PrefabDefinition,
    scene: BABYLON.Scene,
    spawnPos: BABYLON.Vector3 = BABYLON.Vector3.Zero()
  ): BABYLON.AbstractMesh[] {
    const spawned: BABYLON.AbstractMesh[] = [];
    const suffix = `_inst_${Date.now()}`;

    for (const objDef of def.objects) {
      const original = scene.getMeshByName(objDef.name);
      let mesh: BABYLON.AbstractMesh | null = null;

      if (original && !original.isDisposed()) {
        mesh = original.clone(`${objDef.name}${suffix}`, null);
      } else {
        // Fallback placeholder
        mesh = BABYLON.MeshBuilder.CreateBox(`${objDef.name}${suffix}`, { size: 1 }, scene);
        if (objDef.materialColor) {
          const mat = new BABYLON.StandardMaterial(`${objDef.name}${suffix}_mat`, scene);
          mat.diffuseColor = new BABYLON.Color3(...objDef.materialColor);
          mesh.material = mat;
        }
      }

      if (!mesh) continue;

      mesh.position.set(
        spawnPos.x + objDef.position[0],
        spawnPos.y + objDef.position[1],
        spawnPos.z + objDef.position[2]
      );
      mesh.rotationQuaternion = null;
      mesh.rotation.set(objDef.rotation[0], objDef.rotation[1], objDef.rotation[2]);
      mesh.scaling.set(objDef.scaling[0], objDef.scaling[1], objDef.scaling[2]);

      spawned.push(mesh);
    }

    return spawned;
  }

  getAll(): PrefabDefinition[] {
    return Array.from(this._library.values());
  }

  remove(name: string): void {
    this._library.delete(name);
  }

  /** Trigger a browser download of a single prefab as JSON. */
  downloadPrefab(def: PrefabDefinition): void {
    const blob = new Blob([JSON.stringify(def, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${def.name.replace(/[^a-zA-Z0-9_-]/g, "_")}.prefab.json`;
    a.click();
    URL.revokeObjectURL(url);
  }

  /** Parse a JSON string and add the definition to the library. */
  loadFromJSON(json: string): PrefabDefinition {
    const def = JSON.parse(json) as PrefabDefinition;
    if (!def.name || !Array.isArray(def.objects)) {
      throw new Error("Invalid prefab file format");
    }
    this._library.set(def.name, def);
    return def;
  }
}
