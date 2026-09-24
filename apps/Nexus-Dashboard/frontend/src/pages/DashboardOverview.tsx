import { useMemo, useState, type ReactNode } from "react";
import { Link } from "react-router-dom";
import {
  Bell, CalendarBlank, CaretDown, ChartDonut, CheckCircle,
  CloudArrowUp, CloudCheck, Code, Cube, Database, Gear, Globe, HardDrives,
  Key, Lifebuoy, List, MagnifyingGlass, Pulse, Receipt, ShieldCheck, Stack,
  Users, UsersThree, WarningCircle, X,
} from "@phosphor-icons/react";
import {
  Area, AreaChart, CartesianGrid, Cell, Pie, PieChart, ResponsiveContainer,
  Tooltip, XAxis, YAxis,
} from "recharts";
import type { AppEntry, Me } from "../api";

const resourceData = [
  { day: "May 8", cpu: 76, memory: 52, bandwidth: 27 },
  { day: "May 9", cpu: 64, memory: 42, bandwidth: 22 },
  { day: "May 10", cpu: 74, memory: 48, bandwidth: 26 },
  { day: "May 11", cpu: 70, memory: 41, bandwidth: 23 },
  { day: "May 12", cpu: 61, memory: 40, bandwidth: 20 },
  { day: "May 13", cpu: 72, memory: 38, bandwidth: 19 },
  { day: "May 14", cpu: 64, memory: 45, bandwidth: 24 },
];

const billing = [
  { name: "Compute", value: 1200, color: "#3486ff" },
  { name: "Storage", value: 800, color: "#8756f6" },
  { name: "Bandwidth", value: 300, color: "#34d399" },
  { name: "Other", value: 150, color: "#f59e0b" },
];

const navGroups = [
  { label: "Manage", items: [
    ["Services", Stack, "/cloud"], ["Infrastructure", HardDrives, "/cloud/tools"],
    ["Billing", Receipt, "/account"], ["Users", Users, "/admin"],
    ["Teams", UsersThree, "/admin"], ["API Keys", Key, "/cloud/api"],
    ["Settings", Gear, "/account"], ["Support", Lifebuoy, "/report"],
  ] },
  { label: "Other services", items: [
    ["Domains", Globe, "/cloud"], ["Storage", Stack, "/cloud"],
    ["Databases", Database, "/cloud"], ["Monitoring", Pulse, "/cloud"],
    ["Backups", ShieldCheck, "/cloud"],
  ] },
] as const;

function BrandMark() {
  return <CloudCheck aria-hidden="true" weight="duotone" size={30} className="dashboard-brand-icon" />;
}

export default function DashboardOverview({ user, apps, launcher }: { user: Me; apps: AppEntry[]; launcher?: ReactNode }) {
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [range, setRange] = useState("Last 7 days");
  const [serviceFilter, setServiceFilter] = useState("All Services");
  const [notice, setNotice] = useState<string | null>(null);

  const services = useMemo(() => {
    const source = apps.length ? apps.slice(0, 5) : [
      { id: "api", name: "Nexus API", description: "API Gateway Service", path: "/cloud/api", url: "", health: "healthy" as const },
      { id: "auth", name: "Auth Service", description: "Authentication & Users", path: "/account", url: "", health: "healthy" as const },
      { id: "storage", name: "Storage Service", description: "Object Storage", path: "/cloud", url: "", health: "offline" as const },
      { id: "database", name: "Database", description: "PostgreSQL Cluster", path: "/cloud", url: "", health: "healthy" as const },
      { id: "cdn", name: "CDN", description: "Content Delivery Network", path: "/cloud", url: "", health: "healthy" as const },
    ];
    return source.map((app, index) => ({
      ...app,
      uptime: app.health === "healthy" ? (index % 2 ? "100%" : "99.99%") : "99.2%",
      usage: ["12.4K", "8.7K", "2.1TB", "256GB", "1.2TB"][index] ?? "—",
      usageLabel: index < 2 ? "Requests" : index === 4 ? "Bandwidth" : "Storage",
    }));
  }, [apps]);

  const action = (message: string) => {
    setNotice(message);
    window.setTimeout(() => setNotice(null), 2400);
  };

  const initials = (user.username || user.email || "E").slice(0, 1).toUpperCase();

  return (
    <div className="platform-dashboard">
      <aside className={`dashboard-sidebar ${sidebarOpen ? "is-open" : ""}`} aria-label="Platform navigation">
        <div className="dashboard-brand"><BrandMark /><span>tnhc.dev</span></div>
        <Link to="/" className="dashboard-nav-active"><ChartDonut size={19} weight="duotone" />Dashboard</Link>
        {navGroups.map((group) => (
          <nav key={group.label} aria-label={group.label} className="dashboard-nav-group">
            <p>{group.label}</p>
            {group.items.map(([label, Icon, to]) => (
              <Link key={label} to={to} onClick={() => setSidebarOpen(false)}><Icon size={19} />{label}</Link>
            ))}
          </nav>
        ))}
        {launcher && <div className="dashboard-app-launcher">{launcher}</div>}
        <div className="dashboard-account-card">
          <Link to="/account" className="dashboard-account-row">
            <span className="dashboard-avatar">{initials}</span>
            <span><strong>{user.username || "Eric Håkansson"}</strong><small>{user.role || "Admin"}</small></span>
            <CaretDown size={15} />
          </Link>
          <a href="https://tnhc.dev" className="dashboard-site-link">tnhc.dev <Globe size={13} /></a>
        </div>
      </aside>

      {sidebarOpen && <button className="dashboard-scrim" aria-label="Close menu" onClick={() => setSidebarOpen(false)} />}

      <div className="dashboard-stage">
        <header className="dashboard-topbar">
          <button className="dashboard-menu" onClick={() => setSidebarOpen((open) => !open)} aria-label="Open menu">
            {sidebarOpen ? <X size={22} /> : <List size={22} />}
          </button>
          <label className="dashboard-search">
            <MagnifyingGlass size={18} />
            <input aria-label="Search services, resources and docs" placeholder="Search services, resources, docs..." />
            <kbd>⌘ K</kbd>
          </label>
          <div className="dashboard-top-actions">
            <button aria-label="Notifications"><Bell size={21} /></button>
            <Link to="/report" aria-label="Support"><Lifebuoy size={21} /></Link>
            {(user.role === "founder" || user.role === "admin") && <Link to="/admin" className="dashboard-operator">Operator</Link>}
            <Link to="/account" className="dashboard-top-avatar">{initials}<i /></Link>
          </div>
        </header>

        <main className="dashboard-main">
          <section className="dashboard-hero">
            <div><h1>Dashboard</h1><p>Welcome back, {user.username || "Eric"}. Here’s what’s happening with your platform.</p></div>
            <label className="dashboard-select"><CalendarBlank size={18} /><select value={range} onChange={(e) => setRange(e.target.value)}><option>Last 7 days</option><option>Last 30 days</option><option>This year</option></select></label>
          </section>

          <section className="metric-grid" aria-label="Platform metrics">
            <Metric icon={<Cube />} tone="blue" label="Total Services" value={String(apps.length || 12)} detail="↑ 20%" suffix="from last 7 days" />
            <Metric icon={<UsersThree />} tone="purple" label="Active Users" value="248" detail="↑ 15%" suffix="from last 7 days" />
            <Metric icon={<Receipt />} tone="green" label="Revenue" value="$2,450.00" detail="↑ 12%" suffix="from last 7 days" />
            <Metric icon={<ShieldCheck />} tone="amber" label="Uptime" value="99.99%" detail="●" suffix="All systems operational" />
          </section>

          <section className="dashboard-middle-grid">
            <div className="dashboard-panel services-panel">
              <PanelHeading title="Services Overview" action="View all services" to="/cloud" />
              <div className="service-list">
                {services.map((service, index) => (
                  <Link to={service.path} className="service-row" key={service.id}>
                    <span className={`service-icon service-icon-${index % 5}`}>{index === 2 ? <Stack /> : index === 3 ? <Database /> : index === 4 ? <Globe /> : <Code />}</span>
                    <span className="service-name"><strong>{service.name}</strong><small>{service.description || "Nexus platform service"}</small></span>
                    <span className={`service-health ${service.health === "healthy" ? "healthy" : "warning"}`}>{service.health === "healthy" ? <CheckCircle weight="fill" /> : <WarningCircle weight="fill" />}{service.health === "healthy" ? "Healthy" : "Warning"}</span>
                    <span className="service-stat"><strong>{service.uptime}</strong><small>Uptime</small></span>
                    <span className="service-stat"><strong>{service.usage}</strong><small>{service.usageLabel}</small></span>
                  </Link>
                ))}
              </div>
              <Link className="panel-footer-link" to="/cloud">View all services →</Link>
            </div>

            <div className="dashboard-panel usage-panel">
              <div className="panel-heading"><h2>Resource Usage</h2><label className="dashboard-select compact"><select value={serviceFilter} onChange={(e) => setServiceFilter(e.target.value)}><option>All Services</option><option>Compute</option><option>Storage</option></select></label></div>
              <div className="usage-legend"><span className="cpu">CPU Usage</span><span className="memory">Memory Usage</span><span className="bandwidth">Bandwidth</span></div>
              <div className="resource-chart" aria-label={`Resource usage for ${serviceFilter}`}>
                <ResponsiveContainer width="100%" height="100%">
                  <AreaChart data={resourceData} margin={{ top: 8, right: 8, left: -18, bottom: 0 }}>
                    <CartesianGrid vertical={false} stroke="rgba(132,157,188,.12)" />
                    <XAxis dataKey="day" tick={{ fill: "#8da0ba", fontSize: 11 }} axisLine={false} tickLine={false} />
                    <YAxis domain={[0, 100]} tickFormatter={(v) => `${v}%`} tick={{ fill: "#8da0ba", fontSize: 11 }} axisLine={false} tickLine={false} />
                    <Tooltip contentStyle={{ background: "#0d1a2a", border: "1px solid #22344a", borderRadius: 8 }} />
                    <Area type="monotone" dataKey="cpu" stroke="#3486ff" fill="#3486ff" fillOpacity={0.08} strokeWidth={2} />
                    <Area type="monotone" dataKey="memory" stroke="#985eff" fill="#985eff" fillOpacity={0.04} strokeWidth={2} />
                    <Area type="monotone" dataKey="bandwidth" stroke="#34d399" fill="#34d399" fillOpacity={0.03} strokeWidth={2} />
                  </AreaChart>
                </ResponsiveContainer>
              </div>
              <div className="usage-summary"><Summary label="CPU Usage" value="45%" /><Summary label="Memory Usage" value="62%" /><Summary label="Bandwidth" value="23%" /></div>
            </div>
          </section>

          <section className="dashboard-bottom-grid">
            <div className="dashboard-panel quick-panel"><h2>Quick Actions</h2><div className="quick-actions">
              <QuickAction icon={<CloudArrowUp />} label="Deploy Service" onClick={() => action("Deployment flow opened")} />
              <QuickAction icon={<Database />} label="Create Database" onClick={() => action("Database creator opened")} />
              <QuickAction icon={<Globe />} label="Add Domain" onClick={() => action("Domain setup opened")} />
              <QuickAction icon={<Code />} label="View Logs" onClick={() => action("Logs opened")} />
            </div></div>
            <div className="dashboard-panel activity-panel"><PanelHeading title="Recent Activity" action="View all" to="/cloud" /><ActivityRow icon={<CheckCircle />} title="Service Nexus API deployed" time="2 minutes ago" /><ActivityRow icon={<Database />} title="Database backup completed" time="15 minutes ago" /><ActivityRow icon={<Users />} title="New user registered" time="1 hour ago" /></div>
            <div className="dashboard-panel billing-panel"><PanelHeading title="Billing Overview" action="View billing" to="/account" /><div className="billing-content"><div className="billing-total"><small>Current Month</small><strong>$2,450.00</strong><span>↓ 8% <em>from last month</em></span></div><div className="billing-chart"><ResponsiveContainer width="100%" height="100%"><PieChart><Pie data={billing} dataKey="value" innerRadius={34} outerRadius={53} stroke="none">{billing.map((item) => <Cell key={item.name} fill={item.color} />)}</Pie></PieChart></ResponsiveContainer></div><ul>{billing.map((item) => <li key={item.name}><i style={{ background: item.color }} />{item.name}<span>${item.value.toLocaleString()}.00</span></li>)}</ul></div></div>
          </section>
          <footer className="dashboard-footer"><span>© 2026 TNHC DEV. All rights reserved.</span><nav><a href="https://tnhc.dev/docs">Docs</a><a href="https://tnhc.dev/status">Status</a><a href="https://tnhc.dev/privacy">Privacy</a><a href="https://tnhc.dev/terms">Terms</a></nav></footer>
        </main>
      </div>
      {notice && <div className="dashboard-toast" role="status"><CheckCircle weight="fill" />{notice}</div>}
    </div>
  );
}

function Metric({ icon, tone, label, value, detail, suffix }: { icon: React.ReactNode; tone: string; label: string; value: string; detail: string; suffix: string }) {
  return <article className="metric-card"><span className={`metric-icon ${tone}`}>{icon}</span><div><small>{label}</small><strong>{value}</strong><p><b>{detail}</b> {suffix}</p></div></article>;
}

function PanelHeading({ title, action, to }: { title: string; action: string; to: string }) { return <div className="panel-heading"><h2>{title}</h2><Link to={to}>{action} →</Link></div>; }
function Summary({ label, value }: { label: string; value: string }) { return <div><small>{label}</small><strong>{value}</strong><span>Average</span></div>; }
function QuickAction({ icon, label, onClick }: { icon: React.ReactNode; label: string; onClick: () => void }) { return <button onClick={onClick}><span>{icon}</span><small>{label}</small></button>; }
function ActivityRow({ icon, title, time }: { icon: React.ReactNode; title: string; time: string }) { return <div className="activity-row"><span>{icon}</span><div><strong>{title}</strong><small>{time}</small></div></div>; }
