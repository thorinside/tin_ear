#include <distingnt/api.h>
#include <cmath>
#include <new>

// Professional spatial audio function declaration
extern "C" {
void applyMonoSpatialAudio(float *input, float *outputL, float *outputR,
                           int numSamples, float sourceX, float sourceY,
                           float sourceZ);
}

// Add missing constants
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

struct tinEarAlgorithm : _NT_algorithm {
    tinEarAlgorithm() : _NT_algorithm() {
    }

    ~tinEarAlgorithm() = default;

    // Target positioning data (set by parameter changes)
    float targetAzimuth = 0.0f;
    float targetElevation = 0.0f;
    float targetDistance = 1.0f;

    // Current smoothed positioning data
    float currentAzimuth = 0.0f;
    float currentElevation = 0.0f;
    float currentDistance = 1.0f;

    // Current smoothed spatial audio settings
    float sourceX = 1.0f;
    float sourceY = 0.0f;
    float sourceZ = 0.0f;

    // Slew limiting - smooth over approximately 20ms at 48kHz
    static constexpr float SLEW_RATE =
            0.001f; // Adjust this for faster/slower response

    // Audio processing buffers for spatial audio
    static const int MAX_BUFFER_SIZE = 256;
    float tempOutputL[MAX_BUFFER_SIZE]{};
    float tempOutputR[MAX_BUFFER_SIZE]{};

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
};

enum {
    kParamInput,
    kParamOutputL,
    kParamOutputMode,
    kParamOutputR,
    kParamAzimuth,
    kParamElevation,
    kParamDistance,
};

// clang-format off
static const _NT_parameter parameters[] = {
    NT_PARAMETER_AUDIO_INPUT("Input", 1, 1)

    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output L", 1, 13)

  NT_PARAMETER_AUDIO_OUTPUT("Output R", 1, 14)

  {.name = "Azimuth",
     .min = -180,
     .max = 180,
     .def = 0,
     .unit = kNT_unitNone,
     .scaling = 0,
     .enumStrings = nullptr},

  {.name = "Elevation",
     .min = -90,
     .max = 90,
     .def = 0,
     .unit = kNT_unitNone,
     .scaling = 0,
     .enumStrings = nullptr},

  {.name = "Distance (m)",
     .min = 1,
     .max = 100,
     .def = 1,
     .unit = kNT_unitNone,
     .scaling = kNT_scaling10,
     .enumStrings = nullptr},
};
// clang-format on
static constexpr uint8_t effectParams[] = {
    kParamAzimuth, kParamElevation, kParamDistance
};

static constexpr uint8_t routingParams[] = {
    kParamInput, kParamOutputL, kParamOutputMode, kParamOutputR
};

static constexpr _NT_parameterPage pages[] = {
    {.name = "Effect", .numParams = ARRAY_SIZE(effectParams), .params = effectParams},
    {.name = "Routing", .numParams = ARRAY_SIZE(routingParams), .params = routingParams},
};

static constexpr _NT_parameterPages parameterPages = {
    .numPages = ARRAY_SIZE(pages),
    .pages = pages,
};

void calculateRequirements(_NT_algorithmRequirements &req,
                           const int32_t *specifications) {
    req.numParameters = ARRAY_SIZE(parameters);
    req.sram = sizeof(tinEarAlgorithm);
    req.dram = 0;
    req.dtc = 0;
    req.itc = 0;
}

_NT_algorithm *construct(const _NT_algorithmMemoryPtrs &ptrs,
                         const _NT_algorithmRequirements &req,
                         const int32_t *specifications) {
    auto *alg = new(ptrs.sram) tinEarAlgorithm();
    alg->parameters = parameters;
    alg->parameterPages = &parameterPages;
    return alg;
}

void parameterChanged(_NT_algorithm *self, int p) {
    auto *pThis = (tinEarAlgorithm *) self;
    if (p == kParamAzimuth) {
        // Set target azimuth, slew limiting will smooth the transition
        pThis->targetAzimuth =
                pThis->v[kParamAzimuth] * (M_PI / 180.0f); // Convert to radians
    } else if (p == kParamElevation) {
        // Set target elevation, slew limiting will smooth the transition
        pThis->targetElevation =
                pThis->v[kParamElevation] * (M_PI / 180.0f); // Convert to radians
    } else if (p == kParamDistance) {
        // Set target distance, slew limiting will smooth the transition
        pThis->targetDistance = pThis->v[kParamDistance] / 10.0f;
    }
}

void step(_NT_algorithm *self, float *busFrames, int numFramesBy4) {
    auto *pThis = (tinEarAlgorithm *) self;
    const int numFrames = numFramesBy4 * 4;

    // Apply slew limiting to smooth parameter changes
    pThis->currentAzimuth =
            tinEarAlgorithm::slewLimit(pThis->currentAzimuth, pThis->targetAzimuth,
                                       tinEarAlgorithm::SLEW_RATE);

    pThis->currentElevation = tinEarAlgorithm::slewLimit(
        pThis->currentElevation, pThis->targetElevation,
        tinEarAlgorithm::SLEW_RATE);

    pThis->currentDistance = tinEarAlgorithm::slewLimit(
        pThis->currentDistance, pThis->targetDistance,
        tinEarAlgorithm::SLEW_RATE);

    // Update source position based on smoothed angles
    const float distance = pThis->currentDistance;
    pThis->sourceX = distance * sinf(pThis->currentAzimuth); // ±1 at ±90°
    pThis->sourceZ = distance * cosf(pThis->currentAzimuth); //  1  at  0°
    pThis->sourceY = distance * sinf(pThis->currentElevation);

    // Mono input channel
    const float *input = busFrames + (pThis->v[kParamInput] - 1) * numFrames;

    // Output channels
    float *outL = busFrames + (pThis->v[kParamOutputL] - 1) * numFrames;
    float *outR = busFrames + (pThis->v[kParamOutputR] - 1) * numFrames;

    // Output mode (0 = Add, 1 = Replace)
    bool replaceMode = pThis->v[kParamOutputMode];

    // Process audio with professional spatial audio
    // Process in chunks to handle arbitrary buffer sizes
    constexpr int maxChunkSize = tinEarAlgorithm::MAX_BUFFER_SIZE;

    for (int offset = 0; offset < numFrames; offset += maxChunkSize) {
        int currentFrames = (numFrames - offset > maxChunkSize)
                                ? maxChunkSize
                                : (numFrames - offset);

        // Apply spatial audio processing to mono input
        applyMonoSpatialAudio(const_cast<float *>(input + offset), pThis->tempOutputL,
                              pThis->tempOutputR, currentFrames, pThis->sourceX,
                              pThis->sourceY, pThis->sourceZ);

        // Copy spatial audio output to plugin output
        for (int i = 0; i < currentFrames; ++i) {
            if (!replaceMode) {
                outL[offset + i] += pThis->tempOutputL[i];
                outR[offset + i] += pThis->tempOutputR[i];
            } else {
                outL[offset + i] = pThis->tempOutputL[i];
                outR[offset + i] = pThis->tempOutputR[i];
            }
        }
    }
}

static const _NT_factory factory = {
    .guid = NT_MULTICHAR('T', 'h', 'T', 'E'),
    .name = "Tin Ear",
    .description = "Spatial Audio Effect",
    .numSpecifications = 0,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .tags = kNT_tagUtility,
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
