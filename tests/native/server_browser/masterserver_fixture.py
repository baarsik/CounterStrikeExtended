import socket
import subprocess
import sys
import threading

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
    server.bind(('127.0.0.1', 0))
    server.settimeout(0.1)
    stopped = threading.Event()
    requests = []
    def reply():
        while not stopped.is_set():
            try:
                packet, client = server.recvfrom(4096)
            except socket.timeout:
                continue
            requests.append(packet)
            assert packet[:2] == b'1\xff'
            cursor = packet[2:].split(b'\0')[0]
            if cursor == b'0.0.0.0:0':
                # Invalid reply must be ignored, then accept the actual page.
                server.sendto(b'bad header', client)
                payload = bytes([127, 0, 0, 1, 0x69, 0x87])
            elif cursor == b'127.0.0.1:27015':
                payload = bytes([127, 0, 0, 1, 0x69, 0x87, 127, 0, 0, 1, 0x69, 0x88])
            else:
                assert cursor == b'127.0.0.1:27016'
                # A duplicate from two pages ago must not truncate discovery.
                server.sendto(b'\xff\xff\xff\xfff\n' + bytes([127, 0, 0, 1, 0x69, 0x87]), client)
                payload = bytes(6)
            server.sendto(b'\xff\xff\xff\xfff\n' + payload, client)
    worker = threading.Thread(target=reply)
    worker.start()
    address = f'127.0.0.1:{server.getsockname()[1]}'
    if sys.argv[2] == 'backup':
        address = '127.0.0.1:1;' + address + ';' + address
    try:
        subprocess.run([sys.argv[1], address, sys.argv[2]], check=True, timeout=15)
        if sys.argv[2] != 'cancel':
            assert len(requests) >= 2
    finally:
        stopped.set()
        worker.join()
