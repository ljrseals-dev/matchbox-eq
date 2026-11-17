#!/usr/bin/env python3
"""Run the Python reference over checked-in dry WAV samples and report metrics."""
import json
import sys
from pathlib import Path

import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from Source.Pure_python import MatchboxEQ
SAMPLE_FILES = (
    "samples/Fridge_A_dry.wav",
    "samples/HomeFridge_A_dry.wav",
    "samples/MatchboxEQ_A_dry.wav",
)


def to_float_audio(data):
    if data.ndim == 1:
        data = np.column_stack((data, data))
    elif data.ndim == 2 and data.shape[1] == 1:
        data = np.repeat(data, 2, axis=1)
    elif data.ndim == 2 and data.shape[1] > 2:
        data = data[:, :2]
    elif data.ndim != 2:
        raise ValueError("unsupported WAV channel layout")

    if np.issubdtype(data.dtype, np.integer):
        info = np.iinfo(data.dtype)
        scale = float(max(abs(info.min), abs(info.max)))
        return data.astype(np.float64) / scale
    if np.issubdtype(data.dtype, np.floating):
        return data.astype(np.float64)
    raise ValueError("unsupported WAV sample type")


def rms(data):
    return float(np.sqrt(np.mean(np.square(data)))) if data.size else 0.0


def main():
    report = []
    for relative_path in SAMPLE_FILES:
        path = ROOT / relative_path
        sample_rate, raw = wavfile.read(path)
        dry = to_float_audio(raw)
        processor = MatchboxEQ(sample_rate)
        wet = processor.process(dry)

        group_energies = processor.last_group_energy[:processor.last_group_count]
        values = np.concatenate((dry.reshape(-1), wet.reshape(-1), group_energies))
        if not np.all(np.isfinite(values)):
            raise RuntimeError(f"non-finite DSP result for {relative_path}")

        dry_rms = rms(dry)
        wet_rms = rms(wet)
        report.append({
            "file": relative_path,
            "sample_rate": int(sample_rate),
            "frames": int(dry.shape[0]),
            "dry_rms": dry_rms,
            "wet_rms": wet_rms,
            "rms_ratio": wet_rms / dry_rms if dry_rms > 0.0 else 0.0,
            "dry_peak": float(np.max(np.abs(dry))) if dry.size else 0.0,
            "wet_peak": float(np.max(np.abs(wet))) if wet.size else 0.0,
            "group_energy": [float(value) for value in group_energies],
        })

    print(json.dumps(report, indent=2, allow_nan=False))


if __name__ == "__main__":
    main()
