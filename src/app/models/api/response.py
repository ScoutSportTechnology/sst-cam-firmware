from enum import Enum


class ResponseModel(Enum):
	stream_status = 'StreamResponse'
	empty = 'EmptyResponse'
	html = 'HTMLResponse'
