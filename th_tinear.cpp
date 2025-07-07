#include <distingnt/api.h>
#include <cmath>
#include <new>
#include <cstring>  // for memcpy

// Include spatial audio definitions
#include <cstring>  // for memset

// Forward declarations for spatial audio classes
class Biquad {
public:
    Biquad() { clear(); }
    float process(float x);
    void setCoeffs(float _b0, float _b1, float _b2, float _a0, float _a1, float _a2);
    void clear() { b0 = 1; b1 = b2 = a1 = a2 = z1 = z2 = 0; }
private:
    float b0{}, b1{}, b2{}, a1{}, a2{}, z1{}, z2{};
};

class OnePoleLP {
public:
    OnePoleLP() : alpha(0.0f), y1(0.0f) {}
    void setCutoff(float fc);
    float process(float x);
private:
    float alpha, y1;
};

class DelayLine {
public:
    DelayLine() : writeIdx(0) { memset(buf, 0, sizeof(buf)); }
    float process(float x, float delaySamples);
private:
    static constexpr int kMax = 512;
    static constexpr int kMask = kMax - 1;
    float buf[kMax]{};
    int writeIdx;
};

// Per-emitter spatial audio state structure
struct SpatialAudioState {
    Biquad    notchL, notchR, shelfL, shelfR;
    OnePoleLP airLP;
    DelayLine delayL, delayR;
    DelayLine reflDelay;
    
    float prevSinAz;      // smoothed sin(azimuth)
    float prevElevN;      // smoothed elevation norm
    float prevDist;
    
    SpatialAudioState() : prevSinAz(0.0f), prevElevN(0.0f), prevDist(1.0f) {}
};

// Professional spatial audio function declaration
extern "C" {
void applyMonoSpatialAudio(const float *input, float *outputL, float *outputR,
                           int numSamples, float sourceX, float sourceY,
                           float sourceZ, SpatialAudioState* state);
}

// Add missing constants
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Maximum number of emitters supported
constexpr int kMaxEmitters = 8;

// Forward declarations
static const char* const enumStringsAutoSpread[] = {
    "Off",
    "On"
};

static const _NT_parameter commonParameters[] = {
    {.name = "Auto Spread",
     .min = 0,
     .max = 1,
     .def = 0,
     .unit = kNT_unitEnum,
     .scaling = 0,
     .enumStrings = enumStringsAutoSpread},
};

static const _NT_parameter routingParameters[] = {
    {.name = "Output L", .min = 1, .max = 28, .def = 13, .unit = kNT_unitAudioOutput, .scaling = 0, .enumStrings = nullptr},
    {.name = "Output L mode", .min = 0, .max = 1, .def = 0, .unit = kNT_unitOutputMode, .scaling = 0, .enumStrings = nullptr},
    {.name = "Output R", .min = 1, .max = 28, .def = 14, .unit = kNT_unitAudioOutput, .scaling = 0, .enumStrings = nullptr},
};

static const _NT_parameter perEmitterParameters[] = {
    {.name = "Input", .min = 1, .max = 28, .def = 1, .unit = kNT_unitAudioInput, .scaling = 0, .enumStrings = nullptr},
    {.name = "Azimuth", .min = -180, .max = 180, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = nullptr},
    {.name = "Elevation", .min = -90, .max = 90, .def = 0, .unit = kNT_unitNone, .scaling = 0, .enumStrings = nullptr},
    {.name = "Distance", .min = 1, .max = 100, .def = 10, .unit = kNT_unitNone, .scaling = kNT_scaling10, .enumStrings = nullptr},
    {.name = "Gain", .min = -60, .max = 0, .def = 0, .unit = kNT_unitDb, .scaling = 0, .enumStrings = nullptr},
};

static const char* const emitterPageNames[] = {
    "Emitter 1", "Emitter 2", "Emitter 3", "Emitter 4",
    "Emitter 5", "Emitter 6", "Emitter 7", "Emitter 8"
};

static const char* const emitterInputNames[] = {
    "Emitter 1 Input", "Emitter 2 Input", "Emitter 3 Input", "Emitter 4 Input",
    "Emitter 5 Input", "Emitter 6 Input", "Emitter 7 Input", "Emitter 8 Input"
};

// Common parameter indices
enum {
    kParamAutoSpread,
    kNumCommonParameters,
};

// Routing parameter indices
enum {
    kParamOutputL = kNumCommonParameters,
    kParamOutputMode,
    kParamOutputR,
    kNumRoutingParameters = 3,
};

// Per-emitter parameter indices
enum {
    kParamEmitterInput,
    kParamEmitterAzimuth,
    kParamEmitterElevation,
    kParamEmitterDistance,
    kParamEmitterAttenuation,
    kNumPerEmitterParameters,
};

static const uint8_t commonParams[] = { kParamAutoSpread };
static const uint8_t routingParams[] = { kParamOutputL, kParamOutputMode, kParamOutputR };

struct tinEarAlgorithm : _NT_algorithm {
    tinEarAlgorithm(int32_t numEmitters_) 
        : _NT_algorithm(), numEmitters(numEmitters_) {
        pagesDefs.numPages = 2 + numEmitters;  // Common + Emitter pages + Routing page
        pagesDefs.pages = pageDefs;
        
        // Copy common parameters
        memcpy(parameterDefs, commonParameters, kNumCommonParameters * sizeof(_NT_parameter));
        
        // Copy routing parameters
        memcpy(parameterDefs + kNumCommonParameters, routingParameters, kNumRoutingParameters * sizeof(_NT_parameter));
        
        // Copy per-emitter parameters with custom input names
        for (int i = 0; i < numEmitters; ++i) {
            int baseIdx = kNumCommonParameters + kNumRoutingParameters + i * kNumPerEmitterParameters;
            memcpy(parameterDefs + baseIdx, perEmitterParameters, kNumPerEmitterParameters * sizeof(_NT_parameter));
            
            // Customize the input parameter name for this emitter
            parameterDefs[baseIdx + kParamEmitterInput].name = emitterInputNames[i];
            parameterDefs[baseIdx + kParamEmitterInput].def = static_cast<int16_t>(1 + i);
        }
        
        // Create Common page (page 0)
        pageDefs[0].name = "Common";
        pageDefs[0].numParams = kNumCommonParameters;
        pageDefs[0].params = commonParams;
        
        // Create emitter pages (pages 1 to numEmitters)
        for (int i = 0; i < numEmitters; ++i) {
            pageDefs[i + 1].name = emitterPageNames[i];
            pageDefs[i + 1].numParams = kNumPerEmitterParameters;
            uint8_t* p = pageParams + i * kNumPerEmitterParameters;
            pageDefs[i + 1].params = p;
            for (int j = 0; j < kNumPerEmitterParameters; ++j) {
                p[j] = kNumCommonParameters + kNumRoutingParameters + i * kNumPerEmitterParameters + j;
            }
        }
        
        // Create routing page (last page)
        pageDefs[numEmitters + 1].name = "Routing";
        pageDefs[numEmitters + 1].numParams = kNumRoutingParameters;
        pageDefs[numEmitters + 1].params = routingParams;
        
        // Set algorithm members
        parameters = parameterDefs;
        parameterPages = &pagesDefs;
    }

    ~tinEarAlgorithm() = default;
    
    // Number of emitters for this instance
    int32_t numEmitters;

    // Per-emitter data arrays (sized for max emitters)
    float targetAzimuth[kMaxEmitters] = {};
    float targetElevation[kMaxEmitters] = {};
    float targetDistance[kMaxEmitters] = {};
    float targetAttenuation[kMaxEmitters] = {};  // in dB
    
    float currentAzimuth[kMaxEmitters] = {};
    float currentElevation[kMaxEmitters] = {};
    float currentDistance[kMaxEmitters] = {};
    float currentAttenuation[kMaxEmitters] = {};  // in dB
    
    float sourceX[kMaxEmitters] = {};
    float sourceY[kMaxEmitters] = {};
    float sourceZ[kMaxEmitters] = {};
    
    // Auto-spread enabled flag
    bool autoSpreadEnabled = false;

    // Per-emitter spatial audio state (allocated in DTC memory)
    SpatialAudioState* spatialStates = nullptr;

    // Slew limiting - smooth over approximately 20ms at 48kHz
    static constexpr float SLEW_RATE = 0.001f;

    // Audio processing buffers for spatial audio
    static const int MAX_BUFFER_SIZE = 256;
    float tempOutputL[MAX_BUFFER_SIZE]{};
    float tempOutputR[MAX_BUFFER_SIZE]{};
    
    // Dynamic parameter storage
    _NT_parameter parameterDefs[kNumCommonParameters + kNumRoutingParameters + kMaxEmitters * kNumPerEmitterParameters];
    _NT_parameterPages pagesDefs;
    _NT_parameterPage pageDefs[2 + kMaxEmitters];  // Common + Emitter pages + Routing
    uint8_t pageParams[kMaxEmitters * kNumPerEmitterParameters];
    

    // Slew limiting function
    static float slewLimit(float current, float target, float rate) {
        float diff = target - current;
        if (diff > rate) {
            return current + rate;
        } else if (diff < -rate) {
            return current - rate;
        }
        return target;
    }
    
    // Convert dB to linear gain
    static float dbToLinear(float db) {
        return powf(10.0f, db / 20.0f);
    }
};


void calculateRequirements(_NT_algorithmRequirements &req,
                           const int32_t *specifications) {
    int32_t numEmitters = specifications[0];
    
    req.numParameters = kNumCommonParameters + kNumRoutingParameters + numEmitters * kNumPerEmitterParameters;
    req.sram = sizeof(tinEarAlgorithm);
    req.dram = 0;
    req.dtc = numEmitters * sizeof(SpatialAudioState);  // Allocate per-emitter spatial audio state
    req.itc = 0;
}

_NT_algorithm *construct(const _NT_algorithmMemoryPtrs &ptrs,
                         const _NT_algorithmRequirements &,  // unused
                         const int32_t *specifications) {
    int32_t numEmitters = specifications[0];
    
    auto *alg = new(ptrs.sram) tinEarAlgorithm(numEmitters);
    
    // Initialize per-emitter spatial audio states in DTC memory
    if (ptrs.dtc && numEmitters > 0) {
        alg->spatialStates = reinterpret_cast<SpatialAudioState*>(ptrs.dtc);
        for (int i = 0; i < numEmitters; ++i) {
            new(&alg->spatialStates[i]) SpatialAudioState();
        }
    }
    
    return alg;
}

void parameterChanged(_NT_algorithm *self, int p) {
    auto *pThis = (tinEarAlgorithm *) self;
    
    // Handle common parameters
    if (p == kParamAutoSpread) {
        pThis->autoSpreadEnabled = (pThis->v[kParamAutoSpread] == 1);
        
        // If auto-spread is enabled, update all emitter azimuths
        if (pThis->autoSpreadEnabled && pThis->numEmitters > 1) {
            float spreadStep = 180.0f / (pThis->numEmitters - 1);
            for (int i = 0; i < pThis->numEmitters; ++i) {
                pThis->targetAzimuth[i] = (-90.0f + i * spreadStep) * (M_PI / 180.0f);
            }
        }
    }
    
    // Handle per-emitter parameters
    if (p >= kNumCommonParameters + kNumRoutingParameters) {
        int relativeIdx = p - (kNumCommonParameters + kNumRoutingParameters);
        int emitterIdx = relativeIdx / kNumPerEmitterParameters;
        int paramType = relativeIdx % kNumPerEmitterParameters;
        
        if (emitterIdx < pThis->numEmitters) {
            switch (paramType) {
                case kParamEmitterInput:
                    // Input parameter - no action needed
                    break;
                    
                case kParamEmitterAzimuth:
                    if (!pThis->autoSpreadEnabled) {
                        pThis->targetAzimuth[emitterIdx] = 
                            pThis->v[p] * (M_PI / 180.0f);
                    }
                    break;
                    
                case kParamEmitterElevation:
                    pThis->targetElevation[emitterIdx] = 
                        pThis->v[p] * (M_PI / 180.0f);
                    break;
                    
                case kParamEmitterDistance:
                    pThis->targetDistance[emitterIdx] = 
                        pThis->v[p] / 10.0f;
                    break;
                    
                case kParamEmitterAttenuation:
                    pThis->targetAttenuation[emitterIdx] = pThis->v[p];
                    break;
            }
        }
    }
}

void step(_NT_algorithm *self, float *busFrames, int numFramesBy4) {
    auto *pThis = (tinEarAlgorithm *) self;
    const int numFrames = numFramesBy4 * 4;

    // Output channels
    float *outL = busFrames + (pThis->v[kParamOutputL] - 1) * numFrames;
    float *outR = busFrames + (pThis->v[kParamOutputR] - 1) * numFrames;

    // Output mode (0 = Add, 1 = Replace)
    bool replaceMode = pThis->v[kParamOutputMode];
    
    // Clear output buffers if in replace mode
    if (replaceMode) {
        for (int i = 0; i < numFrames; ++i) {
            outL[i] = 0.0f;
            outR[i] = 0.0f;
        }
    }

    // Process in chunks to handle arbitrary buffer sizes
    constexpr int maxChunkSize = tinEarAlgorithm::MAX_BUFFER_SIZE;

    // Process each emitter
    for (int emitter = 0; emitter < pThis->numEmitters; ++emitter) {
        // Apply slew limiting to smooth parameter changes for this emitter
        pThis->currentAzimuth[emitter] = tinEarAlgorithm::slewLimit(
            pThis->currentAzimuth[emitter], pThis->targetAzimuth[emitter],
            tinEarAlgorithm::SLEW_RATE);

        pThis->currentElevation[emitter] = tinEarAlgorithm::slewLimit(
            pThis->currentElevation[emitter], pThis->targetElevation[emitter],
            tinEarAlgorithm::SLEW_RATE);

        pThis->currentDistance[emitter] = tinEarAlgorithm::slewLimit(
            pThis->currentDistance[emitter], pThis->targetDistance[emitter],
            tinEarAlgorithm::SLEW_RATE);
            
        pThis->currentAttenuation[emitter] = tinEarAlgorithm::slewLimit(
            pThis->currentAttenuation[emitter], pThis->targetAttenuation[emitter],
            tinEarAlgorithm::SLEW_RATE * 10.0f); // Faster slew for attenuation

        // Update source position based on smoothed angles
        const float distance = pThis->currentDistance[emitter];
        pThis->sourceX[emitter] = distance * sinf(pThis->currentAzimuth[emitter]);
        pThis->sourceZ[emitter] = distance * cosf(pThis->currentAzimuth[emitter]);
        pThis->sourceY[emitter] = distance * sinf(pThis->currentElevation[emitter]);
        
        // Get input for this emitter
        int inputBusIdx = kNumCommonParameters + kNumRoutingParameters + emitter * kNumPerEmitterParameters + kParamEmitterInput;
        const float *input = busFrames + (pThis->v[inputBusIdx] - 1) * numFrames;
        
        // Calculate linear gain from dB attenuation
        float linearGain = tinEarAlgorithm::dbToLinear(pThis->currentAttenuation[emitter]);

        // Process audio for this emitter
        for (int offset = 0; offset < numFrames; offset += maxChunkSize) {
            int currentFrames = (numFrames - offset > maxChunkSize)
                                    ? maxChunkSize
                                    : (numFrames - offset);

            // Apply spatial audio processing to this emitter's input
            applyMonoSpatialAudio(input + offset, 
                                  pThis->tempOutputL,
                                  pThis->tempOutputR, 
                                  currentFrames, 
                                  pThis->sourceX[emitter],
                                  pThis->sourceY[emitter], 
                                  pThis->sourceZ[emitter],
                                  &pThis->spatialStates[emitter]);

            // Mix this emitter's output to the main output with attenuation
            for (int i = 0; i < currentFrames; ++i) {
                outL[offset + i] += pThis->tempOutputL[i] * linearGain;
                outR[offset + i] += pThis->tempOutputR[i] * linearGain;
            }
        }
    }
}

static const _NT_specification specifications[] = {
    { .name = "Emitters", .min = 1, .max = kMaxEmitters, .def = 1, .type = kNT_typeGeneric },
};

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'h', 'T', 'E'),
    .name = "Tin Ear",
    .description = "Spatial Audio Effect",
    .numSpecifications = ARRAY_SIZE(specifications),
    .specifications = specifications,
    .calculateStaticRequirements = nullptr,
    .initialise = nullptr,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = nullptr,
    .midiRealtime = nullptr,
    .midiMessage = nullptr,
    .tags = kNT_tagUtility,
    .hasCustomUi = nullptr,
    .customUi = nullptr,
    .setupUi = nullptr,
    .serialise = nullptr,
    .deserialise = nullptr,
    .midiSysEx = nullptr,
};

uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersionCurrent;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return reinterpret_cast<uintptr_t>(data == 0 ? &factory : nullptr);
    }
    return 0;
}
