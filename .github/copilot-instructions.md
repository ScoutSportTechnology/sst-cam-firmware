# SST Camera Firmware - AI Agent Instructions

## Project Overview
This is firmware for an AI-powered sports camera built on NVIDIA Jetson Orin Nano with dual IMX477 cameras. The architecture follows **hexagonal architecture** (ports and adapters pattern) with clear separation between business logic and external dependencies.

## Architecture Patterns

### Hexagonal Architecture (Ports and Adapters)
- **Ports** (`app/interfaces/`) define contracts using ABC abstract base classes - these are the "ports"
- **Adapters** (`app/adapters/`) implement external integrations (hardware, networks) - these are the "adapters"  
- **Services** (`app/services/`) contain pure business logic, depend only on ports/interfaces
- **Models** (`app/models/`) are dataclasses representing domain entities

Key example: `VideoService` (business logic) depends on `ICamera` port, implemented by `Picamera2Adapter` (hardware adapter).

### Dual Camera Design
The system always operates with two cameras (`cam0`, `cam1`). Services like `VideoService` coordinate both cameras simultaneously:
```python
# In VideoService.__init__
self.cam0 = cam0  # ICamera
self.cam1 = cam1  # ICamera
```

### Configuration System
- **Hardware-specific**: `config/camera/sensors/` contains sensor-specific configs (IMX477, IMX708)
- **Global settings**: `config/settings.py` uses dataclasses with computed properties
- **Current setup**: IMX477 sensors in 2304x1296@56fps mode

## Development Workflow

### Environment Setup
```bash
# Install dependencies with uv (not pip)
uv sync

# Platform-specific extras available
uv sync --extra jetson  # For Jetson hardware
```

### Code Style (Enforced by Ruff)
- **Indentation**: Tabs only (not spaces)
- **Line length**: 150 characters
- **Quotes**: Single quotes preferred
- **Import sorting**: isort style
- Run: `ruff check` and `ruff format`

### Testing Pattern
Tests are in `src/test/` and follow integration testing approach:
- `test_dual_camera.py` - Tests both cameras with OpenCV display
- `test_single_camera.py` - Individual camera testing
- Tests use direct hardware interaction, not mocks

## Key Components

### Video Pipeline
1. **Capture**: `Picamera2Adapter` → captures frames from hardware
2. **Processing**: `VideoPreProcessorService` → `VideoTransformationService` → `VideoPostProcessorService`
3. **Tracking**: `MotionService`, `ZoomService`, `SideDecisionService` analyze frames
4. **Streaming**: `StreamService` → `StreamProviderService` handles RTMP output

### Frame Data Flow
```python
# Frame model represents image data + metadata
@dataclass
class Frame:
    data: NDArray[uint8]  # OpenCV BGR format
    timestamp: float
    # + camera metadata
```

### Tracking System
- **Detection Models**: Ball, Court, Player detection (planned AI integration)
- **Decision Services**: Motion analysis, zoom control, side switching
- **Boundaries**: Court, bounding box, point models for spatial reasoning

## Hardware Integration Points

### Camera Interface
All camera operations go through `ICamera` interface:
```python
camera.start()           # Initialize capture
frame = camera.get_frame()  # Get current frame
camera.focus()          # Auto-focus trigger
camera.stop()           # Clean shutdown
```

### Streaming Output
RTMP streaming to external servers:
```python
stream_service.stream(
    stream_protocol=StreamProtocol.RTMP, 
    url='rtmp://192.168.101.191/live/stream'
)
```

## Project-Specific Conventions

### Error Handling
- Services use `Logger` from `app.infra.logger` (custom wrapper)
- Hardware failures should gracefully degrade (single camera mode)
- No exceptions in capture loops - use status checking

### Threading Model
- Camera adapters use `threading.Thread` for capture loops
- Frame queues with `maxsize=1` for latest-frame semantics
- Services coordinate multiple threaded adapters

### Data Models
- All models are `@dataclass` (often frozen for immutability)
- Enums use `IntEnum` for serialization compatibility
- Computed properties for derived values (e.g., `resolution` from `mode`)

## Development Priorities
1. **Video streaming** (70% complete) - focus on stability
2. **AI tracking integration** - next major milestone 
3. **Zoom algorithms** - mathematical implementation in progress
4. **Audio integration** - MAX4466 microphones planned
5. **Video storage** - local recording not yet implemented

## Critical Files to Understand
- `src/main.py` - Entry point showing service composition
- `src/app/services/video/video.py` - Core video coordination logic
- `src/config/settings.py` - Hardware configuration and computed properties
- `src/app/adapters/capturer/picamera_adapter.py` - Hardware interface implementation