from dataclasses import dataclass

from .point_model import Point


@dataclass
class Court:
	top_left: Point
	top_right: Point
	bottom_left: Point
	bottom_right: Point
