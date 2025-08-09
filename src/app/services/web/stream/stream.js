const playerContainer = document.getElementById('player-container');
const statusP = document.getElementById('status');
const startBtn = document.getElementById('startBtn');
const stopBtn = document.getElementById('stopBtn');
const statusBtn = document.getElementById('statusBtn');
const focusBtn = document.getElementById('focusBtn');

const FLV_URL = 'http://192.168.101.191:8080/live/livestream.flv';
const HLS_URL = 'http://192.168.101.191:8080/live/livestream.m3u8';

let player = null;

async function startStream() {
  await fetch('/api/stream/start');
  const url = getURL();
  const type = getType();
  if (!player) {
    player = new SrsPlayer('#player', { url, type });
  } else {
    player.switchURL(url);
  }
  checkStatus();
}

async function stopStream() {
  await fetch('/api/stream/stop');
  if (player && typeof player.pause === 'function') {
    player.pause();
  }
  checkStatus();
}

async function checkStatus() {
  try {
    const res = await fetch('/api/stream/status');
    const data = await res.json();
    statusP.innerText = 'Status: ' + data.status;
  } catch {
    statusP.innerText = 'Status: error';
  }
}

async function triggerFocus() {
  await fetch('/api/stream/focus');
}

function getURL() {
  return document.querySelector('input[name="source"]:checked').value === 'flv'
    ? FLV_URL
    : HLS_URL;
}

function getType() {
  return document.querySelector('input[name="source"]:checked').value === 'flv'
    ? 'flv'
    : 'hls';
}

startBtn.onclick = startStream;
stopBtn.onclick = stopStream;
statusBtn.onclick = checkStatus;
focusBtn.onclick = triggerFocus;

document.querySelectorAll('input[name="source"]').forEach(radio => {
  radio.addEventListener('change', () => {
    const url = getURL();
    const type = getType();
    if (!player) {
      player = new SrsPlayer('#player', { url, type });
    } else {
      player.switchURL(url);
    }
  });
});

checkStatus();