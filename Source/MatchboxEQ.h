// MatchboxEQ.h
// Shipping-ready C++ DSP engine
// Critical fixes applied + dead code removed

#pragma once
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class MatchboxEQ
{
public:
    MatchboxEQ (float sample_rate = 48000.0f)
        : sr (sample_rate), init_done (false)
    {
        std::memset (band_energy,   0, sizeof (band_energy));
        std::memset (bp_state0,     0, sizeof (bp_state0));
        std::memset (bp_state1,     0, sizeof (bp_state1));
        std::memset (bp_state2,     0, sizeof (bp_state2));
        std::memset (bp_state3,     0, sizeof (bp_state3));
        std::memset (bell_s0_mid,   0, sizeof (bell_s0_mid));
        std::memset (bell_s1_mid,   0, sizeof (bell_s1_mid));
        std::memset (bell_s2_mid,   0, sizeof (bell_s2_mid));
        std::memset (bell_s3_mid,   0, sizeof (bell_s3_mid));
        std::memset (bell_s0_side,  0, sizeof (bell_s0_side));
        std::memset (bell_s1_side,  0, sizeof (bell_s1_side));
        std::memset (bell_s2_side,  0, sizeof (bell_s2_side));
        std::memset (bell_s3_side,  0, sizeof (bell_s3_side));
    }

    void process (float& inL, float& inR,
                  float asked_bands, int demon_count,
                  float bit_depth, int genre_id,
                  float unison_weight)
    {
        if (! init_done)
            init();

        // Mono analysis
        const float mono = (inL + inR) * 0.5f;
        for (int i = 0; i < 50; ++i)
            analyse_band (mono, i);

        // Clamp groups to [1, 12]
        const int groups = std::max (1, std::min (12, (int) (asked_bands + 0.5f)));
        const int bands_per_group = 50 / groups;
        const int remainder = 50 % groups;

        float group_energy[12] = { 0 };
        int   group_first[12]  = { 0 };
        int   idx = 0;

        for (int g = 0; g < groups; ++g)
        {
            const int size = bands_per_group + (g < remainder ? 1 : 0);
            group_first[g] = idx;
            float sum = 0.0f;
            for (int j = 0; j < size; ++j)
            {
                sum += band_energy[idx];
                ++idx;
            }
            group_energy[g] = sum / (float) size;
        }

        // Demon selection (top-N)
        float temp[50];
        for (int i = 0; i < 50; ++i)
            temp[i] = band_energy[i];

        bool selected[50] = { false };
        const int demons = std::min (50, std::max (0, demon_count));

        for (int s = 0; s < demons; ++s)
        {
            float maxv = -1.0f;
            int   maxi = 0;
            for (int i = 0; i < 50; ++i)
            {
                if (! selected[i] && temp[i] > maxv)
                {
                    maxv = temp[i];
                    maxi = i;
                }
            }
            selected[maxi] = true;
        }

        // Gain calculation
        float gain_50[50];
        for (int i = 0; i < 50; ++i)
        {
            const float freq = matchbox_freqs[i];
            float base = bitrate_attenuation (freq, bit_depth)
                       * std::pow (10.0f, genre_offset (freq, genre_id) / 20.0f)
                       * unison_weight_factor (freq, unison_weight);

            int g = 0;
            for (int j = 0; j < groups - 1; ++j)
                if (i >= group_first[j + 1])
                    g = j + 1;

            if (selected[i])
            {
                const float rel = band_energy[i] / (group_energy[g] + 1e-6f);
                const float cut = 0.3f + rel * 0.5f;
                base *= clamp (cut, 0.2f, 0.9f);
            }
            else
            {
                const float comp = clamp (0.015f / (group_energy[g] + 1e-6f), 0.7f, 1.4f);
                base *= comp;
            }

            gain_50[i] = clamp (base, 0.7079f, 1.4125f); // ±3 dB
        }

        // Mid / side with independent states
        float mid  = (inL + inR) * 0.5f;
        float side = (inL - inR) * 0.5f;
        float out_mid  = mid;
        float out_side = side;

        for (int i = 0; i < 50; ++i)
        {
            out_mid  = apply_bell (out_mid,  i, gain_50[i], false);
            const float sg = clamp (gain_50[i] * 1.15f, 0.7079f, 1.4125f);
            out_side = apply_bell (out_side, i, sg, true);
        }

        inL = out_mid + out_side;
        inR = out_mid - out_side;

        // Soft-clip with unity-gain compensation
        constexpr float drive_comp = 0.85f / 1.1045f;
        inL = std::tanh (inL * 1.3f) * drive_comp;
        inR = std::tanh (inR * 1.3f) * drive_comp;
    }

private:
    float sr;
    bool  init_done;
    float matchbox_freqs[50];
    float band_energy[50];

    // Analysis BPF states
    float bp_state0[50], bp_state1[50], bp_state2[50], bp_state3[50];

    // Independent mid / side bell states
    float bell_s0_mid[50],  bell_s1_mid[50],  bell_s2_mid[50],  bell_s3_mid[50];
    float bell_s0_side[50], bell_s1_side[50], bell_s2_side[50], bell_s3_side[50];

    static constexpr float energy_alpha = 0.001f; // ~20 ms

    void init()
    {
        const float low  = 40.0f;
        const float high = 18000.0f;
        const float ratio = std::pow (high / low, 1.0f / 49.0f);
        float f = low;
        for (int i = 0; i < 50; ++i)
        {
            matchbox_freqs[i] = f;
            f *= ratio;
        }
        init_done = true;
    }

    static inline float clamp (float v, float lo, float hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    void analyse_band (float sig, int idx)
    {
        const float freq  = matchbox_freqs[idx];
        const float w0    = 2.0f * (float) M_PI * freq / sr;
        const float alpha = std::sin (w0) / (2.0f * 0.707f);
        const float cw    = std::cos (w0);
        const float b0 = alpha, b1 = 0.0f, b2 = -alpha;
        const float a0 = 1.0f + alpha, a1 = -2.0f * cw, a2 = 1.0f - alpha;

        const float filtered = (b0 * sig
                              + b1 * bp_state0[idx] + b2 * bp_state1[idx]
                              - a1 * bp_state2[idx] - a2 * bp_state3[idx]) / a0;

        bp_state1[idx] = bp_state0[idx];
        bp_state0[idx] = sig;
        bp_state3[idx] = bp_state2[idx];
        bp_state2[idx] = filtered;

        band_energy[idx] += energy_alpha * (std::abs (filtered) - band_energy[idx]);
    }

    float apply_bell (float x, int idx, float gain_lin, bool is_side)
    {
        const float freq = matchbox_freqs[idx];
        if (freq <= 0.0f || std::abs (gain_lin - 1.0f) < 0.001f)
            return x;

        const float w0 = 2.0f * (float) M_PI * freq / sr;
        const float db = 20.0f * std::log10 (std::max (gain_lin, 1e-12f));
        float q = (db > 0.0f) ? (4.5f - db * 0.25f) : (4.5f + db * 0.25f);
        q = clamp (q, 1.0f, 8.0f);

        const float alpha = std::sin (w0) / (2.0f * q);
        const float cw    = std::cos (w0);

        const float b0 = 1.0f + alpha * gain_lin;
        const float b1 = -2.0f * cw;
        const float b2 = 1.0f - alpha * gain_lin;
        const float a0 = 1.0f + alpha / gain_lin;
        const float a1 = -2.0f * cw;
        const float a2 = 1.0f - alpha / gain_lin;

        float y, s0, s1, s2, s3;

        if (is_side)
        {
            s0 = bell_s0_side[idx]; s1 = bell_s1_side[idx];
            s2 = bell_s2_side[idx]; s3 = bell_s3_side[idx];
            y  = (b0 * x + b1 * s0 + b2 * s1 - a1 * s2 - a2 * s3) / a0;
            bell_s1_side[idx] = s0; bell_s0_side[idx] = x;
            bell_s3_side[idx] = s2; bell_s2_side[idx] = y;
        }
        else
        {
            s0 = bell_s0_mid[idx]; s1 = bell_s1_mid[idx];
            s2 = bell_s2_mid[idx]; s3 = bell_s3_mid[idx];
            y  = (b0 * x + b1 * s0 + b2 * s1 - a1 * s2 - a2 * s3) / a0;
            bell_s1_mid[idx] = s0; bell_s0_mid[idx] = x;
            bell_s3_mid[idx] = s2; bell_s2_mid[idx] = y;
        }

        return y;
    }

    static float bitrate_attenuation (float freq, float bit_depth)
    {
        float cutoff = 22050.0f, slope = 0.0f;
        if (bit_depth <= 16.0f)      { cutoff = 16000.0f; slope = -6.0f; }
        else if (bit_depth <= 20.0f) { cutoff = 20000.0f; slope = -3.0f; }
        if (freq < cutoff)
            return 1.0f;
        return std::pow (10.0f, slope * std::log2 (freq / cutoff) / 20.0f);
    }

    static float genre_offset (float freq, int genre_id)
    {
        static const float profiles[4][3] = {
            { +2.0f, -1.5f, +1.0f },
            { +3.0f, -2.0f, +2.5f },
            { +1.5f, +0.5f, +1.5f },
            {  0.0f,  0.0f,  0.0f }
        };
        const int region = (freq > 3000.0f) ? 2 : ((freq > 300.0f) ? 1 : 0);
        const int g = std::max (0, std::min (3, genre_id));
        return profiles[g][region];
    }

    static float unison_weight_factor (float freq, float weight)
    {
        const float ratio   = freq / 440.0f;
        const float closest = std::round (ratio);
        const float detune  = std::abs (ratio - closest);
        const float factor  = 1.0f - 0.5f * std::exp (-10.0f * detune);
        return factor * weight + (1.0f - weight);
    }
};
