import type { ScanResult, PortEntry } from './types.js';

// ─────────────────────────────────────────────────────────────────────────────
// ANSI colour helpers
// ─────────────────────────────────────────────────────────────────────────────

const C = {
  reset:   '\x1b[0m',
  bold:    '\x1b[1m',
  dim:     '\x1b[2m',
  cyan:    '\x1b[36m',
  green:   '\x1b[32m',
  yellow:  '\x1b[33m',
  red:     '\x1b[31m',
  blue:    '\x1b[34m',
  magenta: '\x1b[35m',
  gray:    '\x1b[90m',
  white:   '\x1b[37m',
} as const;

/** Strip all ANSI escape sequences (for --no-color mode) */
function strip(s: string): string {
  return s.replace(/\x1b\[[0-9;]*m/g, '');
}

function pad(s: string, n: number, right = false): string {
  const plain = strip(s);
  if (plain.length >= n) {
    // Truncate visible characters only; keep ANSI codes by trimming at char boundary
    if (right) return ' '.repeat(n - Math.min(plain.length, n)) + s;
    return s.slice(0, s.length - (plain.length - n));
  }
  const spaces = ' '.repeat(n - plain.length);
  return right ? spaces + s : s + spaces;
}

// ─────────────────────────────────────────────────────────────────────────────
// Column formatters
// ─────────────────────────────────────────────────────────────────────────────

function fmtPort(port: number, noColor: boolean): string {
  const s = String(port).padStart(6);
  if (noColor) return s;
  return `${C.cyan}${C.bold}${s}${C.reset}`;
}

function fmtProtos(protos: string[]): string {
  return protos.join('+').slice(0, 8);
}

function fmtBind(bind: string, noColor: boolean): string {
  if (noColor) return bind;
  const dim = bind === '127.0.0.1' || bind === '::1';
  return dim ? `${C.gray}${bind}${C.reset}` : bind;
}

function fmtProcess(entry: PortEntry, noColor: boolean): string {
  if (entry.process) {
    const name = entry.process.name.slice(0, 16);
    const pid  = String(entry.process.pid);
    if (noColor) return `${name} (${pid})`;
    return `${name}${C.gray} (${pid})${C.reset}`;
  }
  if (entry.container) {
    if (noColor) return '[container]';
    return `${C.blue}[container]${C.reset}`;
  }
  return noColor ? '—' : `${C.gray}—${C.reset}`;
}

function fmtService(entry: PortEntry, noColor: boolean): string {
  if (entry.known) {
    const name = entry.known.name;
    if (noColor) return name;
    const catColor = entry.known.category === 'database' ? C.yellow
      : entry.known.category === 'system'   ? C.white
      : entry.known.category === 'security' ? C.red
      : entry.known.category === 'messaging'? C.magenta
      : C.reset;
    return `${catColor}${name}${C.reset}`;
  }
  if (entry.probe?.title) return entry.probe.title.slice(0, 22);
  return noColor ? '?' : `${C.gray}?${C.reset}`;
}

function fmtHttp(entry: PortEntry, noColor: boolean): string {
  const probe = entry.probe;
  if (!probe) return noColor ? '—' : `${C.gray}—${C.reset}`;

  if (!probe.reachable) {
    const msg = probe.error === 'ECONNREFUSED' ? '✗ tcp' : `✗ ${probe.error ?? 'err'}`;
    return noColor ? msg : `${C.gray}${msg}${C.reset}`;
  }

  const code = probe.statusCode ?? 0;
  const codeStr = noColor
    ? String(code)
    : code >= 200 && code < 300 ? `${C.green}${code}${C.reset}`
    : code >= 300 && code < 400 ? `${C.yellow}${code}${C.reset}`
    : `${C.red}${code}${C.reset}`;

  const embedLabel = probe.embeddable
    ? (noColor ? '✓embed' : `${C.green}✓embed${C.reset}`)
    : (() => {
        const reason = probe.xFrameOptions
          ? probe.xFrameOptions.toUpperCase()
          : probe.frameAncestors
          ? `fa:${probe.frameAncestors.slice(0, 12)}`
          : 'blocked';
        return noColor ? `✗${reason}` : `${C.red}✗${reason}${C.reset}`;
      })();

  const tlsMark = probe.tls ? (noColor ? 's' : `${C.green}s${C.reset}`) : '';

  return `${codeStr}${tlsMark} ${embedLabel}`;
}

function fmtContainer(entry: PortEntry, noColor: boolean): string {
  if (!entry.container) return '';
  const n = entry.container.containerName.slice(0, 20);
  return noColor ? `  [${n}]` : `  ${C.blue}[${n}]${C.reset}`;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public: printReport()
// ─────────────────────────────────────────────────────────────────────────────

export function printReport(result: ScanResult, noColor = false): void {
  const w = (s: string) => process.stdout.write(
    (noColor ? strip(s) : s) + '\n',
  );

  const HR = '─'.repeat(90);

  w('');
  w(`${C.bold}${C.magenta}  ⬡ NEXUS PORTER${C.reset}  ${C.dim}Port Intelligence${C.reset}  ` +
    `${C.gray}${result.hostname}  ·  ${result.scannedAt}${C.reset}`);
  w(`${C.gray}  ${HR}${C.reset}`);
  w('');

  const hasHttp = result.probeEnabled;
  // Column widths
  const COL = { port: 7, proto: 9, bind: 17, proc: 24, svc: 24, http: 28 };

  // Header
  w('  ' +
    pad('PORT',    COL.port,  true)  + '  ' +
    pad('PROTO',   COL.proto)        + '  ' +
    pad('BIND',    COL.bind)         + '  ' +
    pad('PROCESS', COL.proc)         + '  ' +
    pad('SERVICE', COL.svc)          +
    (hasHttp ? '  ' + pad('HTTP', COL.http) : ''));

  w('  ' +
    `${C.gray}` +
    '─'.repeat(COL.port)  + '  ' +
    '─'.repeat(COL.proto) + '  ' +
    '─'.repeat(COL.bind)  + '  ' +
    '─'.repeat(COL.proc)  + '  ' +
    '─'.repeat(COL.svc)   +
    (hasHttp ? '  ' + '─'.repeat(COL.http) : '') +
    `${C.reset}`);

  for (const entry of result.ports) {
    const portStr = fmtPort(entry.port, noColor);
    const protoStr = pad(fmtProtos(entry.protocols), COL.proto);
    const bindStr  = pad(fmtBind(entry.bind, noColor), COL.bind);
    const procStr  = pad(fmtProcess(entry, noColor), COL.proc);
    const svcStr   = pad(fmtService(entry, noColor),  COL.svc);
    const httpStr  = hasHttp ? '  ' + pad(fmtHttp(entry, noColor), COL.http) : '';
    const ctStr    = fmtContainer(entry, noColor);

    w('  ' +
      pad(portStr, COL.port, true) + '  ' +
      protoStr + '  ' +
      bindStr  + '  ' +
      procStr  + '  ' +
      svcStr   +
      httpStr  +
      ctStr);
  }

  w('');
  w(`  ${C.gray}${HR}${C.reset}`);

  // Summary line
  const s = result.summary;
  const parts: string[] = [
    noColor
      ? `${s.totalListening} listening`
      : `${C.bold}${s.totalListening}${C.reset}${C.dim} listening${C.reset}`,
    noColor
      ? `${s.withProcess} identified`
      : `${C.bold}${s.withProcess}${C.reset}${C.dim} identified${C.reset}`,
  ];
  if (s.inContainers > 0) {
    parts.push(noColor
      ? `${s.inContainers} containerised`
      : `${C.blue}${C.bold}${s.inContainers}${C.reset}${C.blue} containerised${C.reset}`);
  }
  if (hasHttp) {
    parts.push(noColor
      ? `${s.httpReachable} HTTP reachable`
      : `${C.green}${C.bold}${s.httpReachable}${C.reset}${C.green} HTTP reachable${C.reset}`);
    if (s.notEmbeddable > 0) {
      parts.push(noColor
        ? `${s.notEmbeddable} not embeddable`
        : `${C.red}${C.bold}${s.notEmbeddable}${C.reset}${C.red} not embeddable${C.reset}`);
    }
  }
  w('  ' + parts.join(`  ${C.gray}·${C.reset}  `));
  w('');

  if (!hasHttp) {
    w(`  ${C.dim}Tip: run with --probe (-p) to check HTTP reachability and iframe embeddability${C.reset}`);
    w('');
  }
}
