import { useRef, useEffect, useCallback } from "react";
import { useEditorStore } from "../stores/useEditorStore";
import {
  Play,
  Pause,
  Volume2,
  VolumeX,
  Maximize,
  Minimize,
  PictureInPicture2,
} from "lucide-react";

export default function VideoPlayer() {
  const videoRef = useRef<HTMLVideoElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  const video = useEditorStore((s) => s.currentlyPlaying);
  const isPlaying = useEditorStore((s) => s.isPlaying);
  const volume = useEditorStore((s) => s.volume);
  const muted = useEditorStore((s) => s.muted);
  const fullscreen = useEditorStore((s) => s.fullscreen);
  const currentTime = useEditorStore((s) => s.currentTime);
  const duration = useEditorStore((s) => s.duration);
  const setIsPlaying = useEditorStore((s) => s.setIsPlaying);
  const setVolume = useEditorStore((s) => s.setVolume);
  const toggleMute = useEditorStore((s) => s.toggleMute);
  const toggleFullscreen = useEditorStore((s) => s.toggleFullscreen);
  const setCurrentTime = useEditorStore((s) => s.setCurrentTime);
  const setDuration = useEditorStore((s) => s.setDuration);
  const seek = useEditorStore((s) => s.seek);

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
    setCurrentTime(el.currentTime);
  }, [setCurrentTime]);

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

  const handleSeek = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const ratio = (e.clientX - rect.left) / rect.width;
    const el = videoRef.current;
    if (!el) return;
    const t = ratio * el.duration;
    el.currentTime = t;
    setCurrentTime(t);
  }, [setCurrentTime]);

  const handleVolume = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const ratio = (e.clientX - rect.left) / rect.width;
    setVolume(ratio);
  }, [setVolume]);

  const handleKeyDown = useCallback((e: KeyboardEvent) => {
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
  }, [isPlaying, setIsPlaying, toggleFullscreen, toggleMute, setCurrentTime]);

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
      className="relative bg-black w-full max-h-[70vh]"
      style={{ aspectRatio: video.width && video.height ? `${video.width}/${video.height}` : "16/9" }}
    >
      <video
        ref={videoRef}
        src={video.url || undefined}
        className="w-full h-full object-contain"
        onTimeUpdate={handleTimeUpdate}
        onLoadedMetadata={handleLoadedMetadata}
        onEnded={handleEnded}
        onClick={handlePlayPause}
        playsInline
      />
      <div className="absolute bottom-0 left-0 right-0 bg-gradient-to-t from-black/80 to-transparent px-4 pt-8 pb-2">
        <div
          className="h-1 bg-zinc-600 rounded cursor-pointer mb-3 group"
          onClick={handleSeek}
        >
          <div
            className="h-full bg-red-600 rounded relative"
            style={{ width: duration > 0 ? `${(currentTime / duration) * 100}%` : "0%" }}
          >
            <div className="absolute right-0 top-1/2 -translate-y-1/2 w-3 h-3 bg-red-600 rounded-full opacity-0 group-hover:opacity-100 transition-opacity" />
          </div>
        </div>
        <div className="flex items-center justify-between text-white">
          <div className="flex items-center gap-3">
            <button onClick={handlePlayPause} className="hover:text-zinc-300">
              {isPlaying ? <Pause size={20} fill="currentColor" /> : <Play size={20} fill="currentColor" />}
            </button>
            <div className="flex items-center gap-1.5 group/vol">
              <button onClick={toggleMute} className="hover:text-zinc-300">
                {muted || volume === 0 ? <VolumeX size={18} /> : <Volume2 size={18} />}
              </button>
              <div
                className="w-20 h-1 bg-zinc-600 rounded cursor-pointer hidden group-hover/vol:block"
                onClick={handleVolume}
              >
                <div
                  className="h-full bg-white rounded"
                  style={{ width: `${(muted ? 0 : volume) * 100}%` }}
                />
              </div>
            </div>
            <span className="text-xs text-zinc-300 font-mono">
              {formatTime(currentTime)} / {formatTime(duration)}
            </span>
          </div>
          <div className="flex items-center gap-2">
            <select
              className="bg-transparent text-xs text-zinc-300 border border-zinc-600 rounded px-1 py-0.5"
              onChange={(e) => {
                const el = videoRef.current;
                if (el) el.playbackRate = parseFloat(e.target.value);
              }}
            >
              {[0.5, 0.75, 1, 1.25, 1.5, 2].map((r) => (
                <option key={r} value={r} className="bg-zinc-800">{r}x</option>
              ))}
            </select>
            <button onClick={handlePiP} className="hover:text-zinc-300">
              <PictureInPicture2 size={16} />
            </button>
            <button onClick={toggleFullscreen} className="hover:text-zinc-300">
              {fullscreen ? <Minimize size={18} /> : <Maximize size={18} />}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
