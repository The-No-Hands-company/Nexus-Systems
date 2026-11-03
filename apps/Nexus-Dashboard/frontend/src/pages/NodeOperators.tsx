export default function NodeOperators() {
  return (
    <section className="mx-auto max-w-3xl space-y-8 px-4 py-6 sm:p-8">
      <div>
        <h1 className="text-2xl font-semibold">Node Operators</h1>
        <p className="mt-2 text-zinc-400">
          Run a Nexus node on your own hardware and join the federated network.
          You keep full control — your machine, your keys, your data.
        </p>
      </div>

      <div className="flex flex-wrap gap-3">
        <a
          href="/cloud/federation"
          className="rounded-lg bg-cyan-500 px-4 py-2 text-sm font-medium text-black hover:bg-cyan-400"
        >
          Enroll a node
        </a>
        <a
          href="https://api.tnhc.dev/node-operators"
          target="_blank"
          rel="noreferrer"
          className="rounded-lg border border-zinc-700 px-4 py-2 text-sm font-medium text-zinc-300 hover:bg-zinc-800"
        >
          Public docs ↗
        </a>
      </div>

      <div className="rounded-lg border border-zinc-800 bg-zinc-900 divide-y divide-zinc-800">
        <div className="p-5">
          <h2 className="text-base font-semibold">What a node does</h2>
          <ul className="mt-3 space-y-1.5 text-sm text-zinc-400 list-disc list-inside">
            <li>Hosts Nexus applications for users on that node</li>
            <li>Participates in federated routing — sites and mail can replicate across peers</li>
            <li>Holds an Ed25519 cryptographic identity that peers use to verify its messages</li>
            <li>Appears in the public node directory</li>
          </ul>
        </div>

        <div className="p-5">
          <h2 className="text-base font-semibold">What you need</h2>
          <div className="mt-3 overflow-x-auto">
            <table className="w-full text-sm text-left">
              <thead>
                <tr className="text-xs uppercase tracking-wide text-zinc-500">
                  <th className="pb-2 pr-4 font-medium">Requirement</th>
                  <th className="pb-2 pr-4 font-medium">Minimum</th>
                  <th className="pb-2 font-medium">Notes</th>
                </tr>
              </thead>
              <tbody className="text-zinc-300">
                {[
                  ["Machine", "Raspberry Pi 4 / small VPS", "Must stay online to serve traffic"],
                  ["Domain", "Any resolvable hostname", "e.g. node.example.com — used as the node's identity"],
                  ["Storage", "1 GB", "You declare the amount at enrollment"],
                  ["Bandwidth", "1 GB / month", "You declare the amount at enrollment"],
                  ["Network", "Outbound port 443", "No public IP needed — Cloudflare Tunnel works"],
                  ["OS", "Linux", "Install script handles the rest"],
                ].map(([req, min, notes]) => (
                  <tr key={req} className="border-t border-zinc-800">
                    <td className="py-2 pr-4 font-medium">{req}</td>
                    <td className="py-2 pr-4">{min}</td>
                    <td className="py-2 text-zinc-500">{notes}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        <div className="p-5">
          <h2 className="text-base font-semibold">Enrollment — two steps</h2>
          <p className="mt-2 text-sm text-zinc-500">
            The split is intentional: this server never sees your private key — it is
            generated on your machine and stays there.
          </p>
          <ol className="mt-4 space-y-4">
            <li className="flex gap-3">
              <span className="flex-shrink-0 mt-0.5 flex h-6 w-6 items-center justify-center rounded-full bg-zinc-800 text-xs font-bold text-cyan-400">1</span>
              <div className="text-sm text-zinc-300">
                <span className="font-medium">Register</span> — go to{" "}
                <a href="/cloud/federation" className="text-cyan-400 hover:underline">Cloud → Federation</a>{" "}
                and click <span className="font-medium">Enroll a node</span>. Fill in your node's
                name, domain, region, and declared capacities.
              </div>
            </li>
            <li className="flex gap-3">
              <span className="flex-shrink-0 mt-0.5 flex h-6 w-6 items-center justify-center rounded-full bg-zinc-800 text-xs font-bold text-cyan-400">2</span>
              <div className="text-sm text-zinc-300">
                <span className="font-medium">Copy the token</span> — you receive a single-use
                enrollment token valid for 24 hours and a ready-to-run install command.
                The token is shown once and not stored.
              </div>
            </li>
            <li className="flex gap-3">
              <span className="flex-shrink-0 mt-0.5 flex h-6 w-6 items-center justify-center rounded-full bg-zinc-800 text-xs font-bold text-cyan-400">3</span>
              <div className="text-sm text-zinc-300 space-y-2">
                <p>
                  <span className="font-medium">Run the install command</span> on the machine
                  that will be the node:
                </p>
                <pre className="rounded bg-zinc-950 border border-zinc-800 px-4 py-3 text-xs text-zinc-300 overflow-x-auto whitespace-pre-wrap break-all">
{`curl -fsSL https://hosting.tnhc.dev/install-node.sh -o install-node.sh \\
  && sh install-node.sh --token <your-token> --api https://hosting.tnhc.dev`}
                </pre>
                <p className="text-zinc-500">
                  The script generates an Ed25519 keypair locally, signs the token, and
                  registers the public key. Your node moves from{" "}
                  <code className="rounded bg-zinc-800 px-1 text-zinc-300">pending</code> →{" "}
                  <code className="rounded bg-zinc-800 px-1 text-zinc-300">active</code>.
                </p>
              </div>
            </li>
          </ol>
        </div>

        <div className="p-5">
          <h2 className="text-base font-semibold">API reference</h2>
          <div className="mt-3 overflow-x-auto">
            <table className="w-full text-sm text-left">
              <thead>
                <tr className="text-xs uppercase tracking-wide text-zinc-500">
                  <th className="pb-2 pr-4 font-medium">Endpoint</th>
                  <th className="pb-2 pr-4 font-medium">Auth</th>
                  <th className="pb-2 font-medium">Description</th>
                </tr>
              </thead>
              <tbody className="text-zinc-300">
                {[
                  ["GET /nodes", "None", "Public federation directory"],
                  ["POST /nodes/enroll", "Session", "Start enrollment; returns single-use token"],
                  ["POST /nodes/claim", "Token", "Finish enrollment with public key + signature"],
                  ["POST /nodes/:id/enrollment/revoke", "Session", "Revoke an unclaimed token"],
                ].map(([ep, auth, desc]) => (
                  <tr key={ep} className="border-t border-zinc-800">
                    <td className="py-2 pr-4 font-mono text-xs text-cyan-400">{ep}</td>
                    <td className="py-2 pr-4 text-zinc-500">{auth}</td>
                    <td className="py-2 text-zinc-400">{desc}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
          <p className="mt-3 text-xs text-zinc-600">
            Base URL: <code className="text-zinc-500">https://hosting.tnhc.dev</code>
          </p>
        </div>
      </div>

      <p className="text-sm text-zinc-600">
        Questions? Open an issue on{" "}
        <a
          href="https://github.com/The-No-Hands-Company/Nexus-Systems"
          target="_blank"
          rel="noreferrer"
          className="text-zinc-400 hover:text-zinc-200"
        >
          GitHub
        </a>
        . Running even a temporary node on a small VPS helps stress-test the
        federation protocol.
      </p>
    </section>
  );
}
