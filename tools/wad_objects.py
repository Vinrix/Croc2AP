"""List the placed objects in Croc 2 level files (original/crocii/*.WAD).

Each level file holds one table of 64-byte placement records (16 little-endian int32s):

  [0] usually 0   [1] usually 0   [2] flags   [3] 0
  [4] x   [5] y   [6] z   [7] 0
  [8] type: offset of the object's script entry point from the file's script base
  [9] number of saved-state slots   [10] offset of the object's saved state in the level (-1 = none)
  [11] number of script variables
  [12] small number (0-255)   [13] flags   [14] small number   [15] offset of the object's parameter block (-1 = none)

placements() returns each record as a 16-tuple in an older frame that starts 16 bytes earlier, which the other
tools index: x, y, z = r[8], r[9], r[10]; type = r[12]; slots = r[13]; slot offset = r[14]; variables = r[15].
(r[0]..r[3] of that frame are the previous record's last four fields.)

The table isn't pointed to by a simple file header (the remaster builds its map header in memory), so it is found
as the longest run of consecutive records that pass the field checks below.
"""
import glob
import os
import struct
import sys


def _record(d, q):
    """The record whose older-frame start is q, or None if the fields don't fit a placement record."""
    if q < 0 or q + 80 > len(d):
        return None
    w = struct.unpack_from('<20i', d, q)
    # w[4..19] is the true record; w[0..3] belongs to whatever came before.
    if not (w[7] == 0 and w[11] == 0):
        return None
    if not (1000 < w[12] < 3000000 and 0 <= w[13] < 64 and -1 <= w[14] < 100000 and 0 <= w[15] < 4096):
        return None
    if not (abs(w[8]) < 4_000_000 and abs(w[9]) < 1_000_000 and abs(w[10]) < 4_000_000):
        return None
    if not (0 <= w[16] < 256 and 0 <= w[18] < 256 and -1 <= w[19] < 1_000_000):
        return None
    return w[:16]


def table(d):
    """(older-frame offset of the first record, number of records) of the placement table."""
    best = (0, 0)
    for align in range(4):
        q = align
        while q + 80 <= len(d):
            if _record(d, q):
                s = q
                while _record(d, q):
                    q += 64
                n = (q - s) // 64
                if n > best[1]:
                    best = (s, n)
            else:
                q += 4
    return best


def placements(path):
    d = open(path, 'rb').read()
    s, n = table(d)
    return [struct.unpack_from('<16i', d, s + 64 * k) for k in range(n)]


if __name__ == '__main__':
    import strat_disasm
    root = sys.argv[1]
    for path in sorted(glob.glob(os.path.join(root, '*.WAD'))):
        recs = placements(path)
        if not recs:
            continue
        d = open(path, 'rb').read()
        base = strat_disasm.find_base(d, sorted({r[12] for r in recs}))
        for r in recs:
            name = strat_disasm.script_name(d, base + r[12]) if base is not None else None
            print(os.path.basename(path)[:-4], name or '?', r[12], r[8], r[9], r[10], r[13], r[14], sep='\t')
