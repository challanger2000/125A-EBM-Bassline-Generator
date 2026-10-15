"""Independent Standard MIDI File validator for 125A v0.2 preview exports.
Python standard library only; independent of C++ generator and MIDI writer.
"""
from pathlib import Path
import struct
import sys


def variable(data, pos):
    value = 0
    for _ in range(4):
        if pos >= len(data):
            raise ValueError('truncated MIDI VLQ')
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 127)
        if not (b & 128):
            return value, pos
    raise ValueError('VLQ too long')


def verify(path, expected_bars=8, allow_empty=False):
    data = Path(path).read_bytes()
    if data[:4] != b'MThd' or struct.unpack('>I', data[4:8])[0] != 6:
        raise ValueError('invalid MIDI header')
    fmt, tracks, ppq = struct.unpack('>HHH', data[8:14])
    if (fmt, tracks, ppq) != (0, 1, 480) or data[14:18] != b'MTrk':
        raise ValueError('unexpected MIDI structure')
    track_length = struct.unpack('>I', data[18:22])[0]
    if track_length + 22 != len(data):
        raise ValueError('incorrect track length')
    pos, tick, active = 22, 0, {}
    onsets = []
    found_tempo = found_meter = ended = False
    while pos < len(data):
        delta, pos = variable(data, pos)
        tick += delta
        if pos >= len(data):
            raise ValueError('missing event')
        status = data[pos]
        pos += 1
        if status == 0xff:
            typ = data[pos]
            pos += 1
            size, pos = variable(data, pos)
            payload = data[pos:pos+size]
            pos += size
            if len(payload) != size:
                raise ValueError('truncated meta')
            if typ == 0x51:
                found_tempo = payload == b'\x07\xa1\x20'
            if typ == 0x58:
                found_meter = payload == bytes([4, 2, 24, 8])
            if typ == 0x2f:
                if payload or pos != len(data):
                    raise ValueError('invalid end of track')
                ended = True
        elif status in (0x90, 0x80):
            key, vel = data[pos:pos+2]
            pos += 2
            if status == 0x90 and vel > 0:
                if key in active or active:
                    raise ValueError('polyphony / stuck note')
                active[key] = tick
                onsets.append(tick)
            else:
                if key not in active or tick <= active[key]:
                    raise ValueError('invalid note off')
                del active[key]
        else:
            raise ValueError(f'unknown/running status {status:02x}')
    if active or not ended or not found_tempo or not found_meter:
        raise ValueError('missing mandatory MIDI contract')
    if tick != expected_bars*4*480 or (not onsets and not allow_empty) or any(t >= tick for t in onsets):
        raise ValueError('wrong bar length or unexpected empty pattern')
    return len(onsets)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--bars', type=int, choices=[1, 2, 4, 8], default=8)
    parser.add_argument('--allow-empty', action='store_true')
    parser.add_argument('--expected-notes', type=int)
    parser.add_argument('paths', nargs='+', type=Path)
    args = parser.parse_args()
    for file in args.paths:
        count = verify(file, args.bars, args.allow_empty)
        if args.expected_notes is not None and count != args.expected_notes:
            raise ValueError('unexpected note count')
        print(file.name, count, 'note-ons — PASS')
