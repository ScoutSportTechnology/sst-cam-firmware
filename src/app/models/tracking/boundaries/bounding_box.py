from dataclasses import dataclass


@dataclass
class BoundingBox:
	x: int
	y: int
	width: int
	height: int