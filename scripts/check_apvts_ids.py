#!/usr/bin/env python3
"""Verify that every JUCE APVTS parameter has a matching editor attachment."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
PROCESSOR = (ROOT / "Source" / "PluginProcessor.cpp").read_text(encoding="utf-8")
EDITOR = (ROOT / "Source" / "PluginEditor.cpp").read_text(encoding="utf-8")

parameter_ids = set(re.findall(
    r'make_unique<AudioParameter(?:Float|Int|Choice|Bool)>\s*\(\s*"([A-Za-z][A-Za-z0-9]*)"',
    PROCESSOR,
))
attachment_ids = set(re.findall(
    r'make_unique<[SC]A>\s*\(\s*proc\.apvts\s*,\s*"([A-Za-z][A-Za-z0-9]*)"',
    EDITOR,
))

if not parameter_ids:
    raise SystemExit("No APVTS parameter declarations found.")
if parameter_ids != attachment_ids:
    missing = sorted(parameter_ids - attachment_ids)
    unknown = sorted(attachment_ids - parameter_ids)
    details = []
    if missing:
        details.append("parameters without editor attachments: " + ", ".join(missing))
    if unknown:
        details.append("editor attachments without parameters: " + ", ".join(unknown))
    raise SystemExit("; ".join(details))

print(f"APVTS/editor contract valid: {len(parameter_ids)} parameter IDs matched.")
