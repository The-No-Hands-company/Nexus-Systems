/** Page-level loading placeholder. Use when the whole page is pending. */
export default function PageSkeleton({ maxWidth = "max-w-4xl" }: { maxWidth?: string }) {
  return (
    <section
      className={`mx-auto ${maxWidth} px-4 py-6 sm:p-8`}
      role="status"
      aria-busy="true"
    >
      <span className="sr-only">Loading</span>
      <div aria-hidden className="space-y-4">
        <div className="h-8 w-48 animate-pulse rounded bg-zinc-700" />
        <div className="h-4 w-72 animate-pulse rounded bg-zinc-700/60" />
        <div className="mt-6 space-y-3">
          {Array.from({ length: 4 }, (_, i) => (
            <div key={i} className="h-12 animate-pulse rounded-lg bg-zinc-800/70" style={{ width: `${100 - i * 5}%` }} />
          ))}
        </div>
      </div>
    </section>
  );
}
