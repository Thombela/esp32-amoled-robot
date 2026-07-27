"""Local HTTP endpoint serving the latest Claude usage payload.

Feeds Veronica's WiFi polling path (firmware/src/wifi_poll.cpp), which is
deliberately decoupled from BLE ownership — see the comment on usage_poll_loop
in claude_usage_daemon_windows.py for why. Binds 127.0.0.1 only; a tunnel
(ngrok, Cloudflare) run separately is what actually exposes this to the
internet — this module never touches the LAN-facing interface itself.

stdlib-only (http.server, hmac, json, threading) — no new project dependency.
"""

import hmac
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


def _make_handler(latest, secret: str):
    class UsageHandler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):  # noqa: A002 - stdlib signature
            pass  # daemon has its own logger; the default stderr access log is noise

        def _send_json(self, status: int, obj: dict) -> None:
            body = json.dumps(obj, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self) -> None:
            if self.path != "/usage":
                self._send_json(404, {"ok": False, "error": "not found"})
                return
            given = self.headers.get("X-Clawd-Key", "")
            if not hmac.compare_digest(given, secret):
                self._send_json(401, {"ok": False, "error": "unauthorized"})
                return
            payload = latest.payload
            if payload is None:
                self._send_json(503, {"ok": False, "error": "no data yet"})
                return
            self._send_json(200, payload)

    return UsageHandler


def maybe_start_http_server(latest, enabled: bool, port: int, secret: str, log=print):
    """Start the local usage HTTP server, or return None if disabled/misconfigured.

    Fails closed: refuses to start when enabled but no secret is configured,
    since this endpoint may be reachable from the public internet via a tunnel.
    """
    if not enabled:
        return None
    if not secret:
        log("HTTP endpoint enabled but http_secret is empty in config — refusing to start")
        return None

    handler = _make_handler(latest, secret)
    server = None
    last_err: OSError | None = None
    for _attempt in range(5):
        try:
            server = ThreadingHTTPServer(("127.0.0.1", port), handler)
            break
        except OSError as e:
            # Port likely still releasing from a just-crashed prior instance
            # (tray_windows.py's crash-restart supervisor can re-enter this
            # same process quickly) — short retry before giving up.
            last_err = e
            time.sleep(0.5)
    if server is None:
        log(f"HTTP server failed to bind 127.0.0.1:{port}: {last_err}")
        return None

    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    log(f"HTTP usage endpoint listening on http://127.0.0.1:{port}/usage")
    return server


def stop_http_server(server) -> None:
    if server is None:
        return
    server.shutdown()
    server.server_close()
