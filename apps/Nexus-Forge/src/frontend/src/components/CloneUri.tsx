export default function CloneUri({ sshUrl, httpsUrl }: { sshUrl: string; httpsUrl: string }) {
  return (
    <div className="clone-uri">
      <h3>Clone</h3>
      <p>
        SSH: <code>{sshUrl}</code>
      </p>
      <p>
        HTTPS: <code>{httpsUrl}</code>
      </p>
    </div>
  );
}
