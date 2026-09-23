"""
LiveEarth Source & Stream Verification Tool
验证摄像头 URL、图片快照或流的可达性与健康度。
"""

import json
import urllib.request
import urllib.error
from typing import Dict, Any

def verify_camera_endpoint(cam: Dict[str, Any], timeout_sec: int = 5) -> Dict[str, Any]:
    url = cam.get("stream_url") or cam.get("thumbnail_url") or cam.get("source_url")
    result = {
        "id": cam.get("id"),
        "url": url,
        "reachable": False,
        "http_code": 0,
        "content_type": "",
        "message": ""
    }
    if not url or not url.startswith("http"):
        result["message"] = "Invalid URL format"
        return result

    req = urllib.request.Request(
        url,
        headers={"User-Agent": "Mozilla/5.0 LiveEarth-VerificationBot/1.0"}
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout_sec) as response:
            result["http_code"] = response.getcode()
            result["content_type"] = response.info().get_content_type()
            result["reachable"] = (result["http_code"] in [200, 204, 301, 302])
            result["message"] = "OK"
    except urllib.error.HTTPError as e:
        result["http_code"] = e.code
        result["message"] = f"HTTP Error: {e.reason}"
    except urllib.error.URLError as e:
        result["message"] = f"Network Error: {e.reason}"
    except Exception as e:
        result["message"] = str(e)

    return result

def verify_all_cameras(cameras_file: str):
    with open(cameras_file, "r", encoding="utf-8") as f:
        cameras = json.load(f)
    print(f"[Validation] Checking {len(cameras)} cameras from {cameras_file}...")
    for cam in cameras:
        res = verify_camera_endpoint(cam)
        status_str = "UP" if res["reachable"] else "DOWN"
        print(f"[{status_str}] {cam['id']}: HTTP {res['http_code']} ({res['content_type']}) - {res['message']}")

if __name__ == "__main__":
    import os
    target = os.path.join(os.path.dirname(__file__), "../../data/cameras.json")
    if os.path.exists(target):
        verify_all_cameras(target)
    else:
        print(f"File not found: {target}")
