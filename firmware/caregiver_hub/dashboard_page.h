#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ShakeCare Caregiver</title>
  <style>
    :root { --ok: #18a85f; --warn: #c68a14; --alert: #d63f3f; --bg: #f5f7fa; --panel: #ffffff; --soft: #eef2f5; --line: #d8e0e7; --text: #17212b; --muted: #667484; --code: #f8fafc; --state: var(--warn); }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: Arial, sans-serif; background: var(--bg); color: var(--text); }
    body.state-normal { --state: var(--ok); }
    body.state-warning { --state: var(--warn); }
    body.state-alert { --state: var(--alert); }
    main { max-width: 1040px; margin: 0 auto; padding: 22px 16px 34px; }
    header { display: flex; justify-content: space-between; gap: 14px; align-items: center; margin-bottom: 18px; }
    h1 { font-size: 28px; margin: 0; letter-spacing: 0; }
    .live { display: flex; align-items: center; gap: 8px; padding: 8px 10px; border: 1px solid var(--line); border-radius: 8px; color: var(--muted); background: var(--panel); }
    .dot { width: 9px; height: 9px; border-radius: 50%; background: var(--state); box-shadow: 0 0 0 0 #0000; animation: ping 1.4s infinite; }
    .layout { display: grid; grid-template-columns: minmax(0, 1.35fr) minmax(280px, .9fr); gap: 14px; }
    .panel { border: 1px solid var(--line); border-radius: 8px; background: var(--panel); padding: 18px; overflow: hidden; box-shadow: 0 10px 24px #23384a12; }
    .hero { min-height: 310px; position: relative; display: grid; align-content: space-between; }
    .hero::before { content: ""; position: absolute; inset: 0; background: linear-gradient(135deg, #ffffff, #eef6f3 58%); pointer-events: none; }
    .hero > * { position: relative; }
    .state-row { display: flex; align-items: center; gap: 22px; }
    .orb { width: 132px; height: 132px; border-radius: 50%; background: radial-gradient(circle at 35% 30%, #fff9, transparent 24%), var(--state); box-shadow: 0 18px 38px #0002; animation: breathe 2.1s ease-in-out infinite; }
    .state-alert .orb { animation: alarm 820ms ease-in-out infinite; }
    .label { font-size: 56px; line-height: 1; font-weight: 800; }
    .sub { margin-top: 8px; color: var(--muted); font-size: 16px; }
    .scan { height: 9px; border-radius: 999px; background: linear-gradient(90deg, transparent, var(--state), transparent); animation: scan 1.5s linear infinite; opacity: .75; }
    .grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 10px; margin-top: 14px; }
    .tile { border: 1px solid var(--line); border-radius: 8px; padding: 14px; background: var(--panel); }
    .k { color: var(--muted); font-size: 12px; margin-bottom: 8px; }
    .v { font-size: 22px; font-weight: 700; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .matrix { display: grid; grid-template-columns: repeat(8, 1fr); gap: 5px; margin-top: 14px; }
    .px { aspect-ratio: 1; border-radius: 4px; background: var(--state); opacity: .42; animation: pixel 1.8s ease-in-out infinite; }
    .terminal { height: 226px; overflow: auto; border: 1px solid var(--line); border-radius: 8px; background: var(--code); padding: 12px; font: 13px/1.55 Consolas, monospace; color: #24313d; }
    .terminal div { white-space: nowrap; animation: slide .18s ease-out; }
    .prompt { color: #596a7b; }
    .timeline { display: flex; align-items: center; gap: 8px; min-height: 58px; border-top: 1px solid var(--line); margin-top: 12px; padding-top: 16px; overflow-x: auto; }
    .point { width: 14px; height: 14px; flex: 0 0 14px; border-radius: 50%; background: var(--state); position: relative; cursor: default; box-shadow: 0 0 0 4px #0000000a; }
    .point.normal { background: var(--ok); }
    .point.warning { background: var(--warn); }
    .point.alert { background: var(--alert); }
    .point:hover::after { content: attr(data-tip); position: absolute; left: 50%; bottom: 22px; transform: translateX(-50%); width: max-content; max-width: 260px; padding: 7px 9px; border: 1px solid var(--line); border-radius: 8px; background: #ffffff; color: var(--text); box-shadow: 0 8px 18px #0002; z-index: 2; }
    .bars { display: flex; align-items: end; gap: 5px; height: 46px; margin-top: 14px; }
    .bar { flex: 1; min-width: 8px; border-radius: 4px 4px 0 0; background: var(--state); opacity: .38; transform-origin: bottom; animation: bar 900ms ease-in-out infinite; }
    .toolbar { display: flex; gap: 8px; margin-top: 12px; }
    button { border: 1px solid var(--line); border-radius: 8px; background: #ffffff; color: var(--text); padding: 10px 12px; cursor: pointer; }
    button.active { border-color: var(--state); color: var(--state); }
    body.pulse .panel { outline: 2px solid var(--state); }
    @keyframes ping { 70% { box-shadow: 0 0 0 10px #0000; } 100% { box-shadow: 0 0 0 0 #0000; } }
    @keyframes breathe { 50% { transform: scale(1.04); } }
    @keyframes alarm { 50% { transform: scale(1.08); filter: brightness(1.2); } }
    @keyframes scan { from { transform: translateX(-65%); } to { transform: translateX(65%); } }
    @keyframes pixel { 50% { opacity: .55; transform: scale(.92); } }
    @keyframes slide { from { transform: translateY(-6px); opacity: 0; } }
    @keyframes bar { 50% { transform: scaleY(.45); opacity: .95; } }
    @media (max-width: 760px) { .layout, .grid { grid-template-columns: 1fr; } .state-row { align-items: flex-start; flex-direction: column; } .label { font-size: 42px; } header { align-items: flex-start; flex-direction: column; } }
  </style>
</head>
<body class="state-warning">
  <main>
    <header>
      <h1>ShakeCare Caregiver</h1>
      <div class="live"><span class="dot"></span><span id="online">offline</span></div>
    </header>
    <section class="layout">
      <div>
        <section class="panel hero">
          <div class="state-row">
            <div class="orb"></div>
            <div>
              <div id="label" class="label">WAITING</div>
              <div id="summary" class="sub">Waiting for elder board signal</div>
            </div>
          </div>
          <div class="scan"></div>
        </section>
        <section class="grid">
          <div class="tile"><div class="k">Last packet</div><div id="age" class="v">none</div></div>
          <div class="tile"><div class="k">Sequence</div><div id="seq" class="v">0</div></div>
          <div class="tile"><div class="k">Uptime</div><div id="uptime" class="v">0s</div></div>
          <div class="tile"><div class="k">Clients</div><div id="clients" class="v">0</div></div>
        </section>
      </div>
      <aside>
        <section class="panel">
          <div class="k">Matrix preview</div>
          <div id="matrix" class="matrix"></div>
          <div class="bars" id="bars"></div>
          <div class="toolbar">
            <button id="freeze">Freeze log</button>
            <button id="clear">Clear log</button>
          </div>
        </section>
        <section class="panel" style="margin-top:14px">
          <div class="k">Bash log</div>
          <div id="log" class="terminal"></div>
          <div id="timeline" class="timeline"></div>
        </section>
      </aside>
    </section>
  </main>
  <script>
    const matrix = document.getElementById('matrix');
    const bars = document.getElementById('bars');
    const log = document.getElementById('log');
    const timeline = document.getElementById('timeline');
    const freeze = document.getElementById('freeze');
    let lastKey = '';
    let lastLogAt = 0;
    let frozen = false;
    for (let i = 0; i < 64; i++) {
      const cell = document.createElement('div');
      cell.className = 'px';
      cell.style.animationDelay = `${(i % 8) * 45 + Math.floor(i / 8) * 30}ms`;
      matrix.appendChild(cell);
    }
    for (let i = 0; i < 18; i++) {
      const bar = document.createElement('div');
      bar.className = 'bar';
      bar.style.height = `${24 + (i % 6) * 13}%`;
      bar.style.animationDelay = `${i * 55}ms`;
      bars.appendChild(bar);
    }
    function fmt(ms) {
      if (ms < 0) return 'none';
      if (ms < 1000) return `${ms}ms`;
      return `${Math.round(ms / 100) / 10}s`;
    }
    function addEvent(data, reason) {
      if (frozen) return;
      const now = new Date();
      const time = now.toLocaleTimeString();
      const line = document.createElement('div');
      line.innerHTML = `<span class="prompt">${time} $</span> shakecare ${reason} --state ${data.stateText.toLowerCase()} --link ${data.online ? 'online' : 'offline'} --seq ${data.seq}`;
      log.appendChild(line);
      while (log.children.length > 80) log.firstChild.remove();
      log.scrollTop = log.scrollHeight;

      const row = document.createElement('div');
      row.className = `point ${data.stateKey}`;
      row.dataset.tip = `${time} | ${data.stateText} | ${data.online ? 'online' : 'offline'} | seq ${data.seq}`;
      timeline.appendChild(row);
      while (timeline.children.length > 36) timeline.firstChild.remove();
      timeline.scrollLeft = timeline.scrollWidth;
    }
    async function poll() {
      try {
        const res = await fetch('/status');
        const data = await res.json();
        const key = `${data.stateKey}-${data.online}`;
        document.body.className = `state-${data.stateKey}`;
        if (key !== lastKey) {
          document.body.classList.add('pulse');
          setTimeout(() => document.body.classList.remove('pulse'), 260);
          addEvent(data, 'state-change');
          lastLogAt = Date.now();
          lastKey = key;
        } else if (Date.now() - lastLogAt >= 5000) {
          addEvent(data, 'heartbeat');
          lastLogAt = Date.now();
        }
        document.getElementById('label').textContent = data.stateText;
        document.getElementById('online').textContent = data.online ? 'online' : 'offline';
        document.getElementById('summary').textContent = data.online ? `Elder board state is ${data.stateText}` : 'No recent ESP-NOW packet';
        document.getElementById('age').textContent = fmt(data.ageMs);
        document.getElementById('seq').textContent = data.seq;
        document.getElementById('uptime').textContent = fmt(data.uptimeMs);
        document.getElementById('clients').textContent = data.clients;
      } catch (e) {
        document.getElementById('online').textContent = 'offline';
      }
    }
    freeze.onclick = () => {
      frozen = !frozen;
      freeze.classList.toggle('active', frozen);
      freeze.textContent = frozen ? 'Resume log' : 'Freeze log';
    };
    document.getElementById('clear').onclick = () => {
      log.innerHTML = '';
      timeline.innerHTML = '';
    };
    setInterval(poll, 400);
    poll();
  </script>
</body>
</html>
)rawliteral";
