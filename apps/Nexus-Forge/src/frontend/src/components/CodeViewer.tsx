export default function CodeViewer({ filePath, content }: { filePath: string; content: string }) {
  return (
    <div className="code-viewer">
      <h3>{filePath}</h3>
      <pre>{content || "No file content available."}</pre>
    </div>
  );
}
