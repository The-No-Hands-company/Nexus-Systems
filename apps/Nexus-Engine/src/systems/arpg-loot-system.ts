import * as BABYLON from "@babylonjs/core";
import type { LootRuntimeState } from "@systems/arpg-runtime-state";
import type { Character } from "@game-types/arpg";

export class ARPGLootSystem {
  collectNearbyLoot(
    loot: LootRuntimeState[],
    playerMesh: BABYLON.Mesh | null,
    character: Character,
    pickupRange: number,
    onLooted: (itemName: string) => void
  ): LootRuntimeState[] {
    if (!playerMesh) return loot;

    return loot.filter((drop) => {
      const distance = BABYLON.Vector3.Distance(drop.mesh.position, playerMesh.position);
      if (distance <= pickupRange) {
        character.inventory.push(drop.item);
        drop.mesh.dispose();
        onLooted(drop.item.name);
        return false;
      }
      return true;
    });
  }
}
