/**
 * Repository names are the one piece of client input that becomes a
 * filesystem path and an argument to git, so they are checked against an
 * allowlist rather than a denylist. Path traversal (`..`, `/`), option
 * injection (a leading `-`) and hidden directories (a leading `.`) are all
 * outside the allowlist, not special cases of it.
 */
const REPO_NAME = /^[a-z0-9][a-z0-9_-]{0,63}$/;

export function isValidRepoName(name: unknown): name is string {
  return typeof name === "string" && REPO_NAME.test(name);
}

/** The bare-repository directory name for a repository. */
export function repoDirName(name: string): string {
  if (!isValidRepoName(name)) throw new Error(`invalid repository name: ${JSON.stringify(name)}`);
  return `${name}.git`;
}
