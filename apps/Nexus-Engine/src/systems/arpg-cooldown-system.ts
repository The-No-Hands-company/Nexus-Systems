export class ARPGCooldownSystem {
  tickCooldown(current: number, deltaTime: number): number {
    return Math.max(0, current - deltaTime);
  }
}
