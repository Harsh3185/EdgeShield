const modes = new Set(["mixed", "hello", "cache", "load", "rate", "custom"]);

export function normalizeTrafficRequest(input = {}) {
  const parsedCount = Number(input.count ?? 1);
  const count = Math.min(Math.max(Number.isFinite(parsedCount) ? Math.trunc(parsedCount) : 1, 1), 100);
  const mode = modes.has(input.mode) ? input.mode : "mixed";
  const path = sanitizePath(input.path || "/hello");
  return { count, mode, path };
}

export function createTrafficTargets(input = {}, seed = Date.now()) {
  const request = normalizeTrafficRequest(input);
  const targets = [];

  for (let index = 0; index < request.count; index += 1) {
    if (request.mode === "hello") {
      targets.push(withDemoQuery("/hello", seed, index));
    } else if (request.mode === "cache") {
      targets.push("/cache-demo");
    } else if (request.mode === "load") {
      targets.push(withDemoQuery("/demo", seed, index));
    } else if (request.mode === "rate") {
      targets.push(withDemoQuery("/limit-demo", seed, index));
    } else if (request.mode === "custom") {
      targets.push(withDemoQuery(request.path, seed, index));
    } else {
      const pattern = index % 4;
      if (pattern === 0) {
        targets.push(withDemoQuery("/hello", seed, index));
      } else if (pattern === 1) {
        targets.push(withDemoQuery("/demo", seed, index));
      } else if (pattern === 2) {
        targets.push("/cache-demo");
      } else {
        targets.push(withDemoQuery("/limit-demo", seed, index));
      }
    }
  }

  return targets;
}

function sanitizePath(path) {
  const trimmed = String(path || "/hello").trim();
  if (!trimmed || /^https?:\/\//i.test(trimmed)) {
    return "/hello";
  }
  return trimmed.startsWith("/") ? trimmed : `/${trimmed}`;
}

function withDemoQuery(path, seed, index) {
  const separator = path.includes("?") ? "&" : "?";
  return `${path}${separator}source=demo-traffic&i=${seed}-${index}`;
}
