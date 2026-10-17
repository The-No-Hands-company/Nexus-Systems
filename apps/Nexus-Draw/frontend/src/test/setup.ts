import { vi } from "vitest";

// jsdom does not implement canvas contexts. Returning null matches the
// production component's no-context guard and keeps tests free of noisy
// "getContext is not implemented" console errors.
vi.spyOn(HTMLCanvasElement.prototype, "getContext").mockReturnValue(null);
