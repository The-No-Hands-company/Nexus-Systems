import { describe, it, expect } from "vitest";
import { inferRegionFromRequest } from "../../src/lib/geoRouting";

describe("inferRegionFromRequest", () => {
  it("returns null when no geo headers present", () => {
    expect(inferRegionFromRequest({})).toBeNull();
  });

  it("uses Fly-Region header and maps to AWS region", () => {
    expect(inferRegionFromRequest({ "fly-region": "sin" })).toBe("ap-southeast-1");
    expect(inferRegionFromRequest({ "fly-region": "jkt" })).toBe("ap-southeast-3");
    expect(inferRegionFromRequest({ "fly-region": "iad" })).toBe("us-east-1");
    expect(inferRegionFromRequest({ "fly-region": "ams" })).toBe("eu-west-1");
    expect(inferRegionFromRequest({ "fly-region": "nrt" })).toBe("ap-northeast-1");
  });

  it("maps Cloudflare CF-IPCountry to region", () => {
    expect(inferRegionFromRequest({ "cf-ipcountry": "ID" })).toBe("ap-southeast-3"); // Indonesia
    expect(inferRegionFromRequest({ "cf-ipcountry": "SG" })).toBe("ap-southeast-1"); // Singapore
    expect(inferRegionFromRequest({ "cf-ipcountry": "DE" })).toBe("eu-central-1");   // Germany
    expect(inferRegionFromRequest({ "cf-ipcountry": "US" })).toBe("us-east-1");      // USA
    expect(inferRegionFromRequest({ "cf-ipcountry": "JP" })).toBe("ap-northeast-1"); // Japan
  });

  it("maps CloudFront-Viewer-Country to region", () => {
    expect(inferRegionFromRequest({ "cloudfront-viewer-country": "AU" })).toBe("ap-southeast-2");
    expect(inferRegionFromRequest({ "cloudfront-viewer-country": "BR" })).toBe("sa-east-1");
  });

  it("uses X-Geo-Region passthrough header verbatim", () => {
    expect(inferRegionFromRequest({ "x-geo-region": "eu-north-1" })).toBe("eu-north-1");
    expect(inferRegionFromRequest({ "x-geo-region": "custom-region" })).toBe("custom-region");
  });

  it("Fly-Region takes priority over CF-IPCountry", () => {
    const result = inferRegionFromRequest({
      "fly-region": "sin",      // → ap-southeast-1
      "cf-ipcountry": "DE",     // → eu-central-1
    });
    expect(result).toBe("ap-southeast-1");
  });

  it("handles lowercase fly-region values", () => {
    expect(inferRegionFromRequest({ "fly-region": "SYD" })).toBe("ap-southeast-2");
  });

  it("unknown fly-region returns unknown- prefix (not null)", () => {
    const result = inferRegionFromRequest({ "fly-region": "xyz" });
    expect(result).toBe("unknown-xyz");
    expect(result).not.toBeNull();
  });

  it("unknown country code falls back to us-east-1", () => {
    expect(inferRegionFromRequest({ "cf-ipcountry": "XX" })).toBe("us-east-1");
  });

  it("country codes are case-insensitive", () => {
    expect(inferRegionFromRequest({ "cf-ipcountry": "id" })).toBe("ap-southeast-3");
    expect(inferRegionFromRequest({ "cf-ipcountry": "ID" })).toBe("ap-southeast-3");
  });
});
