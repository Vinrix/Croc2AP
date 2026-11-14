"""Disassemble the object scripts ("strats") in a Croc 2 level file.

  strat_disasm.py <level.WAD> [name-or-type ...]     e.g.  strat_disasm.py T1L4M001.WAD "Smash Box"

How the pieces fit (all confirmed against the running game):
- Placement records (tools/wad_objects.py) give each object's type, which is the offset of its script's entry point
  from one base per file (the game adds it to the pointer at croc2.exe+0xBC7CE0 and uses the result as the script
  position). The base is found by voting: object scripts open with opcodes 176 212, so the right base is the one
  that puts the most types on that pair.
- A script is a stream of int32 words: an opcode (0..467) followed by its inline arguments. Argument counts are in
  opcode_args.json (tools/opcode_args.py, checked against in-game traces).
- A script names itself: it opens with 176, then 212 <byte offset to its name string, from the word after>.
- Scripts that are only ever spawned (never placed) are found through opcode 124 (push type), which other scripts
  use just before a spawn (94 or 192).
- Jumps (130 jump-if-zero, 132 jump, 137) take a word offset from the word after their argument.
- Values are 20.12 fixed point where they are numbers: 4096 = 1.
"""
import collections
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wad_objects  # noqa: E402

_HERE = os.path.dirname(os.path.abspath(__file__))
ARGS = {int(k): v for k, v in json.load(open(os.path.join(_HERE, 'opcode_args.json'))).items()}
JUMPS = {130: 'jz', 132: 'jmp', 137: 'j137'}
NAMES = {
    1: 'push var', 3: 'push gvar?', 4: 'push value', 5: 'push &var', 7: 'push &gvar?', 10: 'push', 12: 'inc',
    14: 'add?', 15: 'sub?', 16: 'mul?', 18: 'store', 19: 'eq', 20: 'ne?', 21: 'lt?', 88: 'remove self', 94: 'spawn',
    121: 'not', 124: 'push type', 126: 'call', 127: 'return', 135: 'or?', 136: 'and?', 138: 'bit and',
    154: 'hurt croc', 155: 'reset to checkpoint', 172: 'give inventory item', 173: 'gain crystal',
    176: 'begin', 192: 'spawn (args)', 212: 'set name', 329: 'crystal saved?', 330: 'save crystal',
}


def find_base(d, types):
    """File offset that a type is added to, to get its script's entry point."""
    pat = struct.pack('<2i', 176, 212)
    votes = collections.Counter()
    i = d.find(pat)
    while i != -1:
        for t in types:
            votes[i - t] += 1
        i = d.find(pat, i + 1)
    return votes.most_common(1)[0][0] if votes else None


def script_name(d, entry, words=12):
    """Name a script gives itself (176, 212 <byte offset, from the word after>), or None.

    Most scripts open with it; some set variables first, so the first `words` words are checked.
    """
    for k in range(words):
        q = entry + 4 * k
        if q < 0 or q + 12 > len(d):
            return None
        a, b, c = struct.unpack_from('<3i', d, q)
        if a != 176 or b != 212:
            continue
        p = q + 12 + c
        if p < 0 or p >= len(d):
            return None
        e = d.find(bytes(1), p)
        s = d[p:e]
        return s.decode('latin1') if 0 < len(s) < 48 and all(32 <= ch < 127 for ch in s) else None
    return None


def entries(d, base, placed):
    """Every script entry: placed types plus any type pushed by opcode 124 that opens like a script."""
    ents = set(placed)
    for p in range(base + min(placed), min(base + max(placed) + 8000, len(d) - 12), 4):
        a, x = struct.unpack_from('<2i', d, p)
        if a == 124 and 0 < x < 3000000 and script_name(d, base + x):
            ents.add(x)
    return sorted(ents)


def disassemble(d, start, end):
    """Yield (offset, opcode, args) from start up to end."""
    p = start
    while p + 4 <= end:
        op = struct.unpack_from('<i', d, p)[0]
        if not 0 <= op < 468:
            yield p, None, [op]
            p += 4
            continue
        n = ARGS.get(op) or 0
        args = list(struct.unpack_from('<%di' % n, d, p + 4)) if n else []
        yield p, op, args
        p += 4 + 4 * n


def load(path):
    """(data, base, placement records, sorted script entries, {entry: name})."""
    d = open(path, 'rb').read()
    recs = wad_objects.placements(path)
    placed = sorted({r[12] for r in recs})
    base = find_base(d, placed)
    ents = entries(d, base, placed)
    names = {}
    for k, t in enumerate(ents):
        # Look further in for a late "set name", but never past the next script's entry.
        room = (ents[k + 1] - t) // 4 if k + 1 < len(ents) else 80
        names[t] = script_name(d, base + t, max(12, min(80, room))) or '?'
    return d, base, recs, ents, names


def main():
    path = sys.argv[1]
    want = sys.argv[2:]
    d, base, recs, types, tname = load(path)
    print('# %s  script base 0x%X  %d scripts' % (os.path.basename(path)[:-4], base, len(types)))
    for k, t in enumerate(types):
        name = tname[t]
        if want and not any(w == str(t) or w.lower() == name.lower() for w in want):
            continue
        end = base + (types[k + 1] if k + 1 < len(types) else t + 4000)
        ins = list(disassemble(d, base + t, end))
        targets = set()
        for p, op, a in ins:
            if op in JUMPS:
                targets.add(p + 8 + a[0] * 4)
        print('\n## type %d  "%s"  (%d placed)' % (t, name, sum(1 for r in recs if r[12] == t)))
        for p, op, a in ins:
            label = 'L%X:' % (p - base - t) if p in targets else ''
            if op is None:
                print('%-8s %6X  ??? %d' % (label, p - base - t, a[0]))
            elif op in JUMPS:
                print('%-8s %6X  %-4d %-22s -> L%X' % (label, p - base - t, op, JUMPS[op], p + 8 + a[0] * 4 - base - t))
            else:
                print('%-8s %6X  %-4d %-22s %s' % (label, p - base - t, op, NAMES.get(op, ''), ' '.join(map(str, a))))


if __name__ == '__main__':
    main()
