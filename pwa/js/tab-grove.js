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
function pcSvg(kind, d, d2) {
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
