// Professional Spatial Audio Implementation for ARM Cortex-M7  (v2.5)
// -------------------------------------------------------------------
// • No call to atan2f (or fast approximation).  Uses sinAz = x / √(x²+z²).
// • Externalisation cues, smoothing, dual-delay ITD remain from v2.3.
// • Public API unchanged.

#include <cmath>
#include <cstdint>
#include <cstring>      // memset

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ────────────────────────────────────────────────────────────────
// Constants & helpers
// ────────────────────────────────────────────────────────────────
constexpr float kSampleRate   = 48000.0f;
constexpr float kInvSR        = 1.0f / kSampleRate;
constexpr float kSpeedOfSound = 343.0f;            // m / s

static inline float clampf(float x, float lo, float hi)
{
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

// ────────────────────────────────────────────────────────────────
// Biquad with coefficient smoothing
// ────────────────────────────────────────────────────────────────
class Biquad {
public:
    Biquad() { clear(); }

    float process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setCoeffs(float _b0, float _b1, float _b2,
                   float _a0, float _a1, float _a2)
    {
        constexpr float kSmooth = 0.999f;     // ≈5 ms time-constant
        b0 = kSmooth * b0 + (1.0f - kSmooth) * (_b0 / _a0);
        b1 = kSmooth * b1 + (1.0f - kSmooth) * (_b1 / _a0);
        b2 = kSmooth * b2 + (1.0f - kSmooth) * (_b2 / _a0);
        a1 = kSmooth * a1 + (1.0f - kSmooth) * (_a1 / _a0);
        a2 = kSmooth * a2 + (1.0f - kSmooth) * (_a2 / _a0);
    }

    void clear() { b0 = 1; b1 = b2 = a1 = a2 = z1 = z2 = 0; }

private:
    float b0{}, b1{}, b2{}, a1{}, a2{}, z1{}, z2{};
};

// ───────── Filter builders ──────────────────────────────────────
static inline void setNotch(Biquad& f, float fc, float Q = 8.0f)
{
    fc = clampf(fc, 200.0f, kSampleRate * 0.45f);
    float w0    = 2.0f * M_PI * fc * kInvSR;
    float cosw0 = cosf(w0);
    float alpha = sinf(w0) / (2.0f * Q);

    float b0 =  1.0f;
    float b1 = -2.0f * cosw0;
    float b2 =  1.0f;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 =  1.0f - alpha;

    f.setCoeffs(b0, b1, b2, a0, a1, a2);
}

static inline void setHighShelf(Biquad& f, float fc, float dBgain)
{
    fc = clampf(fc, 300.0f, kSampleRate * 0.45f);

    float A     = powf(10.0f, dBgain * 0.05f);
    float w0    = 2.0f * M_PI * fc * kInvSR;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 * 0.70710678f;         // sin/2 * √2
    float beta  = sqrtf(A) * alpha;

    float b0 =      A * ((A + 1) + (A - 1) * cosw0 + 2 * beta);
    float b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
    float b2 =      A * ((A + 1) + (A - 1) * cosw0 - 2 * beta);
    float a0 =          (A + 1) - (A - 1) * cosw0 + 2 * beta;
    float a1 =  2 *     ((A - 1) - (A + 1) * cosw0);
    float a2 =          (A + 1) - (A - 1) * cosw0 - 2 * beta;

    f.setCoeffs(b0, b1, b2, a0, a1, a2);
}

// ────────────────────────────────────────────────────────────────
// One-pole low-pass (air absorption)
// ────────────────────────────────────────────────────────────────
class OnePoleLP {
public:
    OnePoleLP() : alpha(0.0f), y1(0.0f) {}

    void setCutoff(float fc) {
        fc = clampf(fc, 50.0f, 0.45f * kSampleRate);
        float rc = 1.0f / (2.0f * M_PI * fc);
        alpha = kInvSR / (rc + kInvSR);
    }

    float process(float x) {
        y1 += alpha * (x - y1);
        return y1;
    }

private:
    float alpha, y1;
};

// ────────────────────────────────────────────────────────────────
// Fractional delay line (linear) – 512 samples
// ────────────────────────────────────────────────────────────────
class DelayLine {
public:
    DelayLine() : writeIdx(0) { memset(buf, 0, sizeof(buf)); }

    float process(float x, float delaySamples)
    {
        buf[writeIdx] = x;

        float readPos = static_cast<float>(writeIdx) - delaySamples;
        if (readPos < 0) readPos += kMax;

        int   i0   = static_cast<int>(readPos) & kMask;
        int   i1   = (i0 + 1) & kMask;
        float frac = readPos - static_cast<int>(readPos);
        float y    = buf[i0] * (1.0f - frac) + buf[i1] * frac;

        writeIdx = (writeIdx + 1) & kMask;
        return y;
    }

private:
    static constexpr int kMax  = 512;
    static constexpr int kMask = kMax - 1;

    float buf[kMax]{};
    int   writeIdx;
};

// ────────────────────────────────────────────────────────────────
// Per-emitter spatial audio state structure
// ────────────────────────────────────────────────────────────────
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

// ────────────────────────────────────────────────────────────────
// Public API (modified to accept per-emitter state)
// ────────────────────────────────────────────────────────────────
extern "C"
void applyMonoSpatialAudio(const float* in,
                           float* outL,
                           float* outR,
                           const int    numSamples,
                           const float  srcX,
                           const float  srcY,
                           const float  srcZ,
                           SpatialAudioState* state)
{
    // ── 1. Compute target parameters (block) ───────────────────
    float horizDist = sqrtf(srcX * srcX + srcZ * srcZ) + 1.0e-6f; // avoid /0
    float sinAzT    = clampf(srcX / horizDist, -1.0f, 1.0f);      // −1…+1
    float distT     = sqrtf(srcX * srcX + srcY * srcY + srcZ * srcZ + 1.0e-6f);
    float elevT     = asinf(srcY / distT);                        // −π/2…+π/2
    float elevNT    = elevT * (2.0f / M_PI);                      // −1…+1

    // ── 2. Linear ramp across this block ───────────────────────
    float sinAzStep = (sinAzT - state->prevSinAz) / numSamples;
    float elevStep  = (elevNT - state->prevElevN) / numSamples;
    float distStep  = (distT  - state->prevDist ) / numSamples;

    float sinAz = state->prevSinAz;
    float elevN = state->prevElevN;
    float dist  = state->prevDist;

    // Early reflection + LPF set once per block
    float reflDelaySamp = fabsf(srcY) / kSpeedOfSound * kSampleRate;
    float reflScale     = 0.501187f;                      // −6 dB
    float lpCut         = 15000.0f - 1000.0f * (distT - 0.5f);
    state->airLP.setCutoff(clampf(lpCut, 5000.0f, 15000.0f));

    // ── 3. Process audio buffer ─────────────────────────────────
    for (int n = 0; n < numSamples; ++n) {
        sinAz += sinAzStep;
        elevN += elevStep;
        dist  += distStep;

        // ITD delay (positive on lagging ear)
        float itdSamples = 0.0005f * fabsf(sinAz) * kSampleRate; // 0…24

        // Early reflection
        float x = in[n];
        float xRefl = state->reflDelay.process(x, reflDelaySamp) * reflScale;
        float dry = x + xRefl;

        // Air absorption
        float dryLP = state->airLP.process(dry);

        // ITD routing
        float left, right;
        if (sinAz >= 0.0f) {                // source on left → left ear lags
            left  = state->delayL.process(dryLP,  itdSamples);
            right = dryLP;
        } else {                            // source on right
            left  = dryLP;
            right = state->delayR.process(dryLP,  itdSamples);
        }

        // ILD (broadband ±3 dB)
        float ildL = 1.0f + 0.25f * sinAz;
        float ildR = 1.0f - 0.25f * sinAz;
        left  *= ildL;
        right *= ildR;

        // Update filter coefficients every 8 samples
        if ((n & 7) == 0) {
            setHighShelf(state->shelfL, 1500.0f,  8.0f * sinAz);
            setHighShelf(state->shelfR, 1500.0f, -8.0f * sinAz);
            float notchFc = 8000.0f + 2500.0f * elevN;
            setNotch(state->notchL, notchFc);
            setNotch(state->notchR, notchFc);
        }

        // Head-shadow shelf + pinna notch
        left  = state->notchL.process(state->shelfL.process(left));
        right = state->notchR.process(state->shelfR.process(right));

        outL[n] = left;
        outR[n] = right;
    }

    // ── 4. Save smoothed state for next call ────────────────────
    state->prevSinAz = sinAz;
    state->prevElevN = elevN;
    state->prevDist  = dist;
}

