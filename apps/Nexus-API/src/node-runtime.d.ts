declare const process: {
  env: Record<string, string | undefined>;
  once(signal: string, listener: () => void): void;
  exit(code?: number): never;
};
