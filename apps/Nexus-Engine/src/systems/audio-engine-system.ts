export type AudioBusId = "master" | "music" | "sfx" | "ui";
export type AudioLoadPolicy = "preload" | "on-demand" | "stream";

export interface AudioAssetDefinition {
  id: string;
  sourceUrl: string;
  bus: Exclude<AudioBusId, "master">;
  defaultVolume?: number;
  defaultLoop?: boolean;
  spatial?: boolean;
  loadPolicy?: AudioLoadPolicy;
}

export interface AudioPlaybackRequest {
  assetId: string;
  volume?: number;
  loop?: boolean;
  spatialPosition?: { x: number; y: number; z: number };
}

export interface AudioSnapshotPreset {
  id: string;
  busGains: Partial<Record<AudioBusId, number>>;
  transitionSeconds?: number;
}

export interface AudioEngineVoice {
  id: string;
  assetId: string;
  bus: Exclude<AudioBusId, "master">;
  volume: number;
  effectiveVolume: number;
  loop: boolean;
  spatialPosition?: { x: number; y: number; z: number };
  state: "playing";
  startedAtFrame: number;
}

export interface AudioEngineUpdateResult {
  processedEvents: number;
  activeVoices: number;
}

export interface AudioEngineSnapshot {
  frame: number;
  queuedEvents: number;
  activeVoices: number;
  activeSnapshotId: string;
  targetSnapshotId: string | null;
  busGains: Record<AudioBusId, number>;
  effectiveBusGains: Record<AudioBusId, number>;
}

export interface AudioBackendAdapter {
  syncAssets?(assets: readonly AudioAssetDefinition[]): void;
  prepareAsset?(asset: AudioAssetDefinition): void;
  unloadAsset?(assetId: string): void;
  startVoice?(voice: AudioEngineVoice, asset: AudioAssetDefinition): void;
  stopVoice?(voice: AudioEngineVoice): void;
  setBusGain?(bus: AudioBusId, gain: number): void;
  applySnapshot?(snapshotId: string, busGains: Readonly<Record<AudioBusId, number>>): void;
  update?(deltaSeconds: number): void;
}

export interface IAudioEngine {
  registerAsset(asset: AudioAssetDefinition): void;
  registerAssets(assets: AudioAssetDefinition[]): void;
  getAsset(assetId: string): AudioAssetDefinition | null;
  prewarmAssets(assetIds?: string[]): void;
  unloadUnusedAssets(maxIdleSeconds?: number): number;
  ensureLoopPlaying(assetId: string, volume?: number): boolean;
  stopVoicesByAssetId(assetId: string): number;
  duckBus(bus: AudioBusId, targetGain: number, attackSeconds?: number, holdSeconds?: number, releaseSeconds?: number): void;
  queuePlayback(request: AudioPlaybackRequest): void;
  stopVoice(voiceId: string): boolean;
  setBusGain(bus: AudioBusId, gain: number): void;
  getBusGain(bus: AudioBusId): number;
  applySnapshot(snapshotId: string, transitionSeconds?: number): boolean;
  update(deltaSeconds?: number): AudioEngineUpdateResult;
  getSnapshot(): AudioEngineSnapshot;
}

export interface AudioEngineOptions {
  maxVoices?: number;
  backendAdapter?: AudioBackendAdapter;
  snapshotPresets?: AudioSnapshotPreset[];
  initialAssets?: AudioAssetDefinition[];
  defaultUnloadIdleSeconds?: number;
}

interface SnapshotTransitionState {
  targetSnapshotId: string;
  startGains: Record<AudioBusId, number>;
  targetGains: Record<AudioBusId, number>;
  durationSeconds: number;
  elapsedSeconds: number;
}

interface AudioDuckState {
  bus: AudioBusId;
  targetGain: number;
  attackSeconds: number;
  holdSeconds: number;
  releaseSeconds: number;
  elapsedSeconds: number;
}

const DEFAULT_UNLOAD_IDLE_SECONDS = 45;

function clampVolume(value: number | undefined): number {
  const normalized = value ?? 1;
  return Math.max(0, Math.min(1, normalized));
}

function createDefaultBusGains(): Record<AudioBusId, number> {
  return {
    master: 1,
    music: 1,
    sfx: 1,
    ui: 1,
  };
}

function createResolvedSnapshotGains(
  busGains: Partial<Record<AudioBusId, number>>
): Record<AudioBusId, number> {
  const resolved = createDefaultBusGains();
  for (const bus of Object.keys(busGains) as AudioBusId[]) {
    const gain = busGains[bus];
    if (gain !== undefined) {
      resolved[bus] = clampVolume(gain);
    }
  }
  return resolved;
}

function createDefaultSnapshotPresets(): AudioSnapshotPreset[] {
  return [
    { id: "default", busGains: { master: 1, music: 1, sfx: 1, ui: 1 }, transitionSeconds: 0 },
    { id: "combat", busGains: { master: 1, music: 0.75, sfx: 1, ui: 0.9 }, transitionSeconds: 0.2 },
    { id: "menu", busGains: { master: 1, music: 0.9, sfx: 0.45, ui: 1 }, transitionSeconds: 0.18 },
  ];
}

export function createDefaultAudioAssets(): AudioAssetDefinition[] {
  return [
    {
      id: "melee-hit",
      sourceUrl: "/audio/melee-hit.wav",
      bus: "sfx",
      defaultVolume: 0.9,
      spatial: false,
      loadPolicy: "preload",
    },
    {
      id: "enemy-death",
      sourceUrl: "/audio/enemy-death.wav",
      bus: "sfx",
      defaultVolume: 1,
      spatial: true,
      loadPolicy: "on-demand",
    },
    {
      id: "ui-confirm",
      sourceUrl: "/audio/ui-confirm.wav",
      bus: "ui",
      defaultVolume: 0.75,
      spatial: false,
      loadPolicy: "preload",
    },
    {
      id: "footstep",
      sourceUrl: "/audio/footstep.wav",
      bus: "sfx",
      defaultVolume: 0.45,
      spatial: false,
      loadPolicy: "on-demand",
    },
    {
      id: "loot-pickup",
      sourceUrl: "/audio/loot-pickup.wav",
      bus: "ui",
      defaultVolume: 0.7,
      spatial: false,
      loadPolicy: "on-demand",
    },
    {
      id: "ui-navigate",
      sourceUrl: "/audio/ui-navigate.wav",
      bus: "ui",
      defaultVolume: 0.5,
      spatial: false,
      loadPolicy: "on-demand",
    },
    {
      id: "ambient-loop",
      sourceUrl: "/audio/ambient-loop.wav",
      bus: "music",
      defaultVolume: 0.45,
      defaultLoop: true,
      spatial: false,
      loadPolicy: "stream",
    },
    {
      id: "combat-loop",
      sourceUrl: "/audio/combat-loop.wav",
      bus: "music",
      defaultVolume: 0.55,
      defaultLoop: true,
      spatial: false,
      loadPolicy: "stream",
    },
    {
      id: "menu-loop",
      sourceUrl: "/audio/menu-loop.wav",
      bus: "music",
      defaultVolume: 0.42,
      defaultLoop: true,
      spatial: false,
      loadPolicy: "stream",
    },
    {
      id: "cinematic-loop",
      sourceUrl: "/audio/cinematic-loop.wav",
      bus: "music",
      defaultVolume: 0.5,
      defaultLoop: true,
      spatial: false,
      loadPolicy: "stream",
    },
  ];
}

export class AudioAssetRegistry {
  private readonly assets = new Map<string, AudioAssetDefinition>();

  register(asset: AudioAssetDefinition): void {
    this.assets.set(asset.id, {
      ...asset,
      defaultVolume: clampVolume(asset.defaultVolume),
      loadPolicy: asset.loadPolicy ?? "on-demand",
    });
  }

  registerMany(assets: AudioAssetDefinition[]): void {
    for (const asset of assets) {
      this.register(asset);
    }
  }

  get(assetId: string): AudioAssetDefinition | null {
    return this.assets.get(assetId) ?? null;
  }

  list(): AudioAssetDefinition[] {
    return Array.from(this.assets.values());
  }

  listPreloadAssets(): AudioAssetDefinition[] {
    return this.list().filter((asset) => asset.loadPolicy === "preload");
  }
}

export class StubAudioEngineSystem implements IAudioEngine {
  private readonly maxVoices: number;
  private readonly queue: AudioPlaybackRequest[] = [];
  private readonly voices: AudioEngineVoice[] = [];
  private readonly snapshotPresets = new Map<string, AudioSnapshotPreset>();
  private readonly assetRegistry = new AudioAssetRegistry();
  private readonly baseBusGains: Record<AudioBusId, number> = createDefaultBusGains();
  private readonly effectiveBusGains: Record<AudioBusId, number> = createDefaultBusGains();
  private readonly duckStates = new Map<AudioBusId, AudioDuckState>();
  private readonly lastUsedFrameByAssetId = new Map<string, number>();
  private readonly prewarmedAssetIds = new Set<string>();
  private readonly defaultUnloadIdleSeconds: number;
  private backendAdapter: AudioBackendAdapter | null;
  private frame = 0;
  private idCounter = 0;
  private activeSnapshotId = "default";
  private transitionState: SnapshotTransitionState | null = null;

  constructor(options: AudioEngineOptions = {}) {
    this.maxVoices = Math.max(1, options.maxVoices ?? 64);
    this.defaultUnloadIdleSeconds = Math.max(1, options.defaultUnloadIdleSeconds ?? DEFAULT_UNLOAD_IDLE_SECONDS);
    this.backendAdapter = options.backendAdapter ?? null;

    for (const preset of createDefaultSnapshotPresets()) {
      this.snapshotPresets.set(preset.id, preset);
    }
    for (const preset of options.snapshotPresets ?? []) {
      this.snapshotPresets.set(preset.id, preset);
    }

    this.registerAssets(createDefaultAudioAssets());
    this.registerAssets(options.initialAssets ?? []);
    this.primeBackendAdapter();
    this.prewarmAssets();
    this.applySnapshot(this.activeSnapshotId, 0);
  }

  attachBackendAdapter(adapter: AudioBackendAdapter): void {
    this.backendAdapter = adapter;
    this.primeBackendAdapter();
    this.backendAdapter.applySnapshot?.(this.activeSnapshotId, { ...this.effectiveBusGains });
  }

  registerAsset(asset: AudioAssetDefinition): void {
    this.assetRegistry.register(asset);
    this.backendAdapter?.syncAssets?.(this.assetRegistry.list());
    if ((asset.loadPolicy ?? "on-demand") === "preload") {
      this.backendAdapter?.prepareAsset?.(asset);
    }
  }

  registerAssets(assets: AudioAssetDefinition[]): void {
    this.assetRegistry.registerMany(assets);
    this.primeBackendAdapter();
  }

  getAsset(assetId: string): AudioAssetDefinition | null {
    return this.assetRegistry.get(assetId);
  }

  prewarmAssets(assetIds?: string[]): void {
    const targets = assetIds
      ? assetIds
          .map((assetId) => this.assetRegistry.get(assetId))
          .filter((asset): asset is AudioAssetDefinition => asset !== null)
      : this.assetRegistry.listPreloadAssets();

    for (const asset of targets) {
      this.backendAdapter?.prepareAsset?.(asset);
      this.prewarmedAssetIds.add(asset.id);
    }
  }

  unloadUnusedAssets(maxIdleSeconds = this.defaultUnloadIdleSeconds): number {
    const idleFrames = Math.max(1, Math.round(maxIdleSeconds * 60));
    const activeAssetIds = new Set(this.voices.map((voice) => voice.assetId));
    let unloaded = 0;

    for (const asset of this.assetRegistry.list()) {
      if (asset.loadPolicy === "preload") {
        continue;
      }
      if (activeAssetIds.has(asset.id)) {
        continue;
      }

      const lastUsedFrame = this.lastUsedFrameByAssetId.get(asset.id) ?? -1;
      const idleForFrames = this.frame - lastUsedFrame;
      if (idleForFrames < idleFrames) {
        continue;
      }

      this.backendAdapter?.unloadAsset?.(asset.id);
      this.prewarmedAssetIds.delete(asset.id);
      unloaded += 1;
    }

    return unloaded;
  }

  ensureLoopPlaying(assetId: string, volume?: number): boolean {
    const asset = this.assetRegistry.get(assetId);
    if (!asset) {
      return false;
    }

    const alreadyPlaying = this.voices.some((voice) => voice.assetId === assetId && voice.loop);
    const alreadyQueued = this.queue.some((request) => request.assetId === assetId && (request.loop ?? asset.defaultLoop ?? false));
    if (alreadyPlaying || alreadyQueued) {
      return true;
    }

    this.queuePlayback({ assetId, volume, loop: true });
    return true;
  }

  stopVoicesByAssetId(assetId: string): number {
    const matches = this.voices.filter((voice) => voice.assetId === assetId).map((voice) => voice.id);
    for (const voiceId of matches) {
      this.stopVoice(voiceId);
    }
    return matches.length;
  }

  duckBus(
    bus: AudioBusId,
    targetGain: number,
    attackSeconds = 0.04,
    holdSeconds = 0.12,
    releaseSeconds = 0.2
  ): void {
    this.duckStates.set(bus, {
      bus,
      targetGain: clampVolume(targetGain),
      attackSeconds: Math.max(0.001, attackSeconds),
      holdSeconds: Math.max(0, holdSeconds),
      releaseSeconds: Math.max(0.001, releaseSeconds),
      elapsedSeconds: 0,
    });
    this.refreshEffectiveBusGains();
  }

  queuePlayback(request: AudioPlaybackRequest): void {
    this.queue.push(request);
  }

  stopVoice(voiceId: string): boolean {
    const index = this.voices.findIndex((voice) => voice.id === voiceId);
    if (index < 0) {
      return false;
    }

    const [voice] = this.voices.splice(index, 1);
    this.backendAdapter?.stopVoice?.(voice);
    return true;
  }

  setBusGain(bus: AudioBusId, gain: number): void {
    const clampedGain = clampVolume(gain);
    this.baseBusGains[bus] = clampedGain;
    this.refreshEffectiveBusGains();
  }

  getBusGain(bus: AudioBusId): number {
    return this.effectiveBusGains[bus];
  }

  applySnapshot(snapshotId: string, transitionSeconds?: number): boolean {
    const preset = this.snapshotPresets.get(snapshotId);
    if (!preset) {
      return false;
    }

    const targetGains = createResolvedSnapshotGains(preset.busGains);
    const resolvedTransitionSeconds = Math.max(0, transitionSeconds ?? preset.transitionSeconds ?? 0);

    if (resolvedTransitionSeconds === 0) {
      for (const bus of Object.keys(targetGains) as AudioBusId[]) {
        this.setBusGain(bus, targetGains[bus]);
      }
      this.transitionState = null;
      this.activeSnapshotId = snapshotId;
      this.backendAdapter?.applySnapshot?.(snapshotId, { ...this.effectiveBusGains });
      return true;
    }

    this.transitionState = {
      targetSnapshotId: snapshotId,
      startGains: { ...this.baseBusGains },
      targetGains,
      durationSeconds: resolvedTransitionSeconds,
      elapsedSeconds: 0,
    };
    return true;
  }

  update(deltaSeconds = 1 / 60): AudioEngineUpdateResult {
    this.frame += 1;
    let processed = 0;

    this.updateDuckStates(deltaSeconds);

    if (this.transitionState) {
      this.transitionState.elapsedSeconds += deltaSeconds;
      const ratio = Math.min(1, this.transitionState.elapsedSeconds / this.transitionState.durationSeconds);

      for (const bus of Object.keys(this.transitionState.targetGains) as AudioBusId[]) {
        const start = this.transitionState.startGains[bus];
        const target = this.transitionState.targetGains[bus];
        this.setBusGain(bus, start + (target - start) * ratio);
      }

      if (ratio >= 1) {
        this.activeSnapshotId = this.transitionState.targetSnapshotId;
        this.backendAdapter?.applySnapshot?.(this.activeSnapshotId, { ...this.effectiveBusGains });
        this.transitionState = null;
      }
    }

    while (this.queue.length > 0 && this.voices.length < this.maxVoices) {
      const event = this.queue.shift();
      if (!event) {
        break;
      }

      const asset = this.assetRegistry.get(event.assetId);
      if (!asset) {
        continue;
      }

      if (asset.loadPolicy !== "stream" && !this.prewarmedAssetIds.has(asset.id)) {
        this.backendAdapter?.prepareAsset?.(asset);
        this.prewarmedAssetIds.add(asset.id);
      }

      this.lastUsedFrameByAssetId.set(asset.id, this.frame);

      this.idCounter += 1;
      const volume = clampVolume(event.volume ?? asset.defaultVolume);
      const voice: AudioEngineVoice = {
        id: `voice-${this.idCounter}`,
        assetId: asset.id,
        bus: asset.bus,
        volume,
        effectiveVolume: clampVolume(volume * this.effectiveBusGains.master * this.effectiveBusGains[asset.bus]),
        loop: event.loop ?? asset.defaultLoop ?? false,
        spatialPosition: event.spatialPosition,
        state: "playing",
        startedAtFrame: this.frame,
      };
      this.voices.push(voice);
      this.backendAdapter?.startVoice?.(voice, asset);
      processed += 1;
    }

    this.backendAdapter?.update?.(deltaSeconds);
    this.unloadUnusedAssets();

    return {
      processedEvents: processed,
      activeVoices: this.voices.length,
    };
  }

  getSnapshot(): AudioEngineSnapshot {
    return {
      frame: this.frame,
      queuedEvents: this.queue.length,
      activeVoices: this.voices.length,
      activeSnapshotId: this.activeSnapshotId,
      targetSnapshotId: this.transitionState?.targetSnapshotId ?? null,
      busGains: { ...this.baseBusGains },
      effectiveBusGains: { ...this.effectiveBusGains },
    };
  }

  private primeBackendAdapter(): void {
    if (!this.backendAdapter) {
      return;
    }

    const assets = this.assetRegistry.list();
    this.backendAdapter.syncAssets?.(assets);
    for (const bus of Object.keys(this.effectiveBusGains) as AudioBusId[]) {
      this.backendAdapter.setBusGain?.(bus, this.effectiveBusGains[bus]);
    }
  }

  private refreshEffectiveBusGains(): void {
    for (const bus of Object.keys(this.baseBusGains) as AudioBusId[]) {
      const duckMultiplier = this.getDuckMultiplier(bus);
      const effective = clampVolume(this.baseBusGains[bus] * duckMultiplier);
      this.effectiveBusGains[bus] = effective;
      this.backendAdapter?.setBusGain?.(bus, effective);
    }
  }

  private updateDuckStates(deltaSeconds: number): void {
    let changed = false;

    for (const [bus, duckState] of this.duckStates.entries()) {
      duckState.elapsedSeconds += deltaSeconds;
      const totalDuration = duckState.attackSeconds + duckState.holdSeconds + duckState.releaseSeconds;
      if (duckState.elapsedSeconds >= totalDuration) {
        this.duckStates.delete(bus);
        changed = true;
      } else {
        changed = true;
      }
    }

    if (changed) {
      this.refreshEffectiveBusGains();
    }
  }

  private getDuckMultiplier(bus: AudioBusId): number {
    const duckState = this.duckStates.get(bus);
    if (!duckState) {
      return 1;
    }

    if (duckState.elapsedSeconds <= duckState.attackSeconds) {
      const ratio = duckState.elapsedSeconds / duckState.attackSeconds;
      return 1 + (duckState.targetGain - 1) * ratio;
    }

    if (duckState.elapsedSeconds <= duckState.attackSeconds + duckState.holdSeconds) {
      return duckState.targetGain;
    }

    const releaseElapsed = duckState.elapsedSeconds - duckState.attackSeconds - duckState.holdSeconds;
    const releaseRatio = Math.min(1, releaseElapsed / duckState.releaseSeconds);
    return duckState.targetGain + (1 - duckState.targetGain) * releaseRatio;
  }
}
