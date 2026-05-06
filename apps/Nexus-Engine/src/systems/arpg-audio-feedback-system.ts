import * as BABYLON from "@babylonjs/core";
import type { IAudioEngine } from "@systems/audio-engine-system";

type AudioEventSink = Pick<IAudioEngine, "queuePlayback" | "applySnapshot" | "duckBus">;

export class ARPGAudioFeedbackSystem {
  constructor(private readonly audio: AudioEventSink) {}

  onMeleeHit(): void {
    this.audio.queuePlayback({ assetId: "melee-hit", volume: 0.9 });
    this.audio.applySnapshot("combat", 0.2);
  }

  onEnemyDeath(position: BABYLON.Vector3): void {
    this.audio.queuePlayback({
      assetId: "enemy-death",
      volume: 1,
      spatialPosition: { x: position.x, y: position.y, z: position.z },
    });
  }

  onUiConfirm(): void {
    this.audio.queuePlayback({ assetId: "ui-confirm", volume: 0.75 });
    this.audio.duckBus("music", 0.55, 0.03, 0.08, 0.22);
  }

  onUiNavigate(): void {
    this.audio.queuePlayback({ assetId: "ui-navigate", volume: 0.5 });
    this.audio.duckBus("music", 0.7, 0.02, 0.04, 0.14);
  }

  onFootstep(): void {
    this.audio.queuePlayback({ assetId: "footstep", volume: 0.45 });
  }

  onLootPickup(): void {
    this.audio.queuePlayback({ assetId: "loot-pickup", volume: 0.7 });
    this.audio.duckBus("music", 0.65, 0.03, 0.06, 0.16);
  }
}