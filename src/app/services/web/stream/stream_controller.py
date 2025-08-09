from pathlib import Path

from app.interfaces.api.controller import IWebController


class StreamController(IWebController):
	def __init__(self, base_dir: Path) -> None:
		self.base = base_dir / 'web' / 'stream'

	def index(self, css, js, html) -> str:
		html = (self.base / 'index.html').read_text(encoding='utf-8')
		css = (self.base / 'stream.css').read_text(encoding='utf-8')
		js = (self.base / 'stream.js').read_text(encoding='utf-8')
		html = html.replace('{{style}}', css).replace('{{script}}', js)
		return html
