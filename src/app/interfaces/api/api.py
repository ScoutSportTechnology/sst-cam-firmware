from abc import ABC, abstractmethod
from collections.abc import Callable
from typing import Any

from app.models.api import ResponseModel


class IApi(ABC):
	@abstractmethod
	def add_route(
		self,
		path: str,
		endpoint: Callable[..., Any],
		methods: list[str],
		response_model: ResponseModel,
	) -> None: ...

	@abstractmethod
	def expose(self) -> Any: ...
