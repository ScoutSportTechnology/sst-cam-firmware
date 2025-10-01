from dataclasses import dataclass

import numpy as np
from numpy.typing import NDArray


@dataclass
class Frame:
	data: NDArray[np.uint8]
	timestamp: float
