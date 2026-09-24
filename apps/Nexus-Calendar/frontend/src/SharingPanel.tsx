import { useEffect, useState } from "react";
import type { CalendarApi, EventPermission, EventShare } from "./calendar-api";
import type { CalendarRuntime } from "./runtime";

export default function SharingPanel({ eventId, api, runtime }: { eventId: string; api: CalendarApi; runtime: CalendarRuntime }) {
  const [shares, setShares] = useState<EventShare[]>([]); const [subject, setSubject] = useState(""); const [permission, setPermission] = useState<EventPermission>("editor");
  const [token, setToken] = useState<string | null>(null); const [error, setError] = useState<string | null>(null);
  useEffect(() => { void api.listShares(eventId).then(setShares).catch(() => setError("Could not load sharing settings.")); }, [api, eventId]);
  async function grant() { if (!subject.trim()) return; try { const share = await api.grantShare(eventId, subject.trim(), permission); setShares((old) => [...old.filter((item) => item.subject !== share.subject), share]); setSubject(""); } catch { setError("Could not update sharing settings."); } }
  async function remove(subjectToRemove: string) { try { await api.revokeShare(eventId, subjectToRemove); setShares((old) => old.filter((share) => share.subject !== subjectToRemove)); } catch { setError("Could not update sharing settings."); } }
  async function createLink() { try { setToken((await api.createPublicLink(eventId)).token); } catch { setError("Could not create a public link."); } }
  const publicHref = token ? new URL(`/share/${token}`, runtime.publicOrigin ?? window.location.origin).toString() : "";
  return <section aria-label="Sharing settings" className="space-y-3 rounded border border-zinc-800 bg-zinc-950 p-3">
    <h3 className="font-medium">Sharing</h3>{error && <p role="alert" className="text-sm text-red-300">{error}</p>}
    <div className="flex flex-wrap gap-2"><label className="sr-only" htmlFor="share-subject">Person to share with</label><input id="share-subject" aria-label="Person to share with" value={subject} onChange={(e) => setSubject(e.target.value)} placeholder="usr-person" className="min-w-40 flex-1 rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5 text-sm" /><select aria-label="Permission" value={permission} onChange={(e) => setPermission(e.target.value as EventPermission)} className="rounded border border-zinc-700 bg-zinc-900 px-2 py-1.5 text-sm"><option value="editor">Editor</option><option value="viewer">Viewer</option></select><button type="button" onClick={() => void grant()} className="rounded bg-white/10 px-3 py-1.5 text-sm">Grant {permission} access</button></div>
    <ul className="space-y-1">{shares.map((share) => <li key={share.subject} className="flex items-center gap-2 text-sm"><span>{share.subject} · {share.permission}</span><button type="button" onClick={() => void remove(share.subject)} className="text-zinc-400 underline">Remove</button></li>)}</ul>
    {!token ? <button type="button" onClick={() => void createLink()} className="rounded border border-zinc-700 px-3 py-1.5 text-sm">Create public link</button> : <div className="flex flex-wrap gap-2"><output className="max-w-full truncate text-sm text-zinc-300">{publicHref}</output><button type="button" onClick={() => void navigator.clipboard.writeText(publicHref)} className="rounded border border-zinc-700 px-3 py-1.5 text-sm">Copy public link</button><button type="button" onClick={() => void api.revokePublicLink(eventId).then(() => setToken(null)).catch(() => setError("Could not revoke the public link."))} className="rounded px-3 py-1.5 text-sm text-red-300">Revoke public link</button></div>}
  </section>;
}
