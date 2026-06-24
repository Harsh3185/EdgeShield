import express from "express";
import http from "http";
import { WebSocketServer } from "ws";

const app = express();
app.use(express.json({ limit: "1mb" }));
const server = http.createServer(app);
const wss = new WebSocketServer({ noServer: true });

const serviceName = process.env.SERVICE_NAME || "service-a";
const port = Number(process.env.PORT || 3001);
const delayMs = Number(process.env.DELAY_MS || 0);

app.get("/health", (_req, res) => {
  res.json({ ok: true, service: serviceName });
});

app.all("*", async (req, res) => {
  if (delayMs > 0) {
    await new Promise((resolve) => setTimeout(resolve, delayMs));
  }

  res.set("X-Demo-Service", serviceName);
  res.json({
    service: serviceName,
    method: req.method,
    path: req.originalUrl,
    requestId: req.header("x-request-id") || null,
    forwardedFor: req.header("x-forwarded-for") || null,
    edgeProxy: req.header("x-edgeshield-proxy") || null,
    timestamp: new Date().toISOString(),
    headers: req.headers
  });
});

wss.on("connection", (socket, req) => {
  socket.on("message", (data, isBinary) => {
    if (isBinary) {
      socket.send(data, { binary: true });
      return;
    }

    socket.send(JSON.stringify({
      service: serviceName,
      type: "websocket",
      path: req.url,
      message: data.toString(),
      requestId: req.headers["x-request-id"] || null,
      forwardedFor: req.headers["x-forwarded-for"] || null,
      edgeProxy: req.headers["x-edgeshield-proxy"] || null,
      timestamp: new Date().toISOString()
    }));
  });
});

server.on("upgrade", (req, socket, head) => {
  if (!req.url.startsWith("/ws")) {
    socket.destroy();
    return;
  }

  wss.handleUpgrade(req, socket, head, (ws) => {
    wss.emit("connection", ws, req);
  });
});

server.listen(port, "0.0.0.0", () => {
  console.log(`${serviceName} listening on ${port}`);
});
