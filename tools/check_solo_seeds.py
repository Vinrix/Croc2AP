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
NAMES = {7200001: 'Tail Whack', 7200002: 'Butt Stomp', 7200003: 'Climbing Claws', 7200004: 'Power Gloves',
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
    proc = subprocess.Popen([os.path.join(ROOT, 'build', 'test_host.exe'), ini], cwd=ROOT)
    bad = 0
    try:
        for _ in range(40):
            try:
                send({'command': 'get_state'})
                break
            except OSError:
                time.sleep(0.1)
        for seed in range(1, count + 1):
            send({'command': 'start_new_save', 'slot': 0, 'seed': seed * 7919})
            r = send({'command': 'debug_solo_logic'})
            line = []
            for k, sp in enumerate(r['spheres']):
                items = [NAMES.get(i, 'Key %d' % (i - 7200022)) for i in sp['items']]
                line.append('%d: %d checks%s' % (k, sp['checks'], (' [' + ', '.join(items) + ']') if items else ''))
            print('seed %d  door 1 -> stage %d  reachable %d  stranded %d' % (seed, r['door1_stage'], r['reachable'], r['stranded_items']))
            for l in line:
                print('    ' + l)
            if r['stranded_items']:
                bad += 1
    finally:
        proc.terminate()
        try:
            os.remove(ini)
        except OSError:
            pass
    print('%d of %d seeds left a needed item out of reach' % (bad, count))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
