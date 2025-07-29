# src/app/adapters/api/fastapi_adapter.py

from collections.abc import Callable
from typing import Any

from fastapi import FastAPI

from app.interfaces.api import IApi


class FastAPIAdapter(IApi):
	def __init__(self) -> None:
		self._app = FastAPI()

	def add_route(
		self,
		path: str,
		endpoint: Callable[..., Any],
		methods: list[str],
	) -> None:
		"""
		Registers a route in FastAPI using the given path, handler, and methods.
		"""
		self._app.add_api_route(path, endpoint, methods=methods)

	def expose(self) -> FastAPI:
		"""
		Returns the FastAPI instance, ready to be used as the ASGI app.
		"""
		return self._app
