export type Proto = 'tcp' | 'tcp6' | 'udp' | 'udp6';

export interface RawSocket {
  localAddress: string;
  localPort: number;
  remoteAddress: string;
  remotePort: number;
  /** LISTEN, ESTABLISHED, CLOSE, etc. */
  state: string;
  uid: number;
  inode: number;
  proto: Proto;
}

export interface ProcessInfo {
  pid: number;
  /** Short name from /proc/pid/comm */
  name: string;
  /** Full command-line, first 200 chars */
  cmdline: string;
  /** Username resolved from /etc/passwd, or null */
  user: string | null;
  /** Last cgroup entry, used to detect containerised processes */
  cgroup: string | null;
}

export interface HttpProbeResult {
  url: string;
  reachable: boolean;
  statusCode?: number;
  statusText?: string;
  /** Value of the Server response header */
  serverHeader?: string;
  /** <title> text extracted from HTML */
  title?: string;
  contentType?: string;
  /** X-Frame-Options header value, or null if absent */
  xFrameOptions?: string | null;
  /** frame-ancestors directive extracted from Content-Security-Policy, or null */
  frameAncestors?: string | null;
  /** true if this service can safely be embedded in an <iframe> */
  embeddable: boolean;
  tls: boolean;
  responseTimeMs?: number;
  error?: string;
}

export interface KnownService {
  name: string;
  category:
    | 'database'
    | 'web'
    | 'system'
    | 'dev'
    | 'container'
    | 'comms'
    | 'security'
    | 'messaging'
    | 'storage'
    | 'other';
  description: string;
}

export interface ContainerPort {
  containerName: string;
  containerId: string;
  image: string;
  containerPort: number;
  hostPort: number;
  protocol: string;
}

export interface PortEntry {
  port: number;
  /** Primary protocol detected for this port */
  proto: Proto;
  state: string;
  /** Bind address: "0.0.0.0", "127.0.0.1", "::", "::1", etc. */
  bind: string;
  process: ProcessInfo | null;
  /** HTTP probe results (null when probing was not requested) */
  probe: HttpProbeResult | null;
  /** Matched entry from the well-known port database */
  known: KnownService | null;
  container: ContainerPort | null;
  /** All protocols observed for this port number (may include tcp + tcp6) */
  protocols: Proto[];
}

export interface ScanResult {
  scannedAt: string;
  hostname: string;
  probeEnabled: boolean;
  ports: PortEntry[];
  summary: {
    totalListening: number;
    httpReachable: number;
    notEmbeddable: number;
    withProcess: number;
    inContainers: number;
  };
}

export type CapabilityStatus = 'implemented' | 'stub';

export interface PortToolCapability {
  id: string;
  name: string;
  status: CapabilityStatus;
  description: string;
  nextSteps: string[];
}

export interface PortConflictItem {
  port: number;
  taken: boolean;
  owner: ProcessInfo | null;
  known: KnownService | null;
  protocols: Proto[];
}

export interface PortConflictReport {
  scannedAt: string;
  totalTargets: number;
  taken: PortConflictItem[];
  free: number[];
}

export interface PortReconfigurationPlan {
  createdAt: string;
  fromPort: number;
  current: PortEntry | null;
  candidatePort: number | null;
  mode: 'dry-run';
  conflicts: PortConflictReport;
  safetyChecks: string[];
  steps: string[];
  warnings: string[];
}

export interface PortReconfigurationApplyResult {
  ok: false;
  mode: 'stub';
  message: string;
  manualSteps: string[];
}

export interface OutboundIntelligenceStub {
  status: 'stub';
  message: string;
  suggestedSources: string[];
  suggestedFutureEndpoints: string[];
}
