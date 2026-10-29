"""FLS2 package validation and acknowledged USB transfer. No implicit connection."""
import struct
import zlib
import math

MAX_SCENE_BYTES = 262144


def validate_scene(data):
    if not 8 <= len(data) <= MAX_SCENE_BYTES or data[:4] != b"FLS2":
        raise ValueError("Expected an FLS2 scene package, at most 256 KB")
    count, background = struct.unpack_from("<HH", data, 4)
    if count > 32:
        raise ValueError("Scene has more than 32 layers")
    weather=False
    offset = 8
    for _ in range(count):
        if len(data) - offset < 16:
            raise ValueError("Truncated layer header")
        kind, x, y, w, h, frames, fps, fmt, color, secondary, colors, reserved = struct.unpack_from("<BbbBBBBBHHHH", data, offset)
        if (kind > 21 or not -63 <= x <= 63 or not -63 <= y <= 63 or not 1 <= w <= 64
                or not 1 <= h <= 64 or not 1 <= frames <= 32 or not 1 <= fps <= 30
                or fmt not in (12, 24) or colors > 255 or (reserved != 76 if kind>=17 else reserved>15 if kind>=5 else reserved!=0)
                or (kind and (frames != 1 or colors))):
            raise ValueError("Invalid layer dimensions or metadata")
        weather=weather or kind>=17
        offset += 16
        length = 76 if kind>=17 else 0 if kind else w * h * frames
        pixels = offset + colors * 2
        end = pixels + length
        if end > len(data):
            raise ValueError("Truncated layer pixels")
        if kind>=17:
            p=data[pixels:end]
            if p[0]>6 or p[1]>1 or p[2]>1 or p[3]:raise ValueError('Invalid weather flags')
            values=struct.unpack_from('<18f',p,4)
            bounds=[(0,100),(1,60),(1,5),(-8,8),(0,100),(1,30),(1,3),(0,12),(.099,5),(1,8),(0,3),(5,600),(5,600),(.029, .5),(1,2),(0,4),(-90,90),(-180,180)]
            if any(not math.isfinite(n) or not low<=n<=high for n,(low,high) in zip(values,bounds)) or values[12]<values[11]:raise ValueError('Invalid weather parameters')
        if kind==0 and any(index > colors for index in data[pixels:end]):
            raise ValueError("Pixel refers to a missing palette color")
        offset = end
    if offset != len(data):
        raise ValueError("Unexpected trailing scene bytes")
    return {"layers": count, "background": background, "bytes": len(data),"weather":weather}


def deploy_scene(link, data, request, progress=None):
    info = validate_scene(data)
    status = request(link, "status", "STATUS ")
    if "scene_version=2" not in status:
        raise RuntimeError("Install Widget Studio scene-player firmware once before uploading. See Deploy > One-time firmware setup.")

    if info['weather'] and 'weather_version=1' not in status:
        raise RuntimeError('Install the weather-enabled firmware once before uploading weather layers.')

    def acknowledged(command, prefix, timeout=4):
        reply = request(link, command, "SCENE ", timeout=timeout)
        lines = [line.strip() for line in reply.splitlines() if line.startswith("SCENE ")]
        if not lines or not lines[-1].startswith(prefix):
            raise RuntimeError(lines[-1] if lines else "Missing scene acknowledgement")
        return lines[-1]

    crc = zlib.crc32(data)
    acknowledged(f"scene begin {len(data)} {crc:08x}", "SCENE READY")
    try:
        for offset in range(0, len(data), 32):
            chunk = data[offset:offset + 32]
            expected = offset + len(chunk)
            reply = acknowledged(f"scene data {offset} {chunk.hex()}", "SCENE DATA ")
            if reply != f"SCENE DATA {expected}":
                raise RuntimeError("Board acknowledged an unexpected byte offset")
            if progress:
                progress(expected, len(data))
        reply = acknowledged("scene commit", "SCENE OK saved=1 mode=custom", timeout=15)
    except Exception:
        try:
            request(link, "scene abort", "SCENE ", timeout=1)
        except Exception:
            pass
        raise
    return f"Uploaded {info['layers']} layers ({info['bytes']:,} bytes). CRC verified; saved to the board. Custom face is active."
