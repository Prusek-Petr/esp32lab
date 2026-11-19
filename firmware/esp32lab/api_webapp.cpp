// api_webapp.cpp — Serves the ESP32 Lab web app directly from firmware flash.
// All web files are embedded as raw string literals — no LittleFS needed.

#include "config.h"
#include "api_webapp.h"
#include "api_server.h"

// ── index.html ────────────────────────────────────────────────────────────────

static const char FILE_INDEX[] = R"WEBEND(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta name="theme-color" content="#38bdf8">
    <title>ESP32 Lab</title>
    <link rel="manifest" href="manifest.json">
    <link rel="stylesheet" href="css/style.css?v={{VER}}">
</head>
<body>
    <div id="connection-bar">
        <div class="conn-row">
            <input type="text" id="ip-input" value="192.168.4.1" placeholder="IP address">
            <button id="connect-btn">Connect</button>
            <span id="status-dot" class="dot disconnected"></span>
            <span id="status-text">Disconnected</span>
        </div>
        <div id="sys-summary" class="hidden"></div>
    </div>
    <nav id="tab-bar" role="tablist">
        <button class="tab active" data-tab="system" role="tab">System</button>
        <button class="tab" data-tab="gpio" role="tab">GPIO</button>
        <button class="tab" data-tab="grove" role="tab">Sensors</button>
    </nav>
    <main id="tab-content">
        <section id="tab-system" class="tab-panel active" role="tabpanel"></section>
        <section id="tab-gpio"   class="tab-panel" role="tabpanel"></section>
        <section id="tab-grove"  class="tab-panel" role="tabpanel"></section>
    </main>
    <script type="module" src="js/app.js?v={{VER}}"></script>
</body>
</html>
)WEBEND";

// ── css/style.css ─────────────────────────────────────────────────────────────

static const char FILE_STYLE_CSS[] = R"WEBEND(*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

:root {
    --bg: #0f172a;
    --surface: #1e293b;
    --surface2: #334155;
    --border: #475569;
    --text: #f1f5f9;
    --text-dim: #94a3b8;
    --accent: #38bdf8;
    --accent-dim: #0ea5e9;
    --green: #4ade80;
    --red: #f87171;
    --yellow: #fbbf24;
    --orange: #fb923c;
}

html, body {
    height: 100%; width: 100%;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
    font-size: 15px;
    background: var(--bg);
    color: var(--text);
    overflow: hidden;
    overscroll-behavior: none;
    -webkit-user-select: none; user-select: none;
}

#connection-bar {
    background: var(--surface);
    border-bottom: 2px solid var(--accent);
    padding: 10px 14px;
}

.conn-row { display: flex; align-items: center; gap: 8px; }

#ip-input {
    flex: 1; max-width: 160px;
    padding: 8px 10px; border-radius: 8px;
    border: 1px solid var(--border);
    background: var(--surface2); color: var(--text);
    font-size: 14px; font-family: monospace;
}

#connect-btn {
    padding: 8px 18px; border-radius: 8px; border: none;
    background: var(--accent); color: #0f172a;
    font-weight: 700; font-size: 14px; cursor: pointer;
}
#connect-btn:active { opacity: 0.8; }
#connect-btn.connected { background: var(--red); color: #fff; }

.dot { width: 12px; height: 12px; border-radius: 50%; flex-shrink: 0; }
.dot.disconnected { background: var(--red); }
.dot.connected    { background: var(--green); box-shadow: 0 0 8px var(--green); }
.dot.connecting   { background: var(--yellow); animation: pulse 1s infinite; }

@keyframes pulse { 50% { opacity: 0.4; } }

#status-text { font-size: 13px; color: var(--text-dim); }

#sys-summary {
    font-size: 12px; color: var(--text-dim);
    margin-top: 5px; font-family: monospace;
}

.hidden { display: none !important; }

#tab-bar {
    display: flex;
    background: var(--surface);
    border-bottom: 1px solid var(--border);
}

.tab {
    flex: 1; padding: 13px 8px; border: none; background: none;
    color: var(--text-dim); font-size: 14px; font-weight: 600;
    cursor: pointer; border-bottom: 3px solid transparent; letter-spacing: 0.2px;
}
.tab.active { color: var(--accent); border-bottom-color: var(--accent); }

body { display: flex; flex-direction: column; }

#tab-content { flex: 1; overflow-y: auto; -webkit-overflow-scrolling: touch; overscroll-behavior: none; }

.tab-panel { display: none; padding: 14px; }
.tab-panel.active { display: block; }

.card {
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 10px; padding: 14px; margin-bottom: 12px;
}
.card h3 {
    font-size: 11px; text-transform: uppercase; letter-spacing: 1px;
    color: var(--accent); margin-bottom: 10px;
}

.kv {
    display: flex; justify-content: space-between;
    padding: 6px 0; font-size: 14px;
    border-bottom: 1px solid var(--surface2);
}
.kv:last-child { border-bottom: none; }
.kv .label { color: var(--text-dim); }
.kv .value { font-family: monospace; color: var(--text); }

.bar-wrap {
    height: 8px; background: var(--surface2);
    border-radius: 4px; overflow: hidden; margin: 6px 0 4px;
}
.bar-fill {
    height: 100%; border-radius: 4px;
    background: var(--accent); transition: width 0.3s;
}
.bar-fill.warn   { background: var(--yellow); }
.bar-fill.danger { background: var(--red); }

.form-row {
    display: flex; gap: 8px; align-items: center;
    margin-bottom: 10px; flex-wrap: wrap;
}
.form-row label { font-size: 13px; color: var(--text-dim); min-width: 50px; }

input[type="text"], input[type="number"], select {
    padding: 8px 10px; border-radius: 8px; border: 1px solid var(--border);
    background: var(--surface2); color: var(--text);
    font-size: 14px; font-family: monospace; min-width: 0;
}

input[type="range"] { flex: 1; accent-color: var(--accent); }

button {
    padding: 9px 18px; border-radius: 8px; border: none;
    background: var(--accent); color: #0f172a;
    font-weight: 700; font-size: 14px; cursor: pointer; white-space: nowrap;
}
button:active { opacity: 0.8; }
button.secondary {
    background: var(--surface2); color: var(--text);
    border: 1px solid var(--border); font-weight: 600;
}
button.danger { background: var(--red); color: #fff; }

.log { max-height: 240px; overflow-y: auto; font-size: 13px; font-family: monospace; }
.log-entry {
    padding: 6px 8px; border-bottom: 1px solid var(--surface2); line-height: 1.4;
}
.log-entry:last-child { border-bottom: none; }
.log-time { color: var(--text-dim); margin-right: 8px; }

.toggle-group {
    display: flex; border-radius: 8px; overflow: hidden; border: 1px solid var(--border);
}
.toggle-group button {
    border-radius: 0; border: none; background: var(--surface2);
    color: var(--text-dim); padding: 9px 16px; font-size: 14px; font-weight: 600;
}
.toggle-group button.active { background: var(--accent); color: #0f172a; }

.wiring-table { width: 100%; border-collapse: collapse; font-size: 13px; margin: 8px 0; }
.wiring-table th {
    text-align: left; color: var(--text-dim); font-size: 11px;
    text-transform: uppercase; letter-spacing: 0.5px;
    padding: 4px 8px; border-bottom: 1px solid var(--border);
}
.wiring-table td { padding: 7px 8px; border-bottom: 1px solid var(--surface2); font-family: monospace; }
.wiring-table tr:last-child td { border-bottom: none; }
.pin-sensor { color: var(--yellow); }
.pin-esp32  { color: var(--green); }

.wiring-note {
    font-size: 12px; color: var(--text-dim); margin-top: 6px;
    padding: 8px 10px; background: var(--surface2);
    border-radius: 6px; line-height: 1.6;
}
)WEBEND";

// ── js/api.js ─────────────────────────────────────────────────────────────────

static const char FILE_API_JS[] = R"WEBEND(export class ESP32LabAPI {
    constructor() {
        this._ip = '';
        this._connected = false;
        this.onStatusChange = null;
    }

    get ip() { return this._ip; }
    get connected() { return this._connected; }

    connect(ip) {
        this._ip = ip;
        return this.get('/api/system/info').then(data => {
            this._setStatus(true);
            return data;
        }).catch(err => {
            this._setStatus(false);
            throw err;
        });
    }

    disconnect() { this._setStatus(false); }

    async get(path) {
        try {
            const resp = await fetch(`http://${this._ip}${path}`, {
                signal: AbortSignal.timeout(5000)
            });
            if (!resp.ok) {
                const body = await resp.json().catch(() => null);
                throw new Error(body?.error || `${resp.status} ${resp.statusText}`);
            }
            return resp.json();
        } catch (e) {
            if (e.name === 'TimeoutError' || e.name === 'AbortError')
                throw new Error('Request timed out — is the ESP32 reachable?');
            if (e.name === 'TypeError')
                throw new Error('Network error — check WiFi connection');
            throw e;
        }
    }

    async post(path, body) {
        try {
            const resp = await fetch(`http://${this._ip}${path}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body),
                signal: AbortSignal.timeout(5000)
            });
            if (!resp.ok) {
                const data = await resp.json().catch(() => null);
                throw new Error(data?.error || `${resp.status} ${resp.statusText}`);
            }
            return resp.json();
        } catch (e) {
            if (e.name === 'TimeoutError' || e.name === 'AbortError')
                throw new Error('Request timed out — is the ESP32 reachable?');
            if (e.name === 'TypeError')
                throw new Error('Network error — check WiFi connection');
            throw e;
        }
    }

    _setStatus(connected) {
        this._connected = connected;
        if (this.onStatusChange) this.onStatusChange(connected);
    }
}
)WEBEND";

// ── js/app.js ─────────────────────────────────────────────────────────────────

static const char FILE_APP_JS[] = R"WEBEND(import { ESP32LabAPI } from './api.js';
import { initSystem, activateSystem, deactivateSystem } from './tab-system.js';
import { initGpio,   activateGpio,   deactivateGpio   } from './tab-gpio.js';
import { initGrove,  activateGrove,  deactivateGrove  } from './tab-grove.js';

const api = new ESP32LabAPI();

const tabs = {
    system: { init: initSystem, activate: activateSystem, deactivate: deactivateSystem },
    gpio:   { init: initGpio,   activate: activateGpio,   deactivate: deactivateGpio   },
    grove:  { init: initGrove,  activate: activateGrove,  deactivate: deactivateGrove  },
};

let activeTab = 'system';
let sysInfoInterval = null;

const ipInput    = document.getElementById('ip-input');
const connectBtn = document.getElementById('connect-btn');
const statusDot  = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');
const sysSummary = document.getElementById('sys-summary');

const savedIp = localStorage.getItem('esp32lab-ip');
const servedFromDevice = location.hostname !== '' &&
    location.hostname !== 'localhost' &&
    location.hostname !== '127.0.0.1';

if (servedFromDevice) {
    ipInput.value = location.hostname;
} else if (savedIp) {
    ipInput.value = savedIp;
}

if (servedFromDevice && !api.connected) {
    statusDot.className = 'dot connecting';
    statusText.textContent = 'Connecting...';
    api.connect(location.hostname).catch(e => {
        statusText.textContent = e.message || 'Connection failed';
    });
}

api.onStatusChange = (connected) => {
    statusDot.className = 'dot ' + (connected ? 'connected' : 'disconnected');
    statusText.textContent = connected ? 'Connected' : 'Disconnected';
    connectBtn.textContent = connected ? 'Disconnect' : 'Connect';
    connectBtn.classList.toggle('connected', connected);

    if (connected) {
        startSysPolling();
        tabs[activeTab].activate(api);
    } else {
        stopSysPolling();
        sysSummary.classList.add('hidden');
        tabs[activeTab].deactivate(api);
    }
};

connectBtn.addEventListener('click', () => {
    if (api.connected) {
        api.disconnect();
    } else {
        const ip = ipInput.value.trim();
        localStorage.setItem('esp32lab-ip', ip);
        statusDot.className = 'dot connecting';
        statusText.textContent = 'Connecting...';
        api.connect(ip).catch(e => {
            statusText.textContent = e.message || 'Connection failed';
        });
    }
});

ipInput.addEventListener('keydown', e => {
    if (e.key === 'Enter' && !api.connected) connectBtn.click();
});

function startSysPolling() {
    pollSysInfo();
    sysInfoInterval = setInterval(pollSysInfo, 5000);
}
function stopSysPolling() {
    if (sysInfoInterval) { clearInterval(sysInfoInterval); sysInfoInterval = null; }
}
function pollSysInfo() {
    if (!api.connected) return;
    api.get('/api/system/info').then(data => {
        const upSec = Math.floor((data.uptime_ms || 0) / 1000);
        const m = Math.floor(upSec / 60), s = upSec % 60;
        sysSummary.textContent =
            `${data.chip?.model || 'ESP32'}  |  Heap: ${fmt(data.heap?.free)} free  |  Up: ${m}m${s}s`;
        sysSummary.classList.remove('hidden');
    }).catch(() => {});
}

function fmt(n) {
    if (n == null) return '?';
    if (n >= 1048576) return (n / 1048576).toFixed(1) + 'M';
    if (n >= 1024)    return (n / 1024).toFixed(0) + 'K';
    return String(n);
}

document.getElementById('tab-bar').addEventListener('click', e => {
    const btn = e.target.closest('.tab');
    if (!btn) return;
    const name = btn.dataset.tab;
    if (name === activeTab) return;

    if (api.connected) tabs[activeTab].deactivate(api);
    document.querySelector('.tab.active').classList.remove('active');
    document.querySelector('.tab-panel.active').classList.remove('active');

    activeTab = name;
    btn.classList.add('active');
    document.getElementById('tab-' + name).classList.add('active');
    if (api.connected) tabs[activeTab].activate(api);
});

for (const [name, tab] of Object.entries(tabs)) {
    tab.init(document.getElementById('tab-' + name), api);
}

window.addEventListener('beforeunload', () => { if (api.connected) api.disconnect(); });
)WEBEND";

// ── js/tab-system.js ──────────────────────────────────────────────────────────

static const char FILE_SYSTEM_JS[] = R"WEBEND(let panel, interval, currentApi;
let apSsidCached = 'ESP32Lab';

export function initSystem(el) {
    panel = el;
    panel.innerHTML = `
        <div class="card" id="sys-chip"><h3>Chip</h3><div class="kv-list"></div></div>
        <div class="card" id="sys-memory">
            <h3>Memory</h3>
            <div class="kv"><span class="label">Heap</span><span class="value" id="sys-heap-text">--</span></div>
            <div class="bar-wrap"><div class="bar-fill" id="sys-heap-bar"></div></div>
            <div class="kv"><span class="label">PSRAM</span><span class="value" id="sys-psram-text">--</span></div>
            <div class="bar-wrap"><div class="bar-fill" id="sys-psram-bar"></div></div>
        </div>
        <div class="card" id="sys-net"><h3>Network</h3><div class="kv-list"></div></div>
        <div class="card" id="sys-misc"><h3>System</h3><div class="kv-list"></div></div>
        <div class="card" id="sys-device">
            <h3>Device</h3>
            <div class="form-row">
                <label>Name</label>
                <input type="text" id="device-name-input" style="flex:1" placeholder="esp32lab">
                <button id="device-rename-btn" class="secondary">Rename</button>
            </div>
            <div id="device-local-url" style="font-size:12px;color:var(--text-dim);margin:-4px 0 10px;display:none"></div>
            <div class="form-row" style="margin-top:4px">
                <button id="identify-btn">Identify (Blink LED)</button>
            </div>
            <div id="device-hint" style="font-size:12px;color:var(--text-dim);margin-top:8px;display:none"></div>
        </div>
        <div class="card">
            <h3>Grove Pins</h3>
            <div class="form-row">
                <label>D pin</label>
                <input type="number" id="grove-pin-d" min="0" max="50" style="width:70px;font-family:monospace">
                <label>D2 pin</label>
                <input type="number" id="grove-pin-d2" min="0" max="50" style="width:70px;font-family:monospace">
                <button id="grove-pin-save" class="secondary">Save</button>
            </div>
            <div id="grove-pin-hint" style="font-size:12px;color:var(--text-dim);margin-top:4px;display:none"></div>
        </div>
        <div class="card">
            <h3>WiFi Configuration</h3>
            <div id="wifi-status" style="font-size:13px;margin-bottom:12px;color:var(--text-dim)">Loading...</div>
            <div class="form-row">
                <label>Network</label>
                <input type="text" id="wifi-ssid" placeholder="Network name (SSID)" style="flex:1">
                <button id="wifi-scan-btn" class="secondary">Scan</button>
            </div>
            <div id="wifi-scan-results" style="display:none;border:1px solid var(--border);border-radius:6px;margin-bottom:8px;max-height:200px;overflow-y:auto"></div>
            <div class="form-row">
                <label>Password</label>
                <input type="password" id="wifi-pass" placeholder="Password" style="flex:1">
            </div>
            <div id="wifi-local-url" style="font-size:12px;color:var(--text-dim);margin:-4px 0 6px;display:none"></div>
            <div class="form-row">
                <button id="wifi-connect-btn">Connect to Network</button>
                <button id="wifi-forget-btn" class="secondary" style="display:none">Forget WiFi</button>
            </div>
            <div id="wifi-hint" style="font-size:12px;color:var(--text-dim);margin-top:8px;display:none;line-height:1.5"></div>
        </div>
        <div class="card">
            <h3>Firmware Update</h3>
            <div style="font-size:13px;color:var(--text-dim);margin-bottom:12px">
                Running: <span id="ota-current" style="color:var(--text);font-family:monospace">--</span>
                &nbsp;&mdash;&nbsp;upload a new .bin to update over WiFi.
            </div>
            <div class="form-row">
                <input type="file" id="ota-file" accept=".bin" style="flex:1;font-size:13px;min-width:0">
                <button id="ota-btn" class="secondary">Upload</button>
            </div>
            <div id="ota-progress" style="display:none;margin-top:10px">
                <div class="bar-wrap"><div class="bar-fill" id="ota-bar" style="width:0%"></div></div>
                <div id="ota-msg" style="font-size:13px;color:var(--text-dim);margin-top:6px"></div>
            </div>
        </div>
    `;

    panel.querySelector('#identify-btn').onclick = identifyDevice;
    panel.querySelector('#device-rename-btn').onclick = renameDevice;
    panel.querySelector('#grove-pin-save').onclick = saveGrovePins;
    panel.querySelector('#ota-btn').onclick = startOta;
    panel.querySelector('#wifi-scan-btn').onclick = scanWifi;
    panel.querySelector('#wifi-connect-btn').onclick = connectWifi;
    panel.querySelector('#wifi-forget-btn').onclick = forgetWifi;
}

export function activateSystem(api) {
    currentApi = api;
    refresh(api);
    loadWifiConfig();
    loadGrovePins();
    interval = setInterval(() => refresh(api), 5000);
}

export function deactivateSystem() {
    if (interval) { clearInterval(interval); interval = null; }
    currentApi = null;
}

function refresh(api) {
    api.get('/api/system/info').then(render).catch(() => {});
}

function render(d) {
    setKvList('#sys-chip .kv-list', [
        ['Model',    d.chip?.model],
        ['Revision', d.chip?.revision],
        ['Cores',    d.chip?.cores],
        ['Freq',     (d.chip?.freq_mhz ?? '--') + ' MHz'],
    ]);

    const heapUsed = (d.heap?.total || 0) - (d.heap?.free || 0);
    const heapPct  = d.heap?.total ? (heapUsed / d.heap.total * 100) : 0;
    document.getElementById('sys-heap-text').textContent =
        `${fmt(heapUsed)} used / ${fmt(d.heap?.total)} total (${heapPct.toFixed(0)}%)`;
    const heapBar = document.getElementById('sys-heap-bar');
    heapBar.style.width = heapPct + '%';
    heapBar.className = 'bar-fill' + (heapPct > 80 ? ' danger' : heapPct > 60 ? ' warn' : '');

    const psramUsed = (d.psram?.total || 0) - (d.psram?.free || 0);
    const psramPct  = d.psram?.total ? (psramUsed / d.psram.total * 100) : 0;
    document.getElementById('sys-psram-text').textContent = d.psram?.total
        ? `${fmt(psramUsed)} used / ${fmt(d.psram.total)} total (${psramPct.toFixed(0)}%)`
        : 'Not available';
    document.getElementById('sys-psram-bar').style.width = psramPct + '%';

    setKvList('#sys-net .kv-list', [
        ['SSID',         d.wifi?.ssid],
        ['IP',           d.wifi?.ip],
        ['WiFi Clients', d.wifi?.clients],
    ]);

    const upSec = Math.floor((d.uptime_ms || 0) / 1000);
    const h = Math.floor(upSec / 3600), m = Math.floor((upSec % 3600) / 60), s = upSec % 60;
    setKvList('#sys-misc .kv-list', [
        ['Uptime',      `${h}h ${m}m ${s}s`],
        ['SDK',         d.sdk_version],
        ['Flash',       fmt(d.flash?.size)],
        ['Flash Speed', fmt(d.flash?.speed) + 'Hz'],
    ]);

    const otaVer = panel.querySelector('#ota-current');
    if (otaVer && d.firmware) otaVer.textContent = 'v' + d.firmware;

    const nameIn = panel.querySelector('#device-name-input');
    if (nameIn && d.name && !nameIn.value) nameIn.value = d.name;

    const localUrl = panel.querySelector('#device-local-url');
    if (localUrl && d.name) {
        const url = 'http://' + d.name + '.local/';
        localUrl.innerHTML = 'Local address: <a href="' + url + '" style="color:var(--accent);font-family:monospace">' + url + '</a>';
        localUrl.style.display = 'block';
    }

    const wifiLocalUrl = panel.querySelector('#wifi-local-url');
    if (wifiLocalUrl && d.name) {
        const url = 'http://' + d.name + '.local/';
        wifiLocalUrl.innerHTML = 'Local address once connected: <a href="' + url + '" style="color:var(--accent);font-family:monospace">' + url + '</a>';
        wifiLocalUrl.style.display = 'block';
    }

}

function identifyDevice() {
    if (!currentApi) return;
    const btn = panel.querySelector('#identify-btn');
    btn.disabled = true;
    fetch('http://' + currentApi.ip + '/api/system/identify', {method: 'POST'})
        .finally(() => { btn.disabled = false; });
}

function renameDevice() {
    if (!currentApi) return;
    const name = panel.querySelector('#device-name-input').value.trim();
    if (!name) { alert('Please enter a device name.'); return; }
    const hint = panel.querySelector('#device-hint');
    const btn  = panel.querySelector('#device-rename-btn');
    btn.disabled = true;
    hint.style.display = 'block';
    hint.textContent = 'Saving and restarting…';
    fetch('http://' + currentApi.ip + '/api/system/name', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({name: name})
    }).then(() => {
        hint.textContent = 'Device restarting. Reconnect at http://' + name + '.local/';
    }).catch(() => {
        hint.textContent = 'Device restarting. Reconnect at http://' + name + '.local/';
    });
}

function scanWifi() {
    if (!currentApi) return;
    const btn  = panel.querySelector('#wifi-scan-btn');
    const list = panel.querySelector('#wifi-scan-results');
    btn.disabled = true;
    btn.textContent = 'Scanning…';
    list.style.display = 'none';
    list.innerHTML = '';

    function poll() {
        fetch('http://' + currentApi.ip + '/api/wifi/scan')
            .then(r => r.status === 202 ? null : r.json())
            .then(networks => {
                if (!networks) { setTimeout(poll, 1500); return; }
                btn.disabled = false;
                btn.textContent = 'Scan';
                if (networks.length === 0) {
                    list.style.display = 'block';
                    list.innerHTML = '<div style="padding:8px;font-size:13px;color:var(--text-dim)">No networks found.</div>';
                    return;
                }
                list.style.display = 'block';
                list.innerHTML = networks.map(function(n) {
                    var sig = n.rssi > -60 ? '●●●' : n.rssi > -75 ? '●●○' : '●○○';
                    var lock = n.secure ? ' 🔒' : '';
                    var ssidEsc = n.ssid.replace(/&/g,'&amp;').replace(/"/g,'&quot;');
                    return '<div class="wifi-net" data-ssid="' + ssidEsc + '"' +
                        ' style="padding:8px 10px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;font-size:13px;border-bottom:1px solid var(--border)">' +
                        '<span>' + n.ssid + lock + '</span>' +
                        '<span style="color:var(--text-dim);font-size:11px;white-space:nowrap;margin-left:8px">' + sig + ' ' + n.rssi + ' dBm</span>' +
                        '</div>';
                }).join('');
                list.querySelectorAll('.wifi-net').forEach(function(el) {
                    el.addEventListener('pointerover',  function() { el.style.background = 'var(--surface2)'; });
                    el.addEventListener('pointerout',   function() { el.style.background = ''; });
                    el.onclick = function() {
                        panel.querySelector('#wifi-ssid').value = el.dataset.ssid;
                        panel.querySelector('#wifi-pass').focus();
                        list.style.display = 'none';
                    };
                });
            })
            .catch(function() { btn.disabled = false; btn.textContent = 'Scan'; });
    }
    poll();
}

function loadWifiConfig() {
    if (!currentApi) return;
    fetch('http://' + currentApi.ip + '/api/wifi/config')
        .then(r => r.json())
        .then(d => {
            const status  = panel.querySelector('#wifi-status');
            const forget  = panel.querySelector('#wifi-forget-btn');
            const hint    = panel.querySelector('#wifi-hint');
            const ssidIn  = panel.querySelector('#wifi-ssid');
            if (d.mode === 'sta') {
                status.textContent = 'Connected to "' + d.ssid + '" — ' + d.ip;
                forget.style.display = '';
                hint.style.display = 'none';
            } else {
                status.textContent = 'Hotspot mode — not connected to a network.';
                forget.style.display = 'none';
                if (d.saved_ssid) {
                    hint.style.display = 'block';
                    hint.textContent = 'Last saved network "' + d.saved_ssid + '" was unreachable.';
                }
            }
            if (d.ap_ssid) apSsidCached = d.ap_ssid;
            if (d.saved_ssid && !ssidIn.value) ssidIn.value = d.saved_ssid;
        })
        .catch(() => {
            const status = panel.querySelector('#wifi-status');
            if (status) status.textContent = 'Unable to load WiFi status.';
        });
}

function connectWifi() {
    if (!currentApi) return;
    const ssid = panel.querySelector('#wifi-ssid').value.trim();
    const pass = panel.querySelector('#wifi-pass').value;
    if (!ssid) { alert('Please enter a network name.'); return; }
    const btn  = panel.querySelector('#wifi-connect-btn');
    const hint = panel.querySelector('#wifi-hint');
    btn.disabled = true;
    hint.style.display = 'block';
    hint.textContent = 'Saving credentials and restarting…';
    fetch('http://' + currentApi.ip + '/api/wifi/connect', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ssid: ssid, password: pass})
    }).then(() => {
        hint.textContent = 'Device is restarting. Reconnect to your WiFi network and browse to http://esp32lab.local/';
    }).catch(() => {
        hint.textContent = 'Device is restarting. Reconnect to your WiFi network and browse to http://esp32lab.local/';
    });
}

function forgetWifi() {
    if (!currentApi) return;
    if (!confirm('Forget saved network? The device will restart in hotspot mode.')) return;
    const hint = panel.querySelector('#wifi-hint');
    hint.style.display = 'block';
    hint.textContent = 'Clearing credentials and restarting…';
    fetch('http://' + currentApi.ip + '/api/wifi/forget', {method: 'POST'})
        .then(() => { hint.textContent = 'Device restarted in hotspot mode. Connect to the "' + apSsidCached + '" WiFi network.'; })
        .catch(() => { hint.textContent = 'Device restarted in hotspot mode. Connect to the "' + apSsidCached + '" WiFi network.'; });
}

function loadGrovePins() {
    if (!currentApi) return;
    currentApi.get('/api/grove/config').then(r => {
        panel.querySelector('#grove-pin-d').value  = r.pin_d;
        panel.querySelector('#grove-pin-d2').value = r.pin_d2;
    }).catch(() => {});
}

function saveGrovePins() {
    if (!currentApi) return;
    const d  = parseInt(panel.querySelector('#grove-pin-d').value);
    const d2 = parseInt(panel.querySelector('#grove-pin-d2').value);
    if (isNaN(d) || isNaN(d2)) return;
    const hint = panel.querySelector('#grove-pin-hint');
    currentApi.post('/api/grove/pins', { pin_d: d, pin_d2: d2 }).then(r => {
        hint.textContent = `Saved: D=GPIO${r.pin_d}, D2=GPIO${r.pin_d2} — reconfigure sensor on the Sensors tab`;
        hint.style.display = 'block';
    }).catch(e => {
        hint.textContent = `Error: ${e.message}`;
        hint.style.display = 'block';
    });
}

function startOta() {
    if (!currentApi) return;
    const file = panel.querySelector('#ota-file').files[0];
    if (!file) return;

    const bar = panel.querySelector('#ota-bar');
    const msg = panel.querySelector('#ota-msg');
    const btn = panel.querySelector('#ota-btn');
    panel.querySelector('#ota-progress').style.display = 'block';
    bar.style.width = '0%';
    bar.className = 'bar-fill';
    msg.textContent = 'Uploading...';
    btn.disabled = true;

    const form = new FormData();
    form.append('firmware', file, file.name);
    const xhr = new XMLHttpRequest();

    xhr.upload.onprogress = e => {
        if (e.lengthComputable) {
            const pct = Math.round(e.loaded / e.total * 100);
            bar.style.width = pct + '%';
            msg.textContent = 'Uploading... ' + pct + '%';
        }
    };
    xhr.onload = () => {
        btn.disabled = false;
        if (xhr.status === 200) {
            bar.style.width = '100%';
            msg.textContent = 'Update complete — device is rebooting. Reconnect in a few seconds.';
        } else {
            bar.className = 'bar-fill danger';
            try { msg.textContent = 'Failed: ' + JSON.parse(xhr.responseText).error; }
            catch(e) { msg.textContent = 'Update failed.'; }
        }
    };
    xhr.onerror = () => {
        btn.disabled = false;
        bar.style.width = '100%';
        msg.textContent = 'Update complete — device is rebooting. Reconnect in a few seconds.';
    };
    xhr.open('POST', 'http://' + currentApi.ip + '/api/system/update');
    xhr.send(form);
}

function setKvList(selector, pairs) {
    const el = panel.querySelector(selector);
    el.innerHTML = pairs.map(([k, v]) =>
        `<div class="kv"><span class="label">${k}</span><span class="value">${v ?? '--'}</span></div>`
    ).join('');
}

function fmt(n) {
    if (n == null) return '?';
    if (n >= 1048576) return (n / 1048576).toFixed(1) + 'M';
    if (n >= 1024)    return (n / 1024).toFixed(0) + 'K';
    return String(n);
}
)WEBEND";

// ── js/tab-gpio.js ────────────────────────────────────────────────────────────

static const char FILE_GPIO_JS[] = R"WEBEND(let panel, currentApi;
const gpioLog = [];

export function initGpio(el) {
    panel = el;
    panel.innerHTML = `
        <div class="card">
            <h3>GPIO Control</h3>
            <div id="gpio-safe-hint" class="wiring-note" style="margin-bottom:12px">Connect to a device to see safe pins.</div>
            <div class="form-row">
                <label>Pin</label>
                <input type="number" id="gpio-pin" value="0" min="0" max="48" style="width:70px">
            </div>
            <div class="form-row">
                <label>Mode</label>
                <select id="gpio-mode">
                    <option value="input">INPUT</option>
                    <option value="output">OUTPUT</option>
                    <option value="input_pullup">INPUT_PULLUP</option>
                    <option value="input_pulldown">INPUT_PULLDOWN</option>
                </select>
                <button id="gpio-mode-btn" class="secondary">Set Mode</button>
            </div>
            <div class="form-row">
                <button id="gpio-read-btn" class="secondary">Read</button>
                <span id="gpio-read-val" style="font-family:monospace;font-size:16px">--</span>
            </div>
            <div class="form-row">
                <label>Write</label>
                <div class="toggle-group">
                    <button id="gpio-low" class="active">LOW</button>
                    <button id="gpio-high">HIGH</button>
                </div>
                <button id="gpio-write-btn">Write</button>
            </div>
        </div>
        <div class="card">
            <h3>Activity Log</h3>
            <button id="gpio-clear-btn" class="secondary" style="margin-bottom:8px;font-size:12px">Clear</button>
            <div class="log" id="gpio-log">
                <div style="color:var(--text-dim);padding:8px">No activity yet</div>
            </div>
        </div>
    `;

    let writeVal = 0;
    panel.querySelector('#gpio-low').onclick = () => {
        writeVal = 0;
        panel.querySelector('#gpio-low').classList.add('active');
        panel.querySelector('#gpio-high').classList.remove('active');
    };
    panel.querySelector('#gpio-high').onclick = () => {
        writeVal = 1;
        panel.querySelector('#gpio-high').classList.add('active');
        panel.querySelector('#gpio-low').classList.remove('active');
    };

    panel.querySelector('#gpio-mode-btn').onclick = () => {
        if (!currentApi) return;
        const pin = panel.querySelector('#gpio-pin').value;
        const mode = panel.querySelector('#gpio-mode').value;
        currentApi.post(`/api/gpio/${pin}/mode`, { mode })
            .then(r => addLog(`Pin ${r.pin} mode: ${r.mode}`))
            .catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#gpio-read-btn').onclick = () => {
        if (!currentApi) return;
        const pin = panel.querySelector('#gpio-pin').value;
        currentApi.get(`/api/gpio/${pin}`).then(r => {
            panel.querySelector('#gpio-read-val').textContent = r.value ? 'HIGH (1)' : 'LOW (0)';
            addLog(`Pin ${r.pin} = ${r.value ? 'HIGH' : 'LOW'}`);
        }).catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#gpio-write-btn').onclick = () => {
        if (!currentApi) return;
        const pin = panel.querySelector('#gpio-pin').value;
        currentApi.post(`/api/gpio/${pin}`, { value: writeVal })
            .then(r => addLog(`Pin ${r.pin} written ${r.value ? 'HIGH' : 'LOW'}`))
            .catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#gpio-clear-btn').onclick = () => { gpioLog.length = 0; renderLog(); };
}

export function activateGpio(api) {
    currentApi = api;
    api.get('/api/system/info').then(d => {
        const hint  = panel.querySelector('#gpio-safe-hint');
        const pinIn = panel.querySelector('#gpio-pin');
        const max   = d.gpio_max ?? 39;
        const board = d.board ?? 'ESP32';
        const safe  = d.gpio_safe || [];
        pinIn.max = max;
        if (safe.length > 0) {
            pinIn.value = safe[0];
            hint.textContent = 'Board: ' + board + '.  Safe pins: ' + safe.join(', ') + '.';
        } else {
            hint.textContent = 'Board: ' + board + '.  GPIO range: 0–' + max + '.';
        }
    }).catch(() => {});
}
export function deactivateGpio() { currentApi = null; }

function addLog(msg) {
    gpioLog.unshift({ time: new Date().toLocaleTimeString(), msg });
    if (gpioLog.length > 100) gpioLog.pop();
    renderLog();
}

function renderLog() {
    const el = panel.querySelector('#gpio-log');
    if (gpioLog.length === 0) {
        el.innerHTML = '<div style="color:var(--text-dim);padding:8px">No activity yet</div>';
        return;
    }
    el.innerHTML = gpioLog.map(e =>
        `<div class="log-entry"><span class="log-time">${e.time}</span>${e.msg}</div>`
    ).join('');
}
)WEBEND";

// ── js/tab-grove.js ───────────────────────────────────────────────────────────

static const char FILE_GROVE_JS[] = R"WEBEND(let panel, currentApi;
let sensors = [];
let activeSensorId = '';
let sse = null;
const groveLog = [];

export function initGrove(el) {
    panel = el;
    panel.innerHTML = `
        <div class="card">
            <h3>Sensor</h3>
            <div class="form-row">
                <label>Type</label>
                <select id="grove-type" style="flex:1">
                    <option value="">Connect to load sensors...</option>
                </select>
            </div>
            <div id="grove-safety" style="display:none"></div>
            <div id="grove-wiring" style="display:none;margin-top:10px"></div>
            <div class="form-row" style="margin-top:12px">
                <button id="grove-cfg-btn">Configure Sensor</button>
            </div>
        </div>
        <div class="card">
            <h3>Reading</h3>
            <div class="form-row">
                <button id="grove-read-btn" class="secondary">Read Once</button>
                <button id="grove-stream-btn" class="secondary">&#9654; Stream</button>
            </div>
            <div id="grove-value" style="font-size:22px;font-family:monospace;padding:12px 0;min-height:54px;color:var(--accent)">&#8212;</div>
            <div id="grove-digital-row" class="form-row" style="display:none">
                <label>Output</label>
                <div class="toggle-group">
                    <button id="grove-low-btn" class="active">LOW</button>
                    <button id="grove-high-btn">HIGH</button>
                </div>
                <button id="grove-write-btn">Write</button>
            </div>
            <div id="grove-pwm-row" class="form-row" style="display:none">
                <label>Duty</label>
                <input type="range" id="grove-duty" min="0" max="255" value="128" style="flex:1">
                <span id="grove-duty-val" style="font-family:monospace;min-width:28px">128</span>
                <button id="grove-pwm-btn">Set</button>
            </div>
            <div id="grove-fan-row" class="form-row" style="display:none">
                <label>Fan</label>
                <input type="range" id="grove-fan" min="0" max="100" value="100" style="flex:1">
                <span id="grove-fan-val" style="font-family:monospace;min-width:40px">100%</span>
                <button id="grove-fan-btn">Set</button>
            </div>
            <div id="grove-argb-row" class="form-row" style="display:none">
                <label>Colour</label>
                <input type="color" id="grove-argb-col" value="#0000ff" style="width:48px;height:36px;padding:0;border:none;background:none">
                <label>LEDs</label>
                <input type="number" id="grove-argb-cnt" min="1" max="300" value="24" style="width:72px">
                <button id="grove-argb-btn">Set</button>
            </div>
            <div id="grove-rotary-ctrl" style="display:none;margin-top:8px">
                <button id="grove-rotary-reset" class="secondary">Reset Steps</button>
            </div>
        </div>
        <div class="card">
            <h3>Log</h3>
            <button id="grove-log-clear" class="secondary" style="margin-bottom:8px;font-size:12px">Clear</button>
            <div class="log" id="grove-log-el">
                <div style="color:var(--text-dim);padding:8px">No readings yet</div>
            </div>
        </div>
    `;

    const sel = panel.querySelector('#grove-type');
    sel.addEventListener('change', () => updateSafetyInfo(sel.value));

    let digitalWriteVal = 0;
    panel.querySelector('#grove-low-btn').onclick = () => {
        digitalWriteVal = 0;
        panel.querySelector('#grove-low-btn').classList.add('active');
        panel.querySelector('#grove-high-btn').classList.remove('active');
    };
    panel.querySelector('#grove-high-btn').onclick = () => {
        digitalWriteVal = 1;
        panel.querySelector('#grove-high-btn').classList.add('active');
        panel.querySelector('#grove-low-btn').classList.remove('active');
    };

    const dutySlider = panel.querySelector('#grove-duty');
    const dutyValEl  = panel.querySelector('#grove-duty-val');
    dutySlider.addEventListener('input', () => { dutyValEl.textContent = dutySlider.value; });

    panel.querySelector('#grove-cfg-btn').onclick = () => {
        if (!currentApi) return;
        const id = sel.value;
        if (!id) return;
        stopStream();
        currentApi.post('/api/grove/configure', { sensor: id }).then(r => {
            activeSensorId = r.sensor;
            showOutputControls(r.sensor);
            addLog(`Configured: ${r.name} on GPIO${r.pin_d}${r.uses_d2 ? ' + GPIO' + r.pin_d2 : ''}`);
        }).catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#grove-read-btn').onclick = () => {
        if (!currentApi) return;
        currentApi.get('/api/grove/read').then(r => {
            renderReading(r);
            addLog(readingToString(r));
        }).catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#grove-stream-btn').onclick = () => {
        if (sse) stopStream(); else startStream();
    };

    panel.querySelector('#grove-write-btn').onclick = () => {
        if (!currentApi) return;
        currentApi.post('/api/grove/write', { value: digitalWriteVal })
            .then(() => addLog(`Wrote: ${digitalWriteVal ? 'HIGH' : 'LOW'}`))
            .catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#grove-pwm-btn').onclick = () => {
        if (!currentApi) return;
        const duty = parseInt(dutySlider.value);
        currentApi.post('/api/grove/write', { duty })
            .then(() => addLog(`PWM duty: ${duty}`))
            .catch(e => addLog(`Error: ${e.message}`));
    };

    const fanSlider = panel.querySelector('#grove-fan');
    fanSlider.addEventListener('input', () => {
        panel.querySelector('#grove-fan-val').textContent = fanSlider.value + '%';
    });
    panel.querySelector('#grove-fan-btn').onclick = () => {
        if (!currentApi) return;
        currentApi.post('/api/grove/write', { pct: parseInt(fanSlider.value) })
            .then(r => addLog(`Fan: ${r.fan_pct}% (duty ${r.duty})`))
            .catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#grove-argb-btn').onclick = () => {
        if (!currentApi) return;
        const hex = panel.querySelector('#grove-argb-col').value;
        const count = parseInt(panel.querySelector('#grove-argb-cnt').value) || 1;
        const rgb = { r: parseInt(hex.slice(1, 3), 16), g: parseInt(hex.slice(3, 5), 16), b: parseInt(hex.slice(5, 7), 16) };
        currentApi.post('/api/grove/write', { ...rgb, count })
            .then(() => addLog(`ARGB: ${hex} x ${count}`))
            .catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#grove-rotary-reset').onclick = () => {
        if (!currentApi) return;
        currentApi.post('/api/grove/rotary/reset', {}).then(() => {
            addLog('Rotary steps reset to 0');
            renderReading({ sensor: 'rotary', steps: 0, ts: Date.now() });
        }).catch(e => addLog(`Error: ${e.message}`));
    };

    panel.querySelector('#grove-log-clear').onclick = () => { groveLog.length = 0; renderLog(); };
}

export function activateGrove(api) {
    currentApi = api;
    loadSensors();
    api.get('/api/grove/config').then(r => {
        if (r.sensor) {
            activeSensorId = r.sensor;
            const sel = panel.querySelector('#grove-type');
            sel.value = r.sensor;
            updateSafetyInfo(r.sensor);
            showOutputControls(r.sensor);
        }
    }).catch(() => {});
}

export function deactivateGrove() { stopStream(); currentApi = null; }

function loadSensors() {
    if (!currentApi) return;
    currentApi.get('/api/grove/sensors').then(data => {
        sensors = data.sensors || [];
        const sel = panel.querySelector('#grove-type');
        sel.innerHTML = '<option value="">Select sensor type...</option>' +
            sensors.map(s =>
                `<option value="${s.id}">${s.name} (${s.vcc}${s.gpio_safe ? '' : ' &#9888;'})</option>`
            ).join('');
        if (activeSensorId) { sel.value = activeSensorId; updateSafetyInfo(activeSensorId); }
    }).catch(() => {});
}

function updateSafetyInfo(id) {
    const safetyEl = panel.querySelector('#grove-safety');
    const wiringEl = panel.querySelector('#grove-wiring');
    if (!id) { safetyEl.style.display = 'none'; wiringEl.style.display = 'none'; return; }
    const s = sensors.find(x => x.id === id);
    if (!s) return;

    const renderWiring = (d, d2) => {
        wiringEl.innerHTML = buildWiringGuide(id, d, d2);
        wiringEl.style.display = '';
    };

    if (currentApi) {
        currentApi.get('/api/grove/config')
            .then(r => renderWiring(r.pin_d, r.pin_d2))
            .catch(() => renderWiring(4, 3));
    } else {
        renderWiring(4, 3);
    }

    if (s.gpio_safe) {
        safetyEl.style.cssText = 'display:block;background:#0d2818;border:1px solid var(--green);border-radius:8px;padding:10px 12px;margin-top:8px;font-size:13px;line-height:1.5';
        safetyEl.innerHTML = `<strong style="color:var(--green)">&#10003; Safe &mdash; ${s.vcc}</strong><br>${s.voltage_note}`;
    } else {
        safetyEl.style.cssText = 'display:block;background:#2d1111;border:1px solid var(--red);border-radius:8px;padding:10px 12px;margin-top:8px;font-size:13px;line-height:1.5';
        safetyEl.innerHTML = `<strong style="color:var(--red)">&#9888; Caution &mdash; ${s.vcc}</strong><br>${s.voltage_note}`;
    }
}

function buildWiringGuide(sensorId, d, d2) {
    const G = {
        digital_in:  { rows: [['Signal', `GPIO${d}`], ['VCC', '3.3V'], ['GND', 'GND']], note: 'For a button: connect one end to GPIO, other to GND &mdash; use INPUT_PULLUP mode.' },
        digital_out: { rows: [['Output', `GPIO${d}`], ['VCC', '3.3V'], ['GND', 'GND']], note: 'Connect an LED with a 330&#937; resistor in series, or a relay/buzzer module.' },
        analog_in:   { rows: [['Signal', `GPIO${d}  &larr; MAX 3.3V!`], ['VCC', '3.3V'], ['GND', 'GND']], note: 'Never apply more than 3.3V to this pin. ADC reads 0 (0V) to 4095 (3.3V).' },
        pwm_out:     { rows: [['PWM Out', `GPIO${d}`], ['VCC', '3.3V'], ['GND', 'GND']], note: 'Connect an LED + 330&#937; resistor. Duty 0 = off, 128 = half, 255 = full on.' },
        dht11:       { rows: [['VCC', '3.3V'], ['GND', 'GND'], ['DATA', `GPIO${d}`]], note: 'Breakout modules include a pull-up &mdash; no extra parts needed. Bare sensor: add 10k&#937; from DATA to 3.3V.' },
        ds18b20:     { rows: [['VCC (red)', '3.3V'], ['GND (black)', 'GND'], ['DATA (yellow)', `GPIO${d}`]], note: 'Required: 4.7k&#937; resistor between 3.3V and DATA. Without it readings will fail.' },
        hcsr04:      { rows: [['VCC', '5V  (or 3.3V for HC-SR04P)'], ['GND', 'GND'], ['TRIG', `GPIO${d}  (direct)`], ['ECHO', `1k&#937; &rarr; GPIO${d2} &rarr; 2k&#937; &rarr; GND`]], note: 'HC-SR04P (3.3V): connect ECHO directly. Standard HC-SR04 (5V): ECHO pin outputs 5V &mdash; add a 1k&#937;/2k&#937; voltage divider to bring it to ~3.3V before the ESP32 pin. TRIG is safe to drive directly from 3.3V on both versions.' },
        rotary:      { rows: [['VCC (+)', '3.3V'], ['GND (&minus;)', 'GND'], ['CLK', `GPIO${d}`], ['DT', `GPIO${d2}`]], note: 'Works with KY-040 encoder modules. The SW (button) pin is not used here.' },
        pc_fan: {
            rows: [['Fan PWM',`GPIO${d} → 1kΩ → NPN base`],['NPN base','10kΩ → GND'],['NPN collector','MOSFET gate + 1kΩ → ATX 5V'],['NPN emitter','GND'],['MOSFET source','ATX GND'],['MOSFET drain','Fan GND (pin 1, black)'],['Fan +12V (pin 2)','ATX 12V via 2A fuse'],['SS34 diode','across fan, stripe → +12V'],['ESP32 GND','ATX GND']],
            note: 'Inverted logic: 100 % = GPIO LOW. Starting from 0 % the fan gets 100 % for 2 s. Below ~40 % 3-pin fans may stall. Do NOT connect the fan tach (yellow) wire to the ESP32. MOSFET: IRLB8721 (TO-220 front: G D S).'
        },
        argb: {
            rows: [['LED +5V','ATX 5V via 3–5A fuse'],['LED GND','ATX GND + ESP32 GND'],['LED DIN','330Ω ← 74AHCT125 pin 3 (1Y)'],['74AHCT125 pin 2 (1A)',`GPIO${d2}`],['74AHCT125 pin 1 (OE)','GND'],['74AHCT125 pin 14 / 7','ATX 5V / GND'],['1000µF capacitor','LED 5V ↔ GND']],
            note: 'Never power the LEDs from the ESP32. Each LED draws up to 60 mA on full white. Unused 74AHCT125 OE pins (4, 10, 13) → 5V, inputs (5, 9, 12) → GND. The fan pin is released while ARGB is active, so the fans run at 100 %.'
        },
    };
    const g = G[sensorId];
    if (!g) return '';
    const rows = g.rows.map(([sp, ep]) =>
        `<tr><td class="pin-sensor">${sp}</td><td style="color:var(--text-dim);padding:0 6px;font-family:sans-serif">&rarr;</td><td class="pin-esp32">${ep}</td></tr>`
    ).join('');
    return pcSvg(sensorId, d, d2) + `<table class="wiring-table"><thead><tr><th>Sensor Pin</th><th></th><th>ESP32 Pin</th></tr></thead><tbody>${rows}</tbody></table><div class="wiring-note">${g.note}</div>`;
}

function showOutputControls(id) {
    panel.querySelector('#grove-digital-row').style.display = id === 'digital_out' ? 'flex'  : 'none';
    panel.querySelector('#grove-pwm-row').style.display     = id === 'pwm_out'     ? 'flex'  : 'none';
    panel.querySelector('#grove-rotary-ctrl').style.display = id === 'rotary'      ? 'block' : 'none';
    panel.querySelector('#grove-fan-row').style.display  = id === 'pc_fan' ? 'flex' : 'none';
    panel.querySelector('#grove-argb-row').style.display = id === 'argb'   ? 'flex' : 'none';
}

function startStream() {
    if (!currentApi || sse) return;
    const btn = panel.querySelector('#grove-stream-btn');
    btn.textContent = '■ Stop';
    btn.classList.remove('secondary');
    sse = new EventSource(`http://${currentApi.ip}/api/grove/stream`);
    sse.addEventListener('reading', e => {
        const r = JSON.parse(e.data);
        renderReading(r);
        addLog(readingToString(r));
    });
    sse.onerror = () => { stopStream(); addLog('Stream closed'); };
    addLog('Stream started');
}

function stopStream() {
    if (sse) { sse.close(); sse = null; }
    const btn = panel.querySelector('#grove-stream-btn');
    if (btn) { btn.textContent = '▶ Stream'; btn.classList.add('secondary'); }
}

function renderReading(r) {
    const el = panel.querySelector('#grove-value');
    if (r.error) { el.style.color = 'var(--red)'; el.textContent = r.error; return; }
    el.style.color = 'var(--accent)';
    const parts = [];
    if (r.label        !== undefined) parts.push(r.label);
    else if (r.value   !== undefined) parts.push(`${r.value ? 'HIGH' : 'LOW'} (${r.value})`);
    if (r.raw          !== undefined) parts.push(`raw: ${r.raw}  ${r.voltage}V`);
    if (r.temperature  !== undefined) parts.push(`${r.temperature}&deg;C`);
    if (r.humidity     !== undefined) parts.push(`${r.humidity}% RH`);
    if (r.distance_cm  !== undefined) parts.push(`${r.distance_cm} cm`);
    if (r.steps        !== undefined) parts.push(`Steps: ${r.steps}`);
    if (r.fan_pct      !== undefined) parts.push(`Fan ${r.fan_pct}%  (duty ${r.duty}${r.kick ? ', kick-start' : ''})`);
    if (r.color        !== undefined) parts.push(`ARGB ${r.color} x ${r.leds}`);
    if (r.note         !== undefined) parts.push(r.note);
    el.innerHTML = parts.join('  |  ') || '&mdash;';
}

function readingToString(r) {
    if (r.error) return `Error: ${r.error}`;
    const parts = [];
    if (r.label        !== undefined) parts.push(r.label);
    else if (r.value   !== undefined) parts.push(`val=${r.value}`);
    if (r.raw          !== undefined) parts.push(`raw=${r.raw} ${r.voltage}V`);
    if (r.temperature  !== undefined) parts.push(`temp=${r.temperature}C`);
    if (r.humidity     !== undefined) parts.push(`hum=${r.humidity}%`);
    if (r.distance_cm  !== undefined) parts.push(`dist=${r.distance_cm}cm`);
    if (r.steps        !== undefined) parts.push(`steps=${r.steps}`);
    if (r.fan_pct      !== undefined) parts.push(`fan=${r.fan_pct}%`);
    if (r.color        !== undefined) parts.push(`argb=${r.color}x${r.leds}`);
    return parts.join(' ') || JSON.stringify(r);
}

function addLog(msg) {
    groveLog.unshift({ time: new Date().toLocaleTimeString(), msg });
    if (groveLog.length > 100) groveLog.pop();
    renderLog();
}

function renderLog() {
    const el = panel.querySelector('#grove-log-el');
    if (groveLog.length === 0) {
        el.innerHTML = '<div style="color:var(--text-dim);padding:8px">No readings yet</div>';
        return;
    }
    el.innerHTML = groveLog.map(e =>
        `<div class="log-entry"><span class="log-time">${e.time}</span>${e.msg}</div>`
    ).join('');
}

// ── PC fan / ARGB schematics ──────────────────────────────────────────────────
function pcArt(kind, d, d2) {
  var o = '';
  var T = function (x, y, s, a, fs, c, w) {
    return '<text x="' + x + '" y="' + y + '" font-family="Arial,Helvetica,sans-serif" font-size="' + (fs || 11) + '" fill="' + (c || '#1f2937') + '"' + (a ? ' text-anchor="' + a + '"' : '') + (w ? ' font-weight="' + w + '"' : '') + '>' + s + '</text>';
  };
  var W = function (c, p) {
    return '<path d="' + p + '" fill="none" stroke="#1b1f27" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>' +
      '<path d="' + p + '" fill="none" stroke="' + c + '" stroke-width="3.6" stroke-linecap="round" stroke-linejoin="round"/>';
  };
  var dot = function (x, y) { return '<circle cx="' + x + '" cy="' + y + '" r="4" fill="#c0c6cf" stroke="#4b5563" stroke-width="1.2"/>'; };
  var lead = function (p) { return '<path d="' + p + '" fill="none" stroke="#9ca3af" stroke-width="2.4" stroke-linecap="round"/>'; };
  var BC = { 0: '#111', 1: '#7c4a1e', 2: '#dc2626', 3: '#f97316', 4: '#eab308', 5: '#16a34a', 6: '#2563eb', 7: '#7c3aed', 8: '#6b7280', 9: '#f8fafc' };
  // resistor centred at x,y; v = vertical; bands = [d1,d2,mult]
  var res = function (x, y, v, bands, label) {
    var g = '<g transform="translate(' + x + ',' + y + ')' + (v ? ' rotate(90)' : '') + '">' +
      '<rect x="-24" y="-7" width="48" height="14" rx="7" fill="#e3c48f" stroke="#a8875a" stroke-width="1"/>';
    [-14, -7, 0].forEach(function (bx, i) { g += '<rect x="' + bx + '" y="-7" width="4" height="14" fill="' + BC[bands[i]] + '"/>'; });
    g += '<rect x="13" y="-7" width="4" height="14" fill="#c8a03c"/>';
    g += '<rect x="-20" y="-5" width="40" height="3" rx="1.5" fill="#fff" opacity=".35"/></g>';
    return g + (label ? T(v ? x + 14 : x, v ? y + 4 : y - 13, label, v ? null : 'middle', 11, '#1f2937', 'bold') : '');
  };
  var esp = function (x, y, pads) { // ESP32-C3 Super Mini, 104 x 200
    var s = '<rect x="' + x + '" y="' + y + '" width="104" height="200" rx="8" fill="#0f2742" stroke="#08172a" stroke-width="2"/>' +
      '<rect x="' + (x + 34) + '" y="' + (y - 10) + '" width="36" height="22" rx="5" fill="#cfd5dc" stroke="#7b8794"/>' +
      '<rect x="' + (x + 40) + '" y="' + (y - 4) + '" width="24" height="8" rx="3" fill="#59626e"/>' +
      '<rect x="' + (x + 24) + '" y="' + (y + 52) + '" width="56" height="56" rx="3" fill="#c4cad2" stroke="#8b95a1"/>' +
      T(x + 52, y + 78, 'ESP32', 'middle', 10, '#334155', 'bold') + T(x + 52, y + 92, 'C3', 'middle', 10, '#334155', 'bold') +
      '<rect x="' + (x + 30) + '" y="' + (y + 160) + '" width="44" height="14" rx="2" fill="#1e293b"/>' +
      T(x + 52, y + 192, 'Super Mini', 'middle', 9, '#9fb4cc');
    for (var i = 0; i < 8; i++) {
      var py = y + 26 + i * 18;
      s += '<circle cx="' + (x + 10) + '" cy="' + py + '" r="4.5" fill="#d4a93a" stroke="#8a6a17"/><circle cx="' + (x + 10) + '" cy="' + py + '" r="1.8" fill="#0f2742"/>';
      s += '<circle cx="' + (x + 94) + '" cy="' + py + '" r="4.5" fill="#d4a93a" stroke="#8a6a17"/><circle cx="' + (x + 94) + '" cy="' + py + '" r="1.8" fill="#0f2742"/>';
    }
    pads.forEach(function (p) { s += '<circle cx="' + (x + 94) + '" cy="' + (y + 26 + p[0] * 18) + '" r="5.5" fill="none" stroke="#38bdf8" stroke-width="2"/>' + T(x + 110, y + 20 + p[0] * 18, p[1], 'start', 10, '#0f172a', 'bold'); });
    return s;
  };
  var molex = function (x, y, used) { // 4-pin, pins at x+12, +34, +56, +78 ; wires leave downward from y+30
    var cols = ['#e11d48', '#111', '#111', '#facc15'];
    var s = '<rect x="' + x + '" y="' + y + '" width="90" height="30" rx="4" fill="#f4f1e6" stroke="#b9b29a" stroke-width="1.5"/>';
    for (var i = 0; i < 4; i++) {
      var px = x + 12 + i * 22;
      s += '<rect x="' + (px - 7) + '" y="' + (y + 6) + '" width="14" height="16" rx="5" fill="#e4dfcc" stroke="#b9b29a"/>';
      s += '<rect x="' + (px - 3) + '" y="' + (y + 10) + '" width="6" height="8" rx="2" fill="#555"/>';
      if (used.indexOf(i) < 0) s += W(cols[i], 'M' + px + ' ' + (y + 30) + ' v12') + '<circle cx="' + px + '" cy="' + (y + 44) + '" r="4" fill="#9ca3af"/>';
    }
    return s + T(x + 45, y - 18, 'ATX Molex (PSU)', 'middle', 11, '#1f2937', 'bold') +
      T(x + 45, y - 5, '5V · GND · GND · 12V', 'middle', 9, '#475569');
  };
  var fanBody = function (cx, cy, sz, argb) {
    var h = sz / 2, s = '<rect x="' + (cx - h) + '" y="' + (cy - h) + '" width="' + sz + '" height="' + sz + '" rx="10" fill="#22262d" stroke="#0d0f12" stroke-width="2"/>';
    [[-1, -1], [1, -1], [-1, 1], [1, 1]].forEach(function (c) { s += '<circle cx="' + (cx + c[0] * (h - 9)) + '" cy="' + (cy + c[1] * (h - 9)) + '" r="4" fill="#e8eef3"/>'; });
    s += '<circle cx="' + cx + '" cy="' + cy + '" r="' + (h - 8) + '" fill="#111318"/>';
    if (argb) {
      for (var k = 0; k < 18; k++) {
        var a = k / 18 * Math.PI * 2;
        s += '<circle cx="' + (cx + Math.cos(a) * (h - 12)).toFixed(1) + '" cy="' + (cy + Math.sin(a) * (h - 12)).toFixed(1) + '" r="3.2" fill="hsl(' + Math.round(k * 20) + ' 95% 60%)"/>';
      }
    }
    for (var b = 0; b < 7; b++) {
      s += '<path transform="rotate(' + (b * 360 / 7) + ' ' + cx + ' ' + cy + ')" d="M' + cx + ' ' + (cy - 12) + ' C' + (cx + 14) + ' ' + (cy - 22) + ' ' + (cx + 22) + ' ' + (cy - h + 26) + ' ' + (cx + 10) + ' ' + (cy - h + 16) + ' C' + (cx - 2) + ' ' + (cy - h + 22) + ' ' + (cx - 4) + ' ' + (cy - 26) + ' ' + cx + ' ' + (cy - 12) + 'Z" fill="' + (argb ? '#e5e7eb' : '#3a3f48') + '" opacity="' + (argb ? '.82' : '1') + '"/>';
    }
    return s + '<circle cx="' + cx + '" cy="' + cy + '" r="15" fill="#2c3038" stroke="#555"/>' + T(cx, cy + 4, argb ? 'ARGB' : '12V', 'middle', 9, '#cbd5e1', 'bold');
  };
  var mat = function (w, h) {
    return '<defs><pattern id="pcgrid" width="20" height="20" patternUnits="userSpaceOnUse"><path d="M20 0H0V20" fill="none" stroke="#d3dde6" stroke-width="1"/></pattern></defs>' +
      '<rect width="' + w + '" height="' + h + '" rx="10" fill="#e8eef3"/><rect width="' + w + '" height="' + h + '" rx="10" fill="url(#pcgrid)"/>';
  };

  if (kind === 'pc_fan') {
    o += mat(780, 480);
    // --- wires first (components are drawn on top) ---
    o += W('#e11d48', 'M552 72 V84 H420 V126');                       // ATX 5V -> R3
    o += W('#facc15', 'M618 72 V76 H650');                            // 12V -> fuse
    o += W('#facc15', 'M700 76 H752 V360 H650 V290');                 // fuse -> fan red terminal
    o += W('#111', 'M574 72 V96 H534 V452');                          // ATX GND -> GND rail
    o += W('#38bdf8', 'M116 206 H170');                               // GPIO -> R1
    o += W('#38bdf8', 'M234 206 H300 V230');                          // R1 -> junction / R2
    o += W('#38bdf8', 'M300 206 H318 V266 H351 V246');                // junction -> base
    o += W('#94a3b8', 'M300 294 V452');                               // R2 -> GND
    o += W('#94a3b8', 'M363 246 V452');                               // emitter -> GND
    o += W('#f97316', 'M339 246 V290 H462 V246');                     // collector -> gate
    o += W('#e11d48', 'M420 190 V290');                               // R3 -> gate node
    o += W('#a78bfa', 'M478 246 V330 H600 V290');                     // drain -> fan black terminal
    o += W('#94a3b8', 'M494 246 V452');                               // source -> GND
    o += W('#94a3b8', 'M116 296 H150 V452');                          // ESP GND -> GND rail
    o += W('#475569', 'M150 452 H534');                               // GND rail
    // --- components ---
    o += esp(22, 162, [[1, 'GPIO' + d], [6, 'GND']]);
    o += T(74, 142, 'ESP32', 'middle', 12, '#1f2937', 'bold');
    o += lead('M170 206 H234') + res(202, 206, false, [1, 0, 2], '1 kΩ');
    o += lead('M300 230 V294') + res(300, 262, true, [1, 0, 3], '') + T(290, 266, '10 kΩ', 'end', 11, '#1f2937', 'bold') + dot(300, 206);
    // TO-92 BC547, flat face towards viewer: C B E
    o += lead('M339 196 V246 M351 196 V246 M363 196 V246');
    o += '<path d="M326 156 H376 V198 H326 Z" fill="#26292f" stroke="#0d0f12" stroke-width="1.5"/><path d="M326 156 Q351 140 376 156" fill="#33373e" stroke="#0d0f12" stroke-width="1.5"/>';
    o += T(351, 176, 'BC547', 'middle', 9, '#e5e7eb', 'bold') + T(351, 191, 'C  B  E', 'middle', 8.5, '#9ca3af');
    o += T(351, 132, 'NPN', 'middle', 11, '#1f2937', 'bold');
    // R3
    o += lead('M420 126 V190') + res(420, 158, true, [1, 0, 2], '') + T(410, 162, '1 kΩ', 'end', 11, '#1f2937', 'bold') + dot(420, 290);
    // TO-220 IRLB8721: G D S
    o += lead('M462 196 V246 M478 196 V246 M494 196 V246');
    o += '<rect x="446" y="104" width="64" height="44" rx="2" fill="#cfd5dc" stroke="#7b8794" stroke-width="1.5"/><circle cx="478" cy="122" r="8" fill="#e8eef3" stroke="#7b8794" stroke-width="1.5"/>';
    o += '<rect x="448" y="144" width="60" height="54" rx="2" fill="#26292f" stroke="#0d0f12" stroke-width="1.5"/>';
    o += T(478, 166, 'IRLB', 'middle', 9.5, '#e5e7eb', 'bold') + T(478, 178, '8721', 'middle', 9.5, '#e5e7eb', 'bold') + T(478, 192, 'G D S', 'middle', 8.5, '#9ca3af');
    o += T(478, 98, 'MOSFET', 'middle', 11, '#1f2937', 'bold');
    o += dot(339, 246) + dot(351, 246) + dot(363, 246) + dot(462, 246) + dot(478, 246) + dot(494, 246);
    // Molex + fuse
    o += molex(540, 42, [0, 1, 3]);
    o += '<rect x="650" y="66" width="50" height="20" rx="4" fill="#9ca3af" stroke="#4b5563" stroke-width="1.5"/><rect x="662" y="70" width="26" height="12" rx="2" fill="#e5e7eb"/>' + T(675, 80, '2 A', 'middle', 9, '#111', 'bold') + T(675, 60, 'fuse', 'middle', 9);
    // fan + terminal block + diode
    o += fanBody(668, 178, 130, false);
    o += W('#111', 'M616 236 C612 250 600 252 600 262') + W('#e11d48', 'M622 238 C626 250 650 252 650 262') + W('#facc15', 'M628 238 C640 248 700 250 700 262');
    o += '<rect x="584" y="262" width="132" height="28" rx="3" fill="#15803d" stroke="#0b4f26" stroke-width="1.5"/>';
    [600, 650, 700].forEach(function (x) { o += '<circle cx="' + x + '" cy="276" r="8" fill="#cbd5e1" stroke="#64748b"/><path d="M' + (x - 5) + ' 276 h10" stroke="#475569" stroke-width="2"/>'; });
    o += T(600, 304, 'GND', 'middle', 9) + T(650, 304, '+12V', 'middle', 9) + T(700, 304, 'tach: NC', 'middle', 9, '#b45309', 'bold');
    o += lead('M600 320 H650');
    o += '<rect x="612" y="314" width="26" height="12" rx="3" fill="#1f2329"/><rect x="632" y="314" width="4" height="12" fill="#cbd5e1"/>' + dot(600, 320) + dot(650, 320);
    o += T(592, 316, 'SS34 diode', 'end', 9.5, '#1f2937', 'bold');
    o += T(668, 106, '3-pin case fans (daisy-chain)', 'middle', 11, '#1f2937', 'bold');
    o += T(342, 472, 'GND (common: PSU + ESP32)', 'middle', 10, '#475569', 'bold');
    return '<svg viewBox="0 0 780 480" width="100%" style="display:block;margin:4px 0 8px;border-radius:10px">' + o + '</svg>';
  }

  if (kind === 'argb') {
    o += mat(780, 420);
    // wires
    o += W('#38bdf8', 'M116 242 H190 V300 H289 V282');               // GPIO(D2) -> 74AHCT125 pin 2
    o += W('#94a3b8', 'M269 282 V380');                               // pin 1 OE -> GND
    o += W('#94a3b8', 'M389 282 V380');                               // pin 7 -> GND
    o += W('#e11d48', 'M269 218 V150 H548');                          // pin 14 VCC -> 5V
    o += W('#22d3ee', 'M309 282 V320 H400');                          // pin 3 1Y -> R
    o += W('#22d3ee', 'M460 320 H486 V246 H590');                     // R -> DIN
    o += W('#e11d48', 'M548 96 V226 H590');                           // 5V -> LED 5V
    o += W('#475569', 'M570 96 V120 H776 V402 H578 V380');            // ATX GND -> GND rail (around the fan)
    o += W('#94a3b8', 'M590 266 H578 V380');                          // LED GND -> rail
    o += W('#94a3b8', 'M116 314 H150 V380');                          // ESP GND
    o += W('#475569', 'M150 380 H578');                               // GND rail
    o += W('#e11d48', 'M565 186 V192 H548') + W('#475569', 'M575 140 V120');   // cap legs
    // components
    o += esp(22, 162, [[3, 'GPIO' + d2], [7, 'GND']]);
    o += T(74, 142, 'ESP32', 'middle', 12, '#1f2937', 'bold');
    // DIP-14 74AHCT125, notch left; bottom 1..7 L->R, top 14..8 L->R
    for (var i = 0; i < 7; i++) {
      var px = 269 + i * 20;
      o += '<rect x="' + (px - 3) + '" y="270" width="6" height="12" fill="#cbd5e1" stroke="#64748b" stroke-width=".8"/>';
      o += '<rect x="' + (px - 3) + '" y="218" width="6" height="12" fill="#cbd5e1" stroke="#64748b" stroke-width=".8"/>';
    }
    o += '<rect x="254" y="228" width="150" height="44" rx="3" fill="#26292f" stroke="#0d0f12" stroke-width="1.5"/><path d="M254 242 a8 8 0 0 1 0 16" fill="#e8eef3"/>';
    for (i = 0; i < 7; i++) {
      o += T(269 + i * 20, 266, String(i + 1), 'middle', 8, '#cbd5e1') + T(269 + i * 20, 241, String(14 - i), 'middle', 8, '#cbd5e1');
    }
    o += T(329, 254, '74AHCT125', 'middle', 10.5, '#e5e7eb', 'bold');
    o += T(350, 200, 'level shifter 3.3V → 5V', 'middle', 10.5, '#1f2937', 'bold');
    o += dot(269, 282) + dot(289, 282) + dot(309, 282) + dot(389, 282) + dot(269, 218);
    o += T(198, 296, 'pin 2', 'start', 9, '#1f2937') + T(276, 344, 'pin 1 → GND', 'start', 9) + T(312, 316, 'pin 3', 'start', 9) + T(396, 344, 'pin 7 GND', 'start', 9) + T(276, 166, 'pin 14 → 5V', 'start', 9);
    o += lead('M400 320 H460') + res(430, 320, false, [3, 3, 1], '330 Ω');
    // electrolytic capacitor across 5V / GND, close to the Molex
    o += '<rect x="557" y="140" width="22" height="46" rx="4" fill="#1d4ed8" stroke="#1e3a8a"/><rect x="557" y="140" width="22" height="7" rx="3" fill="#cbd5e1"/><rect x="571" y="150" width="5" height="32" fill="#93c5fd" opacity=".85"/>' + T(573, 166, '–', 'middle', 10, '#1e3a8a', 'bold');
    o += T(586, 160, '1000 µF', 'start', 9.5, '#1f2937', 'bold') + dot(548, 192) + dot(575, 120);
    // ARGB fan + connector
    o += fanBody(705, 246, 128, true);
    o += '<rect x="586" y="214" width="20" height="62" rx="3" fill="#f4f1e6" stroke="#b9b29a" stroke-width="1.5"/>';
    o += '<rect x="591" y="220" width="10" height="10" rx="2" fill="#555"/><rect x="591" y="240" width="10" height="10" rx="2" fill="#555"/><rect x="591" y="250" width="10" height="6" rx="2" fill="#d6d0bc"/><rect x="591" y="260" width="10" height="10" rx="2" fill="#555"/>';
    o += T(610, 228, '5V', 'start', 9, '#1f2937', 'bold') + T(610, 249, 'DIN', 'start', 9, '#1f2937', 'bold') + T(610, 269, 'GND', 'start', 9, '#1f2937', 'bold');
    o += T(705, 170, 'ARGB fans / strips', 'middle', 11, '#1f2937', 'bold');
    o += molex(536, 66, [0, 1]);
    o += '<rect x="538" y="104" width="20" height="26" rx="4" fill="#9ca3af" stroke="#4b5563"/>' + T(548, 121, '3A', 'middle', 8.5, '#111', 'bold');
    o += T(335, 402, 'GND (common: PSU + ESP32 + LEDs)', 'middle', 10, '#475569', 'bold');
    return '<svg viewBox="0 0 780 420" width="100%" style="display:block;margin:4px 0 8px;border-radius:10px">' + o + '</svg>';
  }
  return '';
}

function pcSvg(kind, d, d2) {
  const art = pcArt(kind, d, d2);
  if (!art) return '';
  const btn = (v, label, on) => `<button type="button" data-v="${v}" style="padding:5px 12px;font-size:12px;border-radius:6px;border:1px solid #475569;background:${on ? '#38bdf8' : '#334155'};color:${on ? '#0f172a' : '#f1f5f9'};font-weight:600;cursor:pointer" onclick="var w=this.parentNode.parentNode,v=this.dataset.v;w.querySelectorAll('[data-pane]').forEach(function(p){p.style.display=p.dataset.pane===v?'':'none'});this.parentNode.querySelectorAll('button').forEach(function(b){var a=b.dataset.v===v;b.style.background=a?'#38bdf8':'#334155';b.style.color=a?'#0f172a':'#f1f5f9'})">${label}</button>`;
  return `<div><div style="display:flex;gap:6px;margin:6px 0">${btn('art', 'Wiring picture', true)}${btn('sch', 'Schematic', false)}</div>` +
    `<div data-pane="art">${art}<div style="font-size:11px;color:#94a3b8;margin:-4px 0 8px">Pin positions on the board are illustrative &mdash; use the GPIO labels printed on your board.</div></div>` +
    `<div data-pane="sch" style="display:none">${pcSch(kind, d, d2)}</div></div>`;
}

function pcSch(kind, d, d2) {
  const C = { y12:'#fbbf24', v5:'#f87171', sig:'#38bdf8', sw:'#a78bfa', g:'#94a3b8', fg:'#f1f5f9', bg:'#1e293b', dim:'#94a3b8' };
  const w = (c, path) => `<path d="${path}" fill="none" stroke="${c}" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/>`;
  const t = (x, y, s, o) => `<text x="${x}" y="${y}" fill="${(o&&o.c)||C.fg}" font-size="${(o&&o.fs)||11}" font-family="monospace"${o&&o.a?` text-anchor="${o.a}"`:''}>${s}</text>`;
  const box = (x, y, wd, h) => `<rect x="${x}" y="${y}" width="${wd}" height="${h}" rx="3" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>`;
  const dot = (x, y) => `<circle cx="${x}" cy="${y}" r="3" fill="${C.fg}"/>`;
  let b = '';
  if (kind === 'pc_fan') {
    b += box(10,190,100,80) + t(20,210,'ESP32',{fs:12}) + t(20,232,'GPIO'+d,{c:C.sig}) + t(20,258,'GND',{c:C.dim});
    b += w(C.sig,'M110 228 H150 M186 228 H262') + box(150,220,36,16) + t(168,212,'1k',{a:'middle'});
    b += w(C.sig,'M226 228 V246') + box(219,246,14,32) + w(C.g,'M226 278 V330') + t(238,268,'10k') + dot(226,228);
    b += `<circle cx="282" cy="228" r="22" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>`;
    b += w(C.fg,'M270 214 V242 M270 220 L290 206 M270 236 L290 250') + t(310,224,'NPN') + t(310,238,'BC547',{c:C.dim});
    b += w(C.fg,'M290 206 V160') + w(C.g,'M290 250 V330');
    b += w(C.v5,'M290 160 H412') + dot(346,160) + w(C.v5,'M300 62 H380 M346 62 V88 M346 126 V160') + box(339,88,14,38) + t(360,112,'1k');
    b += t(300,52,'ATX +5V',{c:C.v5});
    b += w(C.fg,'M412 140 V180 M420 136 V184 M420 142 H452 M420 160 H452 M420 178 H452') + t(464,156,'MOSFET') + t(464,170,'IRLB8721',{c:C.dim});
    b += t(400,194,'G',{c:C.dim}) + t(456,132,'D',{c:C.dim}) + t(456,196,'S',{c:C.dim});
    b += w(C.sw,'M452 142 V110 H630') + w(C.g,'M452 160 V330') + dot(560,110);
    b += t(470,22,'ATX +12V',{c:C.y12}) + w(C.y12,'M470 32 H492 M520 32 H630 M560 32 V52 M630 32 V58') + `<rect x="492" y="25" width="28" height="14" rx="7" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>` + t(506,56,'2A',{a:'middle',c:C.dim}) + dot(560,32);
    b += `<circle cx="560" cy="74" r="21" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>` + t(560,79,'FAN',{a:'middle',fs:10}) + w(C.sw,'M560 95 V110');
    b += `<path d="M630 60 L621 80 H639 Z" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>` + w(C.fg,'M620 60 H640') + w(C.sw,'M630 80 V110') + t(600,140,'SS34',{c:C.dim});
    b += w(C.g,'M110 258 H130 V330 H640') + t(640,348,'ATX GND',{a:'end',c:C.dim});
    return `<svg viewBox="0 0 660 356" width="100%" style="display:block;margin:4px 0 8px">${b}</svg>`;
  }
  if (kind === 'argb') {
    b += box(10,80,100,74) + t(20,100,'ESP32',{fs:12}) + t(20,122,'GPIO'+d2,{c:C.sig}) + t(20,146,'GND',{c:C.dim});
    b += w(C.sig,'M110 118 H200') + box(200,80,120,80) + t(260,98,'74AHCT125',{a:'middle'});
    b += t(208,122,'1A',{c:C.dim}) + t(208,150,'OE',{c:C.dim}) + t(312,122,'1Y',{a:'end',c:C.dim});
    b += w(C.g,'M200 146 H186 V200') + w(C.v5,'M260 80 V40') + w(C.g,'M260 160 V200');
    b += w(C.sig,'M320 118 H352 M388 118 H450') + box(352,110,36,16) + t(370,104,'330',{a:'middle'});
    b += box(450,80,110,74) + t(460,100,'ARGB',{fs:12}) + t(460,122,'DIN',{c:C.dim}) + t(460,142,'fans + strips',{c:C.dim,fs:10});
    b += w(C.v5,'M505 80 V40') + w(C.g,'M505 154 V200');
    b += w(C.v5,'M600 40 V112') + w(C.fg,'M588 112 H612 M588 120 H612') + w(C.g,'M600 120 V200') + t(616,104,'1000') + t(616,118,'uF');
    b += t(120,30,'ATX +5V (fuse 3-5A)',{c:C.v5}) + w(C.v5,'M120 40 H640') + dot(260,40) + dot(505,40);
    b += w(C.g,'M110 146 H130 V200 H640') + dot(186,200) + dot(260,200) + dot(505,200) + t(640,218,'ATX GND',{a:'end',c:C.dim});
    return `<svg viewBox="0 0 660 224" width="100%" style="display:block;margin:4px 0 8px">${b}</svg>`;
  }
  return '';
}
)WEBEND";

// ── manifest.json ─────────────────────────────────────────────────────────────

static const char FILE_MANIFEST[] = R"WEBEND({
    "name": "ESP32 Lab",
    "short_name": "ESP32Lab",
    "description": "Browser sensor control for ESP32",
    "start_url": "/",
    "display": "standalone",
    "background_color": "#0f172a",
    "theme_color": "#38bdf8",
    "icons": []
}
)WEBEND";

// ── Simple sensor page (served at /) ─────────────────────────────────────────

static const char FILE_SIMPLE_HTML[] = R"WEBEND(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Lab</title>
<style>
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
:root{--bg:#0f172a;--s:#1e293b;--s2:#334155;--bd:#475569;--tx:#f1f5f9;--dim:#94a3b8;--ac:#38bdf8;--gn:#4ade80;--rd:#f87171}
html,body{min-height:100%;background:var(--bg);color:var(--tx);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif}
body{display:flex;flex-direction:column}
header{background:var(--s);border-bottom:2px solid var(--ac);padding:12px 16px;display:flex;align-items:center;gap:10px;flex-shrink:0}
.brand{font-size:18px;font-weight:700;color:var(--ac);flex:1}
#conn{font-size:13px;color:var(--dim)}
#conn.ok{color:var(--gn)}#conn.err{color:var(--rd)}
header a{font-size:12px;color:var(--dim);text-decoration:none;padding:4px 10px;border:1px solid var(--bd);border-radius:6px}
#wrap{padding:14px;max-width:520px;width:100%;margin:0 auto}
.card{background:var(--s);border:1px solid var(--bd);border-radius:10px;padding:16px;margin-bottom:14px}
.lbl{font-size:11px;text-transform:uppercase;letter-spacing:1px;color:var(--ac);margin-bottom:8px}
select{width:100%;padding:10px 12px;border-radius:8px;border:1px solid var(--bd);background:var(--s2);color:var(--tx);font-size:15px;margin-bottom:12px}
#safety{font-size:13px;border-radius:8px;padding:10px 12px;margin-bottom:12px;line-height:1.5}
#safety.safe{background:#0d2818;border:1px solid var(--gn)}
#safety.warn{background:#2d1111;border:1px solid var(--rd)}
#wiring{margin:0 0 14px}
.wt{width:100%;border-collapse:collapse;font-size:13px}
.wt td{padding:7px 8px;border-bottom:1px solid var(--s2);font-family:monospace}
.wt tr:last-child td{border-bottom:none}
.sp{color:#fbbf24}.arr{color:var(--dim);padding:0 6px;font-family:sans-serif}.ep{color:var(--gn)}
.wnote{font-size:12px;color:var(--dim);padding:8px 10px;background:var(--s2);border-radius:6px;line-height:1.6;margin-top:6px}
#val{font-size:28px;font-family:monospace;color:var(--ac);padding:14px 0;min-height:60px;word-break:break-all}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:10px}
button{padding:10px 20px;border-radius:8px;border:none;background:var(--ac);color:#0f172a;font-weight:700;font-size:14px;cursor:pointer;white-space:nowrap}
button:active{opacity:.8}
.sec{background:var(--s2);color:var(--tx);border:1px solid var(--bd);font-weight:600}
.tg{background:var(--s2);color:var(--dim);border:none;border-radius:0;padding:10px 18px;font-size:14px;font-weight:600;cursor:pointer}
.tg.act{background:var(--ac);color:#0f172a}
.tg-grp{display:flex;border-radius:8px;overflow:hidden;border:1px solid var(--bd)}
input[type=range]{flex:1;accent-color:var(--ac)}
.hidden{display:none!important}
.log{max-height:200px;overflow-y:auto;font-size:12px;font-family:monospace;margin-top:8px}
.log-entry{padding:5px 8px;border-bottom:1px solid var(--s2);line-height:1.4;display:flex;gap:6px;align-items:baseline}
.log-entry:last-child{border-bottom:none}
.log-time{color:var(--dim);flex-shrink:0}
.log-msg{flex:1}
.log-dur{color:var(--ac);flex-shrink:0;font-size:11px}
</style>
</head>
<body>
<header>
  <span class="brand">ESP32 Lab</span>
  <span id="conn">Connecting&hellip;</span>
  <a href="/advanced">Advanced &rsaquo;</a>
</header>
<div id="wrap">
  <div class="card">
    <div class="lbl">Sensor</div>
    <select id="sel"><option value="">Connecting&hellip;</option></select>
    <div id="safety" class="hidden"></div>
    <div id="wiring" class="hidden"></div>
    <button id="cfg">Configure Sensor</button>
  </div>
  <div class="card">
    <div id="val">&mdash;</div>
    <div class="row">
      <button id="rd">Read Once</button>
      <button id="st" class="sec">&#9654; Stream</button>
      <button id="ct" class="sec">&#9203; Poll</button>
    </div>
    <div id="dout" class="row hidden">
      <div class="tg-grp">
        <button id="lo" class="tg act">LOW</button>
        <button id="hi" class="tg">HIGH</button>
      </div>
      <button id="wr">Write</button>
    </div>
    <div id="pwm" class="row hidden">
      <span style="font-size:13px;color:var(--dim)">Duty</span>
      <input type="range" id="duty" min="0" max="255" value="128">
      <span id="dv" style="font-family:monospace;min-width:28px">128</span>
      <button id="pset">Set</button>
    </div>
    <div id="fanrow" class="row hidden">
      <span style="font-size:13px;color:var(--dim)">Fan</span>
      <input type="range" id="fpct" min="0" max="100" value="100">
      <span id="fv" style="font-family:monospace;min-width:40px">100%</span>
      <button id="fset">Set</button>
    </div>
    <div id="argbrow" class="row hidden">
      <input type="color" id="acol" value="#0000ff" style="width:48px;height:38px;padding:0;border:none;background:none">
      <span style="font-size:13px;color:var(--dim)">LEDs</span>
      <input type="number" id="acnt" min="1" max="300" value="24" style="width:72px;padding:9px;border-radius:8px;border:1px solid var(--bd);background:var(--s2);color:var(--tx)">
      <button id="aset">Set</button>
    </div>
  </div>
  <div class="card">
    <div style="display:flex;align-items:center;gap:8px;margin-bottom:4px">
      <span class="lbl" style="margin:0;flex:1">Log</span>
      <button id="logclr" class="sec" style="padding:4px 10px;font-size:12px">Clear</button>
    </div>
    <div id="log" class="log"><div style="color:var(--dim);padding:8px">No readings yet</div></div>
  </div>
</div>
<script>
'use strict';
let sensors=[], dval=0, pinD=4, pinD2=3;
let log=[], contInterval=null, pollInterval=null, prevKey=null, stateStart=null;
const $=id=>document.getElementById(id);
const conn=$('conn'), sel=$('sel'), safety=$('safety'), wiring=$('wiring');
const val=$('val'), dout=$('dout'), pwm=$('pwm'), logEl=$('log');

function renderLog() {
  if (!log.length) { logEl.innerHTML='<div style="color:var(--dim);padding:8px">No readings yet</div>'; return; }
  const now = Date.now();
  logEl.innerHTML = log.map(e => {
    const dur = e.live
      ? `<span class="log-dur">${((now-e.ms)/1000).toFixed(1)}s ▶</span>`
      : e.durS !== undefined ? `<span class="log-dur">${e.durS}s</span>` : '';
    return `<div class="log-entry"><span class="log-time">${e.t}</span><span class="log-msg">${e.msg}</span>${dur}</div>`;
  }).join('');
}
function addLog(msg) {
  log.unshift({t:new Date().toLocaleTimeString(), msg, ms:Date.now()});
  if (log.length>100) log.pop();
  renderLog();
}
$('logclr').onclick=()=>{ log=[]; prevKey=null; stateStart=null; renderLog(); };

async function apiFetch(path, opts) {
  const r = await fetch(path, {signal:AbortSignal.timeout(5000), ...opts});
  if (!r.ok) throw new Error((await r.json().catch(()=>({}))).error||r.status);
  return r.json();
}

async function init() {
  try {
    await apiFetch('/api/system/info');
    conn.textContent='Connected'; conn.className='ok';
    const d = await apiFetch('/api/grove/sensors');
    sensors = d.sensors||[];
    sel.innerHTML = '<option value="">Select sensor type…</option>' +
      sensors.map(s=>`<option value="${s.id}">${s.name}</option>`).join('');
    const cfg = await apiFetch('/api/grove/config').catch(()=>({}));
    if (cfg.pin_d)  { pinD=cfg.pin_d; pinD2=cfg.pin_d2; }
    if (cfg.sensor) { sel.value=cfg.sensor; onSel(); }
  } catch(e) {
    conn.textContent='Not connected'; conn.className='err';
    sel.innerHTML='<option value="">Could not connect</option>';
  }
}

sel.addEventListener('change', onSel);
function onSel() {
  const id=sel.value, s=sensors.find(x=>x.id===id);
  if (!s) { safety.className='hidden'; wiring.className='hidden'; return; }
  safety.className = s.gpio_safe ? 'safe' : 'warn';
  safety.innerHTML = s.gpio_safe
    ? `<strong style="color:var(--gn)">&#10003; Safe &mdash; ${s.vcc}</strong><br>${s.voltage_note}`
    : `<strong style="color:var(--rd)">&#9888; Caution &mdash; ${s.vcc}</strong><br>${s.voltage_note}`;
  wiring.innerHTML = buildWiring(id);
  wiring.className = '';
}

$('cfg').addEventListener('click', async () => {
  const id=sel.value; if (!id) return;
  stopStream();
  try {
    const r = await apiFetch('/api/grove/configure', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body:JSON.stringify({sensor:id})
    });
    pinD=r.pin_d; pinD2=r.pin_d2;
    wiring.innerHTML=buildWiring(id);
    dout.classList.toggle('hidden', id!=='digital_out');
    pwm.classList.toggle('hidden', id!=='pwm_out');
    $('fanrow').classList.toggle('hidden', id!=='pc_fan');
    $('argbrow').classList.toggle('hidden', id!=='argb');
    val.textContent='Ready'; val.style.color='var(--ac)';
    addLog('Configured: '+r.name+' on GPIO'+r.pin_d+(r.uses_d2?' + GPIO'+r.pin_d2:''));
  } catch(e) { val.textContent='Error: '+e.message; val.style.color='var(--rd)'; addLog('Error: '+e.message); }
});

$('rd').addEventListener('click', async () => {
  try { const r=await apiFetch('/api/grove/read'); showVal(r); addLog(valText(r)); }
  catch(e) { val.textContent='Error: '+e.message; val.style.color='var(--rd)'; addLog('Error: '+e.message); }
});

// ── Stream: state-change continuous, 300ms, live timer ───────────────────────
$('st').addEventListener('click', ()=>{ contInterval ? stopStream() : startStream(); });
function startStream() {
  stopPoll();
  prevKey=null; stateStart=null;
  $('st').textContent='■ Stop'; $('st').classList.remove('sec');
  addLog('Streaming…');
  contInterval=setInterval(doStream, 300);
}
function stopStream() {
  if (contInterval) { clearInterval(contInterval); contInterval=null; }
  if (log.length && log[0].live) { log[0].live=false; log[0].durS=((Date.now()-log[0].ms)/1000).toFixed(1); }
  prevKey=null;
  $('st').textContent='► Stream'; $('st').classList.add('sec');
  renderLog();
}
async function doStream() {
  try {
    const r=await apiFetch('/api/grove/read'); showVal(r);
    const key=stateKey(r), now=Date.now();
    if (prevKey===null) {
      prevKey=key;
      log.unshift({t:new Date().toLocaleTimeString(), msg:valText(r), ms:now, live:true});
      if (log.length>100) log.pop();
    } else if (key!==prevKey) {
      if (log.length && log[0].live) { log[0].live=false; log[0].durS=((now-log[0].ms)/1000).toFixed(1); }
      prevKey=key;
      log.unshift({t:new Date().toLocaleTimeString(), msg:valText(r), ms:now, live:true});
      if (log.length>100) log.pop();
    }
    renderLog();
  } catch(e) { addLog('Error: '+e.message); }
}
function stateKey(r) {
  if (r.error)                     return 'err';
  if (r.value!==undefined)         return r.value?'1':'0';
  if (r.label!==undefined)         return r.label;
  if (r.distance_cm!==undefined)   return String(Math.round(r.distance_cm/2)*2);
  if (r.temperature!==undefined)   return r.temperature+','+r.humidity;
  if (r.raw!==undefined)           return String(Math.round(r.raw/50)*50);
  return valText(r);
}

// ── Poll: simple read every second, logs every reading ───────────────────────
$('ct').addEventListener('click', ()=>{ pollInterval ? stopPoll() : startPoll(); });
function startPoll() {
  stopStream();
  $('ct').textContent='■ Stop'; $('ct').classList.remove('sec');
  addLog('Polling…');
  pollInterval=setInterval(doPoll, 1000);
}
function stopPoll() {
  if (pollInterval) { clearInterval(pollInterval); pollInterval=null; }
  $('ct').textContent='⏱ Poll'; $('ct').classList.add('sec');
}
async function doPoll() {
  try { const r=await apiFetch('/api/grove/read'); showVal(r); addLog(valText(r)); }
  catch(e) { addLog('Error: '+e.message); }
}

$('lo').onclick=()=>{ dval=0; $('lo').classList.add('act'); $('hi').classList.remove('act'); };
$('hi').onclick=()=>{ dval=1; $('hi').classList.add('act'); $('lo').classList.remove('act'); };
$('wr').onclick=async()=>{
  try { await apiFetch('/api/grove/write',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({value:dval})}); }
  catch(e) { val.textContent='Error: '+e.message; }
};
$('duty').addEventListener('input', ()=>{ $('dv').textContent=$('duty').value; });
$('pset').onclick=async()=>{
  try { await apiFetch('/api/grove/write',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({duty:+$('duty').value})}); }
  catch(e) { val.textContent='Error: '+e.message; }
};

$('fpct').addEventListener('input', ()=>{ $('fv').textContent=$('fpct').value+'%'; });
$('fset').onclick=async()=>{
  try { const r=await apiFetch('/api/grove/write',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({pct:+$('fpct').value})}); showVal(r); addLog(valText(r)); }
  catch(e) { val.textContent='Error: '+e.message; }
};
$('aset').onclick=async()=>{
  const h=$('acol').value, c=Math.max(1,+$('acnt').value||1);
  const body={r:parseInt(h.slice(1,3),16),g:parseInt(h.slice(3,5),16),b:parseInt(h.slice(5,7),16),count:c};
  try { await apiFetch('/api/grove/write',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}); addLog('ARGB '+h+' × '+c); }
  catch(e) { val.textContent='Error: '+e.message; }
};

function valText(r) {
  if (r.error) return 'Error: '+r.error;
  const p=[];
  if (r.temperature!==undefined) p.push(r.temperature+'°C');
  if (r.humidity!==undefined)    p.push(r.humidity+'% RH');
  if (r.distance_cm!==undefined) p.push(r.distance_cm+' cm');
  if (r.fan_pct!==undefined)     p.push('Fan '+r.fan_pct+'% (duty '+r.duty+(r.kick?', kick-start':'')+')');
  if (r.color!==undefined)       p.push('ARGB '+r.color+' × '+r.leds);
  if (r.label!==undefined)       p.push(r.label);
  else if (r.value!==undefined)  p.push(r.value?'HIGH':'LOW');
  if (r.raw!==undefined)         p.push('raw: '+r.raw);
  if (r.note!==undefined)        p.push(r.note);
  return p.join(' | ')||'—';
}
function showVal(r) {
  val.style.color = r.error ? 'var(--rd)' : 'var(--ac)';
  val.textContent = r.error ? r.error : (valText(r)||'—');
}

function buildWiring(id) {
  const W = {
    digital_in: {
      rows:[['Signal',`GPIO${pinD}`],['VCC','3.3V'],['GND','GND']],
      note:'Button: one end to GPIO, other to GND (use INPUT_PULLUP). PIR: OUT→GPIO, VCC→5V, GND→GND. Jumper selects single/continuous fire; trims set sensitivity and delay.'
    },
    digital_out:{
      rows:[['Output',`GPIO${pinD}`],['VCC','3.3V'],['GND','GND']],
      note:'LED: GPIO → 330Ω → LED → GND. Or connect a relay or buzzer module.'
    },
    analog_in:{
      rows:[['Signal',`GPIO${pinD}`],['VCC','3.3V'],['GND','GND']],
      note:'MAX 3.3V on this pin. ADC reads 0–4095 (0V–3.3V).'
    },
    pwm_out:{
      rows:[['PWM',`GPIO${pinD}`],['VCC','3.3V'],['GND','GND']],
      note:'LED: GPIO → 330Ω → LED → GND. Duty 0 = off, 128 = 50%, 255 = full.'
    },
    dht11:{
      rows:[['VCC','3.3V'],['GND','GND'],['DATA',`GPIO${pinD}`]],
      note:'Uses a single-wire serial protocol — one wire carries all data. Breakout modules include a pull-up resistor, no extras needed.'
    },
    hcsr04:{
      rows:[['VCC','5V (USB/VIN)'],['GND','GND'],['TRIG',`GPIO${pinD}`],['ECHO',`GPIO${pinD2} via divider`]],
      note:'5V ECHO needs a voltage divider: 10kΩ from ECHO to GPIO, 20kΩ from GPIO to GND. TRIG connects direct (3.3V logic is fine).'
    },
    ds18b20:{
      rows:[['VCC','3.3V'],['GND','GND'],['DATA',`GPIO${pinD}`]],
      note:'4.7kΩ pull-up between DATA and 3.3V required.'
    },
    rotary:{
      rows:[['VCC','3.3V'],['GND','GND'],['CLK',`GPIO${pinD}`],['DT',`GPIO${pinD2}`]],
      note:'KY-040 module. SW (button) pin is not used here.'
    },
    pc_fan:{
      rows:[['Fan PWM',`GPIO${pinD} → 1kΩ → NPN base`],['NPN base','10kΩ → GND'],['NPN collector','MOSFET gate + 1kΩ → ATX 5V'],['NPN emitter','GND'],['MOSFET source','ATX GND'],['MOSFET drain','Fan GND (pin 1, black)'],['Fan +12V (pin 2)','ATX 12V via 2A fuse'],['SS34 diode','across fan, stripe → +12V'],['ESP32 GND','ATX GND']],
      note:'Inverted logic: 100 % = GPIO LOW. Starting from 0 % the fan gets 100 % for 2 s. Below ~40 % 3-pin fans may stall. Do NOT connect the fan tach (yellow) wire to the ESP32. MOSFET: IRLB8721 (TO-220 front: G D S).'
    },
    argb:{
      rows:[['LED +5V','ATX 5V via 3–5A fuse'],['LED GND','ATX GND + ESP32 GND'],['LED DIN','330Ω ← 74AHCT125 pin 3 (1Y)'],['74AHCT125 pin 2 (1A)',`GPIO${pinD2}`],['74AHCT125 pin 1 (OE)','GND'],['74AHCT125 pin 14 / 7','ATX 5V / GND'],['1000µF capacitor','LED 5V ↔ GND']],
      note:'Never power the LEDs from the ESP32. Each LED draws up to 60 mA on full white. Unused 74AHCT125 OE pins (4, 10, 13) → 5V, inputs (5, 9, 12) → GND. The fan pin is released while ARGB is active, so the fans run at 100 %.'
    },
  };
  const g=W[id]; if (!g) return '';
  const rows=g.rows.map(([sp,ep])=>`<tr><td class="sp">${sp}</td><td class="arr">→</td><td class="ep">${ep}</td></tr>`).join('');
  return pcSvg(id,pinD,pinD2)+`<table class="wt"><tbody>${rows}</tbody></table><div class="wnote">${g.note}</div>`;
}

function pcArt(kind, d, d2) {
  var o = '';
  var T = function (x, y, s, a, fs, c, w) {
    return '<text x="' + x + '" y="' + y + '" font-family="Arial,Helvetica,sans-serif" font-size="' + (fs || 11) + '" fill="' + (c || '#1f2937') + '"' + (a ? ' text-anchor="' + a + '"' : '') + (w ? ' font-weight="' + w + '"' : '') + '>' + s + '</text>';
  };
  var W = function (c, p) {
    return '<path d="' + p + '" fill="none" stroke="#1b1f27" stroke-width="6" stroke-linecap="round" stroke-linejoin="round"/>' +
      '<path d="' + p + '" fill="none" stroke="' + c + '" stroke-width="3.6" stroke-linecap="round" stroke-linejoin="round"/>';
  };
  var dot = function (x, y) { return '<circle cx="' + x + '" cy="' + y + '" r="4" fill="#c0c6cf" stroke="#4b5563" stroke-width="1.2"/>'; };
  var lead = function (p) { return '<path d="' + p + '" fill="none" stroke="#9ca3af" stroke-width="2.4" stroke-linecap="round"/>'; };
  var BC = { 0: '#111', 1: '#7c4a1e', 2: '#dc2626', 3: '#f97316', 4: '#eab308', 5: '#16a34a', 6: '#2563eb', 7: '#7c3aed', 8: '#6b7280', 9: '#f8fafc' };
  // resistor centred at x,y; v = vertical; bands = [d1,d2,mult]
  var res = function (x, y, v, bands, label) {
    var g = '<g transform="translate(' + x + ',' + y + ')' + (v ? ' rotate(90)' : '') + '">' +
      '<rect x="-24" y="-7" width="48" height="14" rx="7" fill="#e3c48f" stroke="#a8875a" stroke-width="1"/>';
    [-14, -7, 0].forEach(function (bx, i) { g += '<rect x="' + bx + '" y="-7" width="4" height="14" fill="' + BC[bands[i]] + '"/>'; });
    g += '<rect x="13" y="-7" width="4" height="14" fill="#c8a03c"/>';
    g += '<rect x="-20" y="-5" width="40" height="3" rx="1.5" fill="#fff" opacity=".35"/></g>';
    return g + (label ? T(v ? x + 14 : x, v ? y + 4 : y - 13, label, v ? null : 'middle', 11, '#1f2937', 'bold') : '');
  };
  var esp = function (x, y, pads) { // ESP32-C3 Super Mini, 104 x 200
    var s = '<rect x="' + x + '" y="' + y + '" width="104" height="200" rx="8" fill="#0f2742" stroke="#08172a" stroke-width="2"/>' +
      '<rect x="' + (x + 34) + '" y="' + (y - 10) + '" width="36" height="22" rx="5" fill="#cfd5dc" stroke="#7b8794"/>' +
      '<rect x="' + (x + 40) + '" y="' + (y - 4) + '" width="24" height="8" rx="3" fill="#59626e"/>' +
      '<rect x="' + (x + 24) + '" y="' + (y + 52) + '" width="56" height="56" rx="3" fill="#c4cad2" stroke="#8b95a1"/>' +
      T(x + 52, y + 78, 'ESP32', 'middle', 10, '#334155', 'bold') + T(x + 52, y + 92, 'C3', 'middle', 10, '#334155', 'bold') +
      '<rect x="' + (x + 30) + '" y="' + (y + 160) + '" width="44" height="14" rx="2" fill="#1e293b"/>' +
      T(x + 52, y + 192, 'Super Mini', 'middle', 9, '#9fb4cc');
    for (var i = 0; i < 8; i++) {
      var py = y + 26 + i * 18;
      s += '<circle cx="' + (x + 10) + '" cy="' + py + '" r="4.5" fill="#d4a93a" stroke="#8a6a17"/><circle cx="' + (x + 10) + '" cy="' + py + '" r="1.8" fill="#0f2742"/>';
      s += '<circle cx="' + (x + 94) + '" cy="' + py + '" r="4.5" fill="#d4a93a" stroke="#8a6a17"/><circle cx="' + (x + 94) + '" cy="' + py + '" r="1.8" fill="#0f2742"/>';
    }
    pads.forEach(function (p) { s += '<circle cx="' + (x + 94) + '" cy="' + (y + 26 + p[0] * 18) + '" r="5.5" fill="none" stroke="#38bdf8" stroke-width="2"/>' + T(x + 110, y + 20 + p[0] * 18, p[1], 'start', 10, '#0f172a', 'bold'); });
    return s;
  };
  var molex = function (x, y, used) { // 4-pin, pins at x+12, +34, +56, +78 ; wires leave downward from y+30
    var cols = ['#e11d48', '#111', '#111', '#facc15'];
    var s = '<rect x="' + x + '" y="' + y + '" width="90" height="30" rx="4" fill="#f4f1e6" stroke="#b9b29a" stroke-width="1.5"/>';
    for (var i = 0; i < 4; i++) {
      var px = x + 12 + i * 22;
      s += '<rect x="' + (px - 7) + '" y="' + (y + 6) + '" width="14" height="16" rx="5" fill="#e4dfcc" stroke="#b9b29a"/>';
      s += '<rect x="' + (px - 3) + '" y="' + (y + 10) + '" width="6" height="8" rx="2" fill="#555"/>';
      if (used.indexOf(i) < 0) s += W(cols[i], 'M' + px + ' ' + (y + 30) + ' v12') + '<circle cx="' + px + '" cy="' + (y + 44) + '" r="4" fill="#9ca3af"/>';
    }
    return s + T(x + 45, y - 18, 'ATX Molex (PSU)', 'middle', 11, '#1f2937', 'bold') +
      T(x + 45, y - 5, '5V · GND · GND · 12V', 'middle', 9, '#475569');
  };
  var fanBody = function (cx, cy, sz, argb) {
    var h = sz / 2, s = '<rect x="' + (cx - h) + '" y="' + (cy - h) + '" width="' + sz + '" height="' + sz + '" rx="10" fill="#22262d" stroke="#0d0f12" stroke-width="2"/>';
    [[-1, -1], [1, -1], [-1, 1], [1, 1]].forEach(function (c) { s += '<circle cx="' + (cx + c[0] * (h - 9)) + '" cy="' + (cy + c[1] * (h - 9)) + '" r="4" fill="#e8eef3"/>'; });
    s += '<circle cx="' + cx + '" cy="' + cy + '" r="' + (h - 8) + '" fill="#111318"/>';
    if (argb) {
      for (var k = 0; k < 18; k++) {
        var a = k / 18 * Math.PI * 2;
        s += '<circle cx="' + (cx + Math.cos(a) * (h - 12)).toFixed(1) + '" cy="' + (cy + Math.sin(a) * (h - 12)).toFixed(1) + '" r="3.2" fill="hsl(' + Math.round(k * 20) + ' 95% 60%)"/>';
      }
    }
    for (var b = 0; b < 7; b++) {
      s += '<path transform="rotate(' + (b * 360 / 7) + ' ' + cx + ' ' + cy + ')" d="M' + cx + ' ' + (cy - 12) + ' C' + (cx + 14) + ' ' + (cy - 22) + ' ' + (cx + 22) + ' ' + (cy - h + 26) + ' ' + (cx + 10) + ' ' + (cy - h + 16) + ' C' + (cx - 2) + ' ' + (cy - h + 22) + ' ' + (cx - 4) + ' ' + (cy - 26) + ' ' + cx + ' ' + (cy - 12) + 'Z" fill="' + (argb ? '#e5e7eb' : '#3a3f48') + '" opacity="' + (argb ? '.82' : '1') + '"/>';
    }
    return s + '<circle cx="' + cx + '" cy="' + cy + '" r="15" fill="#2c3038" stroke="#555"/>' + T(cx, cy + 4, argb ? 'ARGB' : '12V', 'middle', 9, '#cbd5e1', 'bold');
  };
  var mat = function (w, h) {
    return '<defs><pattern id="pcgrid" width="20" height="20" patternUnits="userSpaceOnUse"><path d="M20 0H0V20" fill="none" stroke="#d3dde6" stroke-width="1"/></pattern></defs>' +
      '<rect width="' + w + '" height="' + h + '" rx="10" fill="#e8eef3"/><rect width="' + w + '" height="' + h + '" rx="10" fill="url(#pcgrid)"/>';
  };

  if (kind === 'pc_fan') {
    o += mat(780, 480);
    // --- wires first (components are drawn on top) ---
    o += W('#e11d48', 'M552 72 V84 H420 V126');                       // ATX 5V -> R3
    o += W('#facc15', 'M618 72 V76 H650');                            // 12V -> fuse
    o += W('#facc15', 'M700 76 H752 V360 H650 V290');                 // fuse -> fan red terminal
    o += W('#111', 'M574 72 V96 H534 V452');                          // ATX GND -> GND rail
    o += W('#38bdf8', 'M116 206 H170');                               // GPIO -> R1
    o += W('#38bdf8', 'M234 206 H300 V230');                          // R1 -> junction / R2
    o += W('#38bdf8', 'M300 206 H318 V266 H351 V246');                // junction -> base
    o += W('#94a3b8', 'M300 294 V452');                               // R2 -> GND
    o += W('#94a3b8', 'M363 246 V452');                               // emitter -> GND
    o += W('#f97316', 'M339 246 V290 H462 V246');                     // collector -> gate
    o += W('#e11d48', 'M420 190 V290');                               // R3 -> gate node
    o += W('#a78bfa', 'M478 246 V330 H600 V290');                     // drain -> fan black terminal
    o += W('#94a3b8', 'M494 246 V452');                               // source -> GND
    o += W('#94a3b8', 'M116 296 H150 V452');                          // ESP GND -> GND rail
    o += W('#475569', 'M150 452 H534');                               // GND rail
    // --- components ---
    o += esp(22, 162, [[1, 'GPIO' + d], [6, 'GND']]);
    o += T(74, 142, 'ESP32', 'middle', 12, '#1f2937', 'bold');
    o += lead('M170 206 H234') + res(202, 206, false, [1, 0, 2], '1 kΩ');
    o += lead('M300 230 V294') + res(300, 262, true, [1, 0, 3], '') + T(290, 266, '10 kΩ', 'end', 11, '#1f2937', 'bold') + dot(300, 206);
    // TO-92 BC547, flat face towards viewer: C B E
    o += lead('M339 196 V246 M351 196 V246 M363 196 V246');
    o += '<path d="M326 156 H376 V198 H326 Z" fill="#26292f" stroke="#0d0f12" stroke-width="1.5"/><path d="M326 156 Q351 140 376 156" fill="#33373e" stroke="#0d0f12" stroke-width="1.5"/>';
    o += T(351, 176, 'BC547', 'middle', 9, '#e5e7eb', 'bold') + T(351, 191, 'C  B  E', 'middle', 8.5, '#9ca3af');
    o += T(351, 132, 'NPN', 'middle', 11, '#1f2937', 'bold');
    // R3
    o += lead('M420 126 V190') + res(420, 158, true, [1, 0, 2], '') + T(410, 162, '1 kΩ', 'end', 11, '#1f2937', 'bold') + dot(420, 290);
    // TO-220 IRLB8721: G D S
    o += lead('M462 196 V246 M478 196 V246 M494 196 V246');
    o += '<rect x="446" y="104" width="64" height="44" rx="2" fill="#cfd5dc" stroke="#7b8794" stroke-width="1.5"/><circle cx="478" cy="122" r="8" fill="#e8eef3" stroke="#7b8794" stroke-width="1.5"/>';
    o += '<rect x="448" y="144" width="60" height="54" rx="2" fill="#26292f" stroke="#0d0f12" stroke-width="1.5"/>';
    o += T(478, 166, 'IRLB', 'middle', 9.5, '#e5e7eb', 'bold') + T(478, 178, '8721', 'middle', 9.5, '#e5e7eb', 'bold') + T(478, 192, 'G D S', 'middle', 8.5, '#9ca3af');
    o += T(478, 98, 'MOSFET', 'middle', 11, '#1f2937', 'bold');
    o += dot(339, 246) + dot(351, 246) + dot(363, 246) + dot(462, 246) + dot(478, 246) + dot(494, 246);
    // Molex + fuse
    o += molex(540, 42, [0, 1, 3]);
    o += '<rect x="650" y="66" width="50" height="20" rx="4" fill="#9ca3af" stroke="#4b5563" stroke-width="1.5"/><rect x="662" y="70" width="26" height="12" rx="2" fill="#e5e7eb"/>' + T(675, 80, '2 A', 'middle', 9, '#111', 'bold') + T(675, 60, 'fuse', 'middle', 9);
    // fan + terminal block + diode
    o += fanBody(668, 178, 130, false);
    o += W('#111', 'M616 236 C612 250 600 252 600 262') + W('#e11d48', 'M622 238 C626 250 650 252 650 262') + W('#facc15', 'M628 238 C640 248 700 250 700 262');
    o += '<rect x="584" y="262" width="132" height="28" rx="3" fill="#15803d" stroke="#0b4f26" stroke-width="1.5"/>';
    [600, 650, 700].forEach(function (x) { o += '<circle cx="' + x + '" cy="276" r="8" fill="#cbd5e1" stroke="#64748b"/><path d="M' + (x - 5) + ' 276 h10" stroke="#475569" stroke-width="2"/>'; });
    o += T(600, 304, 'GND', 'middle', 9) + T(650, 304, '+12V', 'middle', 9) + T(700, 304, 'tach: NC', 'middle', 9, '#b45309', 'bold');
    o += lead('M600 320 H650');
    o += '<rect x="612" y="314" width="26" height="12" rx="3" fill="#1f2329"/><rect x="632" y="314" width="4" height="12" fill="#cbd5e1"/>' + dot(600, 320) + dot(650, 320);
    o += T(592, 316, 'SS34 diode', 'end', 9.5, '#1f2937', 'bold');
    o += T(668, 106, '3-pin case fans (daisy-chain)', 'middle', 11, '#1f2937', 'bold');
    o += T(342, 472, 'GND (common: PSU + ESP32)', 'middle', 10, '#475569', 'bold');
    return '<svg viewBox="0 0 780 480" width="100%" style="display:block;margin:4px 0 8px;border-radius:10px">' + o + '</svg>';
  }

  if (kind === 'argb') {
    o += mat(780, 420);
    // wires
    o += W('#38bdf8', 'M116 242 H190 V300 H289 V282');               // GPIO(D2) -> 74AHCT125 pin 2
    o += W('#94a3b8', 'M269 282 V380');                               // pin 1 OE -> GND
    o += W('#94a3b8', 'M389 282 V380');                               // pin 7 -> GND
    o += W('#e11d48', 'M269 218 V150 H548');                          // pin 14 VCC -> 5V
    o += W('#22d3ee', 'M309 282 V320 H400');                          // pin 3 1Y -> R
    o += W('#22d3ee', 'M460 320 H486 V246 H590');                     // R -> DIN
    o += W('#e11d48', 'M548 96 V226 H590');                           // 5V -> LED 5V
    o += W('#475569', 'M570 96 V120 H776 V402 H578 V380');            // ATX GND -> GND rail (around the fan)
    o += W('#94a3b8', 'M590 266 H578 V380');                          // LED GND -> rail
    o += W('#94a3b8', 'M116 314 H150 V380');                          // ESP GND
    o += W('#475569', 'M150 380 H578');                               // GND rail
    o += W('#e11d48', 'M565 186 V192 H548') + W('#475569', 'M575 140 V120');   // cap legs
    // components
    o += esp(22, 162, [[3, 'GPIO' + d2], [7, 'GND']]);
    o += T(74, 142, 'ESP32', 'middle', 12, '#1f2937', 'bold');
    // DIP-14 74AHCT125, notch left; bottom 1..7 L->R, top 14..8 L->R
    for (var i = 0; i < 7; i++) {
      var px = 269 + i * 20;
      o += '<rect x="' + (px - 3) + '" y="270" width="6" height="12" fill="#cbd5e1" stroke="#64748b" stroke-width=".8"/>';
      o += '<rect x="' + (px - 3) + '" y="218" width="6" height="12" fill="#cbd5e1" stroke="#64748b" stroke-width=".8"/>';
    }
    o += '<rect x="254" y="228" width="150" height="44" rx="3" fill="#26292f" stroke="#0d0f12" stroke-width="1.5"/><path d="M254 242 a8 8 0 0 1 0 16" fill="#e8eef3"/>';
    for (i = 0; i < 7; i++) {
      o += T(269 + i * 20, 266, String(i + 1), 'middle', 8, '#cbd5e1') + T(269 + i * 20, 241, String(14 - i), 'middle', 8, '#cbd5e1');
    }
    o += T(329, 254, '74AHCT125', 'middle', 10.5, '#e5e7eb', 'bold');
    o += T(350, 200, 'level shifter 3.3V → 5V', 'middle', 10.5, '#1f2937', 'bold');
    o += dot(269, 282) + dot(289, 282) + dot(309, 282) + dot(389, 282) + dot(269, 218);
    o += T(198, 296, 'pin 2', 'start', 9, '#1f2937') + T(276, 344, 'pin 1 → GND', 'start', 9) + T(312, 316, 'pin 3', 'start', 9) + T(396, 344, 'pin 7 GND', 'start', 9) + T(276, 166, 'pin 14 → 5V', 'start', 9);
    o += lead('M400 320 H460') + res(430, 320, false, [3, 3, 1], '330 Ω');
    // electrolytic capacitor across 5V / GND, close to the Molex
    o += '<rect x="557" y="140" width="22" height="46" rx="4" fill="#1d4ed8" stroke="#1e3a8a"/><rect x="557" y="140" width="22" height="7" rx="3" fill="#cbd5e1"/><rect x="571" y="150" width="5" height="32" fill="#93c5fd" opacity=".85"/>' + T(573, 166, '–', 'middle', 10, '#1e3a8a', 'bold');
    o += T(586, 160, '1000 µF', 'start', 9.5, '#1f2937', 'bold') + dot(548, 192) + dot(575, 120);
    // ARGB fan + connector
    o += fanBody(705, 246, 128, true);
    o += '<rect x="586" y="214" width="20" height="62" rx="3" fill="#f4f1e6" stroke="#b9b29a" stroke-width="1.5"/>';
    o += '<rect x="591" y="220" width="10" height="10" rx="2" fill="#555"/><rect x="591" y="240" width="10" height="10" rx="2" fill="#555"/><rect x="591" y="250" width="10" height="6" rx="2" fill="#d6d0bc"/><rect x="591" y="260" width="10" height="10" rx="2" fill="#555"/>';
    o += T(610, 228, '5V', 'start', 9, '#1f2937', 'bold') + T(610, 249, 'DIN', 'start', 9, '#1f2937', 'bold') + T(610, 269, 'GND', 'start', 9, '#1f2937', 'bold');
    o += T(705, 170, 'ARGB fans / strips', 'middle', 11, '#1f2937', 'bold');
    o += molex(536, 66, [0, 1]);
    o += '<rect x="538" y="104" width="20" height="26" rx="4" fill="#9ca3af" stroke="#4b5563"/>' + T(548, 121, '3A', 'middle', 8.5, '#111', 'bold');
    o += T(335, 402, 'GND (common: PSU + ESP32 + LEDs)', 'middle', 10, '#475569', 'bold');
    return '<svg viewBox="0 0 780 420" width="100%" style="display:block;margin:4px 0 8px;border-radius:10px">' + o + '</svg>';
  }
  return '';
}

function pcSvg(kind, d, d2) {
  const art = pcArt(kind, d, d2);
  if (!art) return '';
  const btn = (v, label, on) => `<button type="button" data-v="${v}" style="padding:5px 12px;font-size:12px;border-radius:6px;border:1px solid #475569;background:${on ? '#38bdf8' : '#334155'};color:${on ? '#0f172a' : '#f1f5f9'};font-weight:600;cursor:pointer" onclick="var w=this.parentNode.parentNode,v=this.dataset.v;w.querySelectorAll('[data-pane]').forEach(function(p){p.style.display=p.dataset.pane===v?'':'none'});this.parentNode.querySelectorAll('button').forEach(function(b){var a=b.dataset.v===v;b.style.background=a?'#38bdf8':'#334155';b.style.color=a?'#0f172a':'#f1f5f9'})">${label}</button>`;
  return `<div><div style="display:flex;gap:6px;margin:6px 0">${btn('art', 'Wiring picture', true)}${btn('sch', 'Schematic', false)}</div>` +
    `<div data-pane="art">${art}<div style="font-size:11px;color:#94a3b8;margin:-4px 0 8px">Pin positions on the board are illustrative &mdash; use the GPIO labels printed on your board.</div></div>` +
    `<div data-pane="sch" style="display:none">${pcSch(kind, d, d2)}</div></div>`;
}

function pcSch(kind, d, d2) {
  const C = { y12:'#fbbf24', v5:'#f87171', sig:'#38bdf8', sw:'#a78bfa', g:'#94a3b8', fg:'#f1f5f9', bg:'#1e293b', dim:'#94a3b8' };
  const w = (c, path) => `<path d="${path}" fill="none" stroke="${c}" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/>`;
  const t = (x, y, s, o) => `<text x="${x}" y="${y}" fill="${(o&&o.c)||C.fg}" font-size="${(o&&o.fs)||11}" font-family="monospace"${o&&o.a?` text-anchor="${o.a}"`:''}>${s}</text>`;
  const box = (x, y, wd, h) => `<rect x="${x}" y="${y}" width="${wd}" height="${h}" rx="3" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>`;
  const dot = (x, y) => `<circle cx="${x}" cy="${y}" r="3" fill="${C.fg}"/>`;
  let b = '';
  if (kind === 'pc_fan') {
    b += box(10,190,100,80) + t(20,210,'ESP32',{fs:12}) + t(20,232,'GPIO'+d,{c:C.sig}) + t(20,258,'GND',{c:C.dim});
    b += w(C.sig,'M110 228 H150 M186 228 H262') + box(150,220,36,16) + t(168,212,'1k',{a:'middle'});
    b += w(C.sig,'M226 228 V246') + box(219,246,14,32) + w(C.g,'M226 278 V330') + t(238,268,'10k') + dot(226,228);
    b += `<circle cx="282" cy="228" r="22" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>`;
    b += w(C.fg,'M270 214 V242 M270 220 L290 206 M270 236 L290 250') + t(310,224,'NPN') + t(310,238,'BC547',{c:C.dim});
    b += w(C.fg,'M290 206 V160') + w(C.g,'M290 250 V330');
    b += w(C.v5,'M290 160 H412') + dot(346,160) + w(C.v5,'M300 62 H380 M346 62 V88 M346 126 V160') + box(339,88,14,38) + t(360,112,'1k');
    b += t(300,52,'ATX +5V',{c:C.v5});
    b += w(C.fg,'M412 140 V180 M420 136 V184 M420 142 H452 M420 160 H452 M420 178 H452') + t(464,156,'MOSFET') + t(464,170,'IRLB8721',{c:C.dim});
    b += t(400,194,'G',{c:C.dim}) + t(456,132,'D',{c:C.dim}) + t(456,196,'S',{c:C.dim});
    b += w(C.sw,'M452 142 V110 H630') + w(C.g,'M452 160 V330') + dot(560,110);
    b += t(470,22,'ATX +12V',{c:C.y12}) + w(C.y12,'M470 32 H492 M520 32 H630 M560 32 V52 M630 32 V58') + `<rect x="492" y="25" width="28" height="14" rx="7" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>` + t(506,56,'2A',{a:'middle',c:C.dim}) + dot(560,32);
    b += `<circle cx="560" cy="74" r="21" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>` + t(560,79,'FAN',{a:'middle',fs:10}) + w(C.sw,'M560 95 V110');
    b += `<path d="M630 60 L621 80 H639 Z" fill="${C.bg}" stroke="${C.fg}" stroke-width="1.4"/>` + w(C.fg,'M620 60 H640') + w(C.sw,'M630 80 V110') + t(600,140,'SS34',{c:C.dim});
    b += w(C.g,'M110 258 H130 V330 H640') + t(640,348,'ATX GND',{a:'end',c:C.dim});
    return `<svg viewBox="0 0 660 356" width="100%" style="display:block;margin:4px 0 8px">${b}</svg>`;
  }
  if (kind === 'argb') {
    b += box(10,80,100,74) + t(20,100,'ESP32',{fs:12}) + t(20,122,'GPIO'+d2,{c:C.sig}) + t(20,146,'GND',{c:C.dim});
    b += w(C.sig,'M110 118 H200') + box(200,80,120,80) + t(260,98,'74AHCT125',{a:'middle'});
    b += t(208,122,'1A',{c:C.dim}) + t(208,150,'OE',{c:C.dim}) + t(312,122,'1Y',{a:'end',c:C.dim});
    b += w(C.g,'M200 146 H186 V200') + w(C.v5,'M260 80 V40') + w(C.g,'M260 160 V200');
    b += w(C.sig,'M320 118 H352 M388 118 H450') + box(352,110,36,16) + t(370,104,'330',{a:'middle'});
    b += box(450,80,110,74) + t(460,100,'ARGB',{fs:12}) + t(460,122,'DIN',{c:C.dim}) + t(460,142,'fans + strips',{c:C.dim,fs:10});
    b += w(C.v5,'M505 80 V40') + w(C.g,'M505 154 V200');
    b += w(C.v5,'M600 40 V112') + w(C.fg,'M588 112 H612 M588 120 H612') + w(C.g,'M600 120 V200') + t(616,104,'1000') + t(616,118,'uF');
    b += t(120,30,'ATX +5V (fuse 3-5A)',{c:C.v5}) + w(C.v5,'M120 40 H640') + dot(260,40) + dot(505,40);
    b += w(C.g,'M110 146 H130 V200 H640') + dot(186,200) + dot(260,200) + dot(505,200) + t(640,218,'ATX GND',{a:'end',c:C.dim});
    return `<svg viewBox="0 0 660 224" width="100%" style="display:block;margin:4px 0 8px">${b}</svg>`;
  }
  return '';
}

init();
</script>
</body>
</html>
)WEBEND";

// ── Route registration ────────────────────────────────────────────────────────

static void sendCached(AsyncWebServerRequest* r, const char* mime, const char* body) {
    auto res = r->beginResponse(200, mime, body);
    res->addHeader("Cache-Control", "public, max-age=86400");
    r->send(res);
}

void setupWebApp() {
    apiServer.http().on("/", HTTP_GET, [](AsyncWebServerRequest* r){
        r->send(200, "text/html", FILE_SIMPLE_HTML);
    });
    apiServer.http().on("/advanced", HTTP_GET, [](AsyncWebServerRequest* r){
        String html(FILE_INDEX);
        html.replace("{{VER}}", FIRMWARE_VERSION);
        r->send(200, "text/html", html);
    });
    apiServer.http().on("/css/style.css",    HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "text/css",               FILE_STYLE_CSS); });
    apiServer.http().on("/js/api.js",        HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "application/javascript", FILE_API_JS);    });
    apiServer.http().on("/js/app.js",        HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "application/javascript", FILE_APP_JS);    });
    apiServer.http().on("/js/tab-system.js", HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "application/javascript", FILE_SYSTEM_JS); });
    apiServer.http().on("/js/tab-gpio.js",   HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "application/javascript", FILE_GPIO_JS);   });
    apiServer.http().on("/js/tab-grove.js",  HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "application/javascript", FILE_GROVE_JS);  });
    apiServer.http().on("/manifest.json",    HTTP_GET,
        [](AsyncWebServerRequest* r){ sendCached(r, "application/json",       FILE_MANIFEST);  });

    Serial.println("[API] Web app routes registered");
}
