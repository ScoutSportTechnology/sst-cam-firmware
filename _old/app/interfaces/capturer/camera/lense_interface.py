from abc import ABC, abstractmethod


class ILense(ABC):
	@abstractmethod
	def __init__(self, fov: int) -> None: ...

	@property
	@abstractmethod
	def fov(self) -> int: ...