#!/usr/bin/env python3
# Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
# SPDX-License-Identifier: AGPL-3.0-only
#
# Makes the Piano preview samples (resources/piano/*.flac) from the VCSL upright piano "Upright Piano, Knight"
# (Versilian Community Sample Library, CC0, https://github.com/sgossner/VCSL). Only needed to remake the samples; the
# finished files are in the project. Needs ffmpeg.
#
# Usage: tools/make_piano_samples.py <folder with the downloaded wav files named keyNNN.wav> <output folder>
#   keyNNN.wav is the file "Player_vl2_rr1_<tag>.wav" (the louder of the two velocity layers) for the note with MIDI number NNN.
#
# Every sample is: cut at the start where the silence ends, at most 3 seconds, a soft fade out at the end, mono, 44.1 kHz,
# the same peak level for all, 16 bit FLAC.
import re
import subprocess
import sys
from pathlib import Path

MAX_SECONDS = 3.0
PEAK_DB = -4.0


def run(args):
    return subprocess.run(args, capture_output=True, text=True, check=True)


def main():
    source, target = Path(sys.argv[1]), Path(sys.argv[2])
    target.mkdir(parents=True, exist_ok=True)
    for wav in sorted(source.glob("key*.wav")):
        number = int(re.search(r"key(\d+)", wav.name).group(1))
        cut = target / f"_cut_{number:03d}.wav"
        # 1. mono, 44.1 kHz, the silence before the note removed, at most 3 seconds
        run(["ffmpeg", "-v", "error", "-y", "-i", str(wav),
             "-af", "pan=mono|c0=0.5*c0+0.5*c1,aresample=44100,silenceremove=start_periods=1:start_threshold=-60dB:start_silence=0.002",
             "-t", str(MAX_SECONDS), "-c:a", "pcm_f32le", str(cut)])
        length = float(run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(cut)]).stdout)
        info = subprocess.run(["ffmpeg", "-hide_banner", "-i", str(cut), "-af", "volumedetect", "-f", "null", "-"], capture_output=True, text=True).stderr
        peak = float(re.search(r"max_volume: (-?[\d.]+) dB", info).group(1))
        fade = min(0.6, length * 0.25)
        out = target / f"piano_{number:03d}.flac"
        # 2. the same peak level, a tiny fade in (no click at the start) and a soft fade out, 16 bit FLAC
        run(["ffmpeg", "-v", "error", "-y", "-i", str(cut),
             "-af", f"volume={PEAK_DB - peak:.2f}dB,afade=t=in:d=0.001,afade=t=out:st={length - fade:.3f}:d={fade:.3f}:curve=qsin",
             "-c:a", "flac", "-sample_fmt", "s16", "-compression_level", "12", str(out)])
        cut.unlink()
        print(f"{out.name}: {length:.2f} s, {out.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
