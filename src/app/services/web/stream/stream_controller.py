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
				img { width: 100%; max-width: none; height: auto; }
			</style>
		</head>
		<body>
			<h1>StrixCam Stream</h1>
			<img src="/api/stream/feed" alt="Video Stream">
			<div>
				<button id="startBtn">Start</button>
        <button id="stopBtn">Stop</button>
        <button id="statusBtn">Status</button>
			</div>
			<p id="status">Status: Unknown</p>

			<script>
        const streamImg = document.querySelector('img');
        const statusP = document.getElementById('status');

        async function startStream() {
          await fetch('/api/stream/start');
          streamImg.src = '/api/stream/feed?' + new Date().getTime();
          checkStatus();
        }

        async function stopStream() {
          await fetch('/api/stream/stop');
          streamImg.src = '';  // 🔌 desconectar stream
          checkStatus();
        }

        async function checkStatus() {
          const res = await fetch('/api/stream/status');
          const data = await res.json();
          statusP.innerText = 'Status: ' + data.status;
        }

        document.querySelector('#startBtn').onclick = startStream;
        document.querySelector('#stopBtn').onclick = stopStream;
        document.querySelector('#statusBtn').onclick = checkStatus;
      </script>

		</body>
		</html>
		"""
		return html
