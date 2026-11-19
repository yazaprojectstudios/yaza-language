#!/usr/bin/env node
// Reproducible 8-Runtime Fullstack Web & SSR Benchmark:
// YAZA Arena-SSR (-O2 native view) vs Go 1.27 html/template vs Node.js v26 vs Fastify 5 vs Express 5 vs React 19 SSR vs Next.js 16 SSR vs PHP 8.5
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawn, execSync } from "node:child_process";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, "..");
const YAZA_BIN = path.join(ROOT, "build/fullstack-ssr-bench");
const CSS_PATH = path.join(ROOT, "templates/fullstack/public/yaza-ui.css");
const WORK = "/tmp/yaza-web-bench";

const CONCURRENCY = 8;
const WARMUP = 80;
const REQS_PER_ROUND = 1500;
const ROUNDS = 3;

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function getTreeRssKb(pid) {
  try {
    const out = execSync(`ps -axo pid=,ppid=,rss=`).toString();
    const rows = out.trim().split("\n").map((l) => {
      const [p, pp, r] = l.trim().split(/\s+/).map(Number);
      return { p, pp, r };
    });
    const wanted = new Set([pid]);
    let changed = true;
    while (changed) {
      changed = false;
      for (const row of rows) {
        if (wanted.has(row.pp) && !wanted.has(row.p)) {
          wanted.add(row.p);
          changed = true;
        }
      }
    }
    let total = 0;
    for (const row of rows) if (wanted.has(row.p)) total += row.r || 0;
    return total;
  } catch {
    return 0;
  }
}

async function waitHealthTimed(port, startedNs, pathName = "/healthz") {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    try {
      await new Promise((resolve, reject) => {
        const req = http.get({ host: "127.0.0.1", port, path: pathName, timeout: 1000 }, (res) => {
          res.resume();
          res.statusCode === 200 ? resolve() : reject(new Error(String(res.statusCode)));
        });
        req.on("error", reject);
        req.on("timeout", () => { req.destroy(); reject(new Error("timeout")); });
      });
      return Number(process.hrtime.bigint() - startedNs) / 1e6;
    } catch {
      await sleep(25);
    }
  }
  throw new Error(`Server on port ${port} (${pathName}) did not become healthy`);
}

function fetchOnce(agent, port, reqPath, headers = {}) {
  return new Promise((resolve, reject) => {
    const t0 = process.hrtime.bigint();
    const req = http.request({ host: "127.0.0.1", port, path: reqPath, method: "GET", headers, agent }, (res) => {
      let bytes = 0;
      res.on("data", (c) => { bytes += c.length; });
      res.on("end", () => {
        const us = Number(process.hrtime.bigint() - t0) / 1000;
        resolve({ status: res.statusCode, bytes, us, etag: res.headers.etag || "" });
      });
    });
    req.on("error", reject);
    req.end();
  });
}

async function benchEndpoint(port, reqPath, headers, expectedStatus, pid, reqs = REQS_PER_ROUND) {
  const agent = new http.Agent({ keepAlive: true, maxSockets: CONCURRENCY });
  for (let i = 0; i < WARMUP; i++) {
    const r = await fetchOnce(agent, port, reqPath, headers);
    if (r.status !== expectedStatus) {
      agent.destroy();
      throw new Error(`Warmup expected ${expectedStatus} on ${reqPath}, got ${r.status}`);
    }
  }
  const roundRps = [], roundP50 = [], roundP95 = [];
  let sampleBytes = 0;
  let peakRssKb = getTreeRssKb(pid);

  for (let round = 0; round < ROUNDS; round++) {
    const latencies = [];
    let nextIdx = 0;
    const tStart = process.hrtime.bigint();
    const workers = Array.from({ length: CONCURRENCY }, async () => {
      while (true) {
        const my = nextIdx++;
        if (my >= reqs) break;
        const r = await fetchOnce(agent, port, reqPath, headers);
        if (r.status !== expectedStatus) throw new Error(`Expected ${expectedStatus}, got ${r.status}`);
        sampleBytes = r.bytes;
        latencies.push(r.us);
      }
    });
    await Promise.all(workers);
    const rssNow = getTreeRssKb(pid);
    if (rssNow > peakRssKb) peakRssKb = rssNow;
    const elapsedSec = Number(process.hrtime.bigint() - tStart) / 1e9;
    latencies.sort((a, b) => a - b);
    roundRps.push(reqs / elapsedSec);
    roundP50.push(latencies[Math.floor(latencies.length * 0.50)]);
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
    peak_rss_mb: Number((peakRssKb / 1024).toFixed(2)),
  };
}

function buildGoPeer() {
  const goDir = path.join(WORK, "go-peer");
  fs.mkdirSync(goDir, { recursive: true });
  const goSrc = `package main
import (
  "encoding/json"
  "fmt"
  "html/template"
  "net/http"
  "os"
  "strings"
)
type Task struct {
  ID       string \`json:"id"\`
  Title    string \`json:"title"\`
  Owner    string \`json:"owner"\`
  Priority string \`json:"priority"\`
  Status   string \`json:"status"\`
}
type PageData struct {
  Items []Task
  CSRF  string
}
const csrfTok = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00"
const cspHeader = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'"
var fragTmpl = template.Must(template.New("frag").Parse(\`<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>{{range .Items}}<tr><td>{{.ID}}</td><td>{{.Title}}</td><td>{{.Priority}}</td><td>{{.Status}}</td><td><input type="hidden" name="_csrf" value="{{$.CSRF}}"></td></tr>{{end}}</tbody></table></section>\`))
var pageTmpl = template.Must(template.New("page").Parse(\`<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · Go SSR</title><link rel="stylesheet" href="/public/yaza-ui.css"><script defer src="/public/yaza-live.js"></script></head><body><main class="yz-content"><h1>Workspace Tasks</h1><section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>{{range .Items}}<tr><td>{{.ID}}</td><td>{{.Title}}</td><td>{{.Priority}}</td><td>{{.Status}}</td><td><input type="hidden" name="_csrf" value="{{$.CSRF}}"></td></tr>{{end}}</tbody></table></section></main></body></html>\`))
func buildTasks() []Task {
  items := make([]Task, 16)
  for i := 1; i <= 16; i++ {
    prio, st := "normal", "open"
    if i%3 == 0 { prio = "high" }
    if i%4 == 0 { st = "done" }
    items[i-1] = Task{ID: fmt.Sprintf("tsk-%d", i), Title: fmt.Sprintf("Arena-SSR Task #%d <verified & safe>", i), Owner: "admin", Priority: prio, Status: st}
  }
  return items
}
func setHeaders(w http.ResponseWriter) {
  w.Header().Set("X-Content-Type-Options", "nosniff")
  w.Header().Set("X-Frame-Options", "DENY")
  w.Header().Set("Referrer-Policy", "strict-origin-when-cross-origin")
  w.Header().Set("Content-Security-Policy", cspHeader)
  w.Header().Set("Cache-Control", "no-store")
}
func main() {
  cssBytes, _ := os.ReadFile(os.Getenv("BENCH_CSS_PATH"))
  etag := fmt.Sprintf("\\"go-%d\\"", len(cssBytes))
  mux := http.NewServeMux()
  mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) {
    setHeaders(w); w.Header().Set("Content-Type", "application/json"); w.Write([]byte(\`{"status":"ok"}\`))
  })
  mux.HandleFunc("/public/yaza-ui.css", func(w http.ResponseWriter, r *http.Request) {
    setHeaders(w)
    w.Header().Set("ETag", etag)
    w.Header().Set("Cache-Control", "public, max-age=300, must-revalidate")
    if r.Header.Get("If-None-Match") == etag { w.WriteHeader(304); return }
    w.Header().Set("Content-Type", "text/css; charset=utf-8"); w.Write(cssBytes)
  })
  mux.HandleFunc("/api/v1/tasks", func(w http.ResponseWriter, r *http.Request) {
    setHeaders(w); w.Header().Set("Content-Type", "application/json")
    b, _ := json.Marshal(buildTasks()); w.Write(b)
  })
  mux.HandleFunc("/tasks", func(w http.ResponseWriter, r *http.Request) {
    setHeaders(w); w.Header().Set("Content-Type", "text/html; charset=utf-8")
    var sb strings.Builder
    fragTmpl.Execute(&sb, PageData{Items: buildTasks(), CSRF: csrfTok})
    w.Write([]byte(sb.String()))
  })
  mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
    setHeaders(w); w.Header().Set("Content-Type", "text/html; charset=utf-8")
    var sb strings.Builder
    pageTmpl.Execute(&sb, PageData{Items: buildTasks(), CSRF: csrfTok})
    w.Write([]byte(sb.String()))
  })
  http.ListenAndServe("127.0.0.1:"+os.Getenv("BENCH_PORT"), mux)
}
`;
  fs.writeFileSync(path.join(goDir, "main.go"), goSrc);
  const goBin = path.join(goDir, "go-ssr-bench");
  execSync(`go build -o "${goBin}" "${path.join(goDir, "main.go")}"`, { cwd: goDir, stdio: "inherit" });
  return goBin;
}

function buildNextApp() {
  const nextDir = path.join(WORK, "next-app");
  fs.mkdirSync(path.join(nextDir, "pages/api/v1"), { recursive: true });
  fs.mkdirSync(path.join(nextDir, "public"), { recursive: true });
  fs.writeFileSync(path.join(nextDir, "package.json"), JSON.stringify({ name: "next-bench", private: true, type: "module" }));
  fs.copyFileSync(CSS_PATH, path.join(nextDir, "public/yaza-ui.css"));

  fs.writeFileSync(path.join(nextDir, "next.config.mjs"), `export default {
  poweredByHeader: false,
  async headers() {
    return [{
      source: "/(.*)",
      headers: [
        { key: "X-Content-Type-Options", value: "nosniff" },
        { key: "X-Frame-Options", value: "DENY" },
        { key: "Referrer-Policy", value: "strict-origin-when-cross-origin" },
        { key: "Content-Security-Policy", value: "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'" }
      ]
    }];
  }
};\n`);

  fs.writeFileSync(path.join(nextDir, "pages/index.js"), `import React from "react";
const CSRF = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
export async function getServerSideProps({ res }) {
  res.setHeader("Cache-Control", "no-store");
  const items = [];
  for (let i = 1; i <= 16; i++) {
    items.push({
      id: "tsk-" + i,
      title: "Arena-SSR Task #" + i + " <verified & safe>",
      owner: "admin",
      priority: i % 3 === 0 ? "high" : "normal",
      status: i % 4 === 0 ? "done" : "open",
    });
  }
  return { props: { items, csrf: CSRF } };
}
export default function Home({ items, csrf }) {
  return React.createElement("main", { className: "yz-content" },
    React.createElement("h1", null, "Workspace Tasks"),
    React.createElement("section", { id: "task-board" },
      React.createElement("div", { className: "yz-metrics" },
        React.createElement("article", { className: "yz-card" }, React.createElement("strong", null, "16"))
      ),
      React.createElement("table", { className: "yz-table" },
        React.createElement("tbody", null,
          items.map((it) => React.createElement("tr", { key: it.id },
            React.createElement("td", null, it.id),
            React.createElement("td", null, it.title),
            React.createElement("td", null, it.priority),
            React.createElement("td", null, it.status),
            React.createElement("td", null, React.createElement("input", { type: "hidden", name: "_csrf", value: csrf }))
          ))
        )
      )
    )
  );
}
`);

  fs.writeFileSync(path.join(nextDir, "pages/api/healthz.js"), `export default function handler(req, res) { res.status(200).json({ status: "ok" }); }\n`);
  fs.writeFileSync(path.join(nextDir, "pages/api/v1/tasks.js"), `export default function handler(req, res) {
  const items = [];
  for (let i = 1; i <= 16; i++) {
    items.push({ id: "tsk-" + i, title: "Arena-SSR Task #" + i + " <verified & safe>", owner: "admin", priority: i % 3 === 0 ? "high" : "normal", status: i % 4 === 0 ? "done" : "open" });
  }
  res.status(200).json(items);
}\n`);

  const nextBin = path.join(WORK, "node_modules/.bin/next");
  execSync(`"${nextBin}" build`, { cwd: nextDir, stdio: "ignore", env: { ...process.env, NEXT_TELEMETRY_DISABLED: "1" } });
  return { nextDir, nextBin };
}

function writeJsPeers() {
  // 1. Raw Node.js
  const nodeFile = path.join(WORK, "peer-node.mjs");
  fs.writeFileSync(nodeFile, `import http from "node:http";
import fs from "node:fs";
const CSS = fs.readFileSync(process.env.BENCH_CSS_PATH);
const ETAG = '"node-' + CSS.length + '"';
const CSP = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
const CSRF = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
const esc = (s) => s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&#39;");
function buildTasks(){const a=[];for(let i=1;i<=16;i++)a.push({id:"tsk-"+i,title:"Arena-SSR Task #"+i+" <verified & safe>",owner:"admin",priority:i%3===0?"high":"normal",status:i%4===0?"done":"open"});return a;}
function renderFrag(items){let o='<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>';for(const it of items)o+='<tr><td>'+esc(it.id)+'</td><td>'+esc(it.title)+'</td><td>'+esc(it.priority)+'</td><td>'+esc(it.status)+'</td><td><input type="hidden" name="_csrf" value="'+esc(CSRF)+'"></td></tr>';return o+'</tbody></table></section>';}
http.createServer((req,res)=>{
  const h={"X-Content-Type-Options":"nosniff","X-Frame-Options":"DENY","Referrer-Policy":"strict-origin-when-cross-origin","Content-Security-Policy":CSP,"Cache-Control":"no-store"};
  if(req.url==="/healthz"){res.writeHead(200,{...h,"Content-Type":"application/json"});return res.end('{"status":"ok"}');}
  if(req.url==="/public/yaza-ui.css"){if(req.headers["if-none-match"]===ETAG){res.writeHead(304,{...h,ETag:ETAG});return res.end();}res.writeHead(200,{...h,"Content-Type":"text/css; charset=utf-8",ETag:ETAG});return res.end(CSS);}
  const items=buildTasks();
  if(req.url==="/api/v1/tasks"){res.writeHead(200,{...h,"Content-Type":"application/json"});return res.end(JSON.stringify(items));}
  if(req.url==="/tasks"){res.writeHead(200,{...h,"Content-Type":"text/html; charset=utf-8"});return res.end(renderFrag(items));}
  res.writeHead(200,{...h,"Content-Type":"text/html; charset=utf-8"});
  return res.end('<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · Node</title><link rel="stylesheet" href="/public/yaza-ui.css"></head><body><main class="yz-content"><h1>Workspace Tasks</h1>'+renderFrag(items)+'</main></body></html>');
}).listen(Number(process.env.BENCH_PORT),"127.0.0.1");
`);

  // 2. Fastify 5
  const fastifyFile = path.join(WORK, "peer-fastify.mjs");
  fs.writeFileSync(fastifyFile, `import Fastify from "fastify";
import fs from "node:fs";
const app = Fastify({ logger: false });
const CSS = fs.readFileSync(process.env.BENCH_CSS_PATH);
const ETAG = '"fastify-' + CSS.length + '"';
const CSP = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
const CSRF = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
const esc = (s) => s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&#39;");
function buildTasks(){const a=[];for(let i=1;i<=16;i++)a.push({id:"tsk-"+i,title:"Arena-SSR Task #"+i+" <verified & safe>",owner:"admin",priority:i%3===0?"high":"normal",status:i%4===0?"done":"open"});return a;}
function renderFrag(items){let o='<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>';for(const it of items)o+='<tr><td>'+esc(it.id)+'</td><td>'+esc(it.title)+'</td><td>'+esc(it.priority)+'</td><td>'+esc(it.status)+'</td><td><input type="hidden" name="_csrf" value="'+esc(CSRF)+'"></td></tr>';return o+'</tbody></table></section>';}
app.addHook("onSend", async (_req, reply, payload) => {
  reply.header("X-Content-Type-Options","nosniff").header("X-Frame-Options","DENY").header("Content-Security-Policy",CSP);
  return payload;
});
app.get("/healthz", async () => ({ status: "ok" }));
app.get("/api/v1/tasks", async () => buildTasks());
app.get("/tasks", async (_req, reply) => reply.type("text/html; charset=utf-8").send(renderFrag(buildTasks())));
app.get("/", async (_req, reply) => reply.type("text/html; charset=utf-8").send('<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · Fastify</title><link rel="stylesheet" href="/public/yaza-ui.css"></head><body><main class="yz-content"><h1>Workspace Tasks</h1>'+renderFrag(buildTasks())+'</main></body></html>'));
await app.listen({ host: "127.0.0.1", port: Number(process.env.BENCH_PORT) });
`);

  // 3. Express 5
  const expressFile = path.join(WORK, "peer-express.mjs");
  fs.writeFileSync(expressFile, `import express from "express";
const app = express();
app.disable("x-powered-by");
const CSP = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
const CSRF = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
const esc = (s) => s.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;").replace(/'/g,"&#39;");
function buildTasks(){const a=[];for(let i=1;i<=16;i++)a.push({id:"tsk-"+i,title:"Arena-SSR Task #"+i+" <verified & safe>",owner:"admin",priority:i%3===0?"high":"normal",status:i%4===0?"done":"open"});return a;}
function renderFrag(items){let o='<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>';for(const it of items)o+='<tr><td>'+esc(it.id)+'</td><td>'+esc(it.title)+'</td><td>'+esc(it.priority)+'</td><td>'+esc(it.status)+'</td><td><input type="hidden" name="_csrf" value="'+esc(CSRF)+'"></td></tr>';return o+'</tbody></table></section>';}
app.use((_req, res, next) => { res.setHeader("X-Content-Type-Options","nosniff"); res.setHeader("X-Frame-Options","DENY"); res.setHeader("Content-Security-Policy",CSP); res.setHeader("Cache-Control","no-store"); next(); });
app.get("/healthz", (_req, res) => res.json({ status: "ok" }));
app.get("/api/v1/tasks", (_req, res) => res.json(buildTasks()));
app.get("/tasks", (_req, res) => res.type("text/html; charset=utf-8").send(renderFrag(buildTasks())));
app.get("/", (_req, res) => res.type("text/html; charset=utf-8").send('<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · Express</title><link rel="stylesheet" href="/public/yaza-ui.css"></head><body><main class="yz-content"><h1>Workspace Tasks</h1>'+renderFrag(buildTasks())+'</main></body></html>'));
app.listen(Number(process.env.BENCH_PORT), "127.0.0.1");
`);

  // 4. React 19 SSR (renderToString on Node.js)
  const reactFile = path.join(WORK, "peer-react-ssr.mjs");
  fs.writeFileSync(reactFile, `import http from "node:http";
import React from "react";
import { renderToString } from "react-dom/server";
const CSP = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'";
const CSRF = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
function buildTasks(){const a=[];for(let i=1;i<=16;i++)a.push({id:"tsk-"+i,title:"Arena-SSR Task #"+i+" <verified & safe>",owner:"admin",priority:i%3===0?"high":"normal",status:i%4===0?"done":"open"});return a;}
function TaskBoard({ items }) {
  return React.createElement("section", { id: "task-board" },
    React.createElement("div", { className: "yz-metrics" }, React.createElement("article", { className: "yz-card" }, React.createElement("strong", null, "16"))),
    React.createElement("table", { className: "yz-table" },
      React.createElement("tbody", null,
        items.map((it) => React.createElement("tr", { key: it.id },
          React.createElement("td", null, it.id),
          React.createElement("td", null, it.title),
          React.createElement("td", null, it.priority),
          React.createElement("td", null, it.status),
          React.createElement("td", null, React.createElement("input", { type: "hidden", name: "_csrf", value: CSRF }))
        ))
      )
    )
  );
}
function Page({ items }) {
  return React.createElement("html", { lang: "en" },
    React.createElement("head", null,
      React.createElement("meta", { charSet: "utf-8" }),
      React.createElement("title", null, "Bench · React 19 SSR"),
      React.createElement("link", { rel: "stylesheet", href: "/public/yaza-ui.css" })
    ),
    React.createElement("body", null,
      React.createElement("main", { className: "yz-content" },
        React.createElement("h1", null, "Workspace Tasks"),
        React.createElement(TaskBoard, { items })
      )
    )
  );
}
http.createServer((req, res) => {
  const h = {"X-Content-Type-Options":"nosniff","X-Frame-Options":"DENY","Content-Security-Policy":CSP,"Cache-Control":"no-store"};
  if (req.url === "/healthz") { res.writeHead(200, {...h,"Content-Type":"application/json"}); return res.end('{"status":"ok"}'); }
  const items = buildTasks();
  if (req.url === "/api/v1/tasks") { res.writeHead(200, {...h,"Content-Type":"application/json"}); return res.end(JSON.stringify(items)); }
  if (req.url === "/tasks") { res.writeHead(200, {...h,"Content-Type":"text/html; charset=utf-8"}); return res.end(renderToString(React.createElement(TaskBoard, { items }))); }
  res.writeHead(200, {...h,"Content-Type":"text/html; charset=utf-8"});
  return res.end("<!doctype html>" + renderToString(React.createElement(Page, { items })));
}).listen(Number(process.env.BENCH_PORT), "127.0.0.1");
`);

  // 5. PHP 8.5 built-in server
  const phpFile = path.join(WORK, "peer-php.php");
  fs.writeFileSync(phpFile, `<?php
$uri = $_SERVER["REQUEST_URI"];
header("X-Content-Type-Options: nosniff");
header("X-Frame-Options: DENY");
header("Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; font-src 'self'; object-src 'none'; base-uri 'self'; form-action 'self'; frame-ancestors 'none'");
header("Cache-Control: no-store");
if ($uri === "/healthz") { header("Content-Type: application/json"); echo '{"status":"ok"}'; exit; }
$items = [];
for ($i = 1; $i <= 16; $i++) {
  $items[] = ["id" => "tsk-$i", "title" => "Arena-SSR Task #$i <verified & safe>", "owner" => "admin", "priority" => ($i % 3 === 0 ? "high" : "normal"), "status" => ($i % 4 === 0 ? "done" : "open")];
}
if ($uri === "/api/v1/tasks") { header("Content-Type: application/json"); echo json_encode($items); exit; }
$csrf = "7bd8e11f66c2ec6a94f1c8d30e2b5a4c112233445566778899aabbccddeeff00";
header("Content-Type: text/html; charset=utf-8");
$frag = '<section id="task-board"><div class="yz-metrics"><article class="yz-card"><strong>16</strong></article></div><table class="yz-table"><tbody>';
foreach ($items as $it) {
  $frag .= '<tr><td>' . htmlspecialchars($it["id"], ENT_QUOTES) . '</td><td>' . htmlspecialchars($it["title"], ENT_QUOTES) . '</td><td>' . htmlspecialchars($it["priority"], ENT_QUOTES) . '</td><td>' . htmlspecialchars($it["status"], ENT_QUOTES) . '</td><td><input type="hidden" name="_csrf" value="' . $csrf . '"></td></tr>';
}
$frag .= '</tbody></table></section>';
if ($uri === "/tasks") { echo $frag; exit; }
echo '<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Bench · PHP 8.5</title><link rel="stylesheet" href="/public/yaza-ui.css"></head><body><main class="yz-content"><h1>Workspace Tasks</h1>' . $frag . '</main></body></html>';
`);

  return { nodeFile, fastifyFile, expressFile, reactFile, phpFile };
}

async function runFramework(spec) {
  const t0 = process.hrtime.bigint();
  const proc = spawn(spec.cmd, spec.args, { cwd: spec.cwd || WORK, env: { ...process.env, ...spec.env }, stdio: "ignore" });
  try {
    const coldMs = await waitHealthTimed(spec.port, t0, spec.healthPath || "/healthz");
    const fullSsr = await benchEndpoint(spec.port, "/", { Accept: "text/html" }, 200, proc.pid, spec.reqs || REQS_PER_ROUND);
    let fragment = null;
    if (spec.hasFragment) {
      fragment = await benchEndpoint(spec.port, "/tasks", { Accept: "text/vnd.yaza.fragment", "X-Yaza-Fragment": "#task-board" }, 200, proc.pid, spec.reqs || REQS_PER_ROUND);
    }
    const jsonApi = await benchEndpoint(spec.port, "/api/v1/tasks", { Accept: "application/json" }, 200, proc.pid, spec.reqs || REQS_PER_ROUND);
    const peakRss = Math.max(fullSsr.peak_rss_mb, fragment ? fragment.peak_rss_mb : 0, jsonApi.peak_rss_mb);
    return {
      name: spec.name,
      footprint: spec.footprint,
      cold_start_ms: Number(coldMs.toFixed(1)),
      peak_rss_mb: peakRss,
      full_ssr: { rps: fullSsr.rps, p50_us: fullSsr.p50_us, p95_us: fullSsr.p95_us, bytes: fullSsr.bytes },
      fragment: fragment ? { rps: fragment.rps, p50_us: fragment.p50_us, p95_us: fragment.p95_us, bytes: fragment.bytes } : null,
      json_api: { rps: jsonApi.rps, p50_us: jsonApi.p50_us, p95_us: jsonApi.p95_us, bytes: jsonApi.bytes },
    };
  } finally {
    try { proc.kill("SIGTERM"); } catch {}
    await sleep(150);
    try { proc.kill("SIGKILL"); } catch {}
  }
}

const goBin = buildGoPeer();
const { nextDir, nextBin } = buildNextApp();
const peers = writeJsPeers();
const yazaKb = Number((fs.statSync(YAZA_BIN).size / 1024).toFixed(1));
const goMb = Number((fs.statSync(goBin).size / (1024 * 1024)).toFixed(2));

const specs = [
  { name: "YAZA v1.0.0 Arena-SSR (-O2, native view)", cmd: YAZA_BIN, args: [], port: 18201, env: { BENCH_PORT: "18201", BENCH_CSS_PATH: CSS_PATH }, hasFragment: true, footprint: `${yazaKb} KB binary` },
  { name: "Go 1.27.3 (net/http + html/template)", cmd: goBin, args: [], port: 18202, env: { BENCH_PORT: "18202", BENCH_CSS_PATH: CSS_PATH }, hasFragment: true, footprint: `${goMb} MB binary` },
  { name: "Node.js v26.0.0 (raw node:http)", cmd: process.execPath, args: [peers.nodeFile], port: 18203, env: { BENCH_PORT: "18203", BENCH_CSS_PATH: CSS_PATH }, hasFragment: true, footprint: "V8 runtime" },
  { name: "Fastify 5.12.8 (Node.js v26)", cmd: process.execPath, args: [peers.fastifyFile], port: 18204, env: { BENCH_PORT: "18204", BENCH_CSS_PATH: CSS_PATH }, hasFragment: true, footprint: "Node + node_modules" },
  { name: "Express 5.3.0 (Node.js v26)", cmd: process.execPath, args: [peers.expressFile], port: 18205, env: { BENCH_PORT: "18205", BENCH_CSS_PATH: CSS_PATH }, hasFragment: true, footprint: "Node + node_modules" },
  { name: "React 19.3.1 SSR (renderToString / Node)", cmd: process.execPath, args: [peers.reactFile], port: 18206, env: { BENCH_PORT: "18206", BENCH_CSS_PATH: CSS_PATH }, hasFragment: true, footprint: "Node + React 19" },
  { name: "Next.js 16.4.6 Production SSR (next start)", cmd: nextBin, args: ["start", "-H", "127.0.0.1", "-p", "18207"], cwd: nextDir, port: 18207, healthPath: "/api/healthz", env: { NODE_ENV: "production", NEXT_TELEMETRY_DISABLED: "1" }, hasFragment: false, reqs: 600, footprint: "365 MB node_modules + .next" },
  { name: "PHP 8.5.6 (built-in server)", cmd: "php", args: ["-S", "127.0.0.1:18208", peers.phpFile], port: 18208, env: {}, hasFragment: true, reqs: 600, footprint: "PHP 8.5 runtime" },
];

const results = [];
for (const spec of specs) {
  process.stderr.write(`==> Benchmarking ${spec.name}...\n`);
  results.push(await runFramework(spec));
}

console.log(JSON.stringify({
  date: "2026-11-19",
  host: `${process.platform} ${process.arch} (10 logical CPUs)`,
  concurrency: CONCURRENCY,
  rounds: ROUNDS,
  requests_per_round: REQS_PER_ROUND,
  results,
}, null, 2));
