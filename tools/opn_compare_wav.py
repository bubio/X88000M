#!/usr/bin/env python3
"""Compare a reference WAV with an X88000M-rendered WAV.

This intentionally uses only waveform-derived features. It does not depend on
any external emulator code or FM implementation details.
"""

import argparse
import math
import wave

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_wav(path):
    with wave.open(path, "rb") as wav:
        channels = wav.getnchannels()
        rate = wav.getframerate()
        width = wav.getsampwidth()
        frames = wav.getnframes()
        data = wav.readframes(frames)
    if width != 2:
        raise SystemExit(f"{path}: only 16-bit PCM WAV is supported")
    audio = np.frombuffer(data, dtype="<i2").astype(np.float32)
    audio = audio.reshape(-1, channels) / 32768.0
    return rate, audio


def choose_channel(audio, requested):
    if requested is not None:
        idx = requested - 1
        if idx < 0 or idx >= audio.shape[1]:
            raise SystemExit(f"channel {requested} is out of range")
        return audio[:, idx], idx
    rms = np.sqrt(np.mean(audio * audio, axis=0))
    idx = int(np.argmax(rms))
    return audio[:, idx], idx


def rms_envelope(audio, hop, win):
    count = max(0, (len(audio) - win) // hop + 1)
    env = np.empty(count, dtype=np.float32)
    for i in range(count):
        segment = audio[i * hop:i * hop + win]
        env[i] = math.sqrt(float(np.mean(segment * segment)))
    return env


def active_range(env, threshold):
    idx = np.flatnonzero(env > threshold)
    if len(idx) == 0:
        return 0, len(env)
    return max(0, int(idx[0]) - 50), min(len(env), int(idx[-1]) + 51)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("reference_wav")
    parser.add_argument("candidate_wav")
    parser.add_argument("--ref-channel", type=int)
    parser.add_argument("--candidate-channel", type=int)
    parser.add_argument("--out", default="/tmp/opn_compare.png")
    parser.add_argument("--spectrogram", default="/tmp/opn_compare_spectrogram.png")
    parser.add_argument("--segment-start", type=float, default=None)
    parser.add_argument("--segment-sec", type=float, default=12.0)
    args = parser.parse_args()

    ref_rate, ref_audio = read_wav(args.reference_wav)
    cand_rate, cand_audio = read_wav(args.candidate_wav)
    if ref_rate != cand_rate:
        raise SystemExit("sample rates differ; resample before comparing")

    ref, ref_ch = choose_channel(ref_audio, args.ref_channel)
    cand, cand_ch = choose_channel(cand_audio, args.candidate_channel)
    rate = ref_rate
    hop = max(1, rate // 100)
    win = 1024

    ref_env = rms_envelope(ref, hop, win)
    cand_env = rms_envelope(cand, hop, win)
    smooth = np.ones(5, dtype=np.float32) / 5.0
    ref_env = np.convolve(ref_env, smooth, mode="same")
    cand_env = np.convolve(cand_env, smooth, mode="same")

    ref_start, ref_end = active_range(ref_env, max(1e-5, float(ref_env.max()) * 0.02))
    cand_start, cand_end = active_range(cand_env, max(1e-5, float(cand_env.max()) * 0.02))
    ref_active = ref_env[ref_start:ref_end]
    cand_active = cand_env[cand_start:cand_end]
    ref_norm = (ref_active - ref_active.mean()) / (ref_active.std() + 1e-9)
    cand_norm = (cand_active - cand_active.mean()) / (cand_active.std() + 1e-9)
    corr = np.correlate(cand_norm, ref_norm, mode="full")
    lag = int(np.argmax(corr)) - (len(ref_norm) - 1)
    offset = (cand_start - ref_start) + lag

    ref0 = max(ref_start, -offset)
    ref1 = min(ref_end, len(cand_env) - offset)
    cand0 = ref0 + offset
    cand1 = ref1 + offset
    sample_ref0 = ref0 * hop
    sample_cand0 = cand0 * hop
    sample_count = min((ref1 - ref0) * hop, len(ref) - sample_ref0, len(cand) - sample_cand0)
    ref_wave = ref[sample_ref0:sample_ref0 + sample_count]
    cand_wave = cand[sample_cand0:sample_cand0 + sample_count]
    gain = float(np.sqrt(np.mean(ref_wave * ref_wave)) /
                 (np.sqrt(np.mean(cand_wave * cand_wave)) + 1e-12))

    ref_aligned_env = ref_env[ref0:ref1]
    cand_aligned_env = cand_env[cand0:cand1] * gain
    env_error = float(np.mean(np.abs(cand_aligned_env - ref_aligned_env)) /
                      (np.mean(ref_aligned_env) + 1e-9))

    time = np.arange(ref0, ref1) * hop / rate
    plt.figure(figsize=(14, 8))
    plt.subplot(3, 1, 1)
    plt.plot(np.arange(len(ref_env)) * hop / rate, ref_env, label=f"reference ch{ref_ch + 1}", linewidth=0.8)
    plt.plot(np.arange(len(cand_env)) * hop / rate, cand_env * gain, label=f"candidate ch{cand_ch + 1} gain matched", linewidth=0.8)
    plt.ylabel("RMS env")
    plt.title("Raw timelines")
    plt.legend()
    plt.subplot(3, 1, 2)
    plt.plot(time, ref_aligned_env, label="reference", linewidth=0.8)
    plt.plot(time, cand_aligned_env, label="candidate aligned", linewidth=0.8)
    plt.ylabel("RMS env")
    plt.title("Envelope after alignment")
    plt.legend()
    plt.subplot(3, 1, 3)
    plt.plot(time, cand_aligned_env - ref_aligned_env, linewidth=0.7)
    plt.axhline(0, color="black", linewidth=0.5)
    plt.ylabel("env diff")
    plt.xlabel("reference time (s)")
    plt.tight_layout()
    plt.savefig(args.out, dpi=150)

    if args.segment_start is None:
        segment_start = ref0 * hop / rate
    else:
        segment_start = args.segment_start
    start_ref = int(segment_start * rate)
    start_cand = int((segment_start + offset * hop / rate) * rate)
    count = int(args.segment_sec * rate)
    ref_seg = ref[start_ref:start_ref + count]
    cand_seg = cand[start_cand:start_cand + count] * gain
    fig, axes = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
    for ax, data, title in [
        (axes[0], ref_seg, "Reference"),
        (axes[1], cand_seg, "Candidate aligned/gain matched"),
    ]:
        ax.specgram(data, NFFT=1024, Fs=rate, noverlap=768, cmap="magma",
                    vmin=-110, vmax=-25)
        ax.set_ylim(0, 16000)
        ax.set_ylabel("Hz")
        ax.set_title(title)
    axes[1].set_xlabel(f"seconds from reference {segment_start:.2f}s")
    plt.tight_layout()
    plt.savefig(args.spectrogram, dpi=150)

    print(f"reference_channel={ref_ch + 1}")
    print(f"candidate_channel={cand_ch + 1}")
    print(f"offset_seconds={offset * hop / rate:.6f}")
    print(f"gain_reference_over_candidate={gain:.6f}")
    print(f"normalized_envelope_error={env_error:.6f}")
    print(f"envelope_plot={args.out}")
    print(f"spectrogram_plot={args.spectrogram}")


if __name__ == "__main__":
    main()
