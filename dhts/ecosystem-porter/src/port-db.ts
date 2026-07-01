import type { KnownService } from './types.js';

// ─────────────────────────────────────────────────────────────────────────────
// Well-known port database
// Categories: system | web | dev | database | messaging | storage |
//             container | comms | security | other
// ─────────────────────────────────────────────────────────────────────────────

const DB: Record<number, KnownService> = {
  // ── System ──────────────────────────────────────────────────────────────
  21:    { name: 'FTP',           category: 'system',    description: 'File Transfer Protocol (control)' },
  22:    { name: 'SSH',           category: 'system',    description: 'Secure Shell remote access' },
  23:    { name: 'Telnet',        category: 'system',    description: 'Unencrypted remote shell (legacy)' },
  53:    { name: 'DNS',           category: 'system',    description: 'Domain Name System' },
  67:    { name: 'DHCP',          category: 'system',    description: 'DHCP server' },
  68:    { name: 'DHCP',          category: 'system',    description: 'DHCP client' },
  111:   { name: 'RPC',           category: 'system',    description: 'Sun Remote Procedure Call portmapper' },
  123:   { name: 'NTP',           category: 'system',    description: 'Network Time Protocol' },
  137:   { name: 'NetBIOS NS',    category: 'system',    description: 'NetBIOS Name Service' },
  139:   { name: 'NetBIOS',       category: 'system',    description: 'NetBIOS session service' },
  161:   { name: 'SNMP',          category: 'system',    description: 'Simple Network Management Protocol' },
  445:   { name: 'SMB',           category: 'system',    description: 'Windows file sharing / Samba' },
  514:   { name: 'Syslog',        category: 'system',    description: 'syslog UDP daemon' },
  631:   { name: 'CUPS',          category: 'system',    description: 'Common UNIX Printing System' },
  2049:  { name: 'NFS',           category: 'system',    description: 'Network File System' },

  // ── Web ─────────────────────────────────────────────────────────────────
  80:    { name: 'HTTP',          category: 'web',       description: 'HyperText Transfer Protocol' },
  443:   { name: 'HTTPS',         category: 'web',       description: 'HTTP Secure (TLS)' },
  8080:  { name: 'Alt HTTP',      category: 'web',       description: 'Alternative HTTP port (common for proxies/dev)' },
  8081:  { name: 'Alt HTTP',      category: 'web',       description: 'Alternative HTTP port' },
  8082:  { name: 'Alt HTTP',      category: 'web',       description: 'Alternative HTTP port' },
  8443:  { name: 'Alt HTTPS',     category: 'web',       description: 'Alternative HTTPS port' },
  9080:  { name: 'Alt HTTP',      category: 'web',       description: 'Alternative HTTP port (IBM WebSphere)' },
  9443:  { name: 'Alt HTTPS',     category: 'web',       description: 'Alternative HTTPS / admin HTTPS' },

  // ── Dev servers ─────────────────────────────────────────────────────────
  1234:  { name: 'Parcel',        category: 'dev',       description: 'Parcel bundler dev server' },
  2978:  { name: 'Nexus Porter',  category: 'dev',       description: 'Nexus Porter port intelligence API' },
  3000:  { name: 'Dev Server',    category: 'dev',       description: 'Common Node.js / React dev server port' },
  3001:  { name: 'Dev Server',    category: 'dev',       description: 'Common Node.js / React dev server port (alt)' },
  3002:  { name: 'Dev Server',    category: 'dev',       description: 'Common dev server port' },
  3003:  { name: 'Dev Server',    category: 'dev',       description: 'Common dev server port' },
  3100:  { name: 'Loki',          category: 'dev',       description: 'Grafana Loki log aggregation' },
  3700:  { name: 'Nexus Network', category: 'dev',       description: 'Nexus-Network federation dashboard' },
  3900:  { name: 'Nexus Vault',   category: 'dev',       description: 'Nexus-Vault secrets manager' },
  4000:  { name: 'Dev Server',    category: 'dev',       description: 'Common dev server port (GraphQL, Phoenix)' },
  4001:  { name: 'Dev Server',    category: 'dev',       description: 'Common dev server port' },
  4200:  { name: 'Angular',       category: 'dev',       description: 'Angular CLI dev server' },
  4317:  { name: 'OTLP gRPC',     category: 'dev',       description: 'OpenTelemetry gRPC ingestion' },
  4318:  { name: 'OTLP HTTP',     category: 'dev',       description: 'OpenTelemetry HTTP ingestion' },
  5000:  { name: 'Dev Server',    category: 'dev',       description: 'Common dev server (Flask, Create React App)' },
  5001:  { name: 'Dev Server',    category: 'dev',       description: 'Common dev server alt port' },
  5173:  { name: 'Vite',          category: 'dev',       description: 'Vite development server (default)' },
  5174:  { name: 'Vite',          category: 'dev',       description: 'Vite development server (alternate)' },
  5175:  { name: 'Vite',          category: 'dev',       description: 'Vite development server (alternate)' },
  6006:  { name: 'Storybook',     category: 'dev',       description: 'Storybook UI component explorer' },
  7700:  { name: 'Meilisearch',   category: 'database',  description: 'Meilisearch search engine' },
  8000:  { name: 'Dev HTTP',      category: 'dev',       description: 'Common dev HTTP server (Python, Django, etc.)' },
  8787:  { name: 'Wrangler',      category: 'dev',       description: 'Cloudflare Workers / Nexus-Cloud dev server' },
  8788:  { name: 'Nexus Hosting', category: 'dev',       description: 'Nexus-Hosting API server' },
  9000:  { name: 'MinIO S3',      category: 'storage',   description: 'MinIO S3-compatible object storage API' },
  9001:  { name: 'MinIO UI',      category: 'storage',   description: 'MinIO web console' },
  9090:  { name: 'Prometheus',    category: 'dev',       description: 'Prometheus metrics server' },
  9091:  { name: 'Pushgateway',   category: 'dev',       description: 'Prometheus push gateway' },
  9093:  { name: 'Alertmanager',  category: 'dev',       description: 'Prometheus Alertmanager' },
  9229:  { name: 'Node Debug',    category: 'dev',       description: 'Node.js inspector / debugger protocol' },
  24678: { name: 'Vite HMR',      category: 'dev',       description: 'Vite Hot Module Replacement WebSocket' },

  // ── Databases ────────────────────────────────────────────────────────────
  1433:  { name: 'MSSQL',         category: 'database',  description: 'Microsoft SQL Server' },
  1521:  { name: 'Oracle DB',     category: 'database',  description: 'Oracle Database listener' },
  3306:  { name: 'MySQL',         category: 'database',  description: 'MySQL / MariaDB' },
  5432:  { name: 'PostgreSQL',    category: 'database',  description: 'PostgreSQL relational database' },
  5433:  { name: 'PostgreSQL',    category: 'database',  description: 'PostgreSQL (alternate / replica port)' },
  5984:  { name: 'CouchDB',       category: 'database',  description: 'Apache CouchDB HTTP API' },
  6379:  { name: 'Redis',         category: 'database',  description: 'Redis in-memory data store' },
  6380:  { name: 'Redis TLS',     category: 'database',  description: 'Redis (TLS / alternate port)' },
  7474:  { name: 'Neo4j HTTP',    category: 'database',  description: 'Neo4j graph database HTTP browser' },
  7687:  { name: 'Neo4j Bolt',    category: 'database',  description: 'Neo4j Bolt binary protocol' },
  8086:  { name: 'InfluxDB',      category: 'database',  description: 'InfluxDB HTTP API' },
  8529:  { name: 'ArangoDB',      category: 'database',  description: 'ArangoDB multi-model database' },
  9042:  { name: 'ScyllaDB/Cassandra', category: 'database', description: 'ScyllaDB / Apache Cassandra CQL native transport' },
  9160:  { name: 'Cassandra',     category: 'database',  description: 'Apache Cassandra Thrift (legacy)' },
  9200:  { name: 'Elasticsearch', category: 'database',  description: 'Elasticsearch HTTP API' },
  9300:  { name: 'Elasticsearch', category: 'database',  description: 'Elasticsearch transport layer (cluster)' },
  5601:  { name: 'Kibana',        category: 'web',       description: 'Kibana data analytics dashboard' },
  27017: { name: 'MongoDB',       category: 'database',  description: 'MongoDB document database' },
  27018: { name: 'MongoDB',       category: 'database',  description: 'MongoDB replset / shardsvr member' },
  28017: { name: 'MongoDB',       category: 'database',  description: 'MongoDB legacy web status interface' },

  // ── Messaging / Queues ───────────────────────────────────────────────────
  1883:  { name: 'MQTT',          category: 'messaging', description: 'MQTT IoT messaging broker' },
  4222:  { name: 'NATS',          category: 'messaging', description: 'NATS messaging server' },
  5222:  { name: 'XMPP Client',   category: 'messaging', description: 'XMPP (Jabber) client connections' },
  5269:  { name: 'XMPP S2S',      category: 'messaging', description: 'XMPP server-to-server federation' },
  5672:  { name: 'RabbitMQ AMQP', category: 'messaging', description: 'RabbitMQ AMQP 0-9-1 broker' },
  9092:  { name: 'Kafka',         category: 'messaging', description: 'Apache Kafka broker' },
  15672: { name: 'RabbitMQ UI',   category: 'messaging', description: 'RabbitMQ management dashboard' },
  2181:  { name: 'ZooKeeper',     category: 'messaging', description: 'Apache ZooKeeper (used by Kafka)' },

  // ── Security ─────────────────────────────────────────────────────────────
  500:   { name: 'IKE',           category: 'security',  description: 'Internet Key Exchange (IPsec VPN)' },
  1194:  { name: 'OpenVPN',       category: 'security',  description: 'OpenVPN UDP tunnel' },
  4500:  { name: 'IPsec NAT-T',   category: 'security',  description: 'IPsec NAT traversal' },
  51820: { name: 'WireGuard',     category: 'security',  description: 'WireGuard VPN (UDP)' },

  // ── Containers / Orchestration ───────────────────────────────────────────
  2375:  { name: 'Docker',        category: 'container', description: 'Docker daemon (unencrypted — never expose externally!)' },
  2376:  { name: 'Docker TLS',    category: 'container', description: 'Docker daemon (TLS encrypted)' },
  2377:  { name: 'Docker Swarm',  category: 'container', description: 'Docker Swarm cluster management' },
  4243:  { name: 'Docker',        category: 'container', description: 'Docker daemon (legacy port)' },
  6443:  { name: 'Kubernetes API',category: 'container', description: 'Kubernetes API server' },
  10250: { name: 'kubelet',       category: 'container', description: 'Kubernetes kubelet HTTPS API' },

  // ── Email / Comms ─────────────────────────────────────────────────────────
  25:    { name: 'SMTP',          category: 'comms',     description: 'Simple Mail Transfer Protocol (outbound)' },
  110:   { name: 'POP3',          category: 'comms',     description: 'Post Office Protocol v3' },
  143:   { name: 'IMAP',          category: 'comms',     description: 'Internet Message Access Protocol' },
  465:   { name: 'SMTPS',         category: 'comms',     description: 'SMTP over TLS (implicit)' },
  587:   { name: 'SMTP MSA',      category: 'comms',     description: 'SMTP mail submission (STARTTLS)' },
  993:   { name: 'IMAPS',         category: 'comms',     description: 'IMAP over TLS' },
  995:   { name: 'POP3S',         category: 'comms',     description: 'POP3 over TLS' },
};

export function lookupPort(port: number): KnownService | null {
  return DB[port] ?? null;
}

export { DB as PORT_DB };
