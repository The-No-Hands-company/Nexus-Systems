/**
 * WCAG 2.1 contrast, used as a build gate.
 *
 * Only Void is exercised on real screens in this increment, so the other three
 * themes ship as data. This is what makes them verified data rather than three
 * untried guesses.
 */

export type RGBA = { r: number; g: number; b: number; a: number };

const HEX = /^#([0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/i;
const RGBA_FN = /^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$/i;

export function parseColor(value: string): RGBA {
  const v = value.trim();

  const hex = v.match(HEX);
  if (hex) {
    let h = hex[1]!;
    if (h.length === 3) h = h.split("").map((c) => c + c).join("");
    const int = parseInt(h.slice(0, 6), 16);
    const a = h.length === 8 ? parseInt(h.slice(6, 8), 16) / 255 : 1;
    return { r: (int >> 16) & 255, g: (int >> 8) & 255, b: int & 255, a };
  }

  const fn = v.match(RGBA_FN);
  if (fn) {
    return {
      r: Number(fn[1]), g: Number(fn[2]), b: Number(fn[3]),
      a: fn[4] === undefined ? 1 : Number(fn[4]),
    };
  }

  // Named colours and modern syntaxes are deliberately unsupported: guessing at
  // a value here would make the gate lie about a colour it cannot actually read.
  throw new Error(`unsupported colour: ${value}`);
}

/** Alpha-composite `fg` over an assumed-opaque `bg`. */
export function composite(fg: RGBA, bg: RGBA): RGBA {
  const a = fg.a;
  return {
    r: fg.r * a + bg.r * (1 - a),
    g: fg.g * a + bg.g * (1 - a),
    b: fg.b * a + bg.b * (1 - a),
    a: 1,
  };
}

function channel(c: number): number {
  const s = c / 255;
  return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
}

function luminance(c: RGBA): number {
  return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
}

export function contrastRatio(fg: string, bg: string): number {
  const back = parseColor(bg);
  const front = parseColor(fg);
  const solid = front.a < 1 ? composite(front, back) : front;
  const l1 = luminance(solid);
  const l2 = luminance(back);
  const [hi, lo] = l1 >= l2 ? [l1, l2] : [l2, l1];
  return (hi + 0.05) / (lo + 0.05);
}

export type Finding = {
  themeId: string;
  pair: string;
  ratio: number;
  required: number;
  ok: boolean;
};

const BACKGROUNDS = ["bg.canvas", "bg.surface", "bg.elevated", "bg.raised"];

/**
 * One rule per colour that actually renders somewhere. `against` overrides
 * the default surfaces a token is checked against — everything sits on the
 * four backgrounds above except `text.inverse`, which never appears on them:
 * it is the primary Button's own label colour, painted only on
 * `accent.primary`, so that is the one pairing that matters.
 *
 * Body text must clear 4.5:1. Muted text, the accent, its hover/active
 * states, the state colours and borders are treated as non-text UI at 3:1 —
 * muted is for de-emphasised and disabled copy, the state colours appear as
 * pills and dots rather than prose, and borders convey a boundary, not text.
 */
const RULES: Array<{ token: string; required: number; against?: string[] }> = [
  { token: "text.primary", required: 4.5 },
  { token: "text.secondary", required: 4.5 },
  { token: "text.muted", required: 3 },
  { token: "text.inverse", required: 4.5, against: ["accent.primary"] },
  { token: "accent.primary", required: 3 },
  { token: "accent.hover", required: 3 },
  { token: "accent.active", required: 3 },
  { token: "state.success", required: 3 },
  { token: "state.warning", required: 3 },
  { token: "state.danger", required: 3 },
  { token: "state.info", required: 3 },
  { token: "border.subtle", required: 3 },
  { token: "border.strong", required: 3 },
];

export function validateTheme(themeId: string, colors: Record<string, string>): Finding[] {
  const findings: Finding[] = [];
  for (const { token, required, against } of RULES) {
    const fg = colors[token];
    if (fg === undefined) throw new Error(`${themeId}: missing token ${token}`);
    for (const bgToken of against ?? BACKGROUNDS) {
      const bg = colors[bgToken];
      if (bg === undefined) throw new Error(`${themeId}: missing token ${bgToken}`);
      const ratio = contrastRatio(fg, bg);
      findings.push({
        themeId,
        pair: `${token} on ${bgToken}`,
        ratio: Math.round(ratio * 100) / 100,
        required,
        ok: ratio >= required,
      });
    }
  }
  return findings;
}
