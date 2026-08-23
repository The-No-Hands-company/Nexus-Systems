declare namespace Bun {
  interface ServeOptions<S = undefined> {
    port: number;
    hostname?: string;
    fetch: (req: Request, server: Server) => Response | Promise<Response>;
    websocket?: {
      open?: (ws: WebSocket<S>) => void;
      message?: (ws: WebSocket<S>, message: string | Buffer) => void;
      close?: (ws: WebSocket<S>, code: number, reason: string) => void;
    };
  }

  interface Server {
    port: number;
    stop: (force?: boolean) => Promise<void>;
    upgrade: (req: Request, options: { data: S }) => boolean;
  }

  function serve<S>(options: ServeOptions<S>): Server;

  interface WebSocket<S = undefined> {
    data: S;
    send: (data: string | Buffer) => void;
    close: (code?: number, reason?: string) => void;
  }
}
