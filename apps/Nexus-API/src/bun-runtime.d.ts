/** Minimal Bun server surface used by the stable gateway.
 *
 * The legacy API package historically pulled the entire bun-types declaration
 * tree into its retired Express application. Keeping the gateway's runtime
 * contract local makes a clean API install deterministic and avoids coupling
 * its release check to unrelated legacy dependencies.
 */
declare namespace Bun {
  interface Server {
    readonly port: number;
    stop(closeActiveConnections?: boolean): void;
  }

  function serve(options: {
    hostname?: string;
    port: number;
    fetch(request: Request): Response | Promise<Response>;
  }): Server;
}

declare module "bun:test" {
  type Expectation = { [name: string]: (...args: unknown[]) => unknown };
  export const afterAll: (fn: () => void | Promise<void>) => void;
  export const beforeAll: (fn: () => void | Promise<void>) => void;
  export const describe: (name: string, fn: () => void) => void;
  export const it: (name: string, fn: () => void | Promise<void>) => void;
  export const expect: ((value: unknown) => Expectation) & {
    arrayContaining(value: unknown): unknown;
    objectContaining(value: unknown): unknown;
  };
}
