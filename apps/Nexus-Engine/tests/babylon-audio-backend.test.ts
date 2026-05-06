import { describe, expect, it } from "vitest";
import * as BABYLON from "@babylonjs/core";
import { BabylonAudioBackendAdapter, type BabylonAudioBackendSound } from "../src/systems/babylon-audio-backend";
import type { AudioAssetDefinition, AudioEngineVoice } from "../src/systems/audio-engine-system";

describe("babylon audio backend adapter", () => {
  it("creates, positions, and volume-updates sounds for active voices", () => {
    const engine = new BABYLON.NullEngine();
    const scene = new BABYLON.Scene(engine);
    const created: Array<{
      name: string;
      url: string;
      options: { autoplay: boolean; loop: boolean; spatialSound: boolean; volume: number };
      events: string[];
      sound: BabylonAudioBackendSound;
    }> = [];

    const adapter = new BabylonAudioBackendAdapter({
      scene,
      soundFactory: (name, url, _scene, options) => {
        const events: string[] = [];
        const sound: BabylonAudioBackendSound = {
          play: () => events.push("play"),
          stop: () => events.push("stop"),
          dispose: () => events.push("dispose"),
          setVolume: (value) => events.push(`volume:${value.toFixed(3)}`),
          setPosition: (position) =>
            events.push(`position:${position.x},${position.y},${position.z}`),
        };
        created.push({ name, url, options, events, sound });
        return sound;
      },
    });

    const asset: AudioAssetDefinition = {
      id: "enemy-death-test",
      sourceUrl: "memory://enemy-death",
      bus: "sfx",
      spatial: true,
      loadPolicy: "preload",
    };
    const voice: AudioEngineVoice = {
      id: "voice-1",
      assetId: asset.id,
      bus: "sfx",
      volume: 0.8,
      effectiveVolume: 0.8,
      loop: false,
      spatialPosition: { x: 3, y: 1, z: 2 },
      state: "playing",
      startedAtFrame: 1,
    };

    adapter.prepareAsset(asset);
    adapter.startVoice(voice, asset);

    expect(created).toHaveLength(1);
    expect(created[0].name).toContain(asset.id);
    expect(created[0].url).toBe(asset.sourceUrl);
    expect(created[0].options.spatialSound).toBe(true);
    expect(created[0].events).toContain("play");
    expect(created[0].events).toContain("position:3,1,2");

    adapter.setBusGain("master", 0.5);
    expect(created[0].events).toContain("volume:0.400");

    adapter.stopVoice(voice);
    expect(created[0].events).toContain("stop");
    expect(created[0].events).toContain("dispose");

    scene.dispose();
    engine.dispose();
  });
});