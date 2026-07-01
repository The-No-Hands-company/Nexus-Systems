import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";

export async function ensureDir(dirPath: string): Promise<void> {
  await mkdir(dirPath, { recursive: true });
}

export async function writeTextFile(filePath: string, content: string): Promise<void> {
  await ensureDir(path.dirname(filePath));
  await writeFile(filePath, content, "utf8");
}
