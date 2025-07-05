# Tin Ear - Professional Spatial Audio Plugin

A high-performance C++ audio plugin for real-time binaural spatial audio processing, optimized for ARM Cortex-M7 embedded systems and the distingNT hardware platform.

## Overview

Tin Ear enables positioning of mono audio sources in 3D virtual space with real-time parameter control. The plugin uses custom HRTF (Head-Related Transfer Function) processing to create convincing binaural audio effects without requiring external audio libraries.

## Features

### 🎯 Real-Time 3D Positioning
- **Azimuth Control**: -180° to +180° horizontal positioning
- **Elevation Control**: -90° to +90° vertical positioning  
- **Distance Control**: 1-100 meters with realistic attenuation
- **Smooth Parameter Interpolation**: Seamless transitions for natural sound movement

### 🔊 Advanced Spatial Audio Processing
- **ITD (Interaural Time Delay)**: Realistic time-of-arrival differences between ears
- **ILD (Interaural Level Difference)**: Frequency-dependent level differences
- **Head-Shadow Filtering**: High-frequency shelf filters simulate head obstruction
- **Pinna Effect Simulation**: Elevation-dependent notch filters for realistic ear shaping
- **Early Reflections**: Distance-based reflection modeling for spatial depth
- **Air Absorption**: High-frequency rolloff for distance realism

### ⚡ Performance Optimized
- **ARM Cortex-M7 Target**: Optimized for embedded audio processing
- **Ultra-Low Latency**: Minimal processing delay for real-time performance
- **Efficient Memory Usage**: Streamlined DSP algorithms with small footprint
- **Self-Contained**: No external audio library dependencies

## Technical Architecture

### Core Components

**`th_tinear.cpp`** - Main plugin implementation
- distingNT_API plugin framework integration
- Real-time parameter handling with slew limiting
- Audio routing and buffer management
- Plugin lifecycle management

**`professional_spatial_audio.cpp`** - DSP engine
- Custom HRTF rendering algorithms
- Biquad filter chains for frequency shaping
- Fractional delay lines for ITD processing
- Real-time coefficient smoothing

### DSP Pipeline

```
Mono Input → Early Reflections → Air Absorption → ITD Processing → 
ILD Scaling → Head-Shadow Filtering → Pinna Notching → Stereo Output
```

### Parameter Space

| Parameter | Range | Description |
|-----------|-------|-------------|
| Azimuth | -180° to +180° | Horizontal source position |
| Elevation | -90° to +90° | Vertical source position |
| Distance | 1m to 100m | Source distance with scaling |
| Input Channel | 1-16 | Source audio input selection |
| Output L/R | 1-16 | Stereo output channel routing |
| Output Mode | Add/Replace | Audio mixing behavior |

## Building

### Prerequisites

- ARM GCC cross-compiler (`arm-none-eabi-gcc`)
- distingNT_API (included as submodule)
- Make build system

### Build Commands

```bash
# Clean previous builds
make clean

# Build plugin binary
make

# Output: plugins/th_tinear_plugin.o
```

### Compiler Settings

- **Target**: ARM Cortex-M7 with FPU
- **Standard**: C++14 
- **Optimization**: Size optimized (`-Os`)
- **ABI**: Hard float with FPv5-D16
- **Features**: No RTTI, no exceptions for minimal runtime

## Installation

The compiled plugin binary (`th_tinear_plugin.o`) can be loaded onto distingNT hardware using the standard plugin installation process.

## Usage

### Basic Operation

1. **Route Audio**: Select input channel containing your mono audio source
2. **Set Outputs**: Choose left and right output channels for binaural audio
3. **Position Source**: Adjust azimuth and elevation for desired spatial location
4. **Set Distance**: Control apparent source distance and associated effects
5. **Output Mode**: Choose Add to mix with existing audio, or Replace to override

### Advanced Tips

- **Smooth Movement**: Parameters include automatic slew limiting for natural transitions
- **Distance Effects**: Longer distances add air absorption and reduce level
- **Elevation Cues**: High elevations create distinctive pinna filtering effects
- **Real-Time Control**: All parameters can be automated or controlled via hardware

## Performance Characteristics

- **Latency**: Sub-millisecond processing delay
- **CPU Usage**: Optimized for real-time embedded processing
- **Memory**: Minimal SRAM footprint with efficient buffer management
- **Sample Rate**: 48kHz optimized with configurable processing

## Development Status

✅ **Complete Implementation**
- Full distingNT_API integration
- Professional spatial audio DSP
- ARM Cortex-M7 optimization
- Real-time parameter control
- Comprehensive build system

## Technical Documentation

For detailed implementation information, see:
- [`CLAUDE.md`](CLAUDE.md) - Development guidelines and technical details
- [`scripts/PRD.txt`](scripts/PRD.txt) - Original product requirements
- Source code comments for DSP algorithm explanations

## License

This project is developed for the distingNT hardware platform. Please refer to distingNT_API licensing terms for usage rights and restrictions.

## Contributing

See [`CLAUDE.md`](CLAUDE.md) for development guidelines, coding standards, and technical architecture details.

---

*Tin Ear - Professional spatial audio processing for embedded systems*