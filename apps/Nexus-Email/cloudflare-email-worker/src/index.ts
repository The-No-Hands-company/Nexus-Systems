export interface Env {
  NEXUS_INGRESS_URL: string;
  NEXUS_INGRESS_TOKEN: string;
  ALLOWED_RECIPIENTS: string;
}

interface IncomingEmail {
  from: string;
  to: string;
  raw: ReadableStream<Uint8Array>;
  rawSize: number;
  setReject(reason: string): void;
}

const MAX_MESSAGE_BYTES = 25 * 1024 * 1024;

export default {
  async email(message: IncomingEmail, env: Env): Promise<void> {
    const allowed = new Set(
      env.ALLOWED_RECIPIENTS.split(",").map((address) => address.trim().toLowerCase()).filter(Boolean),
    );
    if (!allowed.has(message.to.toLowerCase())) {
      message.setReject("Recipient is not hosted here");
      return;
    }
    if (message.rawSize < 1 || message.rawSize > MAX_MESSAGE_BYTES) {
      message.setReject("Message exceeds the 25 MiB mailbox limit");
      return;
    }

    const response = await fetch(env.NEXUS_INGRESS_URL, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${env.NEXUS_INGRESS_TOKEN}`,
        "Content-Type": "application/octet-stream",
        "X-Nexus-Envelope-From": message.from,
        "X-Nexus-Envelope-To": message.to,
      },
      body: message.raw,
    });
    if (!response.ok) {
      // Fail closed: Cloudflare records the Worker failure instead of treating
      // this message as successfully stored in Nexus.
      throw new Error(`Nexus inbound endpoint returned HTTP ${response.status}`);
    }
  },
};
