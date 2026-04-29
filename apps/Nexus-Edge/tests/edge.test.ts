import { describe, expect, test, beforeEach } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";
import {
  recordThreat,
  resolveThreat,
  getThreat,
  listThreats,
  recordAnomaly,
  confirmAnomaly,
  getAnomaly,
  listAnomalies,
  recordResponse,
  listResponses,
  getPolicy,
  getAIGuardrailProfile,
  clearThreats,
} from "../src/state";

describe("nexus-edge MVP contracts", () => {
  beforeEach(() => {
    clearThreats();
  });

  test("systems-api registration payload shape is stable", () => {
    const payload = buildSystemsApiRegistrationPayload("http://localhost:4340");
    expect(payload.id).toBe("nexus-edge");
    expect(payload.name).toBe("Nexus Edge");
    expect(payload.mode).toBe("orchestrated");
    expect(payload.capabilities).toContain("threat-detection");
    expect(payload.capabilities).toContain("edge-enforcement");
    expect(payload.capabilities).toContain("ai-guardrails");
    expect(payload.capabilities).toContain("anomaly-detection");
    expect(payload.metadata.supportsThreatDetection).toBe(true);
    expect(payload.metadata.supportsAnomalyDetection).toBe(true);
    expect(payload.metadata.supportsAIGuardrails).toBe(true);
    expect(payload.upstreamUrl).toBe("http://localhost:4340");
  });

  test("threat recording and resolution flow", () => {
    const threat = recordThreat("nexus-ai", "high", "Excessive token consumption detected");

    expect(threat.toolId).toBe("nexus-ai");
    expect(threat.severity).toBe("high");
    expect(threat.description).toBe("Excessive token consumption detected");
    expect(threat.threatId).toMatch(/^threat-/);
    expect(threat.detectedAt).toBeTruthy();
    expect(threat.resolvedAt).toBeUndefined();

    const resolved = resolveThreat(threat.threatId, "Rate limiting applied");
    expect(resolved?.resolvedAt).toBeTruthy();
    expect(resolved?.resolution).toBe("Rate limiting applied");
  });

  test("anomaly detection and confirmation", () => {
    const anomaly = recordAnomaly("nexus-auth", "behavioral", 0.92, "Unusual login pattern detected");

    expect(anomaly.toolId).toBe("nexus-auth");
    expect(anomaly.anomalyType).toBe("behavioral");
    expect(anomaly.confidence).toBe(0.92);
    expect(anomaly.confirmed).toBe(false);
    expect(anomaly.anomalyId).toMatch(/^anomaly-/);

    const confirmed = confirmAnomaly(anomaly.anomalyId);
    expect(confirmed?.confirmed).toBe(true);
  });

  test("response recording links threats to actions", () => {
    const threat = recordThreat("nexus-vault", "critical", "Unauthorized access attempt");
    const response = recordResponse(threat.threatId, "block", "edge-guardian", "Access denied to suspicious entity");

    expect(response.threatId).toBe(threat.threatId);
    expect(response.action).toBe("block");
    expect(response.executedBy).toBe("edge-guardian");
    expect(response.result).toBe("Access denied to suspicious entity");
    expect(response.responseId).toMatch(/^response-/);
  });

  test("listThreats and listAnomalies return all detected items", () => {
    recordThreat("svc-a", "low", "Minor issue");
    recordThreat("svc-b", "high", "Major issue");
    recordAnomaly("svc-a", "performance", 0.8, "Slow response");
    recordAnomaly("svc-c", "security", 0.85, "Suspicious behavior");

    const threats = listThreats();
    const anomalies = listAnomalies();

    expect(threats.length).toBe(2);
    expect(anomalies.length).toBe(2);
    expect(threats[0].severity).toBe("low");
    expect(threats[1].severity).toBe("high");
  });

  test("policy enforces auto-response and anomaly confidence thresholds", () => {
    const policy = getPolicy();
    expect(policy.enableThreatDetection).toBe(true);
    expect(policy.enableAnomalyDetection).toBe(true);
    expect(policy.autoRespondToThreats).toBe(true);
    expect(policy.anomalyConfidenceThreshold).toBe(0.75);
  });

  test("AI guardrail profile defines behavioral constraints", () => {
    const profile = getAIGuardrailProfile();
    expect(profile.maxConcurrentRequests).toBe(100);
    expect(profile.maxResponseLatencyMs).toBe(5000);
    expect(profile.behaviorCheckIntervalMs).toBe(60000);
    expect(profile.suspicionThreshold).toBe(0.8);
  });

  test("threat and anomaly queries return accurate data", () => {
    const threat = recordThreat("svc-x", "medium", "Resource limit warning");
    const anomaly = recordAnomaly("svc-y", "resource", 0.78);

    const retrievedThreat = getThreat(threat.threatId);
    const retrievedAnomaly = getAnomaly(anomaly.anomalyId);

    expect(retrievedThreat?.description).toBe("Resource limit warning");
    expect(retrievedAnomaly?.confidence).toBe(0.78);
  });

  test("confidence normalization clamps values to 0-1 range", () => {
    const normalAnomalyLow = recordAnomaly("svc", "behavioral", -0.5);
    const normalAnomalyHigh = recordAnomaly("svc", "behavioral", 1.5);

    expect(normalAnomalyLow.confidence).toBe(0);
    expect(normalAnomalyHigh.confidence).toBe(1);
  });
});
