"""Exercise the production C++ worker with a bounded local HTTP fixture."""
import io
import json
import os
import struct
import subprocess
import sys
import threading
import time
import wave
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

seen = set()
failures = []
lock = threading.Lock()
buffer = io.BytesIO()
with wave.open(buffer, "wb") as audio:
    audio.setnchannels(1)
    audio.setsampwidth(2)
    audio.setframerate(24000)
    audio.writeframes(struct.pack("<h", 0) * 2400)
wav = buffer.getvalue()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_POST(self):
        payload = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        code, content_type = 200, "application/json"
        if self.path in ("/tts", "/bad-audio"):
            if self.headers.get("Authorization"):
                failures.append("LLM credential leaked to TTS")
            if self.path == "/bad-audio":
                body = b'{"error":"not audio"}'
            else:
                key = json.dumps(payload, sort_keys=True)
                with lock:
                    if key in seen:
                        code = 409
                    seen.add(key)
                content_type, body = "audio/wav", wav
        else:
            if self.headers.get("Authorization") != "Bearer test-secret":
                failures.append("missing provider authorization")
            if self.path == "/slow":
                time.sleep(3)
            body = json.dumps({"choices": [{"finish_reason": "stop", "message": {
                "content": json.dumps({"reply": "The forest is peaceful today."})}}]}).encode()
            if payload.get('messages', [{}])[0].get('content', '').startswith('Direct the vocal performance'):
                context = json.loads(payload['messages'][1]['content'])
                if 'after loss' in context['current_story_context']:
                    direction = dict(emotion='sorrowful', intensity=1, pace='slow', volume='soft')
                else:
                    direction = dict(emotion='reassuring', intensity=1, pace='measured', volume='conversational')
                body = json.dumps({'choices':[{'finish_reason':'stop','message':{'content':json.dumps(direction)}}]}).encode()
            if self.path == "/fail":
                code = 503
            elif self.path == "/bad":
                body = b'{"choices":[]}'
            elif self.path == "/oversize":
                body = b"x" * 100000
            elif self.path == "/truncated":
                body = body.replace(b'"stop"', b'"length"')
        try:
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
result = subprocess.run([sys.argv[1], f"http://127.0.0.1:{server.server_port}"], timeout=130 if os.getenv("SOH_NPC_LIVE_TTS") else 25)
server.shutdown()
assert not failures, failures
assert len(seen) == 3, "voice and story delivery changes must create distinct speech requests"
assert any('sorrowful' in x for x in seen), "director instruction never reached TTS"
assert all('current_story_context' not in x for x in seen), "story context must not be sent as spoken text"
sys.exit(result.returncode)
