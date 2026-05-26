#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <DNSServer.h>

// ── Wi-Fi ──────────────────────────────────────────────────────────────────
const char* ssid     = "Rover_Command_Link";
const char* password = "password123";

const byte  DNS_PORT = 53;
DNSServer   dnsServer;
WebServer   server(80);

// ── PCA9685 ────────────────────────────────────────────────────────────────
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVOMIN 150
#define SERVOMAX 600

// ── L298N ──────────────────────────────────────────────────────────────────
const int ENA = 13, IN1 = 32, IN2 = 33;
const int ENB = 25, IN3 = 26, IN4 = 27;

int             currentSpeedPWM    = 255;
unsigned long lastDriveCommandMs = 0;
const unsigned long DRIVE_TIMEOUT_MS = 450;

// ── Joint limits ───────────────────────────────────────────────────────────
int minAngle[7]  = {  0,  20,  0,  30,  0,  0, 10};
int maxAngle[7]  = {180, 150, 180, 160, 180, 180, 170};
int homeAngle[7] = { 90, 130,  130,  45,  90,  90, 10};

// ══════════════════════════════════════════════════════════════════════════
//  HTML PAGE — Classic dark UI, mobile-first, fully fixed navigation
// ══════════════════════════════════════════════════════════════════════════
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Jana's Rover</title>
<style>

/* ── TOKENS ─────────────────────────────────────────────────── */
:root {
  --bg:     #0e1118;
  --card:   #181c26;
  --card2:  #11141c;
  --brd:    #272c3d;
  --txt:    #c8d4e8;
  --mute:   #52607a;
  --acc:    #4a8fe8;
  --red:    #e04848;
  --grn:    #3ecf6e;
  --white:  #e8edf6;
}

/* ── RESET ──────────────────────────────────────────────────── */
* {
  box-sizing: border-box;
  margin: 0; padding: 0;
  -webkit-tap-highlight-color: transparent;
}
html { background: var(--bg); }
body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
  color: var(--txt);
  background: var(--bg);
  min-height: 100vh;
  /* bottom padding = tab bar height + safe area */
  padding-bottom: calc(58px + env(safe-area-inset-bottom, 0px));
}

/* ── HEADER ─────────────────────────────────────────────────── */
.hdr {
  position: sticky; top: 0; z-index: 500;
  display: flex; align-items: center; justify-content: space-between;
  padding: 12px 16px;
  background: var(--card);
  border-bottom: 1px solid var(--brd);
}
.hdr-left { display: flex; flex-direction: column; gap: 2px; }
.hdr-title { font-size: 18px; font-weight: 700; color: var(--white); }
.hdr-sub   { font-size: 10px; color: var(--mute); letter-spacing: 1px; }
.brand-name {
  font-family: 'Courier New', 'Lucida Console', monospace;
  font-size: 11px; font-weight: 700;
  letter-spacing: 4px; text-transform: uppercase;
  color: #f5c000; margin-top: 3px;
}
.badge {
  display: flex; align-items: center; gap: 6px;
  padding: 5px 12px; border-radius: 20px;
  border: 1px solid var(--brd);
  font-size: 12px; font-weight: 600; color: var(--txt);
}
.dot {
  width: 8px; height: 8px; border-radius: 50%;
  background: var(--grn);
  animation: blink 2s infinite;
}
.dot.off { background: var(--red); animation: none; }
@keyframes blink { 0%,100%{opacity:1} 50%{opacity:.4} }

/* ── PAGES ──────────────────────────────────────────────────── */
.page { display: none; padding: 12px; }
.page.on { display: block; }

/* ── CARD ───────────────────────────────────────────────────── */
.card {
  background: var(--card);
  border: 1px solid var(--brd);
  border-radius: 12px;
  padding: 14px;
  margin-bottom: 10px;
}
.card-title {
  font-size: 10px; font-weight: 700;
  letter-spacing: 2px; text-transform: uppercase;
  color: var(--mute);
  margin-bottom: 12px;
  padding-bottom: 8px;
  border-bottom: 1px solid var(--brd);
}

/* ── TELEMETRY STRIP ────────────────────────────────────────── */
.tstrip {
  display: grid; grid-template-columns: repeat(3,1fr); gap: 8px;
  margin-bottom: 10px;
}
.tc {
  background: var(--card); border: 1px solid var(--brd);
  border-radius: 10px; padding: 10px 6px; text-align: center;
}
.tc-v { font-size: 20px; font-weight: 700; color: var(--white); }
.tc-l { font-size: 9px; color: var(--mute); letter-spacing: 1.5px; margin-top: 3px; }

/* ── D-PAD ──────────────────────────────────────────────────── */
.dpad {
  display: grid; grid-template-columns: repeat(3,1fr);
  gap: 8px; max-width: 250px; margin: 0 auto;
}
.dp {
  aspect-ratio: 1;
  border-radius: 10px;
  border: 1.5px solid #f5c000;
  background: var(--card2);
  color: #f5c000;
  font-size: 22px; font-weight: 700;
  display: flex; align-items: center; justify-content: center;
  cursor: pointer;
  user-select: none; -webkit-user-select: none;
  touch-action: manipulation;
  transition: background 0.08s, border-color 0.08s, color 0.08s;
}
.dp:active, .dp.hit {
  background: var(--acc); border-color: var(--acc); color: #fff;
}
.dp.stp {
  color: var(--red); border-color: var(--red);
  font-size: 12px; font-weight: 800;
}
.dp.stp:active, .dp.stp.hit {
  background: var(--red); border-color: var(--red); color: #fff;
}
.dp.ph { visibility: hidden; pointer-events: none; }

/* ── SPEED SLIDER ───────────────────────────────────────────── */
.spd-row {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 10px;
}
.spd-lbl { font-size: 13px; color: var(--mute); }
.spd-val { font-size: 24px; font-weight: 700; color: var(--white); }

input[type=range] {
  -webkit-appearance: none; appearance: none;
  width: 100%; height: 20px; border-radius: 10px; outline: none;
  cursor: pointer;
  border: 1px solid var(--brd);
  background: linear-gradient(
    to right,
    var(--acc) 0%, var(--acc) var(--fill,100%),
    var(--card2) var(--fill,100%), var(--card2) 100%
  );
}
input[type=range]::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 28px; height: 28px; border-radius: 50%;
  background: #f5c000;
  border: 2px solid #fff;
  cursor: pointer;
}
input[type=range]::-webkit-slider-runnable-track {
  height: 20px; border-radius: 10px;
}

/* ── JOYSTICK ───────────────────────────────────────────────── */
.joy-wrap {
  display: flex; flex-direction: column;
  align-items: center; gap: 10px;
  padding: 4px 0;
}
.joy-ring {
  position: relative;
  width: 160px; height: 160px;
  border-radius: 50%;
  border: 2px solid var(--brd);
  background: var(--card2);
  touch-action: none;
  user-select: none; -webkit-user-select: none;
}
/* Simple crosshair */
.joy-ring::before {
  content: '';
  position: absolute; left: 0; top: 50%;
  width: 100%; height: 1px;
  background: var(--brd);
}
.joy-ring::after {
  content: '';
  position: absolute; top: 0; left: 50%;
  width: 1px; height: 100%;
  background: var(--brd);
}
/* Cardinal labels */
.joy-n, .joy-s, .joy-w, .joy-e {
  position: absolute;
  font-size: 9px; font-weight: 700; color: var(--mute);
  letter-spacing: 1px; pointer-events: none; z-index: 2;
}
.joy-n { top: 5px;  left: 50%; transform: translateX(-50%); }
.joy-s { bottom: 5px; left: 50%; transform: translateX(-50%); }
.joy-w { left: 5px; top: 50%; transform: translateY(-50%); }
.joy-e { right: 5px; top: 50%; transform: translateY(-50%); }

.joy-knob {
  width: 56px; height: 56px; border-radius: 50%;
  background: var(--card);
  border: 2px solid var(--acc);
  position: absolute;
  top: 52px; left: 52px;   /* (160-56)/2 = 52 */
  pointer-events: none;
  z-index: 3;
}
.joy-dir {
  font-size: 13px; font-weight: 600; color: var(--mute);
  border: 1px solid var(--brd);
  border-radius: 20px; padding: 5px 18px;
  background: var(--card2);
  min-width: 110px; text-align: center;
}

/* ── ARM JOINTS ─────────────────────────────────────────────── */
.joint {
  padding: 11px 0;
  border-bottom: 1px solid var(--brd);
}
.joint:last-of-type { border-bottom: none; }
.jnt-hdr {
  display: flex; justify-content: space-between; align-items: center;
  margin-bottom: 8px;
}
.jnt-name { font-size: 12px; color: var(--mute); }
.jnt-val  { font-size: 20px; font-weight: 700; color: var(--white); }

/* ── PRESETS ────────────────────────────────────────────────── */
.presets {
  display: grid; grid-template-columns: repeat(3,1fr); gap: 8px;
}
.pbtn {
  padding: 14px 6px; border-radius: 10px;
  border: 1px solid var(--brd); background: var(--card2);
  color: var(--txt); font-size: 13px; font-weight: 600;
  cursor: pointer; text-align: center;
  touch-action: manipulation;
  transition: background 0.1s, border-color 0.1s, color 0.1s;
}
.pbtn:active { background: var(--acc); border-color: var(--acc); color: #fff; }
.pbtn-ico { font-size: 20px; display: block; margin-bottom: 4px; }

/* ── SYS PAGE ───────────────────────────────────────────────── */
.srow {
  display: flex; justify-content: space-between; align-items: center;
  padding: 10px 0; border-bottom: 1px solid var(--brd); font-size: 13px;
}
.srow:last-child { border-bottom: none; }
.sk { color: var(--mute); }
.sv { font-weight: 600; color: var(--white); }
.sv.hi { color: var(--acc); }
.note {
  margin-top: 10px; padding: 12px; border-radius: 10px;
  border: 1px solid var(--brd); background: var(--card2);
  font-size: 12px; color: var(--mute); line-height: 1.7;
}

/* ── BOTTOM TAB BAR ─────────────────────────────────────────── */
/* NOTE: NO pseudo-elements on body — that was blocking touches */
.tabs {
  position: fixed; bottom: 0; left: 0; right: 0;
  z-index: 600;                          /* above everything */
  display: flex;
  background: var(--card);
  border-top: 1px solid var(--brd);
  padding-bottom: env(safe-area-inset-bottom, 0);
}
.tb {
  flex: 1;
  padding: 9px 4px 7px;
  background: none; border: none; outline: none;
  cursor: pointer;
  color: var(--mute);
  font-size: 10px; font-weight: 700; letter-spacing: 0.5px;
  display: flex; flex-direction: column; align-items: center; gap: 3px;
  touch-action: manipulation;
  -webkit-tap-highlight-color: transparent;
  transition: color 0.15s;
}
.tb.on { color: var(--acc); }
.tb-ico { font-size: 22px; line-height: 1.1; }
.tb-pip {
  width: 22px; height: 2px; border-radius: 1px;
  background: transparent; margin-top: 2px;
  transition: background 0.15s;
}
.tb.on .tb-pip { background: var(--acc); }

</style>
</head>
<body>

<header class="hdr">
  <div class="hdr-left">
    <div class="hdr-title">Jana's Rover</div>
    <div class="hdr-sub">CONTROL PANEL</div>
    <div class="brand-name">RoboAttis</div>
  </div>
  <div class="badge">
    <div class="dot" id="sDot"></div>
    <span id="sLbl">Online</span>
  </div>
</header>

<section class="page on" id="pg-drive">

  <div class="tstrip">
    <div class="tc">
      <div class="tc-v" id="tmSpd">100</div>
      <div class="tc-l">SPEED %</div>
    </div>
    <div class="tc">
      <div class="tc-v" id="tmDir">—</div>
      <div class="tc-l">DIR</div>
    </div>
    <div class="tc">
      <div class="tc-v" id="tmLnk">OK</div>
      <div class="tc-l">LINK</div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">D-Pad</div>
    <div class="dpad" id="dpadWrap">
      <div class="dp ph"></div>
      <button class="dp" id="dF" type="button">▲</button>
      <div class="dp ph"></div>
      <button class="dp" id="dL" type="button">◀</button>
      <button class="dp stp" id="dS" type="button">STOP</button>
      <button class="dp" id="dR" type="button">▶</button>
      <div class="dp ph"></div>
      <button class="dp" id="dB" type="button">▼</button>
      <div class="dp ph"></div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">Throttle</div>
    <div class="spd-row">
      <span class="spd-lbl">Motor Power</span>
      <span class="spd-val" id="spdVal">100%</span>
    </div>
    <input type="range" id="spdRange" min="0" max="255" value="255"
           style="--fill:100%"
           oninput="onSpd(this)" onchange="sendSpd(this.value)">
  </div>

  <div class="card">
    <div class="card-title">Joystick</div>
    <div class="joy-wrap">
      <div class="joy-ring" id="joyRing">
        <span class="joy-n">FWD</span>
        <span class="joy-s">REV</span>
        <span class="joy-w">L</span>
        <span class="joy-e">R</span>
        <div class="joy-knob" id="joyKnob"></div>
      </div>
      <div class="joy-dir" id="joyDir">Stopped</div>
    </div>
  </div>

</section>

<section class="page" id="pg-arm">

  <div class="card">
    <div class="card-title">Joint Control</div>

    <div class="joint">
      <div class="jnt-hdr"><span class="jnt-name">Base (Ch 0)</span><span class="jnt-val" id="v0">90°</span></div>
      <input type="range" min="0" max="180" value="90" id="j0" oninput="onJ(this,0)" style="--fill:50%">
    </div>
    <div class="joint">
      <div class="jnt-hdr"><span class="jnt-name">Shoulder (Ch 1+2)</span><span class="jnt-val" id="v1">130°</span></div>
      <input type="range" min="40" max="150" value="130" id="j1" oninput="onJ(this,1)" style="--fill:82%">
    </div>
    <div class="joint">
      <div class="jnt-hdr"><span class="jnt-name">Elbow (Ch 3)</span><span class="jnt-val" id="v3">45°</span></div>
      <input type="range" min="30" max="160" value="45" id="j3" oninput="onJ(this,3)" style="--fill:12%">
    </div>
    <div class="joint">
      <div class="jnt-hdr"><span class="jnt-name">Wrist Pitch (Ch 4)</span><span class="jnt-val" id="v4">90°</span></div>
      <input type="range" min="0" max="180" value="90" id="j4" oninput="onJ(this,4)" style="--fill:50%">
    </div>
    <div class="joint">
      <div class="jnt-hdr"><span class="jnt-name">Wrist Roll (Ch 5)</span><span class="jnt-val" id="v5">90°</span></div>
      <input type="range" min="0" max="180" value="90" id="j5" oninput="onJ(this,5)" style="--fill:50%">
    </div>
    <div class="joint">
      <div class="jnt-hdr"><span class="jnt-name">Gripper (Ch 6)</span><span class="jnt-val" id="v6">10°</span></div>
      <input type="range" min="10" max="170" value="10" id="j6" oninput="onJ(this,6)" style="--fill:0%">
    </div>
  </div>

  <div class="card">
    <div class="card-title">Presets</div>
    <div class="presets">
      <button class="pbtn" type="button" id="preHome">
        <span class="pbtn-ico">🏠</span>Home
      </button>
      <button class="pbtn" type="button" id="prePick">
        <span class="pbtn-ico">🤏</span>Pick
      </button>
      <button class="pbtn" type="button" id="preCarry">
        <span class="pbtn-ico">📦</span>Carry
      </button>
    </div>
    <div class="note">
      Ch 2 mirrors Ch 1 (shoulder) automatically.
      All angles are clamped on the ESP32 firmware.
    </div>
  </div>

</section>

<section class="page" id="pg-sys">

  <div class="card">
    <div class="card-title">Network</div>
    <div class="srow"><span class="sk">SSID</span><span class="sv">Rover_Command_Link</span></div>
    <div class="srow"><span class="sk">IP Address</span><span class="sv hi">192.168.4.1</span></div>
    <div class="srow"><span class="sk">Mode</span><span class="sv">Wi-Fi Access Point</span></div>
    <div class="srow"><span class="sk">Status</span><span class="sv hi" id="sysLnk">Online</span></div>
  </div>

  <div class="card">
    <div class="card-title">Hardware</div>
    <div class="srow"><span class="sk">MCU</span><span class="sv">ESP32</span></div>
    <div class="srow"><span class="sk">Servo Driver</span><span class="sv">PCA9685 @ 0x40</span></div>
    <div class="srow"><span class="sk">I2C Bus</span><span class="sv">SDA 21 / SCL 22</span></div>
    <div class="srow"><span class="sk">Motor Driver</span><span class="sv">L298N</span></div>
    <div class="srow"><span class="sk">ENA / ENB</span><span class="sv">GPIO 13 / 25</span></div>
    <div class="srow"><span class="sk">IN1 – IN4</span><span class="sv">GPIO 32, 33, 26, 27</span></div>
    <div class="srow"><span class="sk">PWM Freq</span><span class="sv">50 Hz</span></div>
  </div>

  <div class="card">
    <div class="card-title">Safety</div>
    <div class="srow"><span class="sk">Drive Timeout</span><span class="sv hi">450 ms</span></div>
    <div class="srow"><span class="sk">Heartbeat Rate</span><span class="sv hi">200 ms</span></div>
    <div class="srow"><span class="sk">Servo Pulse</span><span class="sv">150 – 600</span></div>
    <div class="note">
      Motors auto-stop if heartbeat is lost for more than 450 ms.
      The browser sends a keep-alive command every 200 ms while driving.
    </div>
  </div>

</section>

<nav class="tabs">
  <button class="tb on" type="button" id="t-drive">
    <span class="tb-ico">🕹️</span>
    <span>Drive</span>
    <span class="tb-pip"></span>
  </button>
  <button class="tb" type="button" id="t-arm">
    <span class="tb-ico">🦾</span>
    <span>Arm</span>
    <span class="tb-pip"></span>
  </button>
  <button class="tb" type="button" id="t-sys">
    <span class="tb-ico">⚙️</span>
    <span>System</span>
    <span class="tb-pip"></span>
  </button>
</nav>

<script>
'use strict';

// ── TAB NAVIGATION ────────────────────────────────────────────────────
// Uses JS event listeners on <button> elements — most reliable on mobile
var PAGES = ['drive','arm','sys'];

function showTab(name) {
  for (var i = 0; i < PAGES.length; i++) {
    var id = PAGES[i];
    var pg = document.getElementById('pg-' + id);
    var tb = document.getElementById('t-'  + id);
    if (id === name) {
      pg.classList.add('on');
      tb.classList.add('on');
    } else {
      pg.classList.remove('on');
      tb.classList.remove('on');
    }
  }
}

document.getElementById('t-drive').addEventListener('click', function(){ showTab('drive'); });
document.getElementById('t-arm').addEventListener('click',   function(){ showTab('arm');   });
document.getElementById('t-sys').addEventListener('click',   function(){ showTab('sys');   });

// ── API HELPER ────────────────────────────────────────────────────────
var errCount = 0;
function api(url) {
  fetch(url)
    .then(function() {
      errCount = 0;
      setLink(true);
    })
    .catch(function() {
      errCount++;
      if (errCount > 2) setLink(false);
    });
}

function setLink(ok) {
  document.getElementById('sDot').className     = ok ? 'dot' : 'dot off';
  document.getElementById('sLbl').textContent   = ok ? 'Online' : 'Lost';
  document.getElementById('tmLnk').textContent  = ok ? 'OK'     : 'ERR';
  document.getElementById('sysLnk').textContent = ok ? 'Online' : 'Lost';
}

// ── SPEED SLIDER ──────────────────────────────────────────────────────
function onSpd(el) {
  var pct = Math.round(el.value / 255 * 100);
  el.style.setProperty('--fill', pct + '%');
  document.getElementById('spdVal').textContent = pct + '%';
  document.getElementById('tmSpd').textContent  = pct;
}
function sendSpd(v) { api('/speed?val=' + v); }

// ── ARM JOINTS ────────────────────────────────────────────────────────
var jTimers = {};
var jSliders = {0:'j0', 1:'j1', 3:'j3', 4:'j4', 5:'j5', 6:'j6'};

function onJ(el, joint) {
  var a   = el.value;
  var pct = ((a - el.min) / (el.max - el.min) * 100).toFixed(1) + '%';
  el.style.setProperty('--fill', pct);
  document.getElementById('v' + joint).textContent = a + '\u00b0';
  clearTimeout(jTimers[joint]);
  jTimers[joint] = setTimeout(function() {
    api('/set?joint=' + joint + '&angle=' + a);
  }, 50);
}

function applyPreset(vals) {
  for (var j = 0; j < vals.length; j++) {
    if (vals[j] === null) continue;
    var sl = document.getElementById(jSliders[j]);
    if (!sl) continue;
    sl.value = vals[j];
    onJ(sl, j);
    (function(joint, angle) {
      setTimeout(function() { api('/set?joint=' + joint + '&angle=' + angle); }, joint * 90);
    })(j, vals[j]);
  }
}

document.getElementById('preHome').addEventListener('click',  function(){ applyPreset([90,130,null,45,90,90,10]);  });
document.getElementById('prePick').addEventListener('click',  function(){ applyPreset([90,110,null,80,45,90,100]); });
document.getElementById('preCarry').addEventListener('click', function(){ applyPreset([90,80,null,140,90,90,100]); });

// ── DRIVE ENGINE ──────────────────────────────────────────────────────
var driveTimer = null;
var curDir     = 'S';
var DIR_TEXT   = {F:'Forward', B:'Reverse', L:'Left', R:'Right', S:'Stopped'};

function setDir(d) {
  if (d === curDir) return;
  curDir = d;
  var label = DIR_TEXT[d] || 'Stopped';
  document.getElementById('joyDir').textContent = label;
  document.getElementById('tmDir').textContent  = (d === 'S') ? '\u2014' : d;
  clearInterval(driveTimer);
  api('/drive?dir=' + d);
  if (d !== 'S') {
    driveTimer = setInterval(function() { api('/drive?dir=' + d); }, 200);
  }
}
function stopD() { setDir('S'); }

// ── D-PAD ─────────────────────────────────────────────────────────────
var dpadDirs = {dF:'F', dL:'L', dR:'R', dB:'B'};
Object.keys(dpadDirs).forEach(function(id) {
  var dir = dpadDirs[id];
  var btn = document.getElementById(id);
  btn.addEventListener('pointerdown',   function(e){ e.preventDefault(); btn.classList.add('hit');    setDir(dir); }, {passive:false});
  btn.addEventListener('pointerup',     function(e){ e.preventDefault(); btn.classList.remove('hit'); stopD();     }, {passive:false});
  btn.addEventListener('pointerleave',  function(e){ e.preventDefault(); btn.classList.remove('hit'); stopD();     }, {passive:false});
  btn.addEventListener('pointercancel', function(){ btn.classList.remove('hit'); stopD(); });
  btn.addEventListener('contextmenu',   function(e){ e.preventDefault(); });
});

var stopBtn = document.getElementById('dS');
stopBtn.addEventListener('pointerdown',   function(e){ e.preventDefault(); stopD(); stopBtn.classList.add('hit');    }, {passive:false});
stopBtn.addEventListener('pointerup',     function(e){ e.preventDefault(); stopBtn.classList.remove('hit'); }, {passive:false});
stopBtn.addEventListener('pointercancel', function(){ stopBtn.classList.remove('hit'); });
stopBtn.addEventListener('contextmenu',   function(e){ e.preventDefault(); });

// ── JOYSTICK ─────────────────────────────────────────────────────────
var jRing  = document.getElementById('joyRing');
var jKnob  = document.getElementById('joyKnob');
var KNOB_R = 28;   // half of knob (56px / 2)
var DEAD   = 20;   // deadzone radius in px
var jPtr   = null; // active pointer ID

function jGeo() {
  var r = jRing.getBoundingClientRect();
  return {
    cx:   r.width  / 2,
    cy:   r.height / 2,
    maxR: r.width  / 2 - KNOB_R,
    left: r.left,
    top:  r.top
  };
}

jRing.addEventListener('pointerdown', function(e) {
  e.preventDefault();
  jRing.setPointerCapture(e.pointerId);
  jPtr = e.pointerId;
  moveJoy(e);
}, {passive:false});

jRing.addEventListener('pointermove', function(e) {
  if (e.pointerId !== jPtr) return;
  e.preventDefault();
  moveJoy(e);
}, {passive:false});

jRing.addEventListener('pointerup',     function(e){ if(e.pointerId===jPtr) releaseJoy(); }, {passive:false});
jRing.addEventListener('pointercancel', function(e){ if(e.pointerId===jPtr) releaseJoy(); }, {passive:false});

function releaseJoy() {
  jPtr = null;
  jKnob.style.transform = 'translate(0px,0px)';
  stopD();
}

function moveJoy(e) {
  var g    = jGeo();
  var dx   = e.clientX - (g.left + g.cx);
  var dy   = e.clientY - (g.top  + g.cy);
  var dist = Math.sqrt(dx * dx + dy * dy);

  if (dist > g.maxR) {
    var scale = g.maxR / dist;
    dx *= scale;
    dy *= scale;
  }

  jKnob.style.transform = 'translate(' + dx + 'px,' + dy + 'px)';

  var nd = 'S';
  if (dist > DEAD) {
    if (Math.abs(dx) > Math.abs(dy)) {
      nd = dx > 0 ? 'R' : 'L';
    } else {
      nd = dy < 0 ? 'F' : 'B';
    }
  }
  setDir(nd);
}

// Prevent page scroll ONLY inside the joystick and d-pad
document.addEventListener('touchmove', function(e) {
  var t = e.target;
  if (t.closest && (t.closest('#joyRing') || t.closest('#dpadWrap'))) {
    e.preventDefault();
  }
}, {passive:false});
</script>

</body>
</html>
)rawliteral";

// ══════════════════════════════════════════════════════════════════════
//  HELPERS
// ══════════════════════════════════════════════════════════════════════
int clampValue(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void moveServo(int ch, int angle) {
  if (ch < 0 || ch > 6) return;

  // 🚨 THE FIX: Block all direct commands to Channel 2! 
  // Channel 2 is a slave to Channel 1. It is not allowed to think for itself.
  if (ch == 2) return; 

  angle = clampValue(angle, minAngle[ch], maxAngle[ch]);
  int pulse = map(angle, 0, 180, SERVOMIN, SERVOMAX);

  if (ch == 1) {
    // 👇 Change this number to calibrate your physical gear alignment
    int servo2_offset = 0; 
    
    pwm.setPWM(1, 0, pulse);
    pwm.setPWM(2, 0, map(180 - angle + servo2_offset, 0, 180, SERVOMIN, SERVOMAX));
  } else {
    pwm.setPWM(ch, 0, pulse);
  }
}

void executeDrive(char dir) {
  lastDriveCommandMs = millis();
  if (dir == 'F') {
    analogWrite(ENA, currentSpeedPWM); analogWrite(ENB, currentSpeedPWM);
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else if (dir == 'B') {
    analogWrite(ENA, currentSpeedPWM); analogWrite(ENB, currentSpeedPWM);
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  } else if (dir == 'L') {
    analogWrite(ENA, currentSpeedPWM); analogWrite(ENB, currentSpeedPWM);
    digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else if (dir == 'R') {
    analogWrite(ENA, currentSpeedPWM); analogWrite(ENB, currentSpeedPWM);
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  } else {
    stopMotors();
  }
}

// ══════════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  Serial.println("\n[WiFi] Starting AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  delay(1500);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("[WiFi] IP: "); Serial.println(ip);

  dnsServer.start(DNS_PORT, "*", ip);

  Wire.begin(21, 22);
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(100);

  // Gentle servo wakeup — one joint at a time, 400 ms gap
  Serial.println("[ARM] Homing servos...");
  for (int i = 0; i <= 6; i++) {
    moveServo(i, homeAngle[i]);
    delay(400);
  }

  // ── HTTP Routes ─────────────────────────────────────────────────────
  server.on("/", []() {
    server.send_P(200, "text/html", htmlPage);
  });

  server.on("/set", []() {
    if (!server.hasArg("joint") || !server.hasArg("angle")) {
      server.send(400, "text/plain", "Missing args"); return;
    }
    int joint = server.arg("joint").toInt();
    int angle = server.arg("angle").toInt();
    if (joint < 0 || joint > 6) {
      server.send(400, "text/plain", "Bad joint"); return;
    }
    moveServo(joint, angle);
    server.send(200, "text/plain", "OK");
  });

  server.on("/speed", []() {
    if (!server.hasArg("val")) {
      server.send(400, "text/plain", "Missing val"); return;
    }
    currentSpeedPWM = clampValue(server.arg("val").toInt(), 0, 255);
    server.send(200, "text/plain", "OK");
  });

  server.on("/drive", []() {
    if (!server.hasArg("dir")) {
      stopMotors(); server.send(400, "text/plain", "Missing dir"); return;
    }
    char dir = server.arg("dir").charAt(0);
    if (dir != 'F' && dir != 'B' && dir != 'L' && dir != 'R' && dir != 'S') {
      stopMotors(); server.send(400, "text/plain", "Bad dir"); return;
    }
    executeDrive(dir);
    server.send(200, "text/plain", "OK");
  });

  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("[SYS] Ready!");
}

// ══════════════════════════════════════════════════════════════════════
//  LOOP
// ══════════════════════════════════════════════════════════════════════
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Safety watchdog: stop motors if heartbeat is lost
  if (millis() - lastDriveCommandMs > DRIVE_TIMEOUT_MS) {
    stopMotors();
  }
}