#!/usr/bin/env node
// Reproducible Fullstack Arena-SSR Benchmark: YAZA (-O2) vs Node.js HTTP SSR (127.0.0.1 keep-alive)
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawn, execSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const CSS_PATH = path.join(ROOT, "templates/fullstack/public/yaza-ui.css");
const CSS_BUF = fs.readFileSync(CSS_PATH);
const YAZA_BIN = path.join(ROOT, "build/fullstack-ssr-bench");
const CONCURRENCY = 8;
const REQS_PER_ROUND = 2000;
const ROUNDS = 3;

function escapeHtml(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;");
}

function startNodeSsrServer(port) {
  const etag = `"node-${CSS_BUF.length}"`;
  const csp = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
  const csrf = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";

  function buildTasks() {
    const items = [];
    for (let i = 1; i <= 16; i++) {
      items.push({
        id: `tsk-${i}`,
        title: `Arena-SSR Task #${i} <verified & safe>`,
        owner: "admin",
        priority: i % 3 === 0 ? "high" : "normal",
        status: i % 4 === 0 ? "done" : "open",
      });
    }
    return items;
  }

  function renderFragment(items) {
    let out = `<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>`;
    for (const it of items) {
      out += `<tr><td>${escapeHtml(it.id)}</td><td>${escapeHtml(it.title)}</td><td>${escapeHtml(it.priority)}</td><td>${escapeHtml(it.status)}</td><td><input type="hidden" name="_csrf" value="${escapeHtml(csrf)}"></td></tr>`;
    }
    return out + "</tbody></table></section>";
  }

  const srv = http.createServer((req, res) => {
    const baseHeaders = {
      "X-Content-Type-Options": "nosniff",
      "X-Frame-Options": "DENY",
      "Referrer-Policy": "strict-origin-when-cross-origin",
      "Content-Security-Policy": csp,
      "Cache-Control": "no-store",
    };
    if (req.url === "/healthz") {
      res.writeHead(200, { ...baseHeaders, "Content-Type": "application/json" });
      return res.end('{"status":"ok"}');
    }
    if (req.url === "/public/yaza-ui.css") {
      if (req.headers["if-none-match"] === etag) {
        res.writeHead(304, { ...baseHeaders, ETag: etag, "Cache-Control": "public, max-age=300, must-revalidate" });
        return res.end();
      }
      res.writeHead(200, { ...baseHeaders, "Content-Type": "text/css; charset=utf-8", ETag: etag, "Cache-Control": "public, max-age=300, must-revalidate" });
      return res.end(CSS_BUF);
    }
    const items = buildTasks();
    if (req.url === "/api/v1/tasks") {
      res.writeHead(200, { ...baseHeaders, "Content-Type": "application/json" });
      return res.end(JSON.stringify(items));
    }
    if (req.url === "/tasks" && req.headers["x-yaza-fragment"]) {
      res.writeHead(200, { ...baseHeaders, "Content-Type": "text/html; charset=utf-8" });
      return res.end(renderFragment(items));
    }
    const html = `<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · Node SSR</title><link rel="stylesheet" href="/public/yaza-ui.css"><script defer src="/public/yaza-live.js"></script></head><body><main class="yz-content"><h1>Workspace Tasks</h1>${renderFragment(items)}</main></body></html>`;
    res.writeHead(200, { ...baseHeaders, "Content-Type": "text/html; charset=utf-8" });
    return res.end(html);
  });
  return new Promise((resolve) => srv.listen(port, "127.0.0.1", () => resolve({ server: srv, etag })));
}

function getRssMb(pid) {
  try {
    const kb = Number(execSync(`ps -o rss= -p ${pid}`).toString().trim());
    return kb / 1024;
  } catch {
    return 0;
  }
}

async function waitHealth(port) {
  for (let i = 0; i < 40; i++) {
    try {
      await new Promise((res, rej) => {
        const r = http.get(`http://127.0.0.1:${port}/healthz`, (resp) => {
          resp.resume();
          resp.statusCode === 200 ? res() : rej(new Error(String(resp.statusCode)));
        });
        r.on("error", rej);
      });
      return;
    } catch {
      await new Promise((r) => setTimeout(r, 80));
    }
  }
  throw new Error(`Server on port ${port} did not become healthy`);
}

function fetchOnce(agent, port, reqPath, headers = {}) {
  return new Promise((resolve, reject) => {
    const t0 = process.hrtime.bigint();
    const req = http.request({ host: "127.0.0.1", port, path: reqPath, method: "GET", headers, agent }, (res) => {
      let bytes = 0;
      res.on("data", (c) => { bytes += c.length; });
      res.on("end", () => {
        const dtUs = Number(process.hrtime.bigint() - t0) / 1000;
        resolve({ status: res.statusCode, dtUs, bytes, etag: res.headers.etag || "" });
      });
    });
    req.on("error", reject);
    req.end();
  });
}

async function benchEndpoint(port, reqPath, headers, expectedStatus) {
  const agent = new http.Agent({ keepAlive: true, maxSockets: CONCURRENCY });
  // Warmup
  for (let i = 0; i < 80; i++) {
    const w = await fetchOnce(agent, port, reqPath, headers);
    if (w.status !== expectedStatus) throw new Error(`Expected ${expectedStatus}, got ${w.status} on ${reqPath}`);
  }
  const roundRps = [];
  const roundP50 = [];
  const roundP95 = [];
  let sampleBytes = 0;

  for (let r = 0; r < ROUNDS; r++) {
    const latencies = [];
    let remaining = REQS_PER_ROUND;
    const tStart = process.hrtime.bigint();
    await Promise.all(
      Array.from({ length: CONCURRENCY }, async () => {
        while (true) {
          if (remaining <= 0) break;
          remaining--;
          const out = await fetchOnce(agent, port, reqPath, headers);
          if (out.status !== expectedStatus) throw new Error(`Status mismatch ${out.status}`);
          sampleBytes = out.bytes;
          latencies.push(out.dtUs);
        }
      })
    );
    const elapsedSec = Number(process.hrtime.bigint() - tStart) / 1e9;
    latencies.sort((a, b) => a - b);
    roundRps.push(REQS_PER_ROUND / elapsedSec);
    roundP50.push(latencies[Math.floor(latencies.length * 0.5)]);
    roundP95.push(latencies[Math.floor(latencies.length * 0.95)]);
  }
  agent.destroy();
  roundRps.sort((a, b) => a - b);
  roundP50.sort((a, b) => a - b);
  roundP95.sort((a, b) => a - b);
  return {
    rps: Math.round(roundRps[1]),
    p50_us: Math.round(roundP50[1]),
    p95_us: Math.round(roundP95[1]),
    bytes: sampleBytes,
  };
}

const yazaPort = 18110;
const nodePort = 18111;

const yazaProc = spawn(YAZA_BIN, [], {
  env: { ...process.env, BENCH_PORT: String(yazaPort), BENCH_CSS_PATH: CSS_PATH },
  stdio: "ignore",
});

// Spawn Node peer in separate process so Peak RSS is isolated cleanly
const nodePeerScript = `
import http from "node:http";
import fs from "node:fs";
const CSS_BUF = fs.readFileSync(${JSON.stringify(CSS_PATH)});
const etag = '"node-' + CSS_BUF.length + '"';
const csp = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
const csrf = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
const esc = (s) => s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&#39;");
function buildTasks(){const a=[];for(let i=1;i<=16;i++)a.push({id:"tsk-"+i,title:"Arena-SSR Task #"+i+" <verified & safe>",owner:"admin",priority:i%3===0?"high":"normal",status:i%4===0?"done":"open"});return a;}
function renderFrag(items){let o='<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>';for(const it of items)o+='<tr><td>'+esc(it.id)+'</td><td>'+esc(it.title)+'</td><td>'+esc(it.priority)+'</td><td>'+esc(it.status)+'</td><td><input type="hidden" name="_csrf" value="'+esc(csrf)+'"></td></tr>';return o+'</tbody></table></section>';}
http.createServer((req,res)=>{
  const h={"X-Content-Type-Options":"nosniff","X-Frame-Options":"DENY","Referrer-Policy":"strict-origin-when-cross-origin","Content-Security-Policy":csp,"Cache-Control":"no-store"};
  if(req.url==="/healthz"){res.writeHead(200,{...h,"Content-Type":"application/json"});return res.end('{"status":"ok"}');}
  if(req.url==="/public/yaza-ui.css"){
    if(req.headers["if-none-match"]===etag){res.writeHead(304,{...h,ETag:etag,"Cache-Control":"public, max-age=300, must-revalidate"});return res.end();}
    res.writeHead(200,{...h,"Content-Type":"text/css; charset=utf-8",ETag:etag,"Cache-Control":"public, max-age=300, must-revalidate"});return res.end(CSS_BUF);
  }
  const items=buildTasks();
  if(req.url==="/api/v1/tasks"){res.writeHead(200,{...h,"Content-Type":"application/json"});return res.end(JSON.stringify(items));}
  if(req.url==="/tasks"&&req.headers["x-yaza-fragment"]){res.writeHead(200,{...h,"Content-Type":"text/html; charset=utf-8"});return res.end(renderFrag(items));}
  res.writeHead(200,{...h,"Content-Type":"text/html; charset=utf-8"});
  return res.end('<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · Node SSR</title><link rel="stylesheet" href="/public/yaza-ui.css"><script defer src="/public/yaza-live.js"></script></head><body><main class="yz-content"><h1>Workspace Tasks</h1>'+renderFrag(items)+'</main></body></html>');
}).listen(${nodePort},"127.0.0.1");
`;
const nodeProc = spawn(process.execPath, ["--input-type=module", "-e", nodePeerScript], { stdio: "ignore" });

try {
  await waitHealth(yazaPort);
  await waitHealth(nodePort);

  const probeYazaCss = await fetchOnce(new http.Agent(), yazaPort, "/public/yaza-ui.css");
  const probeNodeCss = await fetchOnce(new http.Agent(), nodePort, "/public/yaza-ui.css");

  const workloads = [
    { name: "Full SSR HTML (16 escaped rows)", path: "/", headers: { Accept: "text/html" }, status: 200 },
    { name: "Hypermedia Fragment (#task-board)", path: "/tasks", headers: { Accept: "text/vnd.yaza.fragment", "X-Yaza-Fragment": "#task-board" }, status: 200 },
    { name: "Static CSS Asset (200 OK)", path: "/public/yaza-ui.css", headers: {}, status: 200 },
    { name: "Static CSS Conditional (304 ETag)", path: "/public/yaza-ui.css", headersYaza: { "If-None-Match": probeYazaCss.etag }, headersNode: { "If-None-Match": probeNodeCss.etag }, status: 304 },
    { name: "JSON API Parity (16 items)", path: "/api/v1/tasks", headers: { Accept: "application/json" }, status: 200 },
  ];

  const results = [];
  let yazaPeakRss = 0;
  let nodePeakRss = 0;

  for (const w of workloads) {
    const yRes = await benchEndpoint(yazaPort, w.path, w.headersYaza || w.headers, w.status);
    yazaPeakRss = Math.max(yazaPeakRss, getRssMb(yazaProc.pid));
    const nRes = await benchEndpoint(nodePort, w.path, w.headersNode || w.headers, w.status);
    nodePeakRss = Math.max(nodePeakRss, getRssMb(nodeProc.pid));
    results.push({ workload: w.name, yaza: yRes, node: nRes });
  }

  const binKb = Math.round((fs.statSync(YAZA_BIN).size / 1024) * 10) / 10;
  console.log(JSON.stringify({
    date: "2026-11-19",
    host: `${os.platform()} ${os.arch()} (${os.cpus().length} logical CPUs)`,
    node_version: process.version,
    concurrency: CONCURRENCY,
    requests_per_round: REQS_PER_ROUND,
    rounds: ROUNDS,
    yaza_binary_kb: binKb,
    yaza_peak_rss_mb: Math.round(yazaPeakRss * 100) / 100,
    node_peak_rss_mb: Math.round(nodePeakRss * 100) / 100,
    workloads: results,
  }, null, 2));
} finally {
  yazaProc.kill("SIGTERM");
  nodeProc.kill("SIGTERM");
}
