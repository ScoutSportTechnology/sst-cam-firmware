from dataclasses import dataclass

from app.models.tracker.boundaries import Point


@dataclass
class BallDetection:
	center: Point
	confidence: float
