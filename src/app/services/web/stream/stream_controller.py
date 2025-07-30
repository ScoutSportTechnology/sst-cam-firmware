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
				img { width: 80%; max-width: 800px; border: 2px solid #444; }
			</style>
		</head>
		<body>
			<h1>StrixCam Stream</h1>
			<img src="/api/stream/feed" alt="Video Stream">
			<div>
				<button onclick="fetch('/api/stream/start')">Start</button>
				<button onclick="fetch('/api/stream/stop')">Stop</button>
				<button onclick="checkStatus()">Status</button>
			</div>
			<p id="status">Status: Unknown</p>

			<script>
				async function checkStatus() {
					const res = await fetch('/api/stream/status');
					const data = await res.json();
					document.getElementById('status').innerText = 'Status: ' + data.status;
				}
			</script>
		</body>
		</html>
		"""
		return html
