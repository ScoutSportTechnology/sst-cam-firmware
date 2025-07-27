from dataclasses import dataclass
from enum import IntEnum


class Side(IntEnum):
	LEFT = 0
	RIGHT = 1


@dataclass
class FrameSideDecision:
	side: Side
	confidence: float = 0.0
