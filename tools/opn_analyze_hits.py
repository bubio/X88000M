#!/usr/bin/env python3
"""Analyze FM key-on hits against a reference recording.

The script groups hits by the CH patch state seen in a YM2203 CSV log and
compares short post-key-on windows between a reference WAV and a candidate WAV.
It is designed for drum/percussion channels where the driver rapidly rewrites
FM registers before each hit.
"""

import argparse
import collections
import math
import wave

import numpy as np


CH_SLOT_OFFSETS = {
    0: [0x00, 0x08, 0x04, 0x0C],  # OP1, OP2, OP3, OP4 in natural order
    1: [0x01, 0x09, 0x05, 0x0D],
    2: [0x02, 0x0A, 0x06, 0x0E],
}
OP_PARAM_BASES = [0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90]


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


def patch_signature(regs, channel):
    sig = [regs[0xB0 + channel], regs[0xA0 + channel], regs[0xA4 + channel]]
    for base in OP_PARAM_BASES:
        for off in CH_SLOT_OFFSETS[channel]:
            sig.append(regs[base + off])
    return tuple(sig)


def short_signature_text(sig):
    algo = sig[0] & 0x07
    fb = (sig[0] >> 3) & 0x07
    fnum = ((sig[2] & 0x07) << 8) | sig[1]
    block = (sig[2] >> 3) & 0x07
    params = sig[3:]
    tls = params[4:8]
    ars = params[8:12]
    drs = params[12:16]
    srs = params[16:20]
    slrr = params[20:24]
    rrs = [x & 0x0F for x in slrr]
    sls = [(x >> 4) & 0x0F for x in slrr]
    return (
        f"ALGO={algo} FB={fb} F={fnum:04d} B={block} "
        f"TL={','.join(f'{x:02X}' for x in tls)} "
        f"AR={','.join(f'{x & 0x1F:02d}' for x in ars)} "
        f"DR={','.join(f'{x & 0x1F:02d}' for x in drs)} "
        f"SR={','.join(f'{x & 0x1F:02d}' for x in srs)} "
        f"SL={','.join(f'{x:02d}' for x in sls)} "
        f"RR={','.join(f'{x:02d}' for x in rrs)}"
    )


def load_hits(path, channel):
    regs = [0] * 256
    hits = []
    with open(path, "r", encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) < 4:
                continue
            frame = int(parts[0])
            event = parts[1]
            addr = int(parts[2], 16)
            data = int(parts[3], 16)
            if event == "D":
                regs[addr] = data
                if addr == 0x28:
                    key_ch = data & 0x03
                    key_mask = (data >> 4) & 0x0F
                    if key_ch == channel and key_mask != 0:
                        hits.append((frame, key_mask, patch_signature(regs, channel)))
            elif event == "A":
                # Prescaler address-only events have already been captured in
                # the rendered WAV; they do not affect patch grouping here.
                pass
    return hits


def rms_envelope(audio, win):
    env = np.empty(len(audio), dtype=np.float32)
    half = win // 2
    sq = audio * audio
    csum = np.concatenate(([0.0], np.cumsum(sq, dtype=np.float64)))
    for i in range(len(audio)):
        a = max(0, i - half)
        b = min(len(audio), i + half)
        env[i] = math.sqrt(float((csum[b] - csum[a]) / max(1, b - a)))
    return env


def high_band_ratio(segment, rate):
    if len(segment) < 256:
        return 0.0
    win = np.hanning(len(segment)).astype(np.float32)
    spec = np.abs(np.fft.rfft(segment * win))
    freqs = np.fft.rfftfreq(len(segment), 1.0 / rate)
    total = float(np.sum(spec)) + 1e-12
    high = float(np.sum(spec[(freqs >= 4000.0) & (freqs <= 16000.0)]))
    return high / total


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log_csv")
    parser.add_argument("reference_wav")
    parser.add_argument("candidate_wav")
    parser.add_argument("--fm-ch", type=int, default=3)
    parser.add_argument("--ref-channel", type=int)
    parser.add_argument("--candidate-channel", type=int, default=1)
    parser.add_argument("--offset-sec", type=float, required=True,
                        help="candidate time = reference time + offset")
    parser.add_argument("--gain", type=float, default=1.0,
                        help="multiply candidate waveform by this gain")
    parser.add_argument("--window-ms", type=float, default=240.0)
    parser.add_argument("--min-gap-ms", type=float, default=55.0)
    parser.add_argument("--top", type=int, default=8)
    parser.add_argument("--out", default="/tmp/opn_hit_groups.png")
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()

    channel = args.fm_ch - 1
    if channel < 0 or channel > 2:
        raise SystemExit("--fm-ch must be 1..3")
    ref_rate, ref_audio = read_wav(args.reference_wav)
    cand_rate, cand_audio = read_wav(args.candidate_wav)
    if ref_rate != cand_rate:
        raise SystemExit("sample rates differ; resample before analyzing")
    rate = ref_rate
    ref, ref_ch = choose_channel(ref_audio, args.ref_channel)
    cand, cand_ch = choose_channel(cand_audio, args.candidate_channel)
    cand = cand * args.gain

    hits = load_hits(args.log_csv, channel)
    min_gap = int(args.min_gap_ms * rate / 1000.0)
    filtered = []
    last_frame = -10**18
    for frame, mask, sig in hits:
        if frame - last_frame >= min_gap:
            filtered.append((frame, mask, sig))
            last_frame = frame
    hits = filtered

    groups = collections.defaultdict(list)
    window = int(args.window_ms * rate / 1000.0)
    tail_points = [int(ms * rate / 1000.0) for ms in (50, 100, 200)]
    for frame, mask, sig in hits:
        cand_start = frame
        ref_start = int(round(frame - args.offset_sec * rate))
        if ref_start < 0 or cand_start < 0:
            continue
        if ref_start + window >= len(ref) or cand_start + window >= len(cand):
            continue
        ref_seg = ref[ref_start:ref_start + window]
        cand_seg = cand[cand_start:cand_start + window]
        ref_env = rms_envelope(np.abs(ref_seg), max(64, rate // 500))
        cand_env = rms_envelope(np.abs(cand_seg), max(64, rate // 500))
        ref_peak = float(np.max(ref_env)) + 1e-12
        cand_peak = float(np.max(cand_env)) + 1e-12
        tails = []
        for pt in tail_points:
            idx = min(len(ref_env) - 1, pt)
            tails.append((
                float(ref_env[idx] / ref_peak),
                float(cand_env[idx] / cand_peak),
            ))
        spec_len = min(window, int(0.12 * rate))
        groups[sig].append({
            "frame": frame,
            "mask": mask,
            "ref_peak": ref_peak,
            "cand_peak": cand_peak,
            "peak_ratio": cand_peak / ref_peak,
            "tails": tails,
            "ref_high": high_band_ratio(ref_seg[:spec_len], rate),
            "cand_high": high_band_ratio(cand_seg[:spec_len], rate),
            "ref_env_norm": ref_env / ref_peak,
            "cand_env_norm": cand_env / cand_peak,
        })

    ranked = sorted(groups.items(), key=lambda kv: len(kv[1]), reverse=True)
    print(f"reference_channel={ref_ch + 1}")
    print(f"candidate_channel={cand_ch + 1}")
    print(f"hits={len(hits)} analyzed_groups={len(ranked)}")
    print("rank,count,peakRatio,candHigh/refHigh,tail50_ref,tail50_cand,tail100_ref,tail100_cand,tail200_ref,tail200_cand,signature")
    for rank, (sig, rows) in enumerate(ranked[:args.top], 1):
        peak_ratio = np.mean([r["peak_ratio"] for r in rows])
        ref_high = np.mean([r["ref_high"] for r in rows])
        cand_high = np.mean([r["cand_high"] for r in rows])
        tails = []
        for idx in range(3):
            tails.append(np.mean([r["tails"][idx][0] for r in rows]))
            tails.append(np.mean([r["tails"][idx][1] for r in rows]))
        print(
            f"{rank},{len(rows)},{peak_ratio:.3f},{(cand_high/(ref_high+1e-12)):.3f},"
            f"{tails[0]:.3f},{tails[1]:.3f},{tails[2]:.3f},{tails[3]:.3f},"
            f"{tails[4]:.3f},{tails[5]:.3f},{short_signature_text(sig)}"
        )

    if args.no_plot:
        return

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plot_count = min(args.top, len(ranked))
    if plot_count > 0:
        fig, axes = plt.subplots(plot_count, 1, figsize=(12, 2.2 * plot_count), sharex=True)
        if plot_count == 1:
            axes = [axes]
        x_ms = np.arange(window) * 1000.0 / rate
        for ax, (sig, rows) in zip(axes, ranked[:plot_count]):
            ref_avg = np.mean([r["ref_env_norm"] for r in rows], axis=0)
            cand_avg = np.mean([r["cand_env_norm"] for r in rows], axis=0)
            ax.plot(x_ms, ref_avg, label="reference", linewidth=1.0)
            ax.plot(x_ms, cand_avg, label="candidate", linewidth=1.0)
            ax.set_ylim(0, 1.05)
            ax.set_ylabel("norm env")
            ax.set_title(f"n={len(rows)} {short_signature_text(sig)}", fontsize=9)
            ax.grid(True, alpha=0.2)
        axes[-1].set_xlabel("ms after CH key-on")
        axes[0].legend()
        plt.tight_layout()
        plt.savefig(args.out, dpi=150)
        print(f"plot={args.out}")


if __name__ == "__main__":
    main()
