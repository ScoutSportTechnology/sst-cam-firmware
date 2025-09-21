from abc import ABC, abstractmethod
from typing import Any

from app.models.controller import ControllerPayload


class IController(ABC):
	@abstractmethod
	def __init__(self, service: Any) -> None: ...

	@abstractmethod
	def _on_command(self, payload: ControllerPayload) -> None: ...
