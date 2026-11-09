import { useEffect, useState } from "react";
import type { Task, TaskActivity } from "../types";
import { useApi } from "../runtime";

interface Props {
  task: Task;
}

export function TaskActivityLog({ task }: Props) {
  const api = useApi();
  const [activities, setActivities] = useState<TaskActivity[]>([]);
  const [comment, setComment] = useState("");
  const [submitting, setSubmitting] = useState(false);
  const [loading, setLoading] = useState(true);

  const loadActivities = () => {
    api
      .listTaskActivities(task.id)
      .then((acts) => {
        setActivities(acts);
        setLoading(false);
      })
      .catch(() => setLoading(false));
  };

  useEffect(() => {
    setLoading(true);
    loadActivities();
  }, [task.id]);

  const handlePostComment = async (e: React.FormEvent) => {
    e.preventDefault();
    const message = comment.trim();
    if (!message || submitting) return;

    setSubmitting(true);
    try {
      const created = await api.addComment(task.id, message);
      setActivities((prev) => [...prev, created]);
      setComment("");
    } catch (err) {
      console.error("Failed to post comment", err);
    } finally {
      setSubmitting(false);
    }
  };

  const formatTime = (iso: string) => {
    try {
      const d = new Date(iso);
      return d.toLocaleDateString(undefined, {
        month: "short",
        day: "numeric",
        hour: "2-digit",
        minute: "2-digit",
      });
    } catch {
      return iso;
    }
  };

  const getAuthorDisplay = (subject: string) => {
    if (subject.startsWith("did:key:")) return "did:…";
    return subject.replace(/^usr-/, "").replace(/^user_/, "");
  };

  const getKindBadge = (kind: TaskActivity["kind"]) => {
    switch (kind) {
      case "comment":
        return <span className="rounded bg-sky-950/80 px-1.5 py-0.5 text-[10px] font-medium text-sky-400">Comment</span>;
      case "status_change":
        return <span className="rounded bg-emerald-950/80 px-1.5 py-0.5 text-[10px] font-medium text-emerald-400">Status</span>;
      case "assignment":
        return <span className="rounded bg-purple-950/80 px-1.5 py-0.5 text-[10px] font-medium text-purple-400">Assignee</span>;
      case "update":
        return <span className="rounded bg-amber-950/80 px-1.5 py-0.5 text-[10px] font-medium text-amber-400">Update</span>;
      case "created":
        return <span className="rounded bg-zinc-800 px-1.5 py-0.5 text-[10px] font-medium text-zinc-400">Created</span>;
      default:
        return null;
    }
  };

  return (
    <div className="space-y-4 pt-4 border-t border-zinc-800">
      <div className="flex items-center justify-between">
        <h4 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">Activity & Comments</h4>
        <span className="text-xs text-zinc-500">{activities.length} entries</span>
      </div>

      {/* New Comment Box */}
      <form onSubmit={handlePostComment} className="space-y-2">
        <textarea
          value={comment}
          onChange={(e) => setComment(e.target.value)}
          placeholder="Leave a comment or update on this task…"
          rows={2}
          className="w-full resize-none rounded-lg border border-zinc-700 bg-zinc-900 px-3 py-2 text-xs text-zinc-100 placeholder-zinc-500 focus:border-accent focus:outline-none focus:ring-1 focus:ring-accent"
        />
        <div className="flex justify-end">
          <button
            type="submit"
            disabled={!comment.trim() || submitting}
            className="rounded bg-accent px-3 py-1.5 text-xs font-medium text-accent-foreground shadow transition hover:opacity-90 disabled:opacity-40"
          >
            {submitting ? "Posting…" : "Post Comment"}
          </button>
        </div>
      </form>

      {/* Activity Timeline Feed */}
      {loading ? (
        <div className="py-3 text-center text-xs text-zinc-500">Loading activity…</div>
      ) : activities.length === 0 ? (
        <div className="py-2 text-center text-xs text-zinc-500">No activity yet.</div>
      ) : (
        <div className="space-y-2.5 max-h-60 overflow-y-auto pr-1">
          {activities.map((act) => (
            <div key={act.id} className="flex gap-2.5 rounded-lg border border-zinc-800/80 bg-zinc-900/60 p-2.5 text-xs">
              <div className="flex h-6 w-6 shrink-0 items-center justify-center rounded-full bg-zinc-800 text-[10px] font-bold text-zinc-300">
                {getAuthorDisplay(act.authorSubject).slice(0, 2).toUpperCase()}
              </div>
              <div className="flex-1 space-y-1">
                <div className="flex items-center justify-between gap-2">
                  <div className="flex items-center gap-1.5">
                    <span className="font-semibold text-zinc-200">{getAuthorDisplay(act.authorSubject)}</span>
                    {getKindBadge(act.kind)}
                  </div>
                  <span className="text-[10px] text-zinc-500">{formatTime(act.createdAt)}</span>
                </div>
                <p className="text-zinc-300 leading-relaxed whitespace-pre-wrap">{act.message}</p>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
