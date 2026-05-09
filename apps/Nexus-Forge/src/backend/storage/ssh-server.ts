import { EventEmitter } from "node:events";

export class SSHServer {
  private running = false;
  private emitter = new EventEmitter();

  async start(port = 2222) {
    this.running = true;
    this.emitter.emit("started", { port });
    return { ok: true, port, message: "SSH server stub started" };
  }

  async stop() {
    this.running = false;
    this.emitter.emit("stopped");
    return { ok: true, message: "SSH server stub stopped" };
  }

  isRunning() {
    return this.running;
  }
}
