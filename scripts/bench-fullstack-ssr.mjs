#!/usr/bin/env node
// Production-Grade Multi-Runtime MVC/Web SSR Benchmark:
// YAZA Arena-SSR (-O2) vs Go (net/http + html/template) vs Node.js v26 (node:http)
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawn, execSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const CSS_PATH = path.join(ROOT, "templates/fullstack/public/yaza-ui.css");
const YAZA_BIN = path.join(ROOT, "build/fullstack-ssr-bench");
const GO_BIN = path.join(ROOT, "build/go-ssr-bench");
const CONCURRENCY = 8;
const REQS_PER_ROUND = 2000;
const ROUNDS = 3;

function getRssMb(pid) {
  try {
    return Number(execSync(`ps -o rss= -p ${pid}`).toString().trim()) / 1024;
  } catch {
    return 0;
  }
}

async function waitHealthTimed(port, t0Ns) {
  for (let i = 0; i < 100; i++) {
    try {
      await new Promise((res, rej) => {
        const r = http.get(`http://127.0.0.1:${port}/healthz`, (resp) => {
          resp.resume();
          resp.statusCode === 200 ? res() : rej(new Error(String(resp.statusCode)));
        });
        r.on("error", rej);
      });
      return Math.round((Number(process.hrtime.bigint() - t0Ns) / 1e6) * 10) / 10;
    } catch {
      await new Promise((r) => setTimeout(r, 15));
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
  for (let i = 0; i < 200; i++) {
    const w = await fetchOnce(agent, port, reqPath, headers);
    if (w.status !== expectedStatus) throw new Error(`Expected ${expectedStatus}, got ${w.status} on ${reqPath}`);
  }
  const roundRps = [], roundP50 = [], roundP95 = [];
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
  return { rps: Math.round(roundRps[1]), p50_us: Math.round(roundP50[1]), p95_us: Math.round(roundP95[1]), bytes: sampleBytes };
}

const yazaPort = 18110, goPort = 18112, nodePort = 18111;
const hasGo = fs.existsSync(GO_BIN);
let goVersion = "unavailable";
try { goVersion = execSync("go version").toString().trim(); } catch {}

const tYaza = process.hrtime.bigint();
const yazaProc = spawn(YAZA_BIN, [], {
  env: { ...process.env, BENCH_PORT: String(yazaPort), BENCH_CSS_PATH: CSS_PATH },
  stdio: "ignore",
});

let goProc = null;
const tGo = process.hrtime.bigint();
if (hasGo) {
  goProc = spawn(GO_BIN, [], {
    env: { ...process.env, BENCH_PORT: String(goPort), BENCH_CSS_PATH: CSS_PATH },
    stdio: "ignore",
  });
}

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
const tNode = process.hrtime.bigint();
const nodeProc = spawn(process.execPath, ["--input-type=module", "-e", nodePeerScript], { stdio: "ignore" });

try {
  const yazaStartMs = await waitHealthTimed(yazaPort, tYaza);
  const goStartMs = hasGo ? await waitHealthTimed(goPort, tGo) : null;
  const nodeStartMs = await waitHealthTimed(nodePort, tNode);

  const pYazaCss = await fetchOnce(new http.Agent(), yazaPort, "/public/yaza-ui.css");
  const pGoCss = hasGo ? await fetchOnce(new http.Agent(), goPort, "/public/yaza-ui.css") : { etag: "" };
  const pNodeCss = await fetchOnce(new http.Agent(), nodePort, "/public/yaza-ui.css");

  const workloads = [
    { name: "Full SSR HTML (16 escaped rows + CSP)", path: "/", h: { Accept: "text/html" }, status: 200 },
    { name: "Hypermedia Fragment (#task-board)", path: "/tasks", h: { Accept: "text/vnd.yaza.fragment", "X-Yaza-Fragment": "#task-board" }, status: 200 },
    { name: "Static CSS Asset (200 OK)", path: "/public/yaza-ui.css", h: {}, status: 200 },
    { name: "Static CSS Conditional (304 ETag)", path: "/public/yaza-ui.css", hy: { "If-None-Match": pYazaCss.etag }, hg: { "If-None-Match": pGoCss.etag }, hn: { "If-None-Match": pNodeCss.etag }, status: 304 },
    { name: "JSON API Parity (16 items)", path: "/api/v1/tasks", h: { Accept: "application/json" }, status: 200 },
  ];

  const results = [];
  let yazaRss = 0, goRss = 0, nodeRss = 0;

  for (const w of workloads) {
    const yRes = await benchEndpoint(yazaPort, w.path, w.hy || w.h, w.status);
    yazaRss = Math.max(yazaRss, getRssMb(yazaProc.pid));
    const gRes = hasGo ? await benchEndpoint(goPort, w.path, w.hg || w.h, w.status) : null;
    if (hasGo) goRss = Math.max(goRss, getRssMb(goProc.pid));
    const nRes = await benchEndpoint(nodePort, w.path, w.hn || w.h, w.status);
    nodeRss = Math.max(nodeRss, getRssMb(nodeProc.pid));
    results.push({ workload: w.name, yaza: yRes, go: gRes, node: nRes });
  }

  console.log(JSON.stringify({
    date: new Date().toISOString().slice(0, 10),
    host: `${os.platform()} ${os.arch()} (${os.cpus().length} logical CPUs)`,
    go_version: goVersion,
    node_version: process.version,
    concurrency: CONCURRENCY,
    requests_per_round: REQS_PER_ROUND,
    rounds: ROUNDS,
    footprint: {
      yaza_binary_kb: Math.round((fs.statSync(YAZA_BIN).size / 1024) * 10) / 10,
      go_binary_kb: hasGo ? Math.round((fs.statSync(GO_BIN).size / 1024) * 10) / 10 : null,
      yaza_cold_start_ms: yazaStartMs,
      go_cold_start_ms: goStartMs,
      node_cold_start_ms: nodeStartMs,
      yaza_peak_rss_mb: Math.round(yazaRss * 100) / 100,
      go_peak_rss_mb: hasGo ? Math.round(goRss * 100) / 100 : null,
      node_peak_rss_mb: Math.round(nodeRss * 100) / 100,
    },
    workloads: results,
  }, null, 2));
} finally {
  yazaProc.kill("SIGTERM");
  if (goProc) goProc.kill("SIGTERM");
  nodeProc.kill("SIGTERM");
}
