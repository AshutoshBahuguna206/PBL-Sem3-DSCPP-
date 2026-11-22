#!/usr/bin/env python3
"""Generates every sound and piece of music in the game into assets/sounds/*.wav. All of it is original and synthesised
here (no samples, no licence issues). Run from the project folder:   python3 tools/make_sounds.py   (needs numpy and scipy)
The random generator is seeded, so the files are identical every time.

Style: a 90s Indian TV detective show. Synth brass stabs, a drum machine, timpani and suspense strings, in D minor.
  theme.wav       the opening theme (after log-in, and on the title screen)
  ambient.wav     a quiet investigation loop (pulsing bass, ticking hi-hat, a string pad) that repeats with no seam
  exposed.wav     the dramatic "dun dun DUNNN" when a lie is caught
  door, locked, step, pickup, wrong, lab_success, dial_click, ending, game_over: the effects
Everything is 16-bit mono at 22050 Hz."""
import pathlib, re, wave, zlib
import numpy as np
from scipy.signal import butter, lfilter, sosfilt

SR = 22050
OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "sounds"
rng = np.random.default_rng(1921)

# ------------------------------------------------------------------ basic tools
def tt(dur): return np.arange(max(1, int(dur * SR))) / SR
def _sos(kind, fc):
    wn = np.asarray(fc, dtype=float) / (SR / 2)
    return butter(4, float(wn) if wn.ndim == 0 else list(wn), btype=kind, output="sos")
def lp(x, fc): return sosfilt(_sos("low", fc), x)
def hp(x, fc): return sosfilt(_sos("high", fc), x)
def bp(x, lo, hi): return sosfilt(_sos("band", [lo, hi]), x)
def noise(dur): return rng.normal(0, 1, len(tt(dur)))
def taper(x, ms=25):
    n = max(1, min(int(ms / 1000 * SR), len(x) // 4)); y = x.copy(); y[-n:] *= np.linspace(1, 0, n); return y
def fade(x, a=0.005, r=0.05):
    y = x.copy(); na, nr = min(int(a * SR), len(x) // 2), min(int(r * SR), len(x) // 2)
    if na: y[:na] *= np.linspace(0, 1, na)
    if nr: y[-nr:] *= np.linspace(1, 0, nr)
    return y
def place(dst, src, at, gain=1.0):
    src = taper(src); i = int(round(at * SR)); j = min(len(dst), i + len(src))
    if 0 <= i < len(dst): dst[i:j] += gain * src[:j - i]
def reverb(x, wet=0.25, tail=1.2):
    x = np.concatenate([taper(x, 30), np.zeros(int(tail * SR))]); y = np.zeros_like(x)
    for d, g in [(0.0297, 0.78), (0.0371, 0.76), (0.0411, 0.74), (0.0437, 0.72)]:
        D = int(d * SR); a = np.zeros(D + 1); a[0] = 1; a[D] = -g; y += lfilter([1.0], a, x)
    y /= 4
    for d, g in [(0.005, 0.7), (0.0017, 0.7)]:
        D = int(d * SR); b = np.zeros(D + 1); b[0] = -g; b[D] = 1; a = np.zeros(D + 1); a[0] = 1; a[D] = -g; y = lfilter(b, a, y)
    return (1 - wet) * x + wet * 1.6 * y
def norm(x, peak): return x * (peak / max(1e-9, np.max(np.abs(x))))
def trim(x, floor=0.0015):
    """Cut the silent tail after a sound has died away, keeping a short margin."""
    loud = np.nonzero(np.abs(x) > floor * np.max(np.abs(x)) * 10)[0]
    end = min(len(x), (loud[-1] if len(loud) else len(x) - 1) + int(0.25 * SR))
    return fade(x[:end], 0.0, 0.2)
def save(name, x, loop=False):
    if not loop and name != "chaos.wav": x = trim(x)
    OUT.mkdir(parents=True, exist_ok=True)
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
    print(f"{name:16s} {len(x) / SR:5.1f} s  peak {np.max(np.abs(x)):.2f}")

NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
def hz(name):
    m = re.fullmatch(r"([A-G])(#|b)?(-?\d)", name)
    n = NOTE[m.group(1)] + {"#": 1, "b": -1, None: 0}[m.group(2)] + 12 * (int(m.group(3)) + 1)
    return 440.0 * 2 ** ((n - 69) / 12)

# ------------------------------------------------------------------ instruments
def saw(freq, dur, vib=0.0, vib_rate=5.0):
    t = tt(dur); f = freq * (1 + vib * np.sin(2 * np.pi * vib_rate * t)); ph = 2 * np.pi * np.cumsum(f) / SR
    H = max(1, min(14, int((SR / 2 - 400) / (freq * 1.02))))
    return sum(np.sin(h * ph) / h for h in range(1, H + 1))
def brass(name_or_hz, dur, level=1.0):
    """Synth brass: three detuned saws, bright at the attack and darker as it sustains (like a filter envelope)."""
    f = hz(name_or_hz) if isinstance(name_or_hz, str) else name_or_hz
    total = dur + 0.25; t = tt(total)
    x = sum(saw(f * (1 + d), total, 0.003, 5.2) for d in (-0.004, 0.0, 0.004)) / 3
    amp = np.minimum(1, t / 0.025) * (0.75 + 0.25 * np.exp(-t / 0.12)) * np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.07))
    b = 0.2 + 0.8 * np.exp(-t / 0.22)
    return level * (lp(x, 950) * (1 - b) + lp(x, 3800) * b) * amp
def brass_chord(names, dur, level=1.0): return sum(brass(n, dur, level) for n in names) / max(1, len(names)) ** 0.5
def strings(names, dur, level=1.0, trem=0.0, attack=0.35):
    total = dur + 0.6; t = tt(total)
    x = sum(saw(hz(n) * (1 + d), total, 0.004, 5.5) for n in names for d in (-0.003, 0.003)) / (2 * len(names)) ** 0.5
    env = np.minimum(1, t / attack) * np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.25))
    if trem: env *= 1 - trem * (0.5 + 0.5 * np.sin(2 * np.pi * 9 * t))
    return level * lp(x, 2600) * env
def bass(name, dur, level=1.0):
    f = hz(name); t = tt(dur + 0.05)
    x = 0.7 * saw(f, dur + 0.05) + 0.6 * np.sin(2 * np.pi * f * t)
    env = np.minimum(1, t / 0.004) * (0.6 + 0.4 * np.exp(-t / 0.08)) * np.where(t < dur, 1.0, np.exp(-(t - dur) / 0.02))
    return level * lp(x, 520) * env
def pluck(name, dur=0.6, level=1.0):
    f = hz(name); t = tt(dur)
    return level * (lp(saw(f, dur), 2600) * np.exp(-t / 0.16) + 0.6 * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.35)) * np.minimum(1, t / 0.003)
def kick(level=1.0):
    t = tt(0.35); f = 46 + 110 * np.exp(-t / 0.03)
    return level * (np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.17) + 0.3 * hp(noise(0.35), 2500) * np.exp(-t / 0.003))
def snare(level=1.0):
    t = tt(0.25)
    return level * (0.8 * bp(noise(0.25), 1200, 7000) * np.exp(-t / 0.09) + 0.6 * np.sin(2 * np.pi * 185 * t) * np.exp(-t / 0.05))
def hat(open_=False, level=1.0):
    d = 0.2 if open_ else 0.06; t = tt(d)
    return level * 0.45 * hp(noise(d), 6500) * np.exp(-t / (0.08 if open_ else 0.016))
def tom(freq, level=1.0):
    t = tt(0.45); f = freq * (1 + 0.5 * np.exp(-t / 0.05))
    return level * np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.22)
def timpani(name, level=1.0, dur=1.8):
    f = hz(name); t = tt(dur)
    y = sum(w * np.sin(2 * np.pi * f * r * t) * np.exp(-t / (0.9 / r ** 0.6)) for r, w in ((1, 1), (1.5, 0.45), (1.99, 0.3), (2.44, 0.15)))
    return level * (y + 0.4 * lp(noise(dur), 900) * np.exp(-t / 0.02))
def timp_roll(name, dur, start_level=0.15, end_level=1.0):
    y = np.zeros(len(tt(dur + 1.8))); n = int(dur * 16)
    for i in range(n): place(y, timpani(name, start_level + (end_level - start_level) * i / max(1, n - 1), 1.2), i / 16)
    return y
def cymbal(dur=1.6, swell=False, level=1.0):
    t = tt(dur); x = hp(noise(dur), 5000)
    env = (t / dur) ** 2 if swell else np.exp(-t / (dur / 3))
    return level * 0.5 * x * env
def whoosh(dur=0.5, level=1.0):
    t = tt(dur); return level * bp(noise(dur), 400, 3000) * np.sin(np.pi * t / dur) ** 2
def orch_hit(names, low, dur=0.3, level=1.0):
    y = np.zeros(len(tt(dur + 1.9)))
    place(y, brass_chord(names, dur, 1.0), 0); place(y, strings(names, dur, 0.5, attack=0.01), 0)
    place(y, timpani(low, 0.9), 0); place(y, lp(noise(0.12), 3000) * np.exp(-tt(0.12) / 0.03), 0, 0.5)
    return level * y

class Track:
    """A piece of music laid out in beats."""
    def __init__(self, bpm, beats, tail=3.0):
        self.spb = 60.0 / bpm; self.x = np.zeros(int((beats * self.spb + tail) * SR))
    def at(self, beat, sig, gain=1.0): place(self.x, sig, beat * self.spb, gain); return self
    def dur(self, beats): return beats * self.spb

# ------------------------------------------------------------------ the theme (132 BPM, D minor, about 31 seconds)
def dha(level=1.0):
    """A deep hand-drum stroke (dhol or tabla bass): a bend down in pitch and a slap."""
    t = tt(0.45); f = 70 + 75 * np.exp(-t / 0.04)
    return level * (np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / 0.2) + 0.35 * bp(noise(0.45), 300, 2500) * np.exp(-t / 0.012))
def na(level=1.0, pitch=880.0):
    """A bright, ringing hand-drum stroke (tabla 'na')."""
    t = tt(0.3)
    y = sum(w * np.sin(2 * np.pi * pitch * r * t) * np.exp(-t / (0.12 / r)) for r, w in ((1, 1), (2.0, 0.5), (2.98, 0.3)))
    return level * (y + 0.4 * hp(noise(0.3), 2500) * np.exp(-t / 0.006))
def riser(dur, level=1.0):
    """A build-up: noise opening from dark to bright, with a rising tone underneath."""
    t = tt(dur); x = noise(dur); u = t / dur
    sweep = lp(x, 600) * (1 - u) + hp(x, 1800) * u
    tone = np.sin(2 * np.pi * np.cumsum(180 + 900 * u ** 2) / SR)
    return level * (0.7 * sweep + 0.25 * tone) * u ** 2
def snare_roll(dur, level=1.0):
    y = np.zeros(len(tt(dur + 0.3))); t, gap = 0.0, 0.11
    while t < dur:
        place(y, snare(), t, 0.25 + 0.75 * t / dur); t += gap; gap = max(0.028, gap * 0.93)
    return level * y

def theme():
    """Energetic and dramatic: an impact and a racing string ostinato, a driving groove with hand drums, a brass
    melody, a breakdown that builds with a riser and a snare roll, a climax an octave higher, and three final hits."""
    T = Track(132, 64, tail=3.0); b = T.dur
    ost = ["D4", "D4", "F4", "D4", "A4", "D4", "F4", "D4"]                 # the racing ostinato (sixteenths)
    roots = ["D2", "D2", "Bb1", "C2"]                                       # bass, one per bar, repeating
    chords = [["D4", "F4", "A4"], ["D4", "F4", "A4"], ["D4", "F4", "Bb4"], ["C4", "E4", "G4"]]
    def ostinato(bar, bars, level):
        for k in range(bars * 16):
            T.at(4 * bar + 0.25 * k, spiccato(ost[k % 8], 0.1), level * (1.0 if k % 4 == 0 else 0.7))
    def groove(bar, bars, full=True):
        for k in range(bars):
            beat = 4 * (bar + k)
            for x in (0, 1.5, 2, 2.75): T.at(beat + x, kick(), 0.6)        # a driving kick
            for x in (1, 3): T.at(beat + x, snare(), 0.6)
            for x in range(16 if full else 8): T.at(beat + (0.25 if full else 0.5) * x, hat(), 0.32 if x % 2 else 0.22)
            for x, f in ((0, "dha"), (0.75, "na"), (1.5, "dha"), (2.5, "na"), (3, "dha"), (3.5, "na")):
                T.at(beat + x, dha() if f == "dha" else na(), 0.45 if f == "dha" else 0.3)
            r = roots[(bar + k) % 4]
            for e in range(8):
                note = r if e % 2 == 0 else r[:-1] + str(int(r[-1]) + 1)   # octave-jumping bass
                T.at(beat + 0.5 * e, bass(note, b(0.42)), 0.42)
    def stabs(bar, bars, level):
        for k in range(bars):
            ch = chords[(bar + k) % 4]
            for x in (0, 0.75, 2.5): T.at(4 * (bar + k) + x, brass_chord(ch, b(0.3)), level)
    # bars 1-2: the impact, then the ostinato with a riser and a snare roll
    T.at(0, orch_hit(["D4", "F4", "A4", "D5"], "D2", b(1)), 1.0); T.at(0, cymbal(2.0), 0.6)
    ostinato(0.5, 2, 0.45); T.at(1, riser(b(7)), 0.5); T.at(4, snare_roll(b(4)), 0.5)
    # bars 3-6: the groove arrives, with brass stabs
    T.at(8, orch_hit(["D4", "F4", "A4"], "D2", b(0.5)), 0.9); T.at(8, cymbal(1.6), 0.5)
    groove(2, 4); ostinato(2, 4, 0.35); stabs(2, 4, 0.55)
    # bars 7-10: the melody
    melody = [(0, "A4", 0.75), (0.75, "D5", 0.75), (1.5, "F5", 0.5), (2, "E5", 0.5), (2.5, "D5", 0.5), (3, "A4", 1),
              (4, "Bb4", 0.75), (4.75, "D5", 0.75), (5.5, "F5", 0.5), (6, "G5", 1), (7, "F5", 0.5), (7.5, "E5", 0.5),
              (8, "D5", 0.75), (8.75, "F5", 0.75), (9.5, "A5", 1.5), (11, "G5", 0.5), (11.5, "F5", 0.5),
              (12, "E5", 0.75), (12.75, "C5", 0.75), (13.5, "E5", 0.5), (14, "D5", 2)]
    groove(6, 4); ostinato(6, 4, 0.3)
    for x, n, d in melody: T.at(24 + x, brass(n, b(d) * 0.92), 0.85); T.at(24 + x, pluck(n, 0.4), 0.25)
    for k in range(4): T.at(4 * (6 + k), strings(chords[k], b(4), 0.25))
    # bars 11-12: the breakdown builds
    T.at(40, strings(["D4", "A4", "D5"], b(8), 0.35, trem=0.6, attack=0.1)); ostinato(10, 2, 0.4)
    for x in range(0, 8): T.at(40 + x, kick(), 0.35 + 0.05 * x)
    T.at(40, riser(b(8)), 0.6); T.at(44, snare_roll(b(4)), 0.6)
    # bars 13-15: the climax, an octave higher, with crashes
    T.at(48, orch_hit(["D4", "F4", "A4", "D5"], "D2", b(0.5)), 1.0)
    groove(12, 3); ostinato(12, 3, 0.35); stabs(12, 3, 0.6)
    for k in range(3): T.at(4 * (12 + k), cymbal(1.4), 0.45)
    for x, n, d in melody[:15]:
        up = n[:-1] + str(int(n[-1]) + 1) if int(n[-1]) < 5 else n
        T.at(48 + x, brass(up, b(d) * 0.92), 0.8); T.at(48 + x, brass(n, b(d) * 0.92), 0.5)
    # bar 16: three final hits, DUN ... DUN ... DUNNN
    for x, lvl in ((60, 0.9), (61, 0.95)): T.at(x, orch_hit(["D4", "F4", "A4"], "D2", b(0.4)), lvl)
    T.at(62, orch_hit(["D4", "F4", "A4", "D5"], "D2", b(3)), 1.15); T.at(62, cymbal(2.6), 0.6); T.at(62, timp_roll("D2", b(3), 0.9, 0.1), 0.6)
    y = reverb(T.x, 0.18, 1.2)
    y = np.tanh(2.2 * y / np.max(np.abs(y))) / np.tanh(2.2)              # gentle saturation: more energy, no clipping
    return norm(fade(y, 0.005, 1.5), 0.92)

# ------------------------------------------------------------------ the investigation loop: a suspense underscore
def spiccato(name, dur=0.16, level=1.0):
    """A short, bowed low string note (like a tense TV-drama ostinato)."""
    f = hz(name) if isinstance(name, str) else float(name); t = tt(dur + 0.08)
    x = sum(saw(f * (1 + d), dur + 0.08) for d in (-0.004, 0.004)) / 2
    env = np.minimum(1, t / 0.006) * np.exp(-t / 0.09)
    return level * lp(x, 1300) * env
def tick(level=1.0, accent=False):
    """A clock tick: a short wooden click."""
    t = tt(0.05); f = 2100 if accent else 1800
    return level * (0.7 * bp(noise(0.05), 1500, 4500) * np.exp(-t / 0.004) + 0.5 * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.012))

def ambient():
    """8 bars at 92 BPM in D minor, looping with no seam. Built to keep tension without a tune:
    a ticking clock on every beat; a low ostinato that moves D, E flat (a semitone up: unease), D, then
    B flat and A (pulling back home); high tremolo strings that swell and fade; soft timpani and heartbeat thuds."""
    bpm, bars = 92, 8; T = Track(bpm, bars * 4, tail=3.0); b = T.dur; n = int(round(bars * 4 * T.spb * SR))
    roots = ["D2", "D2", "Eb2", "Eb2", "D2", "D2", "Bb1", "A1"]
    pattern = [0, 0, 12, 0, 0, 7, 12, 0]                                   # semitones above the root, in eighths
    for bar, root in enumerate(roots):
        base = hz(root)
        for e, st in enumerate(pattern):
            T.at(4 * bar + 0.5 * e, spiccato(base * 2 ** (st / 12), 0.15), 0.55 if e % 4 == 0 else 0.38)
    for beat in range(bars * 4):
        T.at(beat, tick(accent=(beat % 4 == 0)), 0.28 if beat % 4 == 0 else 0.2)
    T.at(0, strings(["D4", "A4"], b(15), 0.20, trem=0.55, attack=b(10)))     # a slow swell, then a fade
    T.at(16, strings(["A4", "Bb4"], b(14), 0.17, trem=0.6, attack=b(9)))     # a semitone cluster: something is off
    T.at(0, timpani("D2", 1.0, 2.4), 0.35); T.at(16, timpani("D2", 1.0, 2.4), 0.30)
    for bar in (3, 7):
        T.at(4 * bar + 2, kick(), 0.30); T.at(4 * bar + 2.45, kick(), 0.20)   # two heartbeat thuds
    x = T.x; y = x[:n].copy(); y[:len(x) - n] += x[n:]                         # fold the tail round: no seam
    return norm(lp(y, 6000), 0.36)

# ------------------------------------------------------------------ effects
def exposed():
    y = np.zeros(int(4.0 * SR))
    place(y, whoosh(0.45, 0.6), 0.33)
    place(y, orch_hit(["D3", "F3", "A3"], "D2", 0.22), 0.0)
    place(y, orch_hit(["D3", "F3", "A3"], "D2", 0.22), 0.38)
    place(y, orch_hit(["D3", "Eb3", "A3", "D4"], "D2", 1.2, 1.2), 0.78)       # the last hit is dissonant: something is wrong
    place(y, timp_roll("D2", 0.9, 0.8, 0.2), 0.78, 0.5); place(y, cymbal(2.0), 0.78, 0.45)
    return norm(fade(reverb(y, 0.30, 1.2), 0.002, 0.6), 0.92)
def door():
    y = np.zeros(int(0.65 * SR))
    click = hp(noise(0.02), 2000) * np.exp(-tt(0.02) / 0.002) + 0.5 * np.sin(2 * np.pi * 2300 * tt(0.02)) * np.exp(-tt(0.02) / 0.006)
    place(y, 0.35 * click, 0.0)
    place(y, 0.45 * bp(noise(0.35), 250, 1400) * np.sin(np.pi * tt(0.35) / 0.35) ** 2, 0.03)
    place(y, 0.5 * np.sin(2 * np.pi * 165 * tt(0.12)) * np.exp(-tt(0.12) / 0.04) + 0.5 * bp(noise(0.12), 300, 1100) * np.exp(-tt(0.12) / 0.025), 0.42)
    return norm(fade(reverb(y, 0.15, 0.4), 0.001, 0.1), 0.70)
def locked():
    y = np.zeros(int(0.5 * SR))
    for at in (0.0, 0.08, 0.16):
        tick = bp(noise(0.05), 1500, 5000) * np.exp(-tt(0.05) / 0.006) + 0.6 * np.sin(2 * np.pi * 900 * tt(0.05)) * np.exp(-tt(0.05) / 0.018)
        place(y, tick, at, 0.8)
    place(y, 0.6 * np.sin(2 * np.pi * 120 * tt(0.15)) * np.exp(-tt(0.15) / 0.05) + 0.4 * bp(noise(0.15), 200, 900) * np.exp(-tt(0.15) / 0.03), 0.24)
    return norm(fade(reverb(y, 0.15, 0.3), 0.001, 0.08), 0.72)
def step():
    t = tt(0.13)
    x = bp(noise(0.13), 150, 900) * np.exp(-t / 0.035) + 0.6 * np.sin(2 * np.pi * 90 * t) * np.exp(-t / 0.04)
    return norm(fade(x, 0.001, 0.03), 0.50)
def pickup():
    y = np.zeros(int(1.0 * SR))
    place(y, pluck("A4", 0.8), 0.0); place(y, pluck("D5", 0.9), 0.12); place(y, cymbal(0.9), 0.12, 0.25)
    return norm(fade(reverb(y, 0.30, 0.6), 0.002, 0.25), 0.78)
def wrong():
    y = np.zeros(int(1.0 * SR))
    place(y, tom(110), 0.0, 0.7)
    place(y, lp(brass("E3", 0.2), 1500), 0.0, 0.8); place(y, lp(brass("Eb3", 0.42), 1200), 0.25, 0.8)
    return norm(fade(reverb(y, 0.22, 0.5), 0.002, 0.2), 0.78)
def lab_success():
    y = np.zeros(int(2.2 * SR))
    for k, n in enumerate(("D4", "F4", "A4", "D5")): place(y, pluck(n, 0.7), 0.09 * k, 0.8)
    place(y, cymbal(0.5, swell=True), 0.0, 0.4)
    place(y, brass_chord(["D4", "F#4", "A4"], 0.6), 0.5, 0.8); place(y, timpani("D2"), 0.5, 0.6)
    return norm(fade(reverb(y, 0.30, 0.8), 0.002, 0.3), 0.80)
def objective():
    """A short, bright "objective complete" cue: a rising major arpeggio and a soft shimmer."""
    y = np.zeros(int(1.0 * SR))
    for k, n in enumerate(("D5", "F#5", "A5")): place(y, pluck(n, 0.6), 0.075 * k, 0.75)
    place(y, cymbal(0.7), 0.15, 0.18)
    return norm(fade(reverb(y, 0.25, 0.5), 0.002, 0.2), 0.72)
def knock():
    """Three knocks on a heavy wooden door."""
    y = np.zeros(int(1.0 * SR))
    for at in (0.0, 0.2, 0.4):
        t = tt(0.12)
        hit = 0.9 * np.sin(2 * np.pi * 165 * t) * np.exp(-t / 0.035) + 0.6 * bp(noise(0.12), 250, 1400) * np.exp(-t / 0.02)
        place(y, hit, at)
    return norm(fade(reverb(y, 0.25, 0.5), 0.001, 0.15), 0.80)
def crash():
    """A locked door forced open: a heavy thud, splintering wood, and the door banging against the wall."""
    y = np.zeros(int(1.6 * SR))
    place(y, 0.9 * np.sin(2 * np.pi * np.cumsum(70 + 60 * np.exp(-tt(0.4) / 0.05)) / SR) * np.exp(-tt(0.4) / 0.15), 0.0)
    place(y, 0.8 * bp(noise(0.25), 300, 4000) * np.exp(-tt(0.25) / 0.06), 0.0)
    for k in range(14):                                                   # splinters
        at = 0.02 + rng.random() * 0.35
        place(y, (0.2 + 0.3 * rng.random()) * bp(noise(0.03), 1500, 6000) * np.exp(-tt(0.03) / 0.005), at)
    place(y, 0.6 * np.sin(2 * np.pi * 120 * tt(0.2)) * np.exp(-tt(0.2) / 0.05) + 0.4 * bp(noise(0.2), 200, 1200) * np.exp(-tt(0.2) / 0.04), 0.55)
    return norm(fade(reverb(y, 0.30, 0.8), 0.001, 0.3), 0.92)
def discovery():
    """The moment the body is found: a low dissonant hit, a timpani roll and high strings that hang in the air."""
    y = np.zeros(int(4.5 * SR))
    place(y, orch_hit(["D3", "Eb3", "A3"], "D2", 0.5, 1.0), 0.0)
    place(y, timp_roll("D2", 1.2, 0.7, 0.1), 0.0, 0.5)
    place(y, strings(["A4", "Bb4"], 3.0, 0.35, trem=0.6, attack=0.05), 0.0)
    return norm(fade(reverb(y, 0.32, 1.2), 0.002, 1.2), 0.88)
def running():
    """Several people running: down the wooden stairs, then across the stone hall floor. Irregular, overlapping steps."""
    y = np.zeros(int(4.6 * SR))
    for runner in range(4):
        t0 = 0.15 + runner * 0.55 + rng.random() * 0.2
        gap = 0.17 + rng.random() * 0.05
        pitch = 95 + rng.random() * 50
        for k in range(16):
            at = t0 + k * gap + rng.normal(0, 0.012)
            if at > 4.3: break
            wood = k < 7                                                    # first on the stairs, then the floor
            t = tt(0.11)
            hit = (0.8 * np.sin(2 * np.pi * pitch * (1.3 if wood else 1.0) * t) * np.exp(-t / (0.04 if wood else 0.025))
                   + 0.7 * bp(noise(0.11), 250 if wood else 600, 1500 if wood else 3500) * np.exp(-t / 0.018))
            place(y, hit, at, 0.55 + 0.35 * rng.random())
    env = np.minimum(1, tt(4.6) / 0.6) * np.minimum(1, (4.6 - tt(4.6)) / 0.8)
    return norm(fade(reverb(y * env, 0.28, 0.6), 0.005, 0.3), 0.80)
def pound():
    """Fists pounding on a heavy locked door, harder each time; the door rattles in its frame."""
    y = np.zeros(int(2.4 * SR))
    for k, at in enumerate((0.0, 0.32, 0.6, 0.95, 1.2, 1.45)):
        t = tt(0.18); strength = 0.6 + 0.08 * k
        hit = strength * (0.9 * np.sin(2 * np.pi * np.cumsum(95 + 40 * np.exp(-t / 0.02)) / SR) * np.exp(-t / 0.06)
                          + 0.6 * bp(noise(0.18), 150, 900) * np.exp(-t / 0.03))
        place(y, hit, at)
        place(y, 0.25 * strength * bp(noise(0.12), 1800, 5000) * np.exp(-tt(0.12) / 0.03), at + 0.01)   # the rattle
    return norm(fade(reverb(y, 0.25, 0.5), 0.002, 0.2), 0.85)
def chaos():
    """The tension score under the panic: tremolo strings climbing a semitone cluster, a heartbeat that quickens,
    a timpani rumble, all building for nine seconds and stopping dead where the door gives way."""
    d = 9.0; n = int(d * SR); t = np.arange(n) / SR
    y = np.zeros(n)
    rise = (t / d) ** 1.6
    for name, a in (("D3", 0.9), ("Eb3", 0.8), ("A3", 0.7), ("Bb3", 0.6)):
        y += a * lp(saw(hz(name), d, 0.004, 5.0), 2200) * (1 - 0.6 * (0.5 + 0.5 * np.sin(2 * np.pi * (8 + 6 * rise) * t)))
    y *= 0.12 + 0.88 * rise
    beat, k = 0.0, 0
    while beat < d - 0.2:                                                   # the heartbeat speeds up
        place(y, 1.4 * kick(), beat, 0.5 + 0.5 * beat / d); place(y, 1.0 * kick(), beat + 0.22, 0.4 + 0.4 * beat / d)
        beat += 0.85 - 0.45 * beat / d; k += 1
    place(y, timp_roll("D2", d - 0.5, 0.05, 0.9), 0.3, 0.6)
    y[-int(0.03 * SR):] *= np.linspace(1, 0, int(0.03 * SR))               # stops dead
    return norm(y, 0.85)
def dial_click():
    t = tt(0.12)
    x = hp(noise(0.12), 2200) * np.exp(-t / 0.012) + 0.7 * np.sin(2 * np.pi * 1750 * t) * np.exp(-t / 0.025) + 0.6 * np.sin(2 * np.pi * 120 * t) * np.exp(-t / 0.02)
    return norm(fade(x, 0.001, 0.02), 0.70)
def ending():
    T = Track(100, 16, tail=3.5); b = T.dur
    for beat, ch, low in ((0, ["D4", "F#4", "A4"], "D2"), (1, ["D4", "G4", "B4"], "G1"), (2, ["C#4", "E4", "A4"], "A1"), (3, ["D4", "F#4", "A4", "D5"], "D2")):
        T.at(beat, brass_chord(ch, b(0.9 if beat < 3 else 4)), 0.9); T.at(beat, timpani(low), 0.6)
    for beat, n, d in ((4, "A4", 1), (5, "B4", 1), (6, "C#5", 1), (7, "D5", 1), (8, "F#5", 2), (10, "E5", 1), (11, "D5", 3)):
        T.at(beat, brass(n, b(d) * 0.95), 0.7)
    T.at(4, strings(["D4", "F#4", "A4"], b(8), 0.3)); T.at(11, cymbal(2.5), 0.5); T.at(11, timp_roll("D2", b(2), 0.3, 0.8), 0.5)
    T.at(11, brass_chord(["D4", "F#4", "A4", "D5"], b(4)), 0.8)
    return norm(fade(reverb(T.x, 0.25, 1.5), 0.005, 2.0), 0.88)
def game_over():
    T = Track(80, 10, tail=3.0); b = T.dur
    for beat, ch, low, d in ((0, ["D4", "F4", "A4"], "D2", 1.4), (1.5, ["D4", "F4", "Bb3"], "Bb1", 1.4), (3, ["D4", "G3", "Bb3"], "G1", 1.4), (4.5, ["C#4", "E4", "A3"], "A1", 0.9)):
        T.at(beat, brass_chord(ch, b(d)), 0.75); T.at(beat, timpani(low), 0.5)
    T.at(5.5, orch_hit(["D3", "F3", "A3"], "D2", b(3)), 0.9); T.at(5.5, timp_roll("D2", b(2), 0.8, 0.1), 0.5)
    return norm(fade(reverb(lp(T.x, 3500), 0.30, 1.5), 0.005, 2.0), 0.85)

if __name__ == "__main__":
    for name, fn in (("theme.wav", theme), ("ambient.wav", ambient), ("exposed.wav", exposed), ("door.wav", door),
                     ("locked.wav", locked), ("step.wav", step), ("pickup.wav", pickup), ("wrong.wav", wrong),
                     ("lab_success.wav", lab_success), ("dial_click.wav", dial_click), ("ending.wav", ending),
                     ("game_over.wav", game_over), ("objective.wav", objective),
                     ("knock.wav", knock), ("crash.wav", crash), ("discovery.wav", discovery),
                     ("running.wav", running), ("pound.wav", pound), ("chaos.wav", chaos)):
        rng = np.random.default_rng(zlib.crc32(name.encode()))           # each sound has its own fixed seed, so
        save(name, fn(), loop=(name == "ambient.wav"))                  # changing one never changes another
