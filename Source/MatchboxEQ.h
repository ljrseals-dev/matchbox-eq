// MatchboxEQ.h
// Framework-free MatchboxEQ DSP core.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

class MatchboxEQ
{
public:
    explicit MatchboxEQ (float sample_rate = 48000.0f)
        : sr (sanitise_sample_rate (sample_rate)),
          energy_alpha (1.0f - std::exp (-1.0f / (0.0208f * sr))),
          init_done (false),
          last_group_count (0)
    {
        reset();
    }

    // Clears all filter and envelope history. Call on the audio thread.
    void reset() noexcept
    {
        std::memset (band_energy, 0, sizeof (band_energy));
        std::memset (bp_state0, 0, sizeof (bp_state0));
        std::memset (bp_state1, 0, sizeof (bp_state1));
        std::memset (bp_state2, 0, sizeof (bp_state2));
        std::memset (bp_state3, 0, sizeof (bp_state3));
        std::memset (bell_s0_mid, 0, sizeof (bell_s0_mid));
        std::memset (bell_s1_mid, 0, sizeof (bell_s1_mid));
        std::memset (bell_s2_mid, 0, sizeof (bell_s2_mid));
        std::memset (bell_s3_mid, 0, sizeof (bell_s3_mid));
        std::memset (bell_s0_side, 0, sizeof (bell_s0_side));
        std::memset (bell_s1_side, 0, sizeof (bell_s1_side));
        std::memset (bell_s2_side, 0, sizeof (bell_s2_side));
        std::memset (bell_s3_side, 0, sizeof (bell_s3_side));
        std::memset (last_group_energy, 0, sizeof (last_group_energy));
        last_group_count = 0;
    }

    // Diagnostics are intended for same-thread/offline inspection only.
    // Reading these concurrently with process() is not thread-safe.
    int getLastGroupCount() const noexcept { return last_group_count; }

    float getLastGroupEnergy (int group) const noexcept
    {
        return group >= 0 && group < last_group_count ? last_group_energy[group] : 0.0f;
    }

    void process (float& inL, float& inR,
                  float asked_bands, int demon_count,
                  float bit_depth, int genre_id,
                  float unison_weight)
    {
        if (! init_done)
            init();

        if (! std::isfinite (inL)) inL = 0.0f;
        if (! std::isfinite (inR)) inR = 0.0f;

        const float safe_asked_bands = clamp (std::isfinite (asked_bands) ? asked_bands : 5.0f,
                                              1.0f, 12.0f);
        const int groups = static_cast<int> (safe_asked_bands + 0.5f);
        const int demons = std::max (0, std::min (50, demon_count));
        const float safe_bit_depth = std::isfinite (bit_depth) ? bit_depth : 24.0f;
        const int safe_genre = std::max (0, std::min (3, genre_id));
        const float safe_unison = std::isfinite (unison_weight)
                                ? clamp (unison_weight, 0.0f, 1.0f) : 0.6f;

        // Mono analysis feeds the adaptive group and resonance detectors.
        const float mono = (inL + inR) * 0.5f;
        for (int i = 0; i < 50; ++i)
            analyse_band (mono, i);

        const int bands_per_group = 50 / groups;
        const int remainder = 50 % groups;
        float group_energy[12] = {};
        int group_first[12] = {};
        int idx = 0;

        for (int g = 0; g < groups; ++g)
        {
            const int size = bands_per_group + (g < remainder ? 1 : 0);
            group_first[g] = idx;
            float sum = 0.0f;

            for (int j = 0; j < size; ++j)
                sum += band_energy[idx++];

            group_energy[g] = sum / static_cast<float> (size);
            last_group_energy[g] = group_energy[g];
        }

        for (int g = groups; g < 12; ++g)
            last_group_energy[g] = 0.0f;
        last_group_count = groups;

        // Deterministic top-N resonance selection. Ties select lower indices.
        bool selected[50] = {};
        for (int s = 0; s < demons; ++s)
        {
            float max_value = -1.0f;
            int max_index = -1;

            for (int i = 0; i < 50; ++i)
            {
                if (! selected[i] && band_energy[i] > max_value)
                {
                    max_value = band_energy[i];
                    max_index = i;
                }
            }

            if (max_index >= 0)
                selected[max_index] = true;
        }

        float gain_50[50] = {};
        for (int i = 0; i < 50; ++i)
        {
            const float freq = matchbox_freqs[i];
            float base = bitrate_attenuation (freq, safe_bit_depth)
                       * std::pow (10.0f, genre_offset (freq, safe_genre) / 20.0f)
                       * unison_weight_factor (freq, safe_unison);

            int group = 0;
            for (int j = 0; j < groups - 1; ++j)
                if (i >= group_first[j + 1])
                    group = j + 1;

            if (selected[i])
            {
                const float relative_energy = band_energy[i] / (group_energy[group] + 1.0e-6f);
                const float cut = 0.3f + relative_energy * 0.5f;
                base *= clamp (cut, 0.2f, 0.9f);
            }
            else
            {
                // Keep near-silent groups at unity; smoothly engage compensation
                // only after the measured group envelope clears the noise floor.
                constexpr float k_target = 0.015f;
                constexpr float k_floor = 0.003f;
                constexpr float k_ramp = 0.010f;

                const float raw_comp = clamp (k_target / (group_energy[group] + 1.0e-6f),
                                              0.7f, 1.4f);
                const float gate = clamp ((group_energy[group] - k_floor) / k_ramp,
                                          0.0f, 1.0f);
                base *= 1.0f + gate * (raw_comp - 1.0f);
            }

            gain_50[i] = clamp (base, 0.7079f, 1.4125f);
        }

        // Mid/side paths have independent state for every bell filter.
        const float mid = (inL + inR) * 0.5f;
        const float side = (inL - inR) * 0.5f;
        float out_mid = mid;
        float out_side = side;

        for (int i = 0; i < 50; ++i)
        {
            // Always advance filter history, including at unity gain. Skipping
            // the unity case leaves stale state that causes a transient when
            // adaptive gain later moves away from unity.
            out_mid = apply_bell (out_mid, i, gain_50[i], false);
            const float side_gain = clamp (gain_50[i] * 1.15f, 0.7079f, 1.4125f);
            out_side = apply_bell (out_side, i, side_gain, true);
        }

        constexpr float drive_comp = 0.85f / 1.1045f;
        inL = std::tanh ((out_mid + out_side) * 1.3f) * drive_comp;
        inR = std::tanh ((out_mid - out_side) * 1.3f) * drive_comp;

        if (! std::isfinite (inL)) inL = 0.0f;
        if (! std::isfinite (inR)) inR = 0.0f;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;

    float sr;
    float energy_alpha;
    bool init_done;
    int last_group_count;
    float matchbox_freqs[50] = {};
    float band_energy[50] = {};
    float last_group_energy[12] = {};

    // Analysis band-pass filter state.
    float bp_state0[50] = {}, bp_state1[50] = {};
    float bp_state2[50] = {}, bp_state3[50] = {};

    // Independent mid/side peaking-filter state.
    float bell_s0_mid[50] = {}, bell_s1_mid[50] = {};
    float bell_s2_mid[50] = {}, bell_s3_mid[50] = {};
    float bell_s0_side[50] = {}, bell_s1_side[50] = {};
    float bell_s2_side[50] = {}, bell_s3_side[50] = {};

    static float sanitise_sample_rate (float sample_rate) noexcept
    {
        return std::isfinite (sample_rate) && sample_rate >= 8000.0f
             ? sample_rate : 48000.0f;
    }

    static inline float clamp (float value, float low, float high) noexcept
    {
        return value < low ? low : (value > high ? high : value);
    }

    void init()
    {
        constexpr float low = 40.0f;
        const float high = std::min (18000.0f, sr * 0.45f);
        const float ratio = std::pow (high / low, 1.0f / 49.0f);
        float frequency = low;

        for (int i = 0; i < 50; ++i)
        {
            matchbox_freqs[i] = frequency;
            frequency *= ratio;
        }

        init_done = true;
    }

    void analyse_band (float signal, int index)
    {
        const float frequency = matchbox_freqs[index];
        const float w0 = 2.0f * pi * frequency / sr;
        const float alpha = std::sin (w0) / (2.0f * 0.707f);
        const float cosine = std::cos (w0);
        const float b0 = alpha;
        const float b2 = -alpha;
        const float a0 = 1.0f + alpha;
        const float a1 = -2.0f * cosine;
        const float a2 = 1.0f - alpha;

        const float filtered = (b0 * signal + b2 * bp_state1[index]
                              - a1 * bp_state2[index] - a2 * bp_state3[index]) / a0;

        if (! std::isfinite (filtered))
        {
            bp_state0[index] = bp_state1[index] = 0.0f;
            bp_state2[index] = bp_state3[index] = 0.0f;
            band_energy[index] = 0.0f;
            return;
        }

        bp_state1[index] = bp_state0[index];
        bp_state0[index] = signal;
        bp_state3[index] = bp_state2[index];
        bp_state2[index] = filtered;
        band_energy[index] += energy_alpha * (std::abs (filtered) - band_energy[index]);
    }

    float apply_bell (float input, int index, float gain, bool is_side)
    {
        const float frequency = matchbox_freqs[index];
        if (frequency <= 0.0f)
            return input;

        const float w0 = 2.0f * pi * frequency / sr;
        const float db = 20.0f * std::log10 (std::max (gain, 1.0e-12f));
        const float q_unclamped = db > 0.0f ? 4.5f - db * 0.25f
                                             : 4.5f + db * 0.25f;
        const float q = clamp (q_unclamped, 1.0f, 8.0f);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float cosine = std::cos (w0);

        const float b0 = 1.0f + alpha * gain;
        const float b1 = -2.0f * cosine;
        const float b2 = 1.0f - alpha * gain;
        const float a0 = 1.0f + alpha / gain;
        const float a1 = -2.0f * cosine;
        const float a2 = 1.0f - alpha / gain;

        float* s0 = is_side ? bell_s0_side : bell_s0_mid;
        float* s1 = is_side ? bell_s1_side : bell_s1_mid;
        float* s2 = is_side ? bell_s2_side : bell_s2_mid;
        float* s3 = is_side ? bell_s3_side : bell_s3_mid;

        const float output = (b0 * input + b1 * s0[index] + b2 * s1[index]
                            - a1 * s2[index] - a2 * s3[index]) / a0;

        if (! std::isfinite (output))
        {
            s0[index] = s1[index] = 0.0f;
            s2[index] = s3[index] = 0.0f;
            return 0.0f;
        }

        s1[index] = s0[index];
        s0[index] = input;
        s3[index] = s2[index];
        s2[index] = output;
        return output;
    }

    static float bitrate_attenuation (float frequency, float bit_depth)
    {
        float cutoff = 22050.0f;
        float slope = 0.0f;

        if (bit_depth <= 16.0f)      { cutoff = 16000.0f; slope = -6.0f; }
        else if (bit_depth <= 20.0f) { cutoff = 20000.0f; slope = -3.0f; }

        if (frequency < cutoff)
            return 1.0f;

        return std::pow (10.0f, slope * std::log2 (frequency / cutoff) / 20.0f);
    }

    static float genre_offset (float frequency, int genre_id)
    {
        static constexpr float profiles[4][3] = {
            { +2.0f, -1.5f, +1.0f },
            { +3.0f, -2.0f, +2.5f },
            { +1.5f, +0.5f, +1.5f },
            {  0.0f,  0.0f,  0.0f }
        };

        const int region = frequency > 3000.0f ? 2 : (frequency > 300.0f ? 1 : 0);
        return profiles[std::max (0, std::min (3, genre_id))][region];
    }

    static float unison_weight_factor (float frequency, float weight)
    {
        const float ratio = frequency / 440.0f;
        const float closest = std::floor (ratio + 0.5f);
        const float detune = std::abs (ratio - closest);
        const float factor = 1.0f - 0.5f * std::exp (-10.0f * detune);
        return factor * weight + (1.0f - weight);
    }
};
