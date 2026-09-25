import { describe, expect, it } from "bun:test";
import { HttpError } from "../src/http";
import {
  dateValue,
  enumValue,
  intValue,
  nullable,
  object,
  requiredText,
  subjectValue,
} from "../src/validation";

function rejects(fn: () => unknown, message: string): void {
  try {
    fn();
  } catch (error) {
    expect(error).toBeInstanceOf(HttpError);
    expect((error as HttpError).status).toBe(400);
    expect((error as HttpError).message).toContain(message);
    return;
  }
  throw new Error("expected a 400");
}

describe("validation", () => {
  it("accepts only JSON objects with known fields", () => {
    expect(object({ name: "x" }, ["name"])).toEqual({ name: "x" });
    rejects(() => object([], ["name"]), "JSON object");
    rejects(() => object(null, ["name"]), "JSON object");
    rejects(() => object({ name: "x", owner: "me" }, ["name"]), "unknown field: owner");
  });

  it("trims required text and refuses blank or oversized values", () => {
    expect(requiredText({ name: "  Launch  " }, "name", 200)).toBe("Launch");
    rejects(() => requiredText({ name: "   " }, "name", 200), "name is required");
    rejects(() => requiredText({}, "name", 200), "name is required");
    rejects(() => requiredText({ name: "x".repeat(201) }, "name", 200), "at most 200");
  });

  it("accepts real dates between 2000 and 2199 only", () => {
    expect(dateValue("due")("2026-09-24")).toBe("2026-09-24");
    rejects(() => dateValue("due")("2026-02-30"), "real YYYY-MM-DD");
    rejects(() => dateValue("due")("1999-12-31"), "between 2000 and 2199");
    rejects(() => dateValue("due")(20260924), "YYYY-MM-DD");
  });

  it("distinguishes absent, null and present for nullable fields", () => {
    const parse = dateValue("deadline");
    expect(nullable({}, "deadline", parse)).toBeUndefined();
    expect(nullable({ deadline: null }, "deadline", parse)).toBeNull();
    expect(nullable({ deadline: "2026-10-01" }, "deadline", parse)).toBe("2026-10-01");
  });

  it("checks integers, enums and subjects", () => {
    expect(intValue("lag", -10, 10)(-3)).toBe(-3);
    rejects(() => intValue("lag", -10, 10)(1.5), "integer");
    rejects(() => intValue("lag", -10, 10)(11), "from -10 to 10");
    expect(enumValue("type", ["FS", "SS"] as const)("SS")).toBe("SS");
    rejects(() => enumValue("type", ["FS", "SS"] as const)("XX"), "one of FS, SS");
    expect(subjectValue("subject")("usr-2f9a-1")).toBe("usr-2f9a-1");
    rejects(() => subjectValue("subject")("usr alice"), "Nexus subject");
  });
});
