from collections.abc import Generator
from typing import Any

from app.interfaces import IStream
from app.interfaces.api.controller import IApiController


class StreamController(IApiController):
	def __init__(self) -> None:
		pass

	def index(self) -> str:
		html = """
        <!DOCTYPE html>
        <html>
        <head>
            <title>Stream Control</title>
            <style>
                body { font-family: sans-serif; text-align: center; margin-top: 40px; }
                button { margin: 5px; padding: 10px 20px; font-size: 16px; }
                #streamImg, #player { width: 100%; height: auto; }
                #player-container { display: none; }
                .source-selector { margin-bottom: 20px; }
            </style>
            <!-- Incluye srs-player para FLV/HLS -->
            <script src="https://unpkg.com/srs-player@latest/dist/srs.min.js"></script>
        </head>
        <body>
            <h1>StrixCam Stream</h1>
            
            <!-- Selector de fuente -->
            <div class="source-selector">
                <label><input type="radio" name="source" value="mjpeg" checked> MJPEG</label>
                <label><input type="radio" name="source" value="flv"> HTTP-FLV</label>
                <label><input type="radio" name="source" value="hls"> HLS</label>
            </div>

            <!-- Contenedor MJPEG -->
            <div id="mjpeg-container">
                <img id="streamImg" src="/api/stream/feed" alt="Video Stream">
            </div>

            <!-- Contenedor srs-player -->
            <div id="player-container">
                <video id="player" controls></video>
            </div>

            <div>
                <button id="startBtn">Start</button>
                <button id="stopBtn">Stop</button>
                <button id="statusBtn">Status</button>
                <button id="focusBtn">Focus</button>
            </div>
            <p id="status">Status: Unknown</p>

            <script>
                const streamImg = document.getElementById('streamImg');
                const mjpegContainer = document.getElementById('mjpeg-container');
                const playerContainer = document.getElementById('player-container');
                const statusP = document.getElementById('status');
                let player = null;

                // URLs de tu PC
                const FLV_URL = 'http://192.168.101.191:8080/live/livestream.flv';
                const HLS_URL = 'http://192.168.101.191:8080/live/livestream.m3u8';

                // Funciones de control
                async function startStream() {
                    await fetch('/api/stream/start');
                    if (document.querySelector('input[name="source"]:checked').value === 'mjpeg') {
                        streamImg.src = '/api/stream/feed?' + Date.now();
                    }
                    checkStatus();
                }
                async function stopStream() {
                    await fetch('/api/stream/stop');
                    if (player) player.pause();
                    if (document.querySelector('input[name="source"]:checked').value === 'mjpeg') {
                        streamImg.src = '';
                    }
                    checkStatus();
                }
                async function checkStatus() {
                    const res = await fetch('/api/stream/status');
                    const data = await res.json();
                    statusP.innerText = 'Status: ' + data.status;
                }
                async function triggerFocus() {
                    await fetch('/api/stream/focus');
                }

                // Inicializa control de botones
                document.getElementById('startBtn').onclick = startStream;
                document.getElementById('stopBtn').onclick = stopStream;
                document.getElementById('statusBtn').onclick = checkStatus;
                document.getElementById('focusBtn').onclick = triggerFocus;

                // Cambio de fuente de vídeo
                document.querySelectorAll('input[name="source"]').forEach(radio => {
                    radio.addEventListener('change', () => {
                        const sel = radio.value;
                        if (sel === 'mjpeg') {
                            mjpegContainer.style.display = '';
                            playerContainer.style.display = 'none';
                            if (player) { player.destroy(); player = null; }
                        } else {
                            mjpegContainer.style.display = 'none';
                            playerContainer.style.display = '';
                            const url = sel === 'flv' ? FLV_URL : HLS_URL;
                            const type = sel === 'flv' ? 'flv' : 'hls';
                            if (!player) {
                                player = new SrsPlayer('#player', { url, type });
                            } else {
                                player.switchURL(url);
                            }
                        }
                    });
                });
            </script>
        </body>
        </html>
        """
		return html
