export async function getUserIdByDid(did: string): Promise<string | null> {
  const base = process.env.DID_MAPPER_URL ?? "http://localhost:3000";
  const url = `${base}/user-id?did=${encodeURIComponent(did)}`;
  const res = await fetch(url);
  if (!res.ok) return null;
  const json = await res.json();
  return typeof json?.userId === "string" ? json.userId : null;
}

export async function getDidByUserId(userId: string): Promise<string | null> {
  const base = process.env.DID_MAPPER_URL ?? "http://localhost:3000";
  const url = `${base}/did?userId=${encodeURIComponent(userId)}`;
  const res = await fetch(url);
  if (!res.ok) return null;
  const json = await res.json();
  return typeof json?.did === "string" ? json.did : null;
}
