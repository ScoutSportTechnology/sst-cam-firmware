from dataclasses import dataclass

from app.models.tracker.boundaries import Court


@dataclass
class CourtDetection:
	court: Court
	confidence: float
