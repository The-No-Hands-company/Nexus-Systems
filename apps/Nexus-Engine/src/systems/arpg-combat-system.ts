import * as BABYLON from "@babylonjs/core";
import type { EnemyRuntimeState } from "@systems/arpg-runtime-state";
import type { ARPGCameraMode } from "@systems/arpg-camera-system";

export interface CombatTuning {
  range: number;
  coneHalfAngleDeg: number;
  damage: number;
  cooldown: number;
}

export function getCombatTuning(cameraMode: ARPGCameraMode, enemyDamage: number): CombatTuning {
  if (cameraMode === "first-person") {
    return { range: 2.1, coneHalfAngleDeg: 32, damage: enemyDamage + 4, cooldown: 0.33 };
  }

  if (cameraMode === "third-person") {
    return { range: 3.0, coneHalfAngleDeg: 55, damage: enemyDamage + 1, cooldown: 0.42 };
  }

  return { range: 4.5, coneHalfAngleDeg: 85, damage: enemyDamage - 1, cooldown: 0.52 };
}

export interface ARPGCombatAttackInput {
  playerMesh: BABYLON.Mesh | null;
  attackCooldown: number;
  enemies: EnemyRuntimeState[];
  facingDirection: BABYLON.Vector3;
  activeLockTarget: BABYLON.Mesh | null;
  cameraMode: ARPGCameraMode;
  enemyDamage: number;
  onEnemyKilled: (position: BABYLON.Vector3) => void;
}

export interface ARPGCombatAttackOutput {
  attackCooldown: number;
  enemies: EnemyRuntimeState[];
  activeLockTarget: BABYLON.Mesh | null;
  hits: number;
  kills: number;
}

export class ARPGCombatSystem {
  performMeleeAttack(input: ARPGCombatAttackInput): ARPGCombatAttackOutput {
    const {
      playerMesh,
      attackCooldown,
      enemies,
      facingDirection,
      activeLockTarget,
      cameraMode,
      enemyDamage,
      onEnemyKilled,
    } = input;

    if (!playerMesh || attackCooldown > 0) {
      return {
        attackCooldown,
        enemies,
        activeLockTarget,
        hits: 0,
        kills: 0,
      };
    }

    const tuning = getCombatTuning(cameraMode, enemyDamage);
    let nextLockTarget = activeLockTarget;
    let hits = 0;
    let kills = 0;

    const nextEnemies = enemies.filter((enemy) => {
      const distance = BABYLON.Vector3.Distance(enemy.mesh.position, playerMesh.position);
      if (distance > tuning.range) return true;

      const toEnemy = enemy.mesh.position.subtract(playerMesh.position).normalize();
      const forward = facingDirection.clone().normalize();
      const dot = BABYLON.Vector3.Dot(forward, toEnemy);
      const angle = Math.acos(BABYLON.Scalar.Clamp(dot, -1, 1)) * (180 / Math.PI);

      const passesCone = nextLockTarget === enemy.mesh || angle <= tuning.coneHalfAngleDeg;
      if (!passesCone) return true;

      enemy.health -= tuning.damage;
      hits += 1;
      if (enemy.health > 0) return true;

      if (nextLockTarget === enemy.mesh) {
        nextLockTarget = null;
      }
      onEnemyKilled(enemy.mesh.position.clone());
      enemy.mesh.dispose();
      kills += 1;
      return false;
    });

    return {
      attackCooldown: tuning.cooldown,
      enemies: nextEnemies,
      activeLockTarget: nextLockTarget,
      hits,
      kills,
    };
  }
}
