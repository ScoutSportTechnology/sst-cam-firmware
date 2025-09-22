from dataclasses import dataclass

from app.models.general.sides_model import Sides


@dataclass
class SideDecisionData:
	side: Sides
	confidence: float = 0.0
