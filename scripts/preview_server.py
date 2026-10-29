"""Loopback-only Studio server. USB is opened ONLY after an explicit Deploy POST."""
import argparse
import base64
import http.server
import json
import os
import re
import shutil
import subprocess
import tempfile
import threading
import time
import uuid
from pathlib import Path
from urllib.parse import urlsplit, parse_qs
from widget_providers import providers, sample_data, normalize_data, google_events_preview

ROOT = Path(__file__).resolve().parent.parent
PREVIEW = ROOT / "preview"
JOBS = {}
JOBS_LOCK = threading.Lock()
USB_LOCK = threading.Lock()
AI_LOCK = threading.Lock()
AI_TYPES = "clock date seconds text box ellipse wave boat palm bird orbit rain mountains moon flower fireflies sprite weather forecast calendar island weatherSun weatherClouds weatherRain weatherSnow weatherLightning".split()


def ai_prompt(body):
    if body.get("mode") not in ("widget", "face", "variation"):
        raise ValueError("Choose a widget, face, or variation")
    prompt = body.get("prompt", "")
    if not isinstance(prompt, str) or not 1 <= len(prompt.strip()) <= 4000:
        raise ValueError("Describe your idea in 1–4000 characters")
    context = json.dumps(body.get("face"), separators=(",", ":")) if body.get("mode") == "variation" else "null"
    if len(context) > 300000:
        raise ValueError("Face context is too large")
    return (
        "Create editable DeskDisplay Widget Studio data. Return only the requested JSON schema. "
        "Do not run tools, read files, or modify anything. All designs use integer pixels on a 64x64 canvas. "
        "Allowed widget types: " + ", ".join(AI_TYPES) + ". A face has 0–32 independently movable widgets. "
        "For a novel character or animation, use a sprite: width and height 1–64, palette of 1–15 #RRGGBB colors, "
        "1–32 frames, fps 1–30. Each frame is a string with exactly width*height hexadecimal characters; "
        "0 is transparent and 1–f index the palette. Make small legible pixel art with consistent frame dimensions. "
        "Widgets have x,y (-63..63), w,h (1..64), colors, font segment/block, format 12/24, optional text, "
        "effect none/bob/sway/drift/blink/orbit, speed 0..4, amount 0..10, period 1..16 seconds. "
        "Scene weather effects use weatherSun, weatherClouds, weatherRain, weatherSnow, weatherLightning. Preserve face.weather configuration in variations; these live effects run in the dashboard and cannot deploy to the board yet. "
        "Island elements use type island and islandPart 0 clock, 1 date, 2 sea, 3..5 waves, 6 shore, 7..8 palms, 9 boat, 10..11 birds. Preserve islandPart, original geometry, speed 1, and effect none when varying an existing Island face. "
        "clock/date/seconds are live data and must use effect none for USB deployment. "
        "dateFormat supports YYYY MM DD MMM ddd and [literal words]. binding links a widget to clock.date, weather.current, weather.forecast, calendar.events, weather.tempF or weather.precipProbability; use none for static visuals. "
        "Weather and calendar currently use sample data, never claim they are connected. Animation tracks have duration 1..16, loop, easing linear/smooth/step and up to 32 keys {t,x,y,scale,opacity} with positions as offsets. "
        "For widget requests, return reusable widgets and face null. For face/variation requests, return a new face "
        "with its independently editable widgets, plus any new reusable assets in widgets. Never claim live weather, "
        "network or sensor integrations; only the supported live data types are available. "
        f"Mode: {body['mode']}\nCurrent face: {context}\nUser request: {prompt}"
    )


def run_ai(job_id, prompt):
    try:
        executable = shutil.which("codex")
        if not executable:
            raise RuntimeError("Codex CLI was not found. Use Copy request for chat, or install/sign in to Codex CLI.")
        artifacts = ROOT / "artifacts"
        artifacts.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="studio-ai-", dir=artifacts) as folder:
            output = Path(folder) / "result.json"
            command = [executable, "exec", "--sandbox", "read-only", "--skip-git-repo-check", "--ephemeral",
                       "--output-schema", str(PREVIEW / "widget-response.schema.json"),
                       "--output-last-message", str(output), "--cd", folder, "-"]
            result = subprocess.run(command, input=prompt, text=True, encoding="utf-8", errors="replace",
                                    capture_output=True, timeout=300, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            if result.returncode or not output.exists():
                raise RuntimeError("Codex generation failed. Check your CLI sign-in/account access, or use Copy request for chat. " + result.stderr[-600:])
            if output.stat().st_size > 2000000:
                raise RuntimeError("Generated data is too large; ask for a smaller sprite or fewer frames.")
            generated = json.loads(output.read_text(encoding="utf-8"))
        with JOBS_LOCK:
            JOBS[job_id].update(status="done", result=generated)
    except subprocess.TimeoutExpired:
        with JOBS_LOCK:
            JOBS[job_id].update(status="error", error="Generation exceeded five minutes. Try a smaller widget or use the chat workflow.")
    except Exception as error:
        with JOBS_LOCK:
            JOBS[job_id].update(status="error", error=str(error))
    finally:
        AI_LOCK.release()


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(PREVIEW), **kwargs)

    def allowed_host(self):
        return self.headers.get("Host") in (f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}")

    def end_headers(self):
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def send_json(self, data, status=200):
        payload = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self):
        if not self.allowed_host():
            return self.send_json({"error": "Loopback host required"}, 403)
        if self.path == "/api/capabilities":
            # Deliberately does not enumerate, probe, or connect to serial devices.
            return self.send_json({"ai": shutil.which("codex") is not None, "sceneVersion": 2})
        if self.path.startswith("/api/jobs/"):
            job_id = self.path.rsplit("/", 1)[-1]
            with JOBS_LOCK:
                job = JOBS.get(job_id)
                return self.send_json(dict(job) if job else {"error": "Job not found"}, 200 if job else 404)
        if urlsplit(self.path).path == '/api/weather/current':
            try:
                from scene_weather import current_weather
                return self.send_json(current_weather(parse_qs(urlsplit(self.path).query).get('location',['Boston, MA'])[0]))
            except (ValueError,KeyError,TypeError) as error:
                return self.send_json({'error':str(error)},400)
            except Exception:
                return self.send_json({'error':'Weather service unavailable. Last successful conditions are retained; retry shortly.'},502)
        if self.path == '/api/providers':
            return self.send_json({'providers': providers()})
        if urlsplit(self.path).path == '/api/data':
            try:
                return self.send_json(sample_data(parse_qs(urlsplit(self.path).query).get('date',[None])[0]))
            except (ValueError, TypeError) as error:
                return self.send_json({'error':str(error)},400)
        if self.path.startswith("/api/"):
            return self.send_json({"error": "Unknown API route"}, 404)
        return super().do_GET()

    def do_POST(self):
        origin = self.headers.get("Origin")
        allowed = (f"http://127.0.0.1:{self.server.server_port}", f"http://localhost:{self.server.server_port}")
        if not self.allowed_host() or origin not in allowed or self.headers.get("X-Face-Lab") != "1" or self.headers.get_content_type() != "application/json":
            return self.send_json({"error": "Use the local Studio page for this action"}, 403)
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 2000000:
                raise ValueError("Request body is too large or empty")
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict):
                raise ValueError("Expected a JSON object")
            if self.path == '/api/data/preview':
                return self.send_json(normalize_data(body))
            if self.path == '/api/calendar/preview':
                return self.send_json({'events':google_events_preview(body.get('items'),body.get('referenceDate')),'connected':False})
            if self.path == "/api/ai":
                prompt = ai_prompt(body)
                if not AI_LOCK.acquire(blocking=False):
                    return self.send_json({"error": "A generation is already running"}, 409)
                job_id = uuid.uuid4().hex
                with JOBS_LOCK:
                    for old in [k for k,v in JOBS.items() if time.time()-v["created"] > 900]:
                        del JOBS[old]
                    JOBS[job_id] = {"status": "running", "created": time.time()}
                threading.Thread(target=run_ai, args=(job_id, prompt), daemon=True).start()
                return self.send_json({"id": job_id}, 202)
            if self.path == "/api/deploy":
                from scene_protocol import validate_scene, deploy_scene
                data = base64.b64decode(body.get("scene", ""), validate=True)
                validate_scene(data)
                port = body.get("port", "")
                if not isinstance(port, str) or port and not re.fullmatch(r"COM[1-9][0-9]{0,3}", port, re.IGNORECASE):
                    raise ValueError("Use a Windows serial port such as COM4, or leave it blank")
                if not USB_LOCK.acquire(blocking=False):
                    return self.send_json({"error": "An upload is already running"}, 409)
                try:
                    from device import connect, request
                    with connect(port or None) as link:
                        message = deploy_scene(link, data, request)
                finally:
                    USB_LOCK.release()
                return self.send_json({"message": message})
            return self.send_json({"error": "Unknown API route"}, 404)
        except (ValueError, TypeError, KeyError, AttributeError) as error:
            return self.send_json({"error": str(error)}, 400)
        except (Exception, SystemExit) as error:
            return self.send_json({"error": str(error)}, 500)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    with http.server.ThreadingHTTPServer(("127.0.0.1", args.port), Handler) as server:
        print(f"DeskDisplay Widget Studio: http://127.0.0.1:{args.port}", flush=True)
        print("No device is opened until you click Upload selected face.", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
