"""Numerical reference checks; not execution of firmware or a hardware benchmark."""
import math
import random

RATE = 32000
PERIOD = 32 << 16


def render(data, freq, count, phase=0):
    step = freq * PERIOD // RATE
    out = []
    for _ in range(count):
        index, fraction = divmod(phase, 65536)
        value = (data[index] * (65536 - fraction) + data[(index + 1) & 31] * fraction) >> 8
        out.append((value * 255) >> 8)
        phase = (phase + step) & (PERIOD - 1)
    return out, phase


wave = [round(110 * math.sin(2 * math.pi * i / 32)) for i in range(32)]
for freq in (110, 440, 1000, 1047, 2000, 4000, 8000, 12000, 15999):
    values, _ = render(wave, freq, 4096)
    assert min(values) < 0 < max(values), freq
    assert math.sqrt(sum(x*x for x in values) / len(values)) > 1000, freq

whole, end = render(wave, 1733, 2048)
parts, phase = [], 0
for length in (1, 17, 64, 511, 5, 1450):
    block, phase = render(wave, 1733, length, phase)
    parts.extend(block)
assert parts == whole and phase == end

# Arbitrary phases/tables must remain bounded; interpolating signed 8-bit
# data at full channel volume fits safely in signed 32-bit intermediates.
rng = random.Random(73)
for _ in range(300):
    data = [rng.randrange(-128, 128) for _ in range(32)]
    values, phase = render(data, rng.randrange(1, RATE//2), 64, rng.randrange(PERIOD))
    assert 0 <= phase < PERIOD
    assert all(-32640 <= value <= 32385 for value in values)

# Original fast PSG path cancels Fs/4. Edge integration retains the tone.
k = 65536 * 8000 // RATE
old, new = [], []
for i in range(64):
    a, b = i*k, (i+1)*k
    old.append(0 if ((a-k) ^ (a+k)) & 32768 else (-128 if a & 32768 else 127))
    value = 127 if a & 32768 else -128
    if (a ^ b) & 32768:
        value = math.trunc(value * (32768-(a & 32767)-(b & 32767)) / k)
    new.append(value)
assert not any(old) and min(new) < 0 < max(new)


def limit(value):
    if value > 24576:
        return 32767 - 8191*8191 // (8191+value-24576)
    if value < -24576:
        return -(32767 - 8191*8191 // (8191-value-24576))
    return value


last = -32768
for value in range(-300000, 300001):
    out = limit(value)
    assert -32767 <= out <= 32767 and out >= last
    assert out == -limit(-value)
    if abs(value) <= 24576:
        assert out == value
    last = out
print('PASS: SCC frequency range, block continuity, bounds; PSG Fs/4; output limiter.')
