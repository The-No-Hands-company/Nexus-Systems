import {
  CalendarDots,
  PaintBrush,
  Kanban,
  Globe,
  Envelope,
  ChatCircleDots,
  Cloud,
  Terminal,
  AppWindow,
  Camera,
  VideoCamera,
  MusicNote,
  FileMagnifyingGlass,
  Wallet,
} from "@phosphor-icons/react";
import type { Icon as PhosphorIcon } from "@phosphor-icons/react";

export type AppMeta = {
  Icon: PhosphorIcon;
  color: string;
};

const MAP: Record<string, AppMeta> = {
  "nexus-calendar":  { Icon: CalendarDots,        color: "#60A5FA" },
  "nexus-draw":      { Icon: PaintBrush,           color: "#A78BFA" },
  "nexus-project":   { Icon: Kanban,               color: "#34D399" },
  "nexus-hosting":   { Icon: Globe,                color: "#F472B6" },
  "nexus-email":     { Icon: Envelope,             color: "#FB923C" },
  "nexus-mail":      { Icon: Envelope,             color: "#FB923C" },
  "nexus-chat":      { Icon: ChatCircleDots,       color: "#4ADE80" },
  "nexus-cloud":     { Icon: Cloud,                color: "#38BDF8" },
  "nexus-terminal":  { Icon: Terminal,             color: "var(--nexus-color-state-warning)" },
  "nexus-photos":    { Icon: Camera,               color: "#F9A8D4" },
  "nexus-video":     { Icon: VideoCamera,          color: "#FCA5A5" },
  "nexus-music":     { Icon: MusicNote,            color: "#86EFAC" },
  "nexus-search":    { Icon: FileMagnifyingGlass,  color: "#FDE68A" },
  "nexus-wallet":    { Icon: Wallet,               color: "var(--nexus-color-accent-primary)" },
};

function hashColor(id: string): string {
  let h = 0;
  for (let i = 0; i < id.length; i++) h = (Math.imul(h, 31) + id.charCodeAt(i)) >>> 0;
  const hue = h % 360;
  return `hsl(${hue}, 60%, 65%)`;
}

export function appMeta(id: string): AppMeta {
  return MAP[id] ?? { Icon: AppWindow, color: hashColor(id) };
}
