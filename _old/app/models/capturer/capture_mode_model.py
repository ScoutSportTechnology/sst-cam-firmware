from .fps_model import FPS
from .hdr_model import HDR
from .resolution_model import Resolution

CaptureMode = tuple[Resolution, FPS, HDR]
