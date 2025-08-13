from dataclasses import dataclass

from app.models.tracker.boundaries import Point


@dataclass
class ZoomData:
	center: Point
	zoom_level: float
