import {
  deleteObject,
  getObject,
  putObject,
  type S3Config,
} from "../../apps/Nexus-Cloud/src/storage/s3";

type ProbeEnvironment = Record<string, string | undefined>;

export type StorageProbeOperations = {
  putObject: typeof putObject;
  getObject: typeof getObject;
  deleteObject: typeof deleteObject;
};

const liveOperations: StorageProbeOperations = {
  putObject,
  getObject,
  deleteObject,
};

function probeTimeoutMilliseconds(environment: ProbeEnvironment): number {
  const raw = environment.NEXUS_ROTATION_S3_PROBE_TIMEOUT_SECONDS?.trim() || "20";
  const seconds = Number(raw);
  if (!Number.isInteger(seconds) || seconds < 1 || seconds > 120) {
    throw new Error("NEXUS_ROTATION_S3_PROBE_TIMEOUT_SECONDS must be an integer from 1 to 120");
  }
  return seconds * 1_000;
}

function required(environment: ProbeEnvironment, name: string): string {
  const value = environment[name]?.trim();
  if (!value) throw new Error(`${name} is required for the authenticated storage probe`);
  return value;
}

function probeConfiguration(environment: ProbeEnvironment): {
  config: S3Config;
  bucket: string;
} {
  const prefix = (environment.NEXUS_STORAGE_S3_BUCKET_PREFIX?.trim() || "nexus").toLowerCase();
  const bucket = required(environment, "NEXUS_ROTATION_S3_PROBE_BUCKET").toLowerCase();
  if (!bucket.startsWith(`${prefix}-`)) {
    throw new Error("NEXUS_ROTATION_S3_PROBE_BUCKET must use the configured bucket prefix");
  }
  if (!/^[a-z0-9][a-z0-9.-]{1,61}[a-z0-9]$/.test(bucket)) {
    throw new Error("NEXUS_ROTATION_S3_PROBE_BUCKET is not a valid S3 bucket name");
  }

  return {
    bucket,
    config: {
      endpoint: required(environment, "NEXUS_STORAGE_S3_ENDPOINT"),
      accessKey: required(environment, "NEXUS_STORAGE_S3_ACCESS_KEY"),
      secretKey: required(environment, "NEXUS_STORAGE_S3_SECRET_KEY"),
      region: environment.NEXUS_STORAGE_S3_REGION?.trim() || "us-east-1",
    },
  };
}

export async function runStorageProbe(
  environment: ProbeEnvironment = process.env,
  operations: StorageProbeOperations = liveOperations,
  timeoutMilliseconds: number = probeTimeoutMilliseconds(environment),
): Promise<void> {
  const { bucket, config } = probeConfiguration(environment);
  const key = `recovery-probe-${crypto.randomUUID()}`;
  const payload = `nexus-recovery-${crypto.randomUUID()}`;
  const abortController = new AbortController();
  let timeoutHandle: ReturnType<typeof setTimeout> | undefined;
  const deadline = new Promise<never>((_, reject) => {
    timeoutHandle = setTimeout(() => {
      abortController.abort();
      reject(new Error("storage probe deadline exceeded"));
    }, timeoutMilliseconds);
  });
  const bounded = <T>(operation: Promise<T>): Promise<T> => Promise.race([operation, deadline]);
  let cleanupNeeded = true;
  let objectDeleted = false;

  try {
    const putResponse = await bounded(
      operations.putObject(
        config,
        bucket,
        key,
        payload,
        "text/plain",
        abortController.signal,
      ),
    );
    if (!putResponse.ok) throw new Error("put failed");

    const getResponse = await bounded(
      operations.getObject(config, bucket, key, abortController.signal),
    );
    if (!getResponse.ok || (await getResponse.text()) !== payload) {
      throw new Error("read-back failed");
    }

    const deleteResponse = await bounded(
      operations.deleteObject(config, bucket, key, abortController.signal),
    );
    if (!deleteResponse.ok) throw new Error("delete failed");
    objectDeleted = true;
  } catch {
    if (cleanupNeeded && !objectDeleted) {
      try {
        await bounded(operations.deleteObject(config, bucket, key, abortController.signal));
      } catch {
        // The caller receives one generic failure and performs paired rollback.
      }
    }
    throw new Error("authenticated storage probe failed");
  } finally {
    cleanupNeeded = false;
    if (timeoutHandle !== undefined) clearTimeout(timeoutHandle);
  }
}

if (import.meta.main) {
  try {
    await runStorageProbe();
    console.log("authenticated storage probe passed");
  } catch (error) {
    const message = error instanceof Error ? error.message : "authenticated storage probe failed";
    console.error(message);
    process.exitCode = 1;
  }
}
