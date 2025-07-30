from enum import Enum

from fastapi.responses import HTMLResponse, JSONResponse, StreamingResponse

from app.models.api import ResponseModel

FastAPIResponseClassMap = {
	ResponseModel.empty: JSONResponse,
	ResponseModel.stream_status: StreamingResponse,
	ResponseModel.html: HTMLResponse,
}

FastAPIMediaTypeMap = {
	ResponseModel.empty: 'application/json',
	ResponseModel.stream_status: 'multipart/x-mixed-replace; boundary=frame',
	ResponseModel.html: 'text/html',
}
