import * as BABYLON from "@babylonjs/core";
import type { EnemyRuntimeState } from "@systems/arpg-runtime-state";

export interface EnemyAISystemConfig {
  aggroRange: number;
  stopRange: number;
  speed: number;
}

export class ARPGEnemyAISystem {
  update(
    enemies: EnemyRuntimeState[],
    playerMesh: BABYLON.Mesh | null,
    deltaTime: number,
    config: EnemyAISystemConfig
  ): void {
    if (!playerMesh) return;

    enemies.forEach((enemy) => {
      const toPlayer = playerMesh.position.subtract(enemy.mesh.position);
      const distance = toPlayer.length();

      if (distance <= config.aggroRange && distance > config.stopRange) {
        const dir = toPlayer.normalize();
        enemy.mesh.position.addInPlace(dir.scale(config.speed * deltaTime));
      }
    });
  }
}
