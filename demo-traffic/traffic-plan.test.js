import assert from "node:assert/strict";
import test from "node:test";
import { createTrafficTargets, normalizeTrafficRequest } from "./traffic-plan.js";

test("normalizes count and mode for demo traffic requests", () => {
  assert.deepEqual(normalizeTrafficRequest({ count: 999, mode: "unknown" }), {
    count: 100,
    mode: "mixed",
    path: "/hello"
  });
});

test("creates cache targets that reuse the same path", () => {
  assert.deepEqual(createTrafficTargets({ count: 3, mode: "cache" }, 1234), [
    "/cache-demo",
    "/cache-demo",
    "/cache-demo"
  ]);
});

test("creates unique custom targets and keeps the path safe", () => {
  assert.deepEqual(createTrafficTargets({ count: 2, mode: "custom", path: "custom-demo?x=1" }, 1234), [
    "/custom-demo?x=1&source=demo-traffic&i=1234-0",
    "/custom-demo?x=1&source=demo-traffic&i=1234-1"
  ]);
});
