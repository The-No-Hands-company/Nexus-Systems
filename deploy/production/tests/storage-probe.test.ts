import { describe, expect, test } from "bun:test";
import {
  runStorageProbe,
  type StorageProbeOperations,
} from "../storage-probe";

const probeEnvironment = {
  NEXUS_STORAGE_S3_ENDPOINT: "http://127.0.0.1:9000",
  NEXUS_STORAGE_S3_ACCESS_KEY: "TEST_ACCESS_SENTINEL",
  NEXUS_STORAGE_S3_SECRET_KEY: "TEST_SECRET_SENTINEL", // pragma: allowlist secret
  NEXUS_STORAGE_S3_REGION: "us-east-1",
  NEXUS_STORAGE_S3_BUCKET_PREFIX: "nexus",
  NEXUS_ROTATION_S3_PROBE_BUCKET: "nexus-recovery",
};

describe("authenticated storage recovery probe", () => {
  test("writes, verifies, and deletes one disposable object", async () => {
    const calls: string[] = [];
    let stored = "";
    const operations: StorageProbeOperations = {
      async putObject(_config, bucket, _key, body) {
        calls.push(`put:${bucket}`);
        stored = String(body);
        return new Response(null, { status: 200 });
      },
      async getObject(_config, bucket) {
        calls.push(`get:${bucket}`);
        return new Response(stored, { status: 200 });
      },
      async deleteObject(_config, bucket) {
        calls.push(`delete:${bucket}`);
        return new Response(null, { status: 204 });
      },
    };

    await runStorageProbe(probeEnvironment, operations);

    expect(calls).toEqual([
      "put:nexus-recovery",
      "get:nexus-recovery",
      "delete:nexus-recovery",
    ]);
  });

  test("attempts cleanup when read-back verification fails", async () => {
    const calls: string[] = [];
    const operations: StorageProbeOperations = {
      async putObject() {
        calls.push("put");
        return new Response(null, { status: 200 });
      },
      async getObject() {
        calls.push("get");
        return new Response("wrong payload", { status: 200 });
      },
      async deleteObject() {
        calls.push("delete");
        return new Response(null, { status: 204 });
      },
    };

    await expect(runStorageProbe(probeEnvironment, operations)).rejects.toThrow(
      "authenticated storage probe failed",
    );
    expect(calls).toEqual(["put", "get", "delete"]);
  });

  test("rejects a bucket outside the configured prefix before making requests", async () => {
    const calls: string[] = [];
    const operations: StorageProbeOperations = {
      async putObject() {
        calls.push("put");
        return new Response(null, { status: 200 });
      },
      async getObject() {
        calls.push("get");
        return new Response(null, { status: 200 });
      },
      async deleteObject() {
        calls.push("delete");
        return new Response(null, { status: 204 });
      },
    };

    await expect(
      runStorageProbe(
        { ...probeEnvironment, NEXUS_ROTATION_S3_PROBE_BUCKET: "foreign-bucket" },
        operations,
      ),
    ).rejects.toThrow("configured bucket prefix");
    expect(calls).toEqual([]);
  });

  test("uses a fresh bounded cleanup deadline when a PUT times out", async () => {
    const calls: string[] = [];
    let putSignal: AbortSignal | undefined;
    let cleanupSignal: AbortSignal | undefined;
    let cleanupStartedAborted: boolean | undefined;
    const operations: StorageProbeOperations = {
      async putObject(_config, _bucket, _key, _body, _contentType, signal) {
        calls.push("put");
        putSignal = signal;
        return new Promise<Response>(() => {});
      },
      async getObject() {
        return new Response(null, { status: 200 });
      },
      async deleteObject(_config, _bucket, _key, signal) {
        calls.push("delete");
        cleanupSignal = signal;
        cleanupStartedAborted = signal?.aborted;
        if (signal?.aborted) throw new Error("cleanup received an already-aborted signal");
        return new Promise<Response>((_resolve, reject) => {
          signal?.addEventListener(
            "abort",
            () => reject(new Error("cleanup deadline exceeded")),
            { once: true },
          );
        });
      },
    };
    const startedAt = performance.now();

    await expect(runStorageProbe(probeEnvironment, operations, 20)).rejects.toThrow(
      "authenticated storage probe failed",
    );

    expect(performance.now() - startedAt).toBeLessThan(500);
    expect(calls).toEqual(["put", "delete"]);
    expect(cleanupStartedAborted).toBe(false);
    expect(cleanupSignal).not.toBe(putSignal);
    expect(putSignal?.aborted).toBe(true);
    expect(cleanupSignal?.aborted).toBe(true);
  });
});
