# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++ audio plugin project for real-time binaural audio processing using HRTF (Head-Related Transfer Function) data. The plugin enables positioning sound sources in 3D virtual space with real-time control.

## Key Features

- **Real-Time Control**: Users can adjust X, Y, Z coordinates of audio sources in real time
- **Smooth Interpolation**: Seamless transitions between data points for natural sound movement
- **Lightweight Data Handling**: Optimized HRTF data for minimal processing load, suitable for ARM processors
- **User Interface**: Intuitive UI for positioning and controlling sound sources

## Technical Requirements

- **Core Implementation**: Custom spatial audio processing using optimized DSP algorithms
- **Spatial Audio Features**:
  - Real-time HRTF synthesis with biquad filters
  - ITD (Interaural Time Delay) and ILD (Interaural Level Difference) processing
  - Head-shadow and pinna effect simulation
  - Early reflection and air absorption modeling
- **Compatibility**: Optimized for ARM Cortex-M7 embedded systems
- **Performance**: Ultra-low latency processing with minimal memory footprint
- **Dependencies**: 
  - distingNT_API for plugin framework (submodule at `libs/distingNT_API`)
  - VISR framework available but not currently used (submodule at `libs/VISR`)
  - Self-contained spatial audio implementation in `professional_spatial_audio.cpp`

## Development Status

The project has a functional implementation with:
- Complete plugin framework integration using distingNT_API
- Custom spatial audio processing implementation in `professional_spatial_audio.cpp`
- Main plugin code in `th_tinear.cpp` with real-time parameter control
- ARM Cortex-M7 optimized build system
- PRD document available at `scripts/PRD.txt` for reference

## Plugin Development Guide for distingNT_API

### Core Architecture

#### Plugin Structure
- Inherit from `_NT_algorithm` base class
- Define a custom algorithm structure 
- Use `_NT_factory` for plugin lifecycle management

#### Key Callback Methods
1. `calculateRequirements()`: 
   - Define memory requirements
   - Specify parameter count
   - Allocate SRAM/DRAM/DTC/ITC memory
   ```cpp
   void calculateRequirements(_NT_algorithmRequirements& req, const int32_t* specifications) {
       req.numParameters = ARRAY_SIZE(parameters);
       req.sram = sizeof(YourAlgorithmStruct);
       req.dram = 0;  // Optional additional memory
       req.dtc = 0;   // Performance-critical data
       req.itc = 0;   // Code memory
   }
   ```

2. `construct()`: 
   - Create plugin instance
   - Initialize parameters and parameter pages
   ```cpp
   _NT_algorithm* construct(const _NT_algorithmMemoryPtrs& ptrs, 
                            const _NT_algorithmRequirements& req, 
                            const int32_t* specifications) {
       YourAlgorithm* alg = new (ptrs.sram) YourAlgorithm();
       alg->parameters = parameters;
       alg->parameterPages = &parameterPages;
       return alg;
   }
   ```

3. `step()`: 
   - Primary audio processing method
   - Process audio buffers 
   ```cpp
   void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
       YourAlgorithm* pThis = (YourAlgorithm*)self;
       int numFrames = numFramesBy4 * 4;
       // Audio processing logic here
   }
   ```

#### Parameter Management
- Define parameters using `_NT_parameter`
- Create parameter pages for UI organization
- Use macros like `NT_PARAMETER_AUDIO_INPUT()`, `NT_PARAMETER_AUDIO_OUTPUT()`
```cpp
static const _NT_parameter parameters[] = {
    NT_PARAMETER_AUDIO_INPUT("Input", 1, 1),
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13),
    // Custom parameters
};
```

#### Optional Advanced Features
- `draw()`: Custom UI rendering
- `midiMessage()`: Handle MIDI input
- `customUi()`: Custom UI interaction
- `serialise()`/`deserialise()`: Preset saving/loading

#### Plugin Entry Point
```cpp
uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
    case kNT_selector_version:
        return kNT_apiVersionCurrent;
    case kNT_selector_numFactories:
        return 1;
    case kNT_selector_factoryInfo:
        return (uintptr_t)(data == 0 ? &factory : NULL);
    }
    return 0;
}
```

### Compilation Notes
- Target: ARM Cortex-M7
- Compiler Flags:
  ```
  -std=c++11
  -mcpu=cortex-m7
  -mfpu=fpv5-d16
  -mfloat-abi=hard
  -fno-rtti 
  -fno-exceptions
  -Os
  ```

## Next Steps

Based on the PRD, the project needs:
1. C++ plugin framework setup (VST3, AU, AAX)
2. HRTF data processing implementation
3. Real-time audio processing pipeline
4. User interface development
5. Performance optimization for ARM processors

## Current Plugin Implementation Details

### TH Tinear HRTF Plugin Architecture

#### Core Functionality
- Stereo input and output with independent routing
- 3D spatial positioning through azimuth and elevation parameters
- Placeholder for Steam Audio HRTF rendering

#### Parameter Space
- Input Left/Right channels
- Output Left/Right channels
- Azimuth: -180 to 180 degrees
- Elevation: -90 to 90 degrees

#### Spatial Positioning Algorithm
- Converts azimuth to X/Z coordinates on a unit sphere
- Converts elevation to Y coordinate
- Fixed source distance of 1 meter from listener
- Listener always positioned at (0, 0, 0)

#### Current Implementation Details
- Custom HRTF rendering with biquad filters and delay lines
- Real-time ITD (Interaural Time Delay) processing
- ILD (Interaural Level Difference) simulation
- Head-shadow filtering with frequency-dependent shelf filters
- Pinna effect simulation with elevation-dependent notch filters
- Early reflections and air absorption modeling
- Smooth parameter interpolation for seamless audio transitions

#### Technical Features
- Professional spatial audio processing without external dependencies
- Optimized DSP algorithms for ARM Cortex-M7
- Minimal memory footprint suitable for embedded systems
- Real-time parameter smoothing for natural sound movement
- Distance-dependent air absorption simulation

#### Compilation Notes
- Targets ARM Cortex-M7 architecture
- Compiled with minimal C++ runtime
- Uses custom plugin framework from distingNT_API
- Self-contained spatial audio implementation