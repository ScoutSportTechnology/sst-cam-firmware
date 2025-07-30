# src/app/adapters/api/fastapi_adapter.py

from collections.abc import Callable
from typing import Any

from fastapi import FastAPI
from fastapi.responses import HTMLResponse, JSONResponse, StreamingResponse

from app.adapters.software.api.fastapi_map_adapter import (
	FastAPIMediaTypeMap,
	FastAPIResponseClassMap,
)
from app.interfaces.api import IApi
from app.models.api import ResponseModel


class FastAPIAdapter(IApi):
	def __init__(self) -> None:
		self._app = FastAPI()

	def add_route(
		self,
		path: str,
		endpoint: Callable[..., Any],
		methods: list[str],
		response_model: ResponseModel = ResponseModel.empty,
	) -> None:
		"""
		Wraps the endpoint with the correct FastAPI response class and media type,
		if needed (e.g., for streaming).
		"""

		response_class = FastAPIResponseClassMap[response_model]
		media_type = FastAPIMediaTypeMap.get(response_model)

		def wrapped_endpoint():
			result = endpoint()
			if response_class == StreamingResponse:
				return StreamingResponse(result, media_type=media_type)
			elif response_class == HTMLResponse:
				return HTMLResponse(content=result, media_type=media_type)
			else:
				return JSONResponse(result, media_type=media_type)

		self._app.add_api_route(
			path=path,
			endpoint=wrapped_endpoint,
			methods=methods,
			response_class=response_class,
		)

	def expose(self) -> FastAPI:
		return self._app
