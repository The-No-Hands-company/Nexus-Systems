import { describe, expect, it } from "vitest";
import {
  StubAudioEngineSystem,
  type AudioAssetDefinition,
  type AudioBackendAdapter,
  type AudioEngineVoice,
} from "../src/systems/audio-engine-system";

const testAssets: AudioAssetDefinition[] = [
  { id: "sfx-hit", sourceUrl: "memory://sfx-hit", bus: "sfx", loadPolicy: "on-demand" },
  { id: "ui-confirm-test", sourceUrl: "memory://ui-confirm", bus: "ui", loadPolicy: "preload" },
  { id: "music-loop", sourceUrl: "memory://music-loop", bus: "music", defaultLoop: true, loadPolicy: "stream" },
  { id: "sfx-step", sourceUrl: "memory://sfx-step", bus: "sfx", loadPolicy: "on-demand" },
  { id: "sfx-swing", sourceUrl: "memory://sfx-swing", bus: "sfx", loadPolicy: "on-demand" },
  { id: "sfx-alert", sourceUrl: "memory://sfx-alert", bus: "sfx", loadPolicy: "on-demand" },
];

describe("audio engine runtime stub", () => {
  it("queues playback and converts requests into active voices during update", () => {
    const audio = new StubAudioEngineSystem({ initialAssets: testAssets });

    audio.queuePlayback({ assetId: "sfx-hit", volume: 0.8 });
    audio.queuePlayback({ assetId: "ui-confirm-test" });

    const before = audio.getSnapshot();
    expect(before.frame).toBe(0);
    expect(before.queuedEvents).toBe(2);
    expect(before.activeVoices).toBe(0);

    const result = audio.update();

    expect(result.processedEvents).toBe(2);
    expect(result.activeVoices).toBe(2);

    const after = audio.getSnapshot();
    expect(after.frame).toBe(1);
    expect(after.queuedEvents).toBe(0);
    expect(after.activeVoices).toBe(2);
  });

  it("respects max voices and leaves overflow events queued", () => {
    const audio = new StubAudioEngineSystem({ maxVoices: 1, initialAssets: testAssets });

    audio.queuePlayback({ assetId: "music-loop", loop: true });
    audio.queuePlayback({ assetId: "sfx-step" });

    const update = audio.update();
    expect(update.processedEvents).toBe(1);
    expect(update.activeVoices).toBe(1);

    const snapshot = audio.getSnapshot();
    expect(snapshot.queuedEvents).toBe(1);
  });

  it("can stop a voice and returns false for missing ids", () => {
    const audio = new StubAudioEngineSystem({ maxVoices: 2, initialAssets: testAssets });
    audio.queuePlayback({ assetId: "sfx-swing" });
    audio.update();

    expect(audio.stopVoice("voice-1")).toBe(true);
    expect(audio.getSnapshot().activeVoices).toBe(0);
    expect(audio.stopVoice("voice-1")).toBe(false);
  });

  it("supports bus gains, snapshot presets, and backend adapter notifications", () => {
    const startedVoices: AudioEngineVoice[] = [];
    const stoppedVoices: string[] = [];
    const appliedSnapshots: string[] = [];

    const adapter: AudioBackendAdapter = {
      startVoice: (voice) => startedVoices.push(voice),
      stopVoice: (voice) => stoppedVoices.push(voice.id),
      applySnapshot: (snapshotId) => appliedSnapshots.push(snapshotId),
    };

    const audio = new StubAudioEngineSystem({
      backendAdapter: adapter,
      initialAssets: testAssets,
      snapshotPresets: [{ id: "stealth", busGains: { music: 0.35, sfx: 0.55, ui: 0.8 }, transitionSeconds: 0.25 }],
    });

    audio.setBusGain("master", 0.5);
    audio.setBusGain("sfx", 0.4);
    expect(audio.getBusGain("master")).toBe(0.5);
    expect(audio.getBusGain("sfx")).toBe(0.4);

    expect(audio.applySnapshot("stealth", 0.25)).toBe(true);
    expect(audio.getSnapshot().activeSnapshotId).toBe("default");
    expect(audio.getSnapshot().targetSnapshotId).toBe("stealth");

    audio.update(0.125);
    expect(audio.getSnapshot().busGains.music).toBeCloseTo(0.675, 5);
    expect(audio.getSnapshot().busGains.sfx).toBeCloseTo(0.475, 5);

    audio.update(0.125);
    expect(audio.getSnapshot().activeSnapshotId).toBe("stealth");
    expect(audio.getSnapshot().targetSnapshotId).toBeNull();
    expect(audio.getSnapshot().busGains.music).toBe(0.35);
    expect(audio.getSnapshot().busGains.sfx).toBe(0.55);
    expect(appliedSnapshots).toContain("stealth");

    audio.queuePlayback({ assetId: "sfx-alert", volume: 1 });
    audio.update();

    expect(startedVoices).toHaveLength(1);
    expect(startedVoices[0].effectiveVolume).toBeCloseTo(0.55, 5);

    expect(audio.stopVoice(startedVoices[0].id)).toBe(true);
    expect(stoppedVoices).toEqual([startedVoices[0].id]);
  });

  it("supports asset prewarming and unload policy for idle assets", () => {
    const preparedAssets: string[] = [];
    const unloadedAssets: string[] = [];

    const adapter: AudioBackendAdapter = {
      prepareAsset: (asset) => preparedAssets.push(asset.id),
      unloadAsset: (assetId) => unloadedAssets.push(assetId),
    };

    const audio = new StubAudioEngineSystem({
      backendAdapter: adapter,
      initialAssets: [
        { id: "ambient-stream", sourceUrl: "memory://amb", bus: "music", loadPolicy: "stream" },
        { id: "rare-sfx", sourceUrl: "memory://rare", bus: "sfx", loadPolicy: "on-demand" },
      ],
      defaultUnloadIdleSeconds: 1,
    });

    audio.prewarmAssets(["rare-sfx"]);
    expect(preparedAssets).toContain("rare-sfx");

    audio.queuePlayback({ assetId: "rare-sfx" });
    audio.update(1 / 60);
    expect(audio.getSnapshot().activeVoices).toBe(1);

    expect(audio.stopVoice("voice-1")).toBe(true);
    audio.update(1 / 60);
    const unloaded = audio.unloadUnusedAssets(0);
    expect(unloaded).toBeGreaterThanOrEqual(1);
    expect(unloadedAssets).toContain("rare-sfx");
  });

  it("supports loop deduping and temporary bus ducking", () => {
    const audio = new StubAudioEngineSystem({ initialAssets: testAssets });

    expect(audio.ensureLoopPlaying("music-loop", 0.6)).toBe(true);
    expect(audio.ensureLoopPlaying("music-loop", 0.6)).toBe(true);

    let result = audio.update(1 / 60);
    expect(result.processedEvents).toBe(1);
    expect(audio.getSnapshot().activeVoices).toBe(1);

    audio.duckBus("music", 0.4, 0.1, 0.1, 0.2);
    audio.update(0.05);
    expect(audio.getSnapshot().effectiveBusGains.music).toBeCloseTo(0.7, 5);

    audio.update(0.05);
    expect(audio.getSnapshot().effectiveBusGains.music).toBeCloseTo(0.4, 5);

    audio.update(0.1);
    expect(audio.getSnapshot().effectiveBusGains.music).toBeCloseTo(0.4, 5);

    audio.update(0.2);
    expect(audio.getSnapshot().effectiveBusGains.music).toBeCloseTo(1, 5);

    expect(audio.stopVoicesByAssetId("music-loop")).toBe(1);
  });
});
