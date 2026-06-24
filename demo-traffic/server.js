import { createReadStream } from "node:fs";
import { readFile } from "node:fs/promises";
import { createServer } from "node:http";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";
import { createTrafficTargets } from "./traffic-plan.js";

const root = fileURLToPath(new URL(".", import.meta.url));
const publicDir = join(root, "public");
const port = Number(process.env.PORT || 4010);
const proxyBase = process.env.EDGESHIELD_PROXY_URL || "http://127.0.0.1:8080";

const contentTypes = {
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8"
};

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url || "/", `http://${request.headers.host || "localhost"}`);

    if (request.method === "GET" && url.pathname === "/health") {
      return sendJson(response, 200, { ok: true, proxyBase });
    }

    if (request.method === "POST" && url.pathname === "/api/traffic") {
      const body = await readJson(request);
      const started = Date.now();
      const targets = createTrafficTargets(body, started);
      const results = await Promise.allSettled(targets.map(sendProxyRequest));
      const statuses = results.map((result) => result.status === "fulfilled" ? result.value : 0);
      return sendJson(response, 200, {
        ok: true,
        proxyBase,
        sent: targets.length,
        statuses,
        targets,
        elapsed_ms: Date.now() - started
      });
    }

    if (request.method === "GET") {
      return serveStatic(url.pathname, response);
    }

    return sendJson(response, 405, { ok: false, message: "Method not allowed" });
  } catch (error) {
    return sendJson(response, 500, { ok: false, message: error.message });
  }
});

server.listen(port, "0.0.0.0", () => {
  console.log(`EdgeShield demo traffic interface listening on ${port}, proxy=${proxyBase}`);
});

async function sendProxyRequest(target) {
  const response = await fetch(`${proxyBase}${target}`, {
    headers: { "User-Agent": "EdgeShield-demo-traffic" }
  });
  await response.arrayBuffer();
  return response.status;
}

async function readJson(request) {
  const chunks = [];
  for await (const chunk of request) {
    chunks.push(chunk);
  }
  if (chunks.length === 0) {
    return {};
  }
  return JSON.parse(Buffer.concat(chunks).toString("utf8"));
}

async function serveStatic(pathname, response) {
  const relativePath = pathname === "/" ? "index.html" : pathname.slice(1);
  const safePath = normalize(relativePath).replace(/^(\.\.[/\\])+/, "");
  const fullPath = join(publicDir, safePath);

  if (!fullPath.startsWith(publicDir)) {
    return sendJson(response, 404, { ok: false, message: "Not found" });
  }

  try {
    await readFile(fullPath);
    response.writeHead(200, { "content-type": contentTypes[extname(fullPath)] || "application/octet-stream" });
    createReadStream(fullPath).pipe(response);
  } catch {
    sendJson(response, 404, { ok: false, message: "Not found" });
  }
}

function sendJson(response, status, body) {
  response.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  response.end(JSON.stringify(body));
}
