from abc import ABC, abstractmethod


class IStream(ABC):
	@abstractmethod
	def start(self) -> None:
		"""Start the stream."""
		pass

	@abstractmethod
	def stop(self) -> None:
		"""Stop the stream."""
		pass

	@abstractmethod
	def get_status(self) -> str:
		"""Get the current status of the stream."""
		pass

	@abstractmethod
	def get_feed(self) -> list[bytes]:
		"""Retrieve data from the stream."""
		pass
