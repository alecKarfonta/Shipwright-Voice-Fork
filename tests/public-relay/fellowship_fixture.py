"""Local-only UI fixture peers. Run against a relay bound to 127.0.0.1:43413.
Start the native QA client in room 'Fellowship QA', team 'River' first.
These are labeled test peers, not real players. Ctrl-C closes both connections.
"""
import json
import socket
import threading
import time


def peer(name, message):
    with socket.create_connection(('127.0.0.1', 43413)) as sock:
        def send(packet):
            sock.sendall(json.dumps(packet).encode() + b'\0')
        def social(op, **fields):
            send(dict(type='SOCIAL', version=1, op=op, **fields))
        state = dict(name=name, teamId='River', isSaveLoaded=True, seed=0,
                     sceneNum=85, curRoomNum=0, clientVersion='fixture')
        send(dict(type='HANDSHAKE', roomId='Fellowship QA', clientState=state))
        reader = sock.makefile('rb')
        buffer = bytearray()
        own = 0
        greeted = False
        answered = set()
        while True:
            char = reader.read(1)
            if not char:
                return
            if char != b'\0':
                buffer.extend(char)
                continue
            packet = json.loads(buffer)
            buffer.clear()
            if packet['type'] == 'ALL_CLIENT_STATE':
                for player in packet['state']:
                    if player.get('self'):
                        own = player['clientId']
                    if player.get('name') == 'Arin':
                        state['clientVersion'] = player['clientVersion']
                send(dict(type='UPDATE_CLIENT_STATE', state=state))
            if packet['type'] == 'SERVER_CAPABILITIES':
                social('hello')
            if packet['type'] == 'SOCIAL' and packet['op'] == 'state':
                if not greeted:
                    social('chat', channel='party', text=message, requestId=1)
                    greeted = True
                check = packet.get('check', {})
                if check.get('id') and check['id'] not in answered and str(own) in check['members']:
                    social('ready', checkId=check['id'], ready=True)
                    answered.add(check['id'])
            print(name, packet['type'], packet.get('op', ''), flush=True)


if __name__ == '__main__':
    for name, message in [('Brynn [QA]', 'I have the lantern. Ready for the next watch.'),
                          ('Kael [QA]', 'Let us meet at the forest gate before we begin.')]:
        threading.Thread(target=peer, args=(name, message), daemon=True).start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
