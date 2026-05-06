import * as BABYLON from "@babylonjs/core";
import type { Item } from "@game-types/arpg";

export interface EnemyRuntimeState {
  mesh: BABYLON.Mesh;
  health: number;
  maxHealth: number;
}

export interface LootRuntimeState {
  mesh: BABYLON.Mesh;
  item: Item;
}

export interface ARPGRuntimeState {
  attackCooldown: number;
  enemies: EnemyRuntimeState[];
  loot: LootRuntimeState[];
}

export function createARPGRuntimeState(): ARPGRuntimeState {
  return {
    attackCooldown: 0,
    enemies: [],
    loot: [],
  };
}
