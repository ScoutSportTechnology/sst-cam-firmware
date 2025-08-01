import logging
from enum import Enum


class LogLevel(Enum):
	INFO = ('INFO', logging.INFO, '\033[92m')
	ERROR = ('ERROR', logging.ERROR, '\033[91m')
	WARNING = ('WARNING', logging.WARNING, '\033[93m')
	DEBUG = ('DEBUG', logging.DEBUG, '\033[94m')

	def __init__(self, label: str, log_level: int, color: str):
		self.label = label
		self.log_level = log_level
		self.color = color


class Logger:
	def __init__(self, name: str | None = None) -> None:
		name_logger = 'uvicorn.error'
		self.logger = logging.getLogger(name_logger)
		self.logger.setLevel(LogLevel.INFO.log_level)
		self.name = name

	def __format_msg__(self, level: LogLevel, msg: object, args: tuple) -> str:
		endc = '\033[0m'
		args_str = f' {args}' if args else ''
		name_str = f'[{self.name}] ' if self.name else ''
		return f'{level.color}{level.label}{endc}: {name_str}{msg}{args_str}'

	def __log__(self, level: LogLevel, msg: object, *args: object) -> None:
		print(self.__format_msg__(level, msg, args), flush=True)
		self.logger.log(level.log_level, msg, *args)

	# Optional convenience methods
	def info(self, msg: object, *args: object) -> None:
		self.__log__(LogLevel.INFO, msg, *args)

	def error(self, msg: object, *args: object) -> None:
		self.__log__(LogLevel.ERROR, msg, *args)

	def warning(self, msg: object, *args: object) -> None:
		self.__log__(LogLevel.WARNING, msg, *args)

	def debug(self, msg: object, *args: object) -> None:
		self.__log__(LogLevel.DEBUG, msg, *args)
