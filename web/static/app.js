"use strict";

const $ = (id) => document.getElementById(id);

async function api(path, options) {
  const resp = await fetch(path, options);
  const data = await resp.json().catch(() => ({}));
  if (!resp.ok) {
    throw new Error(data.error || ("HTTP " + resp.status));
  }
  return data;
}

function out(id, html, isError) {
  const node = $(id);
  node.innerHTML = html;
  node.className = "out" + (isError ? " err" : "");
}

function esc(s) {
  return String(s).replace(/[&<>"]/g, (c) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c])
  );
}

function flightTable(flights) {
  if (!flights || !flights.length) return '<span class="err">无结果</span>';
  let h = '<table><tr><th>ID</th><th>航班号</th><th>类型</th><th>起降</th><th>起飞</th><th>到达</th><th>票价</th></tr>';
  for (const f of flights) {
    h += `<tr><td>${f.id}</td><td>${f.flight_no}</td><td>${f.is_international ? "Intl" : "Dome"}</td>` +
         `<td>${f.from_airport}→${f.to_airport}</td><td>${esc(f.dep_time)}</td><td>${esc(f.arr_time)}</td><td>${f.fare}</td></tr>`;
  }
  return h + "</table>";
}

function routesList(routes) {
  if (!routes || !routes.length) return '<span class="err">无可行方案</span>';
  let h = `<span class="ok">共 ${routes.length} 条方案</span>`;
  routes.forEach((r, i) => { h += `<div>方案 ${i + 1}: ${r.join(" → ")}</div>`; });
  return h;
}

async function loadStats() {
  try {
    const s = await api("/api/stats");
    const rows = [
      ["起飞最早", s.earliest_departure], ["起飞最晚", s.latest_departure],
      ["飞行最短", s.shortest_duration], ["飞行最长", s.longest_duration],
      ["票价最低", s.cheapest], ["票价最高", s.most_expensive],
    ];
    let h = '<table><tr><th>统计项</th><th>航班</th></tr>';
    for (const [label, f] of rows) {
      h += `<tr><td>${label}</td><td>${f ? `ID ${f.id} · ${f.from_airport}→${f.to_airport} · ${esc(f.dep_time)}` : "—"}</td></tr>`;
    }
    out("stats-out", h + "</table>");
  } catch (e) { out("stats-out", e.message, true); }
}

async function queryDirect() {
  try {
    const from = $("direct-from").value, to = $("direct-to").value;
    const data = await api(`/api/flights?from=${from}&to=${to}`);
    out("direct-out", flightTable(data.flights));
  } catch (e) { out("direct-out", e.message, true); }
}

async function getFlight() {
  try {
    const id = $("flight-id").value;
    const f = await api(`/api/flights/${id}`);
    out("crud-out", flightTable([f]));
  } catch (e) { out("crud-out", e.message, true); }
}

async function delFlight() {
  try {
    const id = $("flight-id").value;
    await api(`/api/flights/${id}`, { method: "DELETE" });
    out("crud-out", `<span class="ok">已删除航班 ${id}</span>`);
  } catch (e) { out("crud-out", e.message, true); }
}

function parseFlightJson() {
  const j = JSON.parse($("flight-json").value);
  return j;
}

async function addFlight() {
  try {
    const f = await api("/api/flights", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(parseFlightJson()),
    });
    out("crud-out", `<span class="ok">已新增：</span>` + flightTable([f]));
  } catch (e) { out("crud-out", e.message, true); }
}

async function updateFlight() {
  try {
    const id = $("flight-id").value;
    const f = await api(`/api/flights/${id}`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(parseFlightJson()),
    });
    out("crud-out", `<span class="ok">已修改：</span>` + flightTable([f]));
  } catch (e) { out("crud-out", e.message, true); }
}

async function suspendAirport() {
  try {
    const id = $("airport-id").value;
    await api("/api/airports/suspend", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: Number(id) }),
    });
    out("sr-out", `<span class="ok">机场 ${id} 已暂停</span>`);
  } catch (e) { out("sr-out", e.message, true); }
}

async function resumeAirport() {
  try {
    const id = $("airport-id").value;
    await api("/api/airports/resume", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id: Number(id) }),
    });
    out("sr-out", `<span class="ok">机场 ${id} 已恢复</span>`);
  } catch (e) { out("sr-out", e.message, true); }
}

async function maxFlights() {
  try {
    const start = $("max-start").value;
    const r = await api(`/api/trips/max-flights?start=${start}`);
    out("max-out", `<div>最多可乘坐 <b>${r.max_count}</b> 次航班：</div>` + routesList(r.routes));
  } catch (e) { out("max-out", e.message, true); }
}

async function searchAirport() {
  try {
    const q = $("search-q").value;
    const data = await api(`/api/airports/search?q=${encodeURIComponent(q)}`);
    let h = '<table><tr><th>机场</th><th>相似度</th></tr>';
    for (const r of data.results) {
      h += `<tr><td>${esc(r.airport.full_name)}</td><td>${r.score.toFixed(4)}</td></tr>`;
    }
    out("search-out", h + "</table>");
  } catch (e) { out("search-out", e.message, true); }
}

async function recommend() {
  try {
    const from = $("rec-from").value, to = $("rec-to").value;
    const data = await api(`/api/airports/recommend?from=${from}&to=${to}`);
    out("rec-out", `<span class="ok">同省推荐 ${data.count} 条：</span>` + flightTable(data.flights));
  } catch (e) { out("rec-out", e.message, true); }
}

async function busiest() {
  try {
    const p = new URLSearchParams();
    const set = (k, id) => { const v = $(id).value; if (v) p.set(k, v); };
    set("dep_start", "busy-dep-start"); set("dep_end", "busy-dep-end");
    set("arr_start", "busy-arr-start"); set("arr_end", "busy-arr-end");
    const data = await api(`/api/airports/busiest?${p.toString()}`);
    let h = '<table><tr><th>机场</th><th>起飞</th><th>降落</th><th>合计</th></tr>';
    for (const b of data.busiest) {
      h += `<tr><td>${esc(b.full_name)} (${b.airport_id})</td><td>${b.departures}</td><td>${b.arrivals}</td><td>${b.total}</td></tr>`;
    }
    out("busy-out", h + "</table>");
  } catch (e) { out("busy-out", e.message, true); }
}

async function connectivity() {
  try {
    const from = $("conn-from").value, to = $("conn-to").value;
    const data = await api(`/api/routes/connect?from=${from}&to=${to}&max_transfers=1`);
    out("conn-out", routesList(data.routes));
  } catch (e) { out("conn-out", e.message, true); }
}

async function optimal() {
  try {
    const from = $("opt-from").value, to = $("opt-to").value, c = $("opt-criteria").value;
    const data = await api(`/api/routes/optimal?from=${from}&to=${to}&criteria=${c}`);
    out("opt-out", routesList(data.routes));
  } catch (e) { out("opt-out", e.message, true); }
}

loadStats();
