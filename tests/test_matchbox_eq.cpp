#include "MatchboxEQ.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool condition, const char* message)
    {
        if (! condition)
        {
            std::fprintf (stderr, "FAIL: %s\n", message);
            ++failures;
        }
    }

    bool finite_pair (float left, float right)
    {
        return std::isfinite (left) && std::isfinite (right);
    }

    void run_block (MatchboxEQ& eq, std::vector<float>& left,
                    std::vector<float>& right, float asked = 5.0f,
                    int demons = 8, float bit_depth = 24.0f,
                    int genre = 0, float unison = 0.6f)
    {
        for (std::size_t i = 0; i < left.size(); ++i)
            eq.process (left[i], right[i], asked, demons, bit_depth, genre, unison);
    }
}

int main()
{
    {
        MatchboxEQ eq;
        std::vector<float> left (4096, 0.0f);
        std::vector<float> right (4096, 0.0f);
        run_block (eq, left, right);
        check (std::all_of (left.begin(), left.end(), [] (float x) { return x == 0.0f; }),
               "digital silence remains silent on left");
        check (std::all_of (right.begin(), right.end(), [] (float x) { return x == 0.0f; }),
               "digital silence remains silent on right");
        check (eq.getLastGroupCount() == 5, "group diagnostics report configured group count");
    }

    {
        MatchboxEQ eq;
        std::vector<float> left (12000);
        std::vector<float> right (12000);
        float input_peak = 0.0f;

        for (std::size_t i = 0; i < left.size(); ++i)
        {
            const float sample = 0.0002f * std::sin (0.071f * static_cast<float> (i))
                               + 0.0001f * std::sin (0.193f * static_cast<float> (i));
            left[i] = sample;
            right[i] = sample * 0.9f;
            input_peak = std::max (input_peak, std::max (std::abs (left[i]), std::abs (right[i])));
        }

        run_block (eq, left, right);
        float output_peak = 0.0f;
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            check (finite_pair (left[i], right[i]), "quiet signal output remains finite");
            output_peak = std::max (output_peak, std::max (std::abs (left[i]), std::abs (right[i])));
        }
        check (output_peak <= input_peak * 2.5f + 1.0e-7f,
               "quiet signal is not excessively amplified");
        check (eq.getLastGroupCount() == 5, "quiet signal group count is stable");
        for (int group = 0; group < eq.getLastGroupCount(); ++group)
            check (std::isfinite (eq.getLastGroupEnergy (group))
                   && eq.getLastGroupEnergy (group) >= 0.0f,
                   "group energy diagnostics are finite and non-negative");
    }

    {
        const float sample_rates[] = { 8000.0f, 32000.0f, 44100.0f, 48000.0f,
                                       96000.0f, 192000.0f, 0.0f,
                                       std::numeric_limits<float>::quiet_NaN() };
        for (float rate : sample_rates)
        {
            MatchboxEQ eq (rate);
            for (int i = 0; i < 4096; ++i)
            {
                float left = 0.2f * std::sin (0.13f * static_cast<float> (i));
                float right = left;
                eq.process (left, right, 5.0f, 8, 24.0f, 0, 0.6f);
                check (finite_pair (left, right), "supported/fallback sample rate remains stable");
                check (std::abs (left - right) < 1.0e-5f,
                       "dual-mono input remains dual-mono");
            }
        }
    }

    {
        MatchboxEQ eq;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        float left = inf;
        float right = 0.25f;
        eq.process (left, right, nan, -100, nan, 999, nan);
        check (finite_pair (left, right), "invalid samples and parameters cannot poison DSP state");

        left = 0.1f;
        right = -0.1f;
        eq.process (left, right, 1000000.0f, 1000, 24.0f, -99, 1.5f);
        check (finite_pair (left, right), "out-of-range controls are safely bounded");

        left = std::numeric_limits<float>::max();
        right = std::numeric_limits<float>::max();
        eq.process (left, right, 5.0f, 8, 24.0f, 0, 0.6f);
        check (finite_pair (left, right), "extreme finite samples are bounded before DSP");
    }

    {
        MatchboxEQ reused;
        MatchboxEQ fresh;
        std::vector<float> warm_left (2048);
        std::vector<float> warm_right (2048);
        std::vector<float> actual_left (2048);
        std::vector<float> actual_right (2048);
        std::vector<float> expected_left (2048);
        std::vector<float> expected_right (2048);

        for (std::size_t i = 0; i < warm_left.size(); ++i)
        {
            warm_left[i] = 0.3f * std::sin (0.17f * static_cast<float> (i));
            warm_right[i] = 0.2f * std::cos (0.11f * static_cast<float> (i));
            actual_left[i] = expected_left[i] = 0.1f * std::sin (0.07f * static_cast<float> (i));
            actual_right[i] = expected_right[i] = 0.1f * std::cos (0.09f * static_cast<float> (i));
        }

        run_block (reused, warm_left, warm_right);
        reused.reset();
        run_block (reused, actual_left, actual_right);
        run_block (fresh, expected_left, expected_right);

        for (std::size_t i = 0; i < actual_left.size(); ++i)
        {
            check (std::abs (actual_left[i] - expected_left[i]) < 1.0e-7f
                   && std::abs (actual_right[i] - expected_right[i]) < 1.0e-7f,
                   "reset restores fresh-instance output");
        }
    }

    if (failures != 0)
    {
        std::fprintf (stderr, "%d regression check(s) failed\n", failures);
        return 1;
    }

    std::puts ("All MatchboxEQ DSP regression checks passed.");
    return 0;
}
