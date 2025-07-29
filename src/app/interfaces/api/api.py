from abc import ABC, abstractmethod
from collections.abc import Callable
from typing import Any


class IApi(ABC):
	@abstractmethod
	def add_route(
		self,
		path: str,
		endpoint: Callable[..., Any],
		methods: list[str],
	) -> None:
		"""
		Registers a route handler for the given path and methods.

		:param path: The URL path (e.g., '/api/stream/start')
		:param endpoint: The handler function to execute
		:param methods: HTTP methods (e.g., ['GET'], ['POST'])
		"""
		pass

	@abstractmethod
	def expose(self) -> Any:
		"""
		Returns the framework-specific app instance (e.g., FastAPI, Flask app).
		This is what you pass to the web server to run the app.
		"""
		pass
