import * as BABYLON from "@babylonjs/core";
import type { ARPGInputCommand } from "@systems/arpg-input-system";
import type { ARPGCameraMode } from "@systems/arpg-camera-system";
import type { EnemyRuntimeState } from "@systems/arpg-runtime-state";
import type { ARPGCombatSystem } from "@systems/arpg-combat-system";

export interface ARPGCombatInteractionContext {
  playerMesh: BABYLON.Mesh | null;
  attackCooldown: number;
  enemies: EnemyRuntimeState[];
  facingDirection: BABYLON.Vector3;
  activeLockTarget: BABYLON.Mesh | null;
  cameraMode: ARPGCameraMode;
  enemyDamage: number;
  combatSystem: Pick<ARPGCombatSystem, "performMeleeAttack">;
  onEnemyKilled: (position: BABYLON.Vector3) => void;
}

export interface ARPGCombatInteractionResult {
  handled: boolean;
  attackCooldown: number;
  enemies: EnemyRuntimeState[];
  activeLockTarget: BABYLON.Mesh | null;
  hits: number;
  kills: number;
  statusMessage: string | null;
}

export function isCombatInteractionCommand(command: ARPGInputCommand): boolean {
  return command === "melee";
}

export class ARPGCombatInteractionSystem {
  handleCommand(
    command: ARPGInputCommand,
    context: ARPGCombatInteractionContext
  ): ARPGCombatInteractionResult {
    if (!isCombatInteractionCommand(command)) {
      return {
        handled: false,
        attackCooldown: context.attackCooldown,
        enemies: context.enemies,
        activeLockTarget: context.activeLockTarget,
        hits: 0,
        kills: 0,
        statusMessage: null,
      };
    }

    const result = context.combatSystem.performMeleeAttack({
      playerMesh: context.playerMesh,
      attackCooldown: context.attackCooldown,
      enemies: context.enemies,
      facingDirection: context.facingDirection,
      activeLockTarget: context.activeLockTarget,
      cameraMode: context.cameraMode,
      enemyDamage: context.enemyDamage,
      onEnemyKilled: context.onEnemyKilled,
    });

    return {
      handled: true,
      attackCooldown: result.attackCooldown,
      enemies: result.enemies,
      activeLockTarget: result.activeLockTarget,
      hits: result.hits,
      kills: result.kills,
      statusMessage: result.kills > 0 ? `Melee hit: ${result.kills} enemy defeated` : null,
    };
  }
}