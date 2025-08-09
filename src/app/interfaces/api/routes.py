from abc import ABC, abstractmethod
from collections.abc import Callable
from typing import Any

from app.interfaces.api.controller import IApiController


class IApiRoutes(ABC):
	@abstractmethod
	def register(self) -> None: ...


class IWebRoutes(ABC):
	@abstractmethod
	def register(self) -> None: ...
