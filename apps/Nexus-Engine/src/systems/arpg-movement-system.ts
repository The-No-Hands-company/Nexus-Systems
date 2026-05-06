import * as BABYLON from "@babylonjs/core";

export interface ARPGMovementUpdateInput {
  playerMesh: BABYLON.Mesh | null;
  keys: Record<string, boolean>;
  orbitYaw: number;
  deltaTime: number;
  movementSpeed: number;
  activeLockTarget: BABYLON.Mesh | null;
  facingDirection: BABYLON.Vector3;
  characterPosition: { x: number; y: number; z: number };
}

export class ARPGMovementSystem {
  update(input: ARPGMovementUpdateInput): BABYLON.Vector3 {
    const {
      playerMesh,
      keys,
      orbitYaw,
      deltaTime,
      movementSpeed,
      activeLockTarget,
      facingDirection,
      characterPosition,
    } = input;

    if (!playerMesh) return facingDirection;

    const axis = new BABYLON.Vector3(0, 0, 0);
    if (keys["w"] || keys["arrowup"]) axis.z += 1;
    if (keys["s"] || keys["arrowdown"]) axis.z -= 1;
    if (keys["a"] || keys["arrowleft"]) axis.x -= 1;
    if (keys["d"] || keys["arrowright"]) axis.x += 1;

    if (axis.lengthSquared() === 0) {
      return facingDirection;
    }

    axis.normalize();

    const cameraForward = new BABYLON.Vector3(
      Math.sin(orbitYaw),
      0,
      Math.cos(orbitYaw)
    ).normalize();
    const cameraRight = new BABYLON.Vector3(cameraForward.z, 0, -cameraForward.x);
    const movement = cameraRight.scale(axis.x).add(cameraForward.scale(axis.z));
    movement.normalize();

    const velocity = movement.scale(movementSpeed * deltaTime);
    playerMesh.position.addInPlace(velocity);

    const nextFacing = movement.clone();

    if (activeLockTarget) {
      const lockDir = activeLockTarget.position
        .subtract(playerMesh.position)
        .normalize();
      playerMesh.rotation.y = Math.atan2(lockDir.x, lockDir.z);
      nextFacing.copyFrom(lockDir);
    } else {
      const yaw = Math.atan2(movement.x, movement.z);
      playerMesh.rotation.y = yaw;
    }

    characterPosition.x = playerMesh.position.x;
    characterPosition.y = playerMesh.position.y;
    characterPosition.z = playerMesh.position.z;

    return nextFacing;
  }
}
