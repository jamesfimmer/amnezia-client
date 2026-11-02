#!/usr/bin/env python3
"""Mock server-list API for the VPN client fork.

Serves a stable, deterministic list of AmneziaWG configs so the client can be
tested without a real backend. Keys are derived from the device id, so the same
device always gets the same configs.

Run:
    python tools/mock-provider/server.py --port 8080

Then point the client at it:
    set PROVIDER_API_URL=http://127.0.0.1:8080
or build with -DCLIENT_PROVIDER_API_URL=http://127.0.0.1:8080
"""

import argparse
import base64
import hashlib
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

SERVERS = [
    {"id": "de-1", "name": "Germany", "host": "de1.example.com", "port": 51820, "subnet": "10.66.10"},
    {"id": "nl-1", "name": "Netherlands", "host": "nl1.example.com", "port": 51821, "subnet": "10.66.20"},
    {"id": "fi-1", "name": "Finland", "host": "fi1.example.com", "port": 51822, "subnet": "10.66.30"},
]


def key_for(*parts: str) -> str:
    digest = hashlib.sha256(":".join(parts).encode()).digest()
    return base64.b64encode(digest).decode()


def build_config(server: dict, device_id: str) -> str:
    client_index = int(hashlib.sha256(device_id.encode()).hexdigest()[:2], 16) % 200 + 2
    return f"""[Interface]
PrivateKey = {key_for('client-priv', device_id, server['id'])}
Address = {server['subnet']}.{client_index}/32
DNS = {server['subnet']}.1
MTU = 1280
Jc = 6
Jmin = 64
Jmax = 900
S1 = 24
S2 = 32
S3 = 24
S4 = 16
H1 = 1234567
H2 = 2345678
H3 = 3456789
H4 = 4567890
HeaderProtectionKey = {key_for('header-protection', server['id'])}

[Peer]
PublicKey = {key_for('server-pub', server['id'])}
PresharedKey = {key_for('psk', server['id'])}
Endpoint = {server['host']}:{server['port']}
AllowedIPs = 0.0.0.0/0, ::/0
PersistentKeepalive = 25
"""


class Handler(BaseHTTPRequestHandler):
    def _send(self, status: int, payload: dict) -> None:
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path == "/v1/health":
            self._send(200, {"status": "ok"})
            return
        if path != "/v1/servers":
            self._send(404, {"error": "not found"})
            return

        device_id = self.headers.get("X-Device-Id", "anonymous")
        servers = [
            {
                "id": server["id"],
                "name": server["name"],
                "countryCode": server["id"].split("-")[0].upper(),
                "config": build_config(server, device_id),
            }
            for server in SERVERS
        ]
        self._send(200, {"servers": servers})

    def log_message(self, fmt: str, *args) -> None:
        print("%s - %s" % (self.address_string(), fmt % args))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"Mock provider listening on http://{args.host}:{args.port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
