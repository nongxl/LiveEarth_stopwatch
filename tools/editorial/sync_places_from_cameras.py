#!/usr/bin/env python3
import json
import os

BASE_DIR = os.path.join(os.path.dirname(__file__), "../..")
PLACES_JSON = os.path.join(BASE_DIR, "data/places.json")
CAMERAS_JSON = os.path.join(BASE_DIR, "data/cameras.json")

def sync():
    with open(PLACES_JSON, "r", encoding="utf-8") as f:
        places = json.load(f)

    with open(CAMERAS_JSON, "r", encoding="utf-8") as f:
        cameras = json.load(f)

    cam_map = {c["place_id"]: c for c in cameras if c.get("permission_status") == "APPROVED" and c.get("status") == "ONLINE"}

    updated_count = 0
    for p in places:
        pid = p["id"]
        if pid in cam_map:
            cam = cam_map[pid]
            p["online"] = True
            p["cam_name"] = cam["name"]
            p["cam_source"] = cam["source_name"]
            p["snapshot_url"] = cam["stream_url"]
            updated_count += 1
            print(f"[v] Updated {pid}: {p['name']} -> {cam['stream_url']}")
        else:
            p["online"] = False
            p["snapshot_url"] = ""

    with open(PLACES_JSON, "w", encoding="utf-8") as f:
        json.dump(places, f, indent=2, ensure_ascii=False)

    print(f"Successfully synced {updated_count} verified cameras to places.json")

if __name__ == "__main__":
    sync()
