// Professional Spatial Audio Implementation for ARM Cortex-M7 (v2.1)
// Adds key psycho-acoustic cues – pinna notch, high-shelf ILD, early reflection,
// and air-absorption low-pass – **without changing the public interface**.
//
// Change log 2025-07-04:
//   • v2   – first full externalisation features.
//   • v2.1 – replaced std::fill with a for-loop for embedded GCC compatibility.
//
// Author: ChatGPT

#include <cmath>
#include <cstdint>
#include <cstring>   // for memset (optional)

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ────────────────────────────────────────────────────────────────
// Fast atan2f approximation for ARM Cortex-M (unchanged)
// ────────────────────────────────────────────────────────────────
static inline float fast_atan2f(float y, float x)
{
    if (x == 0.0f && y == 0.0f) return 0.0f;

    float abs_y = (y < 0.0f) ? -y : y;
    float abs_x = (x < 0.0f) ? -x : x;
    float angle;

    if (abs_x >= abs_y) {
        float r = abs_y / (abs_x + 1.0e-10f);
        if (r < 0.4f) {
            float r2 = r * r;
            angle = r * (1.0f - r2 * (1.0f / 3.0f - r2 * 0.2f));
        } else {
            angle = (M_PI * 0.25f) * r / (1.0f + 0.28f * r);
        }
    } else {
        float r = abs_x / (abs_y + 1.0e-10f);
        if (r < 0.4f) {
            float r2 = r * r;
            angle = (M_PI * 0.5f) -
                    r * (1.0f - r2 * (1.0f / 3.0f - r2 * 0.2f));
        } else {
            angle = (M_PI * 0.5f) -
                    (M_PI * 0.25f) * r / (1.0f + 0.28f * r);
        }
    }

    if (x < 0.0f)       angle = (y >= 0.0f) ?  (M_PI - angle)
                                            : -(M_PI - angle);
    else if (y < 0.0f)  angle = -angle;

    return angle;
}

// ────────────────────────────────────────────────────────────────
// Constants & helpers
// ────────────────────────────────────────────────────────────────
constexpr float kSampleRate   = 48000.0f;
constexpr float kInvSR        = 1.0f / kSampleRate;
constexpr float kSpeedOfSound = 343.0f;       // m/s

static inline float clampf(float x, float lo, float hi)
{
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

// ────────────────────────────────────────────────────────────────
// Biquad – Direct Form-I (transposed)
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
        b0 = _b0 / _a0;
        b1 = _b1 / _a0;
        b2 = _b2 / _a0;
        a1 = _a1 / _a0;
        a2 = _a2 / _a0;
    }

    void clear() { b0 = 1; b1 = b2 = a1 = a2 = z1 = z2 = 0; }

private:
    float b0{}, b1{}, b2{}, a1{}, a2{}, z1{}, z2{};
};

// --- Notch filter (pinna cue) ----------------------------------------------
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

// --- High-shelf ILD (head-shadow) ------------------------------------------
static inline void setHighShelf(Biquad& f, float fc, float dBgain)
{
    fc = clampf(fc, 300.0f, kSampleRate * 0.45f);

    float A     = powf(10.0f, dBgain * 0.05f);    // 10^(dB/20)
    float w0    = 2.0f * M_PI * fc * kInvSR;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 * 0.70710678f;            // sin/2 * √2
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
// One-pole low-pass for air absorption
// ────────────────────────────────────────────────────────────────
class OnePoleLP {
public:
    OnePoleLP() : alpha(0.0f), y1(0.0f) {}

    void setCutoff(float fc) {
        fc = clampf(fc, 50.0f, 0.45f * kSampleRate);
        float rc = 1.0f / (2.0f * M_PI * fc);
        alpha = kInvSR / (rc + kInvSR);           // dt = 1/Fs
    }

    float process(float x) {
        y1 += alpha * (x - y1);
        return y1;
    }

private:
    float alpha, y1;
};

// ────────────────────────────────────────────────────────────────
// Fractional delay line (linear-interp) – 512 samples
// ────────────────────────────────────────────────────────────────
class DelayLine {
public:
    DelayLine() : writeIdx(0) {
        for (int i = 0; i < kMax; ++i) buf[i] = 0.0f;   // std::fill removed
        // Alternatively: memset(buf, 0, sizeof(buf));
    }

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

    float buf[kMax];
    int   writeIdx;
};

// ────────────────────────────────────────────────────────────────
// Per-channel processing objects (static = persist across calls)
// ────────────────────────────────────────────────────────────────
static Biquad notchL, notchR, shelfL, shelfR;
static OnePoleLP airLP;
static DelayLine itdDelay, reflDelay;

// ────────────────────────────────────────────────────────────────
// Public API (unchanged)
// ────────────────────────────────────────────────────────────────
extern "C"
void applyMonoSpatialAudio(float* in,
                           float* outL,
                           float* outR,
                           int    numSamples,
                           float  srcX,
                           float  srcY,
                           float  srcZ)
{
    // ───────── 1. Update geometry-dependent parameters ──────────
    float dist = sqrtf(srcX * srcX + srcY * srcY + srcZ * srcZ + 1.0e-6f);

    // Azimuth: −π … +π (0 = front, +90° = left ear)
    float azim = fast_atan2f(srcX, srcZ);

    // Elevation: −π/2 … +π/2 (positive = above)
    float elev = asinf(srcY / dist);
    float elevNorm = clampf(elev * (2.0f / M_PI), -1.0f, 1.0f);

    // ----- Interaural time delay (±0.5 ms) -----
    float itdSecs = 0.0005f * sinf(azim);           // centre-panned = 0
    float itdSamp = itdSecs * kSampleRate;          // ±24 samples

    // ----- Interaural level difference (broadband) -----
    float ildGainL = 1.0f + 0.25f * sinf(azim);     // ±3 dB
    float ildGainR = 1.0f - 0.25f * sinf(azim);

    // ----- High-shelf ILD -----
    setHighShelf(shelfL, 1500.0f,  8.0f * sinf(azim));
    setHighShelf(shelfR, 1500.0f, -8.0f * sinf(azim));

    // ----- Pinna notch -----
    float notchFc = 8000.0f + 2500.0f * elevNorm;
    setNotch(notchL, notchFc);
    setNotch(notchR, notchFc);

    // ----- Distance low-pass -----
    float lpCut = 15000.0f - 1000.0f * (dist - 0.5f);   // 15 k → 5 k (0.5–10 m)
    airLP.setCutoff(clampf(lpCut, 5000.0f, 15000.0f));

    // ----- Early reflection -----
    float reflDelaySamp = fabsf(srcY) / kSpeedOfSound * kSampleRate; // ≤ ~10 ms
    float reflScale     = 0.501187f;                                 // −6 dB

    // ───────── 2. Process buffer ────────────────────────────────
    for (int n = 0; n < numSamples; ++n) {
        float x  = in[n];

        // Early reflection (mono)
        float refl = reflDelay.process(x, reflDelaySamp) * reflScale;
        float dry  = x + refl;

        // Air absorption
        float dryLP = airLP.process(dry);

        // ITD (delay left vs right)
        float dl = itdDelay.process(dryLP,  itdSamp);   // +delay = left ear late
        float dr = itdDelay.process(dryLP, -itdSamp);

        // ILD (broadband gain)
        dl *= ildGainL;
        dr *= ildGainR;

        // High-shelf head-shadow
        dl = shelfL.process(dl);
        dr = shelfR.process(dr);

        // Pinna notch
        dl = notchL.process(dl);
        dr = notchR.process(dr);

        // Output
        outL[n] = dl;
        outR[n] = dr;
    }
}

