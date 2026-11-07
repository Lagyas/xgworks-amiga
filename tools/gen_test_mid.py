"""A test MIDI file without anyone's music: format 1, 3 tracks, tempo
changes, XG System On, an XG parameter change, running status, note-on with
velocity 0, note-offs, program / control changes, a lyric and an F7 escape.

    python gen_test_mid.py out.mid"""
import struct
import sys


def vl(v):
    out = [v & 0x7F]
    v >>= 7
    while v:
        out.insert(0, (v & 0x7F) | 0x80)
        v >>= 7
    return bytes(out)


def track(events):
    """events: (delta, bytes) in order."""
    body = b"".join(vl(d) + b for d, b in events) + vl(0) + b"\xFF\x2F\x00"
    return b"MTrk" + struct.pack(">I", len(body)) + body


def sysex(data):
    return b"\xF0" + vl(len(data)) + bytes(data)


conductor = [
    (0, b"\xFF\x51\x03" + (500000).to_bytes(3, "big")),
    (0, sysex([0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7])),
    (480, sysex([0x43, 0x10, 0x4C, 0x08, 0x00, 0x07, 0x00, 0xF7])),
    (0, b"\xFF\x05\x04Ahoj"),
]
for k in range(40):  # an accelerando: tempo every 37 ticks, odd values
    conductor.append((37, b"\xFF\x51\x03" + (500000 - k * 3331).to_bytes(3, "big")))
conductor.append((1, b"\xF7" + vl(3) + bytes([0xF8, 0xFA, 0xFC])))

melody = [(0, b"\xC0\x00"), (0, b"\xB0\x07\x64")]
for i in range(64):
    n = 60 + (i * 7) % 12
    melody.append((0 if i == 0 else 7, bytes([0x90, n, 90])))
    melody.append((113, bytes([n, 0])))            # running status, velocity 0 = off
bass = [(0, b"\xC1\x20")]
for i in range(32):
    bass.append((0 if i == 0 else 11, bytes([0x91, 36 + i % 5, 100])))
    bass.append((229, bytes([0x81, 36 + i % 5, 64])))  # a real note-off
    bass.append((0, bytes([0xE1, i & 0x7F, 0x40])))     # pitch bend
data = b"MThd" + struct.pack(">IHHH", 6, 1, 3, 480) + track(conductor) + track(melody) + track(bass)
open(sys.argv[1], "wb").write(data)
