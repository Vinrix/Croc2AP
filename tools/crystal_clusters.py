"""Group a stage's white crystal sources into spatial clusters and list the landmarks beside each one.

  crystal_clusters.py <folder with the level .WAD files> <stage, e.g. T1L4> [objects_vars.tsv]

Used to write worlds/croc2/data/logic.json: each cluster is matched to the walkthroughs by its landmarks
(gongs, hearts, keys, coloured crystals, jelly markers, Clockwork Gobbo markers, monkey bars, ropes).
The optional .tsv is the in-game object dump; it supplies each coloured crystal's colour.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import strat_disasm  # noqa: E402

COLOURS = ['red', 'green', 'purple', 'blue', 'yellow']
IGNORE = re.compile(r'^(\?|Camera|Blank|crystal)$|Smash Box|ring of crystals|effect|Effect|sound|Trail|butterfly', re.I)
LINK = 5200  # two sources closer than this (y counted double) are one cluster


def load(root, stage, tsv=None):
    path = os.path.join(root, stage + 'M001.WAD')
    d, base, recs, ents, names = strat_disasm.load(path)
    colour = {}
    if tsv and os.path.exists(tsv):
        for line in open(tsv, encoding='utf-8', errors='replace'):
            f = line.rstrip('\n').split('\t')
            if len(f) > 5 and f[0] == stage + 'M001' and 'bonus crystal' in f[1]:
                colour[(int(f[2]), int(f[4]))] = COLOURS[int(f[5]) // 4096 % 5]
    src, marks = [], []
    n = 1
    for r in recs:
        name = names.get(r[12], '?')
        x, y, z = r[8], r[9], r[10]
        if name == 'crystal':
            src.append((n, 1, 'loose', x, y, z)); n += 1
        elif 'Smash Box' in name:
            src.append((n, 5, 'box', x, y, z)); n += 5
        elif name == 'ring of crystals':
            src.append((n, 8, 'ring', x, y, z)); n += 8
        elif not IGNORE.search(name):
            label = 'START' if name == 'WalkingCroc' else name
            if 'bonus crystal' in name:
                label = colour.get((x, z), '?') + ' crystal'
            marks.append((label, x, y, z))
    return src, marks


def dist(a, b):
    return ((a[3] - b[3]) ** 2 + (2 * (a[4] - b[4])) ** 2 + (a[5] - b[5]) ** 2) ** 0.5


def clusters(src):
    left = list(src)
    out = []
    while left:
        grp = [left.pop(0)]
        grew = True
        while grew:
            grew = False
            for s in left[:]:
                if any(dist(s, g) < LINK for g in grp):
                    grp.append(s); left.remove(s); grew = True
        out.append(sorted(grp))
    return sorted(out, key=lambda g: g[0][0])


def ranges(nums):
    nums = sorted(nums)
    out, a, b = [], nums[0], nums[0]
    for n in nums[1:]:
        if n == b + 1:
            b = n
        else:
            out.append((a, b)); a = b = n
    out.append((a, b))
    return ','.join(str(a) if a == b else '%d-%d' % (a, b) for a, b in out)


def main():
    root, stage = sys.argv[1], sys.argv[2]
    src, marks = load(root, stage, sys.argv[3] if len(sys.argv) > 3 else None)
    for k, g in enumerate(clusters(src)):
        nums = [n for s in g for n in range(s[0], s[0] + s[1])]
        cx = sum(s[3] for s in g) // len(g); cy = sum(s[4] for s in g) // len(g); cz = sum(s[5] for s in g) // len(g)
        boxes = [s for s in g if s[2] == 'box']
        near = []
        for m in marks:
            dmin = min(((m[1] - s[3]) ** 2 + (m[3] - s[5]) ** 2) ** 0.5 for s in g)
            if dmin < 9000:
                near.append('%s(%+d)' % (m[0], (m[2] - cy) // 100))
        print('C%02d n=%-3d #%s | loose %s | box %s | at %d,%d,%d | %s' % (
            k, len(nums), ranges(nums), ranges([s[0] for s in g if s[2] == 'loose']) if any(s[2] == 'loose' for s in g) else '-',
            ' '.join('%d-%d' % (s[0], s[0] + 4) for s in boxes) or '-', cx // 100, cy // 100, cz // 100, ' '.join(sorted(set(near)))))
    print('LANDMARKS: ' + ' '.join('%s@%d,%d,%d' % (m[0], m[1] // 100, m[2] // 100, m[3] // 100) for m in marks
                                    if re.search(r'crystal|GONG|Gong|Key|Cage|Clockwork|Jelly|Golden|Heart|warp|START|Balloon|Rope|Chest|Wheel|Switch|Valve|Button|TNT|Push', m[0])))


if __name__ == '__main__':
    main()
