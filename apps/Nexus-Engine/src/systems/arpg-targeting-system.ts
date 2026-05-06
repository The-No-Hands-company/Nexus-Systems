import * as BABYLON from "@babylonjs/core";
import type { ARPGInputCommand } from "@systems/arpg-input-system";
import type { EnemyRuntimeState } from "@systems/arpg-runtime-state";

export interface ARPGTargetingContext {
  playerMesh: BABYLON.Mesh | null;
  enemies: EnemyRuntimeState[];
  activeLockTarget: BABYLON.Mesh | null;
  acquireRange?: number;
  retainRange?: number;
}

export interface ARPGTargetingCommandResult {
  handled: boolean;
  activeLockTarget: BABYLON.Mesh | null;
  statusMessage: string | null;
}

export function isTargetingCommand(command: ARPGInputCommand): boolean {
  return command === "toggleLock" || command === "clearLock";
}

export class ARPGTargetingSystem {
  updateLifecycle(context: ARPGTargetingContext): BABYLON.Mesh | null {
    const { playerMesh, activeLockTarget } = context;
    const retainRange = context.retainRange ?? 26;

    if (!playerMesh || !activeLockTarget) {
      return null;
    }

    if (activeLockTarget.isDisposed() || !activeLockTarget.isEnabled()) {
      return null;
    }

    const dist = BABYLON.Vector3.Distance(playerMesh.position, activeLockTarget.position);
    if (dist > retainRange) {
      return null;
    }

    return activeLockTarget;
  }

  handleCommand(
    command: ARPGInputCommand,
    context: ARPGTargetingContext
  ): ARPGTargetingCommandResult {
    if (!isTargetingCommand(command)) {
      return {
        handled: false,
        activeLockTarget: context.activeLockTarget,
        statusMessage: null,
      };
    }

    if (command === "clearLock") {
      return {
        handled: true,
        activeLockTarget: null,
        statusMessage: null,
      };
    }

    if (!context.playerMesh) {
      return {
        handled: true,
        activeLockTarget: null,
        statusMessage: null,
      };
    }

    if (context.activeLockTarget) {
      return {
        handled: true,
        activeLockTarget: null,
        statusMessage: "Lock-on cleared",
      };
    }

    const nextTarget = this.findNearestLockTarget(context);
    if (!nextTarget) {
      return {
        handled: true,
        activeLockTarget: null,
        statusMessage: "No lock-on target in range",
      };
    }

    return {
      handled: true,
      activeLockTarget: nextTarget,
      statusMessage: `Locked on ${nextTarget.name}`,
    };
  }

  private findNearestLockTarget(context: ARPGTargetingContext): BABYLON.Mesh | null {
    const { playerMesh, enemies } = context;
    const acquireRange = context.acquireRange ?? 20;
    if (!playerMesh) return null;

    let nearest: BABYLON.Mesh | null = null;
    let nearestDist = Number.POSITIVE_INFINITY;

    for (const enemy of enemies) {
      if (enemy.mesh.isDisposed() || !enemy.mesh.isEnabled()) continue;
      const dist = BABYLON.Vector3.Distance(playerMesh.position, enemy.mesh.position);
      if (dist < nearestDist && dist <= acquireRange) {
        nearest = enemy.mesh;
        nearestDist = dist;
      }
    }

    return nearest;
  }
}