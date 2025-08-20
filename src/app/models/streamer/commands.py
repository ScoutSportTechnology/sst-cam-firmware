from enum import Enum


class StreamCommands(Enum):
	START = 'start'
	STOP = 'stop'
	RESTART = 'restart'
	STATUS = 'status'
	FOCUS = 'focus'
	STREAM = 'stream'

	def __str__(self):
		return self.value
