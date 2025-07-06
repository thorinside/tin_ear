#include <distingnt/api.h>
#include <cmath>
#include <new>
#include <cstring>  // for memcpy

// Professional spatial audio function declaration
extern "C" {
void applyMonoSpatialAudio(const float *input, float *outputL, float *outputR,
                           int numSamples, float sourceX, float sourceY,
                           float sourceZ);
}

// Add missing constants
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// Maximum number of emitters supported
constexpr int kMaxEmitters = 8;

// Common parameter indices
enum {
    kParamOutputL,
    kParamOutputMode,
    kParamOutputR,
    kParamAutoSpread,
    kNumCommonParameters,
};

// Per-emitter parameter indices (relative to emitter base)
enum {
    kParamEmitterInput,
    kParamEmitterAzimuth,
    kParamEmitterElevation,
    kParamEmitterDistance,
    kParamEmitterAttenuation,
    kNumPerEmitterParameters,
};

struct tinEarAlgorithm : _NT_algorithm {
    tinEarAlgorithm(int32_t numEmitters_) 
        : _NT_algorithm(), numEmitters(numEmitters_) {
        // Initialize page definitions
        pagesDefs.numPages = 1 + numEmitters;  // Routing page + one page per emitter
        pagesDefs.pages = pageDefs;
        
        // Copy common parameters
        memcpy(parameterDefs, commonParameters, kNumCommonParameters * sizeof(_NT_parameter));
        
        // Create per-emitter parameters
        for (int i = 0; i < numEmitters; ++i) {
            int baseIdx = kNumCommonParameters + i * kNumPerEmitterParameters;
            
            // Create parameter definitions for this emitter
            parameterDefs[baseIdx + kParamEmitterInput] = {
                .name = emitterInputNames[i],
                .min = 1, .max = 28, .def = static_cast<int16_t>(1 + i),
                .unit = kNT_unitNone, .scaling = 0, .enumStrings = nullptr
            };
            
            parameterDefs[baseIdx + kParamEmitterAzimuth] = {
                .name = emitterAzimuthNames[i],
                .min = -180, .max = 180, .def = 0,
                .unit = kNT_unitNone, .scaling = 0, .enumStrings = nullptr
            };
            
            parameterDefs[baseIdx + kParamEmitterElevation] = {
                .name = emitterElevationNames[i],
                .min = -90, .max = 90, .def = 0,
                .unit = kNT_unitNone, .scaling = 0, .enumStrings = nullptr
            };
            
            parameterDefs[baseIdx + kParamEmitterDistance] = {
                .name = emitterDistanceNames[i],
                .min = 1, .max = 100, .def = 10,
                .unit = kNT_unitNone, .scaling = kNT_scaling10, .enumStrings = nullptr
            };
            
            parameterDefs[baseIdx + kParamEmitterAttenuation] = {
                .name = emitterAttenuationNames[i],
                .min = -60, .max = 0, .def = 0,
                .unit = kNT_unitDb, .scaling = 0, .enumStrings = nullptr
            };
            
            // Create page for this emitter
            pageDefs[i].name = emitterPageNames[i];
            pageDefs[i].numParams = kNumPerEmitterParameters;
            uint8_t* p = pageParams + i * kNumPerEmitterParameters;
            pageDefs[i].params = p;
            for (int j = 0; j < kNumPerEmitterParameters; ++j) {
                p[j] = baseIdx + j;
            }
        }
        
        // Create routing page
        pageDefs[numEmitters].name = "Routing";
        pageDefs[numEmitters].numParams = kNumCommonParameters;
        pageDefs[numEmitters].params = routingParams;
        
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

    // Slew limiting - smooth over approximately 20ms at 48kHz
    static constexpr float SLEW_RATE = 0.001f;

    // Audio processing buffers for spatial audio
    static const int MAX_BUFFER_SIZE = 256;
    float tempOutputL[MAX_BUFFER_SIZE]{};
    float tempOutputR[MAX_BUFFER_SIZE]{};
    
    // Dynamic parameter storage
    _NT_parameter parameterDefs[kNumCommonParameters + kMaxEmitters * kNumPerEmitterParameters];
    _NT_parameterPages pagesDefs;
    _NT_parameterPage pageDefs[1 + kMaxEmitters];  // Routing + emitter pages
    uint8_t pageParams[kMaxEmitters * kNumPerEmitterParameters];
    
    // Static parameter names
    static const char* emitterPageNames[kMaxEmitters];
    static const char* emitterInputNames[kMaxEmitters];
    static const char* emitterAzimuthNames[kMaxEmitters];
    static const char* emitterElevationNames[kMaxEmitters];
    static const char* emitterDistanceNames[kMaxEmitters];
    static const char* emitterAttenuationNames[kMaxEmitters];
    static const _NT_parameter commonParameters[kNumCommonParameters];
    static const uint8_t routingParams[kNumCommonParameters];

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

// Static member definitions
static const char* const enumStringsAutoSpread[] = {
    "Off",
    "On"
};

const char* tinEarAlgorithm::emitterPageNames[kMaxEmitters] = {
    "Emitter 1", "Emitter 2", "Emitter 3", "Emitter 4",
    "Emitter 5", "Emitter 6", "Emitter 7", "Emitter 8"
};

const char* tinEarAlgorithm::emitterInputNames[kMaxEmitters] = {
    "Input 1", "Input 2", "Input 3", "Input 4",
    "Input 5", "Input 6", "Input 7", "Input 8"
};

const char* tinEarAlgorithm::emitterAzimuthNames[kMaxEmitters] = {
    "Azimuth 1", "Azimuth 2", "Azimuth 3", "Azimuth 4",
    "Azimuth 5", "Azimuth 6", "Azimuth 7", "Azimuth 8"
};

const char* tinEarAlgorithm::emitterElevationNames[kMaxEmitters] = {
    "Elevation 1", "Elevation 2", "Elevation 3", "Elevation 4",
    "Elevation 5", "Elevation 6", "Elevation 7", "Elevation 8"
};

const char* tinEarAlgorithm::emitterDistanceNames[kMaxEmitters] = {
    "Distance 1", "Distance 2", "Distance 3", "Distance 4",
    "Distance 5", "Distance 6", "Distance 7", "Distance 8"
};

const char* tinEarAlgorithm::emitterAttenuationNames[kMaxEmitters] = {
    "Atten 1", "Atten 2", "Atten 3", "Atten 4",
    "Atten 5", "Atten 6", "Atten 7", "Atten 8"
};

// clang-format off
const _NT_parameter tinEarAlgorithm::commonParameters[kNumCommonParameters] = {
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output L", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT("Output R", 1, 14)
    {.name = "Auto-Spread",
     .min = 0,
     .max = 1,
     .def = 0,
     .unit = kNT_unitEnum,
     .scaling = 0,
     .enumStrings = enumStringsAutoSpread},
};
// clang-format on
const uint8_t tinEarAlgorithm::routingParams[kNumCommonParameters] = {
    kParamOutputL, kParamOutputMode, kParamOutputR, kParamAutoSpread
};

void calculateRequirements(_NT_algorithmRequirements &req,
                           const int32_t *specifications) {
    int32_t numEmitters = specifications[0];
    
    req.numParameters = kNumCommonParameters + numEmitters * kNumPerEmitterParameters;
    req.sram = sizeof(tinEarAlgorithm);
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm *construct(const _NT_algorithmMemoryPtrs &ptrs,
                         const _NT_algorithmRequirements &,  // unused
                         const int32_t *specifications) {
    int32_t numEmitters = specifications[0];
    
    auto *alg = new(ptrs.sram) tinEarAlgorithm(numEmitters);
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
    if (p >= kNumCommonParameters) {
        int relativeIdx = p - kNumCommonParameters;
        int emitterIdx = relativeIdx / kNumPerEmitterParameters;
        int paramType = relativeIdx % kNumPerEmitterParameters;
        
        if (emitterIdx < pThis->numEmitters) {
            switch (paramType) {
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
        int inputBusIdx = kNumCommonParameters + emitter * kNumPerEmitterParameters + kParamEmitterInput;
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
                                  pThis->sourceZ[emitter]);

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
