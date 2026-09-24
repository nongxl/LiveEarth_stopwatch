#!/usr/bin/env python3
"""
LiveEarth Official Stream Candidates Probe Tool
验证权威官方机构公开快照流的真实连通性、Content-Type 及响应大小
"""

import urllib.request
import urllib.error
import time

CANDIDATES = [
    {
        "id": "us-kilauea-volcano",
        "name": "Kilauea Volcano Caldera",
        "operator": "USGS (United States Geological Survey)",
        "license": "Public Domain (US Federal Agency)",
        "url": "https://volcanoes.usgs.gov/vsc/captures/kilauea/KWcam-latest.jpg"
    },
    {
        "id": "us-yellowstone",
        "name": "Yellowstone Old Faithful Geyser",
        "operator": "National Park Service (NPS)",
        "license": "Public Domain (US Federal Agency)",
        "url": "https://www.nps.gov/features/yell/live/oldFaithfulCam.jpg"
    },
    {
        "id": "jp-mt-fuji",
        "name": "Mount Fuji Lake Kawaguchiko",
        "operator": "Fujigoko TV Consortium",
        "license": "Public Tourism & Weather Service",
        "url": "https://images.fujigoko.tv/live/camera/kawaguchiko.jpg"
    },
    {
        "id": "us-mauna-kea",
        "name": "Mauna Kea Observatory",
        "operator": "University of Hawaii Institute for Astronomy",
        "license": "Public Astronomy Observatory Camera",
        "url": "http://mkwc.ifa.hawaii.edu/current/cams/uh88-n.jpg"
    },
    {
        "id": "ch-zermatt-matterhorn",
        "name": "Zermatt Matterhorn",
        "operator": "Zermatt Bergbahnen AG",
        "license": "Public Ski & Tourism Panorama",
        "url": "https://zermatt.roundshot.com/gornergrat/full.jpg"
    }
]

def probe_all():
    print("=" * 65)
    print(" Probing Tier-A Official Public Camera Snapshot Endpoints")
    print("=" * 65)
    
    headers = {
        "User-Agent": "Mozilla/5.0 LiveEarth-ResearchBot/1.0"
    }
    
    results = []
    for cand in CANDIDATES:
        url = cand["url"]
        print(f"\n[*] Probing {cand['name']} ({cand['operator']})...")
        print(f"    URL: {url}")
        
        req = urllib.request.Request(url, headers=headers)
        t0 = time.time()
        try:
            with urllib.request.urlopen(req, timeout=6) as response:
                latency_ms = int((time.time() - t0) * 1000)
                code = response.getcode()
                c_type = response.info().get_content_type()
                c_len = response.info().get("Content-Length")
                c_len_kb = int(c_len) // 1024 if c_len else -1
                
                print(f"    -> [SUCCESS] HTTP {code} | {c_type} | {c_len_kb} KB | Latency: {latency_ms}ms")
                results.append((cand, True, code, c_type, c_len_kb))
        except urllib.error.HTTPError as e:
            print(f"    -> [FAILED] HTTP {e.code}: {e.reason}")
            results.append((cand, False, e.code, str(e.reason), 0))
        except Exception as e:
            print(f"    -> [ERROR] {e}")
            results.append((cand, False, -1, str(e), 0))
            
    print("\n" + "=" * 65)
    print(" PROBE SUMMARY:")
    for cand, success, code, c_type, c_len in results:
        status = "ONLINE (QUALIFIED)" if success and "image" in c_type else "UNAVAILABLE"
        print(f" - {cand['name']}: {status} (HTTP {code}, {c_len}KB)")
    print("=" * 65)

if __name__ == "__main__":
    probe_all()
