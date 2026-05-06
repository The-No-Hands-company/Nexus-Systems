import * as BABYLON from "@babylonjs/core";
import type {
  AudioAssetDefinition,
  AudioBackendAdapter,
  AudioBusId,
  AudioEngineVoice,
} from "@systems/audio-engine-system";

export interface BabylonAudioBackendSound {
  play(): void;
  stop(): void;
  dispose(): void;
  setVolume(value: number): void;
  setPosition?(position: BABYLON.Vector3): void;
}

export interface BabylonAudioBackendOptions {
  scene: BABYLON.Scene;
  soundFactory?: (
    name: string,
    url: string,
    scene: BABYLON.Scene,
    options: {
      autoplay: boolean;
      loop: boolean;
      spatialSound: boolean;
      volume: number;
    }
  ) => BabylonAudioBackendSound;
}

interface ActiveVoiceState {
  voice: AudioEngineVoice;
  sound: BabylonAudioBackendSound;
}

export class BabylonAudioBackendAdapter implements AudioBackendAdapter {
  private readonly scene: BABYLON.Scene;
  private readonly soundFactory: NonNullable<BabylonAudioBackendOptions["soundFactory"]>;
  private readonly preparedAssetIds = new Set<string>();
  private readonly activeVoices = new Map<string, ActiveVoiceState>();
  private readonly busGains: Record<AudioBusId, number> = {
    master: 1,
    music: 1,
    sfx: 1,
    ui: 1,
  };

  constructor(options: BabylonAudioBackendOptions) {
    this.scene = options.scene;
    this.soundFactory = options.soundFactory ?? ((name, url, scene, soundOptions) => {
      return new BABYLON.Sound(name, url, scene, undefined, soundOptions);
    });
  }

  syncAssets(_assets: readonly AudioAssetDefinition[]): void {}

  prepareAsset(asset: AudioAssetDefinition): void {
    this.preparedAssetIds.add(asset.id);
  }

  unloadAsset(assetId: string): void {
    this.preparedAssetIds.delete(assetId);

    for (const [voiceId, entry] of this.activeVoices.entries()) {
      if (entry.voice.assetId !== assetId) {
        continue;
      }

      entry.sound.stop();
      entry.sound.dispose();
      this.activeVoices.delete(voiceId);
    }
  }

  startVoice(voice: AudioEngineVoice, asset: AudioAssetDefinition): void {
    if (!this.preparedAssetIds.has(asset.id) && asset.loadPolicy !== "stream") {
      this.prepareAsset(asset);
    }

    const sound = this.soundFactory(`${asset.id}-${voice.id}`, asset.sourceUrl, this.scene, {
      autoplay: false,
      loop: voice.loop,
      spatialSound: Boolean(asset.spatial || voice.spatialPosition),
      volume: this.getEffectiveVoiceVolume(voice),
    });

    if (voice.spatialPosition && sound.setPosition) {
      sound.setPosition(
        new BABYLON.Vector3(
          voice.spatialPosition.x,
          voice.spatialPosition.y,
          voice.spatialPosition.z
        )
      );
    }

    sound.setVolume(this.getEffectiveVoiceVolume(voice));
    sound.play();
    this.activeVoices.set(voice.id, { voice, sound });
  }

  stopVoice(voice: AudioEngineVoice): void {
    const entry = this.activeVoices.get(voice.id);
    if (!entry) {
      return;
    }

    entry.sound.stop();
    entry.sound.dispose();
    this.activeVoices.delete(voice.id);
  }

  setBusGain(bus: AudioBusId, gain: number): void {
    this.busGains[bus] = gain;
    for (const entry of this.activeVoices.values()) {
      entry.sound.setVolume(this.getEffectiveVoiceVolume(entry.voice));
    }
  }

  applySnapshot(_snapshotId: string, _busGains: Readonly<Record<AudioBusId, number>>): void {
    for (const entry of this.activeVoices.values()) {
      entry.sound.setVolume(this.getEffectiveVoiceVolume(entry.voice));
    }
  }

  update(_deltaSeconds: number): void {}

  private getEffectiveVoiceVolume(voice: AudioEngineVoice): number {
    return voice.volume * this.busGains.master * this.busGains[voice.bus];
  }
}