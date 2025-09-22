from dataclasses import dataclass
from typing import Any, Optional


@dataclass
class ControllerPayload:
	command: str
	data: dict[str, Any]
