let panel, currentApi;
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

    panel.querySelector('#grove-log-clear').onclick = () => {
        groveLog.length = 0;
        renderLog();
    };
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

export function deactivateGrove() {
    stopStream();
    currentApi = null;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function loadSensors() {
    if (!currentApi) return;
    currentApi.get('/api/grove/sensors').then(data => {
        sensors = data.sensors || [];
        const sel = panel.querySelector('#grove-type');
        sel.innerHTML = '<option value="">Select sensor type...</option>' +
            sensors.map(s =>
                `<option value="${s.id}">${s.name} (${s.vcc}${s.gpio_safe ? '' : ' ⚠'})</option>`
            ).join('');
        if (activeSensorId) {
            sel.value = activeSensorId;
            updateSafetyInfo(activeSensorId);
        }
    }).catch(() => {});
}

function updateSafetyInfo(id) {
    const safetyEl = panel.querySelector('#grove-safety');
    const wiringEl = panel.querySelector('#grove-wiring');
    if (!id) {
        safetyEl.style.display = 'none';
        wiringEl.style.display = 'none';
        return;
    }
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
        renderWiring(4, 5);
    }

    if (s.gpio_safe) {
        safetyEl.style.cssText =
            'display:block;background:#0d2818;border:1px solid var(--green);' +
            'border-radius:8px;padding:10px 12px;margin-top:8px;font-size:13px;line-height:1.5';
        safetyEl.innerHTML =
            `<strong style="color:var(--green)">&#10003; Safe &mdash; ${s.vcc}</strong><br>${s.voltage_note}`;
    } else {
        safetyEl.style.cssText =
            'display:block;background:#2d1111;border:1px solid var(--red);' +
            'border-radius:8px;padding:10px 12px;margin-top:8px;font-size:13px;line-height:1.5';
        safetyEl.innerHTML =
            `<strong style="color:var(--red)">&#9888; Caution &mdash; ${s.vcc}</strong><br>${s.voltage_note}`;
    }
}

function buildWiringGuide(sensorId, d, d2) {
    const G = {
        digital_in: {
            rows: [['Signal', `GPIO${d}`], ['VCC', '3.3V'], ['GND', 'GND']],
            note: 'For a button: connect one end to GPIO, other to GND — then use INPUT_PULLUP mode.'
        },
        digital_out: {
            rows: [['Output', `GPIO${d}`], ['VCC', '3.3V'], ['GND', 'GND']],
            note: 'Connect an LED with a 330&#937; resistor in series. Or a relay/buzzer module.'
        },
        analog_in: {
            rows: [['Signal', `GPIO${d}  ← MAX 3.3V!`], ['VCC', '3.3V'], ['GND', 'GND']],
            note: 'Never apply more than 3.3V to this pin. ADC reads 0 (0V) to 4095 (3.3V).'
        },
        pwm_out: {
            rows: [['PWM Out', `GPIO${d}`], ['VCC', '3.3V'], ['GND', 'GND']],
            note: 'Connect an LED + 330&#937; resistor. Duty 0 = off, 128 = half brightness, 255 = full on.'
        },
        dht11: {
            rows: [['VCC', '3.3V'], ['GND', 'GND'], ['DATA', `GPIO${d}`]],
            note: 'Breakout modules include a pull-up resistor &mdash; no extra parts needed. Bare sensor: add 10k&#937; from DATA to 3.3V.'
        },
        ds18b20: {
            rows: [['VCC (red)', '3.3V'], ['GND (black)', 'GND'], ['DATA (yellow)', `GPIO${d}`]],
            note: 'Required: 4.7k&#937; resistor between 3.3V and DATA. Without it readings will fail.'
        },
        hcsr04: {
            rows: [
                ['VCC', '3.3V  ← HC-SR04P only!'],
                ['GND', 'GND'],
                ['TRIG', `GPIO${d}  (trigger)`],
                ['ECHO', `GPIO${d2}  (echo)`]
            ],
            note: '&#9888; Use HC-SR04<strong>P</strong> (3.3V version). Standard HC-SR04 outputs 5V on ECHO and will damage your ESP32.'
        },
        rotary: {
            rows: [['VCC (+)', '3.3V'], ['GND (–)', 'GND'], ['CLK', `GPIO${d}`], ['DT', `GPIO${d2}`]],
            note: 'Works with KY-040 encoder modules. The SW (button) pin is not used here.'
        },
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
        `<tr><td class="pin-sensor">${sp}</td>` +
        `<td style="color:var(--text-dim);padding:0 6px;font-family:sans-serif">→</td>` +
        `<td class="pin-esp32">${ep}</td></tr>`
    ).join('');
    return pcSvg(sensorId, d, d2) + `<table class="wiring-table">` +
        `<thead><tr><th>Sensor Pin</th><th></th><th>ESP32 Pin</th></tr></thead>` +
        `<tbody>${rows}</tbody></table>` +
        `<div class="wiring-note">${g.note}</div>`;
}

function showOutputControls(id) {
    panel.querySelector('#grove-digital-row').style.display  = id === 'digital_out' ? 'flex'  : 'none';
    panel.querySelector('#grove-pwm-row').style.display      = id === 'pwm_out'     ? 'flex'  : 'none';
    panel.querySelector('#grove-rotary-ctrl').style.display  = id === 'rotary'      ? 'block' : 'none';
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
    if (r.temperature  !== undefined) parts.push(`${r.temperature}°C`);
    if (r.humidity     !== undefined) parts.push(`${r.humidity}% RH`);
    if (r.distance_cm  !== undefined) parts.push(`${r.distance_cm} cm`);
    if (r.steps        !== undefined) parts.push(`Steps: ${r.steps}`);
    if (r.fan_pct      !== undefined) parts.push(`Fan ${r.fan_pct}%  (duty ${r.duty}${r.kick ? ', kick-start' : ''})`);
    if (r.color        !== undefined) parts.push(`ARGB ${r.color} x ${r.leds}`);
    if (r.note         !== undefined) parts.push(r.note);
    el.textContent = parts.join('  |  ') || '—';
}

function readingToString(r) {
    if (r.error) return `Error: ${r.error}`;
    const parts = [];
    if (r.label        !== undefined) parts.push(r.label);
    else if (r.value   !== undefined) parts.push(`val=${r.value}`);
    if (r.raw          !== undefined) parts.push(`raw=${r.raw} ${r.voltage}V`);
    if (r.temperature  !== undefined) parts.push(`temp=${r.temperature}°C`);
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
