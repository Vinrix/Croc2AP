"""Generate solo seeds with the built mod and play each one in logic.

  check_solo_seeds.py [number of seeds]

Starts build/test_host.exe with build/croc2_ap_hook.dll (as the test suite does), starts a new randomizer save
for each seed, and asks the mod to walk it sphere by sphere ("debug_solo_logic"). Prints, per seed, how many
checks each sphere opens and which abilities, keys and consumables turn up there, and fails if any needed item
is left out of reach.
"""
import json
import os
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = 43831
NAMES = {7200163: 'Mine Cart', 7200001: 'Tail Whack', 7200002: 'Butt Stomp', 7200003: 'Climbing Claws', 7200004: 'Power Gloves',
         7200005: 'Lever Wrench', 7200006: 'Vehicle License', 7200007: 'Airplane Ammo', 7200161: 'Long Jump',
         7200162: 'High Jump', 7200142: 'Clockwork', 7200143: 'Strawberry jelly', 7200144: 'Orange jelly',
         7200145: 'Lime jelly'}


def send(obj):
    s = socket.create_connection(('127.0.0.1', PORT), 5)
    s.sendall((json.dumps(obj) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf.decode())


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 5
    ini = os.path.join(ROOT, 'build', 'check_solo.ini')
    with open(ini, 'w') as f:
        f.write('[ArchipelagoMod]\nenabled = true\nport = %d\n' % PORT)
    def start_host():
        p = subprocess.Popen([os.path.join(ROOT, 'build', 'test_host.exe'), ini], cwd=ROOT)
        for _ in range(150):  # it can take a few seconds to start listening
            try:
                send({'command': 'get_state'})
                break
            except OSError:
                time.sleep(0.1)
        return p, time.time()

    proc, started = start_host()
    bad = 0
    try:
        depth = []          # spheres per seed
        last_ability = []   # sphere in which the last ability is found
        revisit_seeds = 0   # seeds where an item found in sphere 2+ opens more checks in a stage already entered
        late_revisits = 0   # ... where that item was found in sphere 4 or later
        chains = 0          # seeds where a revisit itself holds a needed item (unlock -> old stage -> another unlock)
        verbose = count <= 5
        for seed in range(1, count + 1):
            if time.time() - started > 7:  # tests/test_host.cpp exits by itself after 12 seconds
                proc.terminate()
                proc.wait()
                proc, started = start_host()
            send({'command': 'start_new_save', 'slot': 0, 'seed': seed * 7919})
            r = send({'command': 'debug_solo_logic'})
            spheres = r['spheres']
            first_seen = {}
            revisit = late = chain = False
            for k, sp in enumerate(spheres):
                back = [s for s, n in enumerate(sp['stages']) if n and s in first_seen and first_seen[s] < k]
                for s, n in enumerate(sp['stages']):
                    if n and s not in first_seen:
                        first_seen[s] = k
                if back and k >= 2:
                    revisit = True
                    if k >= 4:
                        late = True
                    # Does a later sphere open only because of something found here? Approximated by: this
                    # sphere re-enters an old stage and also yields a needed item.
                    if sp['items']:
                        chain = True
            depth.append(len(spheres))
            ab = [k for k, sp in enumerate(spheres) if any(i in NAMES and i not in (7200142, 7200143, 7200144, 7200145) for i in sp['items'])]
            last_ability.append(max(ab) if ab else 0)
            revisit_seeds += revisit
            late_revisits += late
            chains += chain
            if r['stranded_items'] or r.get('lost_items'):
                bad += 1
                print('seed %d: %d needed items out of reach, %d items on checks that cannot be collected' % (
                    seed, r['stranded_items'], r.get('lost_items', 0)))
            if verbose:
                print('seed %d  door 1 -> stage %d  reachable %d  stranded %d' % (seed, r['door1_stage'], r['reachable'], r['stranded_items']))
                for k, sp in enumerate(spheres):
                    items = [NAMES.get(i, 'Key %d' % (i - 7200022)) for i in sp['items']]
                    where = ' '.join('s%d:%d' % (s, n) for s, n in enumerate(sp['stages']) if n)
                    print('    %d: %d checks (%s)%s' % (k, sp['checks'], where, (' [' + ', '.join(items) + ']') if items else ''))
        n = float(count)
        print('seeds %d   spheres min %d / avg %.1f / max %d' % (count, min(depth), sum(depth) / n, max(depth)))
        print('last ability found in sphere: min %d / avg %.1f / max %d' % (min(last_ability), sum(last_ability) / n, max(last_ability)))
        print('seeds where a later find re-opens a stage already entered: %d (%.0f%%)' % (revisit_seeds, 100 * revisit_seeds / n))
        print('  of those, the find was in sphere 4 or later: %d (%.0f%%)' % (late_revisits, 100 * late_revisits / n))
        print('  and the re-opened checks held another needed item: %d (%.0f%%)' % (chains, 100 * chains / n))
    finally:
        proc.terminate()
        try:
            os.remove(ini)
        except OSError:
            pass
    print('%d of %d seeds left a needed item out of reach or lost one on a dead check' % (bad, count))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
