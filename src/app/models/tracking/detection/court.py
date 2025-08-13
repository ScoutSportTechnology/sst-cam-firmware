from dataclasses import dataclass

from src.app.models.tracking.boundaries import Court


@dataclass
class CourtDetection:
    court: Court
    confidence: float