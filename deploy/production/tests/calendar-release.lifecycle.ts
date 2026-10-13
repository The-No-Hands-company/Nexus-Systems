import { cp, mkdtemp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const systemsRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../..");
const calendarRoot = path.join(systemsRoot, "apps/Nexus-Calendar");
const deployRoot = path.join(systemsRoot, "apps/Nexus-Deploy");
const cloudRoot = path.join(systemsRoot, "apps/Nexus-Cloud");
const identityRoot = path.join(systemsRoot, "packages/nexus-identity");
const token = "disposable-calendar-release-test-token";
const cloudKey = `disposable-${crypto.randomUUID()}`;
let auth: ReturnType<typeof Bun.serve> | undefined;
const headers = (extra: Record<string, string> = {}) => ({
  authorization: `Bearer ${token}`,
  accept: "application/json",
  "content-type": "application/json",
  ...extra,
});
// Cloud only, and without Deploy's bearer: Cloud's checkApiKey prefers a Bearer
// token over x-api-key, so carrying both makes it check the wrong credential.
const cloudHeaders = { accept: "application/json", "content-type": "application/json", "x-api-key": cloudKey };

async function requestJson<T>(url: string, init: RequestInit = {}): Promise<T> {
  const response = await fetch(url, { ...init, signal: AbortSignal.timeout(10_000) });
  const text = await response.text();
  if (!response.ok) throw new Error(`${init.method ?? "GET"} ${url} returned ${response.status}: ${text.slice(0, 500)}`);
  return text ? JSON.parse(text) as T : {} as T;
}

async function waitFor<T>(label: string, fn: () => Promise<T | null>, accept: (value: T) => boolean, timeoutMs = 180_000): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  let last: T | null = null;
  while (Date.now() < deadline) {
    last = await fn().catch(() => null);
    if (last !== null && accept(last)) return last;
    await Bun.sleep(1000);
  }
  throw new Error(`Timed out waiting for ${label}; last value: ${JSON.stringify(last)}`);
}

function freePort(): number {
  const probe = Bun.serve({ hostname: "127.0.0.1", port: 0, fetch: () => new Response("ok") });
  const port = probe.port;
  probe.stop(true);
  return port;
}

function startProcess(command: string[], cwd: string, env: Record<string, string>) {
  return Bun.spawn(command, {
    cwd,
    env: { PATH: process.env.PATH ?? "/usr/bin:/bin", HOME: process.env.HOME ?? os.tmpdir(), TMPDIR: os.tmpdir(), ...env },
    stdout: "ignore",
    stderr: "inherit",
  });
}

async function stopProcess(process: ReturnType<typeof startProcess> | undefined) {
  if (!process || process.exitCode !== null) return;
  process.kill("SIGTERM");
  await Promise.race([process.exited, Bun.sleep(5000)]);
  if (process.exitCode === null) process.kill("SIGKILL");
}

function git(repoDir: string, ...args: string[]) {
  const result = Bun.spawnSync(["git", ...args], { cwd: repoDir, stdout: "pipe", stderr: "pipe" });
  if (result.exitCode !== 0) throw new Error(`git ${args.join(" ")} failed: ${result.stderr.toString()}`);
}

async function createSourceFixture(): Promise<string> {
  const temp = await mkdtemp(path.join(os.tmpdir(), "tnhc-calendar-release-"));
  try {
    const repo = path.join(temp, "source");
    const app = path.join(repo, "apps/Nexus-Calendar");
    await mkdir(app, { recursive: true });
    // Same shape as the monorepo: Calendar imports packages/nexus-identity, so
    // the image is built from the repo root with -f apps/Nexus-Calendar/Dockerfile.
    await cp(path.join(calendarRoot, "src"), path.join(app, "src"), { recursive: true });
    await cp(path.join(calendarRoot, "Dockerfile"), path.join(app, "Dockerfile"));
    await cp(path.join(calendarRoot, "Dockerfile.dockerignore"), path.join(app, "Dockerfile.dockerignore"));
    await cp(path.join(calendarRoot, "package.json"), path.join(app, "package.json"));
    const identity = path.join(repo, "packages/nexus-identity");
    await mkdir(identity, { recursive: true });
    await cp(path.join(identityRoot, "src"), path.join(identity, "src"), { recursive: true });
    await cp(path.join(identityRoot, "package.json"), path.join(identity, "package.json"));
    git(repo, "init", "-b", "main");
    git(repo, "config", "user.name", "Nexus lifecycle test");
    git(repo, "config", "user.email", "nexus-lifecycle@localhost");
    return temp;
  } catch (error) {
    await rm(temp, { recursive: true, force: true });
    throw error;
  }
}

async function updateVersion(appDir: string, version: string, repoDir: string) {
  const packagePath = path.join(appDir, "package.json");
  const pkg = JSON.parse(await readFile(packagePath, "utf8")) as { version: string };
  pkg.version = version;
  await writeFile(packagePath, `${JSON.stringify(pkg, null, 2)}\n`);
  git(repoDir, "add", "-A");
  git(repoDir, "commit", "-m", `Calendar ${version}`);
}

const host = `calendar-proof-${crypto.randomUUID().slice(0, 8)}.nexus.local`;
const toolId = `calendar-proof-${crypto.randomUUID().replaceAll("-", "")}`;
const publicUrl = `http://${host}`;
let appPort = 0;
let cloudPort = 0;
let deployPort = 0;

let tempDir = "";
let stateDir = "";
let projectId = "";
let cloudProcess: ReturnType<typeof startProcess> | undefined;
let deployProcess: ReturnType<typeof startProcess> | undefined;
const imageTags = new Set<string>();
let firstDeploymentId = "";
let proxyModule: typeof import("../proxy") | undefined;
let exposureRequested = false;

try {
  const docker = Bun.spawnSync(["docker", "info"], { stdout: "ignore", stderr: "pipe" });
  if (docker.exitCode !== 0) throw new Error(`Docker daemon is unavailable; run this on a Linux Docker-enabled disposable runner. ${docker.stderr.toString().trim()}`);

  appPort = freePort();
  cloudPort = freePort();
  deployPort = freePort();
  auth = Bun.serve({ hostname: "127.0.0.1", port: 0, fetch(request) {
    if (new URL(request.url).pathname !== "/api/v1/auth/check" || request.headers.get("authorization") !== `Bearer ${token}`) {
      return Response.json({ authorized: false }, { status: 401 });
    }
    return Response.json({ authorized: true, userId: "usr-calendar-release-test", user: { id: "usr-calendar-release-test", username: "release-test" } });
  } });

  stateDir = await mkdtemp(path.join(os.tmpdir(), "tnhc-calendar-stack-"));
  const cloudState = path.join(stateDir, "cloud");
  const deployState = path.join(stateDir, "deploy");
  await mkdir(cloudState, { recursive: true });
  await mkdir(deployState, { recursive: true });
  const cloudBase = `http://127.0.0.1:${cloudPort}`;
  const deployBase = `http://127.0.0.1:${deployPort}`;

  cloudProcess = startProcess(["bun", "run", path.join(cloudRoot, "src/server.ts")], cloudState, {
    PORT: String(cloudPort), NEXUS_BIND_HOST: "127.0.0.1", NEXUS_CLOUD_DOMAIN: "nexus.local",
    NEXUS_CLOUD_API_KEY: cloudKey, SYSTEMS_API_MODE: "standalone",
  });
  deployProcess = startProcess(["node", "--import", "tsx", path.join(deployRoot, "src/index.ts")], deployRoot, {
    PORT: String(deployPort), BASE_URL: deployBase, BASE_DOMAIN: "nexus.local", DATA_DIR: deployState,
    APP_SECRET: crypto.randomUUID(), JWT_SECRET: crypto.randomUUID(), ADMIN_PASSWORD: crypto.randomUUID(),
    NEXUS_AUTH_URL: `http://127.0.0.1:${auth.port}`, DOCKER_NETWORK: "host",
    // The fixture repository is a local git repo; production Deploy refuses these.
    DEPLOY_ALLOW_LOCAL_REPOS: "true", DEPLOY_HEALTH_TIMEOUT_MS: "30000",
  });

  await waitFor("isolated Cloud startup", async () => fetch(`${cloudBase}/health`).then((response) => response.ok ? true : null), (ok) => ok === true, 30_000);
  await waitFor("isolated Deploy startup", async () => fetch(`${deployBase}/api/health`).then((response) => response.ok ? true : null), (ok) => ok === true, 30_000);
  await requestJson(`${deployBase}/api/health`);
  await requestJson(`${cloudBase}/health`);
  tempDir = await createSourceFixture();
  const repoDir = path.join(tempDir, "source");
  const appDir = path.join(repoDir, "apps/Nexus-Calendar");
  await updateVersion(appDir, "0.1.0-proof-a", repoDir);

  const created = await requestJson<{ project: { id: string } }>(`${deployBase}/api/projects`, {
    method: "POST", headers: headers(), body: JSON.stringify({
      name: toolId, repo: `file://${repoDir}`, branch: "main", buildContext: ".",
      dockerfile: "apps/Nexus-Calendar/Dockerfile", healthPath: "/health",
      buildCommand: "bun run check", startCommand: "bun run src/index.ts", port: appPort,
      customDomain: host,
    }),
  });
  projectId = created.project.id;
  await requestJson(`${deployBase}/api/projects/${projectId}/env`, {
    method: "PUT", headers: headers(), body: JSON.stringify({ env: {
      PORT: String(appPort), NEXUS_BIND_HOST: "0.0.0.0", NEXUS_CALENDAR_DB: `/tmp/${toolId}.sqlite`,
      NEXUS_CALENDAR_TOOL_ID: toolId, NEXUS_CALENDAR_PATH: "/calendar-proof", NEXUS_CALENDAR_PUBLIC_URL: publicUrl,
      NEXUS_CALENDAR_BASE_URL: `http://127.0.0.1:${appPort}`, NEXUS_CLOUD_URL: cloudBase,
      ...(cloudKey ? { NEXUS_CLOUD_API_KEY: cloudKey } : {}),
    } }),
  });

  async function deploy(commitSha: string) {
    const result = await requestJson<{ deployment: { id: string } }>(`${deployBase}/api/projects/${projectId}/deploy`, {
      method: "POST", headers: headers(), body: JSON.stringify({ commitSha }),
    });
    const depId = result.deployment.id;
    const project = await waitFor("deployment completion", async () => {
      const result = await requestJson<{ project: { status: string; imageTag?: string }; deployments: Array<{ id: string; status: string; imageTag?: string }> }>(`${deployBase}/api/projects/${projectId}`, { headers: headers() });
      const dep = result.deployments.find((item) => item.id === depId);
      if (dep?.imageTag) imageTags.add(dep.imageTag);
      return dep ? { project: result.project, dep } : null;
    }, (value) => ["live", "failed"].includes(value.dep.status));
    if (project.dep.status !== "live") throw new Error(`Deployment ${depId} failed: ${(project.dep as { logs?: string[] }).logs?.slice(-15).join("\n") ?? JSON.stringify(project.dep)}`);
    return depId;
  }

  async function assertExposedVersion(expected: string) {
    await waitFor(`Calendar version ${expected}`, async () => {
      const response = await fetch(`http://127.0.0.1:${appPort}/api/v1/status`).catch(() => null);
      if (!response?.ok) return null;
      return await response.json() as { service: string; version: string; ready: boolean };
    }, (value) => value.service === toolId && value.version === expected && value.ready);

    const tool = await waitFor("Cloud healthy registration", async () => requestJson<{ tool?: { health: string } }>(`${cloudBase}/api/v1/tools/${toolId}`, { headers: cloudHeaders }), (value) => value.tool?.health === "healthy");
    if (tool.tool?.health !== "healthy") throw new Error("Cloud did not confirm Calendar health");

    if (!exposureRequested) {
      await requestJson(`${cloudBase}/api/v1/exposures`, {
        method: "POST", headers: cloudHeaders, body: JSON.stringify({ toolId, desiredHost: host }),
      });
      exposureRequested = true;
    }

    const routes = await requestJson<{ routes: Array<{ domain: string; upstream: string }> }>(`${cloudBase}/api/v1/routes`, { headers: cloudHeaders });
    const route = routes.routes.find((item) => item.domain === host);
    if (!route || route.upstream !== `http://127.0.0.1:${appPort}`) throw new Error(`Cloud route mismatch: ${JSON.stringify(route)}`);

    process.env.CLOUD_URL = cloudBase;
    process.env.NEXUS_CLOUD_API_KEY = cloudKey;
    process.env.DOMAIN = "nexus.local";
    process.env.GATE_SKIP_AUTH = "true";
    process.env.POLL_INTERVAL_MS = "0";
    process.env.CACHE_TTL_MS = "0";
    proxyModule ??= await import("../proxy");
    const mapped = proxyModule.buildRouteMap(routes.routes as never)[host];
    if (mapped?.upstream !== route.upstream) throw new Error(`Production proxy route map mismatch: ${JSON.stringify(mapped)}`);
    const response = await proxyModule.handleRequest(new Request(`http://${host}/api/v1/status`));
    const body = await response.json() as { service?: string; version?: string };
    if (response.status !== 200 || body.service !== toolId || body.version !== expected) {
      throw new Error(`Proxy did not serve ${expected}: ${response.status} ${JSON.stringify(body)}`);
    }
  }

  firstDeploymentId = await deploy("proof-a");
  await assertExposedVersion("0.1.0-proof-a");

  await updateVersion(appDir, "0.1.0-proof-b", repoDir);
  const secondDeploymentId = await deploy("proof-b");
  await assertExposedVersion("0.1.0-proof-b");

  const goodDockerfile = await readFile(path.join(appDir, "Dockerfile"), "utf8");
  await writeFile(path.join(appDir, "Dockerfile"), "THIS IS AN INTENTIONALLY INVALID DOCKERFILE\n");
  git(repoDir, "add", "apps/Nexus-Calendar/Dockerfile");
  git(repoDir, "commit", "-m", "Add an intentionally broken release");
  const failedBuild = await requestJson<{ deployment: { id: string } }>(`${deployBase}/api/projects/${projectId}/deploy`, {
    method: "POST", headers: headers(), body: JSON.stringify({ commitSha: "proof-failed" }),
  });
  const failedDeployment = await waitFor("intentional failed deployment", async () => {
    const result = await requestJson<{ deployments: Array<{ id: string; status: string }> }>(`${deployBase}/api/projects/${projectId}`, { headers: headers() });
    return result.deployments.find((item) => item.id === failedBuild.deployment.id) ?? null;
  }, (value) => value.status === "failed");
  if (failedDeployment.status !== "failed") throw new Error("The intentionally invalid release did not fail");
  await assertExposedVersion("0.1.0-proof-b");

  // A release that builds but never becomes healthy must be rejected, and the
  // release that was live before it must be serving again. This is what makes
  // Deploy's "live" a claim about the app rather than about `docker run`.
  const liveBefore = await requestJson<{ project: { imageTag?: string } }>(`${deployBase}/api/projects/${projectId}`, { headers: headers() });
  await writeFile(path.join(appDir, "Dockerfile"), `${goodDockerfile}\nCMD ["bun", "-e", "process.exit(1)"]\n`);
  await updateVersion(appDir, "0.1.0-proof-crash", repoDir);
  const crashed = await requestJson<{ deployment: { id: string } }>(`${deployBase}/api/projects/${projectId}/deploy`, {
    method: "POST", headers: headers(), body: JSON.stringify({ commitSha: "proof-crash" }),
  });
  const crashOutcome = await waitFor("unhealthy release rejection", async () => {
    const result = await requestJson<{ project: { status: string; imageTag?: string }; deployments: Array<{ id: string; status: string; imageTag?: string; logs: string[] }> }>(`${deployBase}/api/projects/${projectId}`, { headers: headers() });
    const dep = result.deployments.find((item) => item.id === crashed.deployment.id);
    if (dep?.imageTag) imageTags.add(dep.imageTag);
    return dep && ["live", "failed"].includes(dep.status) && result.project.status !== "building" ? { project: result.project, dep } : null;
  }, () => true);
  if (crashOutcome.dep.status !== "failed") throw new Error(`A release that exits on boot was marked ${crashOutcome.dep.status}: ${crashOutcome.dep.logs.slice(-8).join(" | ")}`);
  if (crashOutcome.project.status !== "live" || crashOutcome.project.imageTag !== liveBefore.project.imageTag) {
    throw new Error(`Previous release was not restored: ${JSON.stringify(crashOutcome.project)} | ${crashOutcome.dep.logs.slice(-8).join(" | ")}`);
  }
  await assertExposedVersion("0.1.0-proof-b");

  await requestJson(`${deployBase}/api/deployments/${firstDeploymentId}/rollback`, { method: "POST", headers: headers(), body: "{}" });
  await waitFor("rollback deployment", async () => {
    const result = await requestJson<{ deployments: Array<{ triggeredBy: string; status: string; imageTag?: string }> }>(`${deployBase}/api/projects/${projectId}`, { headers: headers() });
    const rollback = result.deployments.find((item) => item.triggeredBy === "rollback" && item.imageTag === [...imageTags][0]);
    if (rollback?.status === "live") return rollback;
    return null;
  }, (value) => value.status === "live");
  await assertExposedVersion("0.1.0-proof-a");
  console.log(`Calendar lifecycle passed: ${toolId} (${firstDeploymentId} -> ${secondDeploymentId} -> broken build kept b -> crashing release rejected, b restored -> rollback to a)`);
} finally {
  const deployBase = `http://127.0.0.1:${deployPort}`;
  const cloudBase = `http://127.0.0.1:${cloudPort}`;
  if (projectId && deployPort) await fetch(`${deployBase}/api/projects/${projectId}`, { method: "DELETE", headers: headers() }).catch(() => {});
  if (cloudPort) await fetch(`${cloudBase}/api/v1/tools/${encodeURIComponent(toolId)}`, { method: "DELETE", headers: cloudHeaders }).catch(() => {});
  await stopProcess(deployProcess);
  await stopProcess(cloudProcess);
  auth?.stop(true);
  if (appPort) Bun.spawnSync(["docker", "rm", "-f", `nexus-app-${toolId}`], { stdout: "ignore", stderr: "ignore" });
  for (const tag of imageTags) Bun.spawnSync(["docker", "image", "rm", "-f", tag], { stdout: "ignore", stderr: "ignore" });
  const taggedImages = Bun.spawnSync(["docker", "images", "-q", `nexus/${toolId}`], { stdout: "pipe", stderr: "ignore" }).stdout.toString().trim().split("\n").filter(Boolean);
  for (const image of taggedImages) Bun.spawnSync(["docker", "image", "rm", "-f", image], { stdout: "ignore", stderr: "ignore" });
  if (tempDir) await rm(tempDir, { recursive: true, force: true });
  if (stateDir) await rm(stateDir, { recursive: true, force: true });
}
