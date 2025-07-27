from dataclasses import dataclass

from app.models.general.point import Point


@dataclass
class ZoomData:
	center: Point
	zoom_level: float
