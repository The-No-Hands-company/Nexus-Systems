interface DiffViewProps {
  oldRef: string;
  newRef: string;
  diffContent: string;
}

export default function DiffView({ oldRef, newRef, diffContent }: DiffViewProps) {
  return (
    <div className="diff-view">
      <h3>Diff: {oldRef} → {newRef}</h3>
      <pre>{diffContent || "Diff output is not yet available."}</pre>
    </div>
  );
}
