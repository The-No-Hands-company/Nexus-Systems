import { useRef, useEffect, useCallback, useState } from "react";
import { useEditorStore } from "../stores/useEditorStore";
import Play from "lucide-react/dist/esm/icons/play.js";
import Pause from "lucide-react/dist/esm/icons/pause.js";
import Volume2 from "lucide-react/dist/esm/icons/volume-2.js";
import VolumeX from "lucide-react/dist/esm/icons/volume-x.js";
import Maximize from "lucide-react/dist/esm/icons/maximize.js";
import Minimize from "lucide-react/dist/esm/icons/minimize.js";
import PictureInPicture2 from "lucide-react/dist/esm/icons/picture-in-picture-2.js";

export default function VideoPlayer() {
  const videoRef = useRef<HTMLVideoElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const lastSavedTime = useRef(0);

  const video = useEditorStore((s) => s.currentlyPlaying);
  const isPlaying = useEditorStore((s) => s.isPlaying);
  const volume = useEditorStore((s) => s.volume);
  const muted = useEditorStore((s) => s.muted);
  const fullscreen = useEditorStore((s) => s.fullscreen);
  const currentTime = useEditorStore((s) => s.currentTime);
  const duration = useEditorStore((s) => s.duration);
  const hlsQuality = useEditorStore((s) => s.hlsQuality);
  const subtitlesEnabled = useEditorStore((s) => s.subtitlesEnabled);
  const resumePromptTime = useEditorStore((s) => s.resumePromptTime);

  const setIsPlaying = useEditorStore((s) => s.setIsPlaying);
  const setVolume = useEditorStore((s) => s.setVolume);
  const toggleMute = useEditorStore((s) => s.toggleMute);
  const toggleFullscreen = useEditorStore((s) => s.toggleFullscreen);
  const setCurrentTime = useEditorStore((s) => s.setCurrentTime);
  const setDuration = useEditorStore((s) => s.setDuration);
  const setHlsQuality = useEditorStore((s) => s.setHlsQuality);
  const setSubtitlesEnabled = useEditorStore((s) => s.setSubtitlesEnabled);
  const setResumePromptTime = useEditorStore((s) => s.setResumePromptTime);
  const saveProgress = useEditorStore((s) => s.saveProgress);

  const [activeChapterTitle, setActiveChapterTitle] = useState("");

  useEffect(() => {
    const el = videoRef.current;
    if (!el) return;
    if (isPlaying) {
      el.play().catch(() => {});
    } else {
      el.pause();
    }
  }, [isPlaying]);

  useEffect(() => {
    const el = videoRef.current;
    if (!el) return;
    el.volume = volume;
  }, [volume]);

  useEffect(() => {
    const el = videoRef.current;
    if (!el) return;
    el.muted = muted;
  }, [muted]);

  useEffect(() => {
    if (!containerRef.current) return;
    if (fullscreen) {
      containerRef.current.requestFullscreen?.();
    } else {
      document.exitFullscreen?.();
    }
  }, [fullscreen]);

  useEffect(() => {
    const el = videoRef.current;
    if (!el) return;
    if (Math.abs(el.currentTime - currentTime) > 0.5) {
      el.currentTime = currentTime;
    }
  }, [currentTime]);

  const handleTimeUpdate = useCallback(() => {
    const el = videoRef.current;
    if (!el) return;
    const cur = el.currentTime;
    setCurrentTime(cur);

    // Track active chapter
    if (video?.chapters && video.chapters.length > 0) {
      let matched = video.chapters[0].title;
      for (const ch of video.chapters) {
        if (cur >= ch.time) matched = ch.title;
      }
      setActiveChapterTitle(matched);
    }

    // Throttle progress saving to every 5s
    if (video && Math.abs(cur - lastSavedTime.current) > 5) {
      lastSavedTime.current = cur;
      saveProgress(video.id, cur);
    }
  }, [setCurrentTime, video, saveProgress]);

  const handleLoadedMetadata = useCallback(() => {
    const el = videoRef.current;
    if (!el) return;
    setDuration(el.duration);
  }, [setDuration]);

  const handleEnded = useCallback(() => {
    setIsPlaying(false);
  }, [setIsPlaying]);

  const handlePlayPause = useCallback(() => {
    setIsPlaying(!isPlaying);
  }, [isPlaying, setIsPlaying]);

  const handleKeyDown = useCallback(
    (e: KeyboardEvent) => {
      const el = videoRef.current;
      if (!el) return;
      if (e.target instanceof HTMLInputElement) return;
      switch (e.key) {
        case " ":
          e.preventDefault();
          setIsPlaying(!isPlaying);
          break;
        case "f":
          toggleFullscreen();
          break;
        case "m":
          toggleMute();
          break;
        case "ArrowLeft":
          el.currentTime = Math.max(0, el.currentTime - 5);
          setCurrentTime(el.currentTime);
          break;
        case "ArrowRight":
          el.currentTime = Math.min(el.duration, el.currentTime + 5);
          setCurrentTime(el.currentTime);
          break;
      }
    },
    [isPlaying, setIsPlaying, toggleFullscreen, toggleMute, setCurrentTime],
  );

  useEffect(() => {
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [handleKeyDown]);

  const handlePiP = useCallback(async () => {
    const el = videoRef.current;
    if (!el) return;
    try {
      if (document.pictureInPictureElement) {
        await document.exitPictureInPicture();
      } else {
        await el.requestPictureInPicture();
      }
    } catch {}
  }, []);

  const formatTime = (s: number) => {
    const m = Math.floor(s / 60);
    const sec = Math.floor(s % 60);
    return `${m}:${sec.toString().padStart(2, "0")}`;
  };

  if (!video) return null;

  return (
    <div
      ref={containerRef}
      className="relative bg-black w-full max-h-[70vh] group/player overflow-hidden select-none"
      style={{ aspectRatio: video.width && video.height ? `${video.width}/${video.height}` : "16/9" }}
    >
      <video
        ref={videoRef}
        src={video.url || undefined}
        aria-label={video.title}
        className="w-full h-full object-contain"
        onTimeUpdate={handleTimeUpdate}
        onLoadedMetadata={handleLoadedMetadata}
        onEnded={handleEnded}
        onClick={handlePlayPause}
        playsInline
      >
        {subtitlesEnabled && (
          <track
            kind="subtitles"
            src={`/api/v1/video/videos/${video.id}/subtitles.vtt`}
            srcLang="en"
            label="English CC"
            default
          />
        )}
      </video>

      {/* Resume Playback Prompt Banner */}
      {resumePromptTime !== null && resumePromptTime > 5 && (
        <div className="absolute top-4 left-4 z-30 flex items-center gap-2.5 px-3.5 py-2 bg-zinc-900/95 border border-accent/60 rounded-lg shadow-xl backdrop-blur-md text-xs text-zinc-100">
          <span>Resume playback at <strong className="text-accent">{formatTime(resumePromptTime)}</strong>?</span>
          <button
            type="button"
            onClick={() => {
              const el = videoRef.current;
              if (el) el.currentTime = resumePromptTime;
              setCurrentTime(resumePromptTime);
              setResumePromptTime(null);
              setIsPlaying(true);
            }}
            className="px-2.5 py-1 rounded bg-accent text-zinc-900 font-semibold hover:bg-accent-hover transition-colors"
          >
            Resume
          </button>
          <button
            type="button"
            onClick={() => setResumePromptTime(null)}
            className="px-2 py-1 rounded bg-zinc-800 text-zinc-400 hover:text-zinc-200"
          >
            Dismiss
          </button>
        </div>
      )}

      {/* Live Active Chapter Title Indicator */}
      {activeChapterTitle && (
        <div className="absolute top-4 right-4 z-20 px-2.5 py-1 rounded bg-black/60 backdrop-blur-md text-[11px] font-mono text-zinc-300 border border-zinc-800/80">
          {activeChapterTitle}
        </div>
      )}

      {/* Bottom Controls Bar */}
      <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/95 via-black/60 to-transparent px-4 pt-10 pb-2.5 transition-opacity duration-200">
        {/* Timeline Seek Bar with Chapter Markers */}
        <div className="relative w-full mb-2.5">
          <label className="sr-only" htmlFor="video-seek">Seek video</label>
          <input
            id="video-seek"
            type="range"
            min={0}
            max={Math.max(duration, 1)}
            step={0.1}
            value={Math.min(currentTime, Math.max(duration, 1))}
            onChange={(e) => {
              const next = Number(e.target.value);
              const el = videoRef.current;
              if (el) el.currentTime = next;
              setCurrentTime(next);
            }}
            aria-valuetext={`${formatTime(currentTime)} of ${formatTime(duration)}`}
            className="w-full h-2 accent-accent cursor-pointer focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
          />

          {/* Chapter Markers Overlay */}
          {duration > 0 && video.chapters && (
            <div className="absolute inset-x-0 top-1/2 -translate-y-1/2 h-2 pointer-events-none flex items-center">
              {video.chapters.map((ch, i) => {
                const pct = (ch.time / duration) * 100;
                return (
                  <div
                    key={i}
                    style={{ left: `${pct}%` }}
                    className="absolute w-1 h-3 bg-white/70 rounded-full shadow-sm hover:scale-150 transition-transform"
                    title={`${ch.title} (${formatTime(ch.time)})`}
                  />
                );
              })}
            </div>
          )}
        </div>

        <div className="flex items-center justify-between text-white">
          <div className="flex items-center gap-3">
            <button
              type="button"
              aria-label={isPlaying ? "Pause video" : "Play video"}
              onClick={handlePlayPause}
              className="min-h-[44px] min-w-[44px] rounded hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              {isPlaying ? <Pause size={20} fill="currentColor" aria-hidden="true" /> : <Play size={20} fill="currentColor" aria-hidden="true" />}
            </button>

            <div className="flex items-center gap-1.5 group/vol">
              <button
                type="button"
                aria-label={muted || volume === 0 ? "Unmute video" : "Mute video"}
                onClick={toggleMute}
                className="min-h-[44px] min-w-[44px] rounded hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
              >
                {muted || volume === 0 ? <VolumeX size={18} aria-hidden="true" /> : <Volume2 size={18} aria-hidden="true" />}
              </button>
              <label className="sr-only" htmlFor="video-volume">Volume</label>
              <input
                id="video-volume"
                type="range"
                min={0}
                max={1}
                step={0.01}
                value={muted ? 0 : volume}
                onChange={(e) => setVolume(Number(e.target.value))}
                aria-label="Volume"
                className="w-20 h-2 accent-accent focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
              />
            </div>

            <span className="text-xs text-zinc-300 font-mono">
              {formatTime(currentTime)} / {formatTime(duration)}
            </span>
          </div>

          <div className="flex items-center gap-2">
            {/* HLS Adaptive Bitrate Quality Dropdown */}
            <select
              aria-label="HLS streaming quality"
              value={hlsQuality}
              onChange={(e) => setHlsQuality(e.target.value as any)}
              className="min-h-[36px] bg-zinc-900 text-xs text-accent border border-accent/40 rounded px-2 py-0.5 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent font-mono"
            >
              <option value="auto">Auto (HLS)</option>
              <option value="1080p">1080p60</option>
              <option value="720p">720p30</option>
              <option value="480p">480p30</option>
            </select>

            {/* Subtitles CC Toggle */}
            <button
              type="button"
              aria-label={subtitlesEnabled ? "Disable subtitles" : "Enable subtitles"}
              aria-pressed={subtitlesEnabled}
              onClick={() => setSubtitlesEnabled(!subtitlesEnabled)}
              className={`min-h-[36px] px-2 text-xs font-bold font-mono rounded border transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent ${
                subtitlesEnabled
                  ? "bg-accent text-zinc-900 border-accent"
                  : "bg-transparent text-zinc-400 border-zinc-700 hover:text-white"
              }`}
            >
              CC
            </button>

            {/* Playback speed */}
            <select
              aria-label="Playback speed"
              className="min-h-[36px] bg-transparent text-xs text-zinc-300 border border-zinc-600 rounded px-1.5 py-0.5 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent font-mono"
              onChange={(e) => {
                const el = videoRef.current;
                if (el) el.playbackRate = parseFloat(e.target.value);
              }}
            >
              {[0.5, 0.75, 1, 1.25, 1.5, 2].map((r) => (
                <option key={r} value={r} className="bg-zinc-800">
                  {r}x
                </option>
              ))}
            </select>

            {/* Picture in Picture */}
            <button
              type="button"
              aria-label="Picture in picture"
              onClick={handlePiP}
              className="min-h-[44px] min-w-[44px] rounded hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              <PictureInPicture2 size={16} aria-hidden="true" />
            </button>

            {/* Fullscreen */}
            <button
              type="button"
              aria-label={fullscreen ? "Exit fullscreen" : "Enter fullscreen"}
              onClick={toggleFullscreen}
              className="min-h-[44px] min-w-[44px] rounded hover:text-zinc-300 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent"
            >
              {fullscreen ? <Minimize size={18} aria-hidden="true" /> : <Maximize size={18} aria-hidden="true" />}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
