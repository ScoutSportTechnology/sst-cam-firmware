from app.interfaces.capturer.camera import ILense


class Lense(ILense):
	def __init__(self, fov: int) -> None:
		self.__fov = fov

	@property
	def fov(self) -> int:
		return self.__fov
