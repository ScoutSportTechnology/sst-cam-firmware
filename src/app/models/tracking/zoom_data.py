from dataclasses import dataclass

from src.app.models.tracking.boundaries import Point


@dataclass
class ZoomData:
    center: Point
    zoom_level: float