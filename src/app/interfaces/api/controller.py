from abc import ABC, abstractmethod
from collections.abc import Callable
from typing import Any


class IApiController(ABC):
	pass


class IWebController(IApiController):
	@abstractmethod
	def index(self, css, js, html) -> str: ...
