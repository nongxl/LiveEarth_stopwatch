#!/usr/bin/env python3
"""
LiveEarth Firmware Data Exporter Tool (Phase 7 & Phase 8)
读取 data/places.json，自动生成并注入 src/place_marker.cpp 中的 GLOBAL_PLACES 数组。
确保数据与固件完全解耦，支持一键式自动化构建更新。
"""

import json
import os
import sys

BASE_DIR = os.path.join(os.path.dirname(__file__), "../..")
PLACES_JSON = os.path.join(BASE_DIR, "data/places.json")
MARKER_CPP = os.path.join(BASE_DIR, "src/place_marker.cpp")
OUTPUT_SNIPPET = os.path.join(BASE_DIR, "data/places_cpp.txt")

def export_firmware_data():
    print(f"[*] Reading {PLACES_JSON}...")
    with open(PLACES_JSON, "r", encoding="utf-8") as f:
        places = json.load(f)

    print(f"[*] Formatting {len(places)} places into C++ PROGMEM array...")
    lines = []
    lines.append("extern const PlaceLocation GLOBAL_PLACES[] = {")
    for p in places:
        # 兼容字段名
        pid = p.get("id", "")
        name = p.get("name", "")
        local_name = p.get("local_name", "")
        country = p.get("country", "")
        region = p.get("region", "")
        lat = p.get("lat") if "lat" in p else p.get("latitude", 0.0)
        lon = p.get("lon") if "lon" in p else p.get("longitude", 0.0)
        elev = p.get("elev") if "elev" in p else p.get("elevation_m", 0)
        utc = p.get("utc") if "utc" in p else p.get("utc_offset", 0)
        cat = p.get("cat") if "cat" in p else p.get("category", "")
        color = p.get("color") if "color" in p else p.get("theme_color", 0xFFFF)
        desc = p.get("desc") if "desc" in p else p.get("description", "")
        hl = p.get("hl") if "hl" in p else p.get("highlight", "")
        cam_name = p.get("cam_name", "Official Stream")
        src = p.get("src") if "src" in p else p.get("cam_source", "Official Operator")
        url = p.get("url") if "url" in p else p.get("snapshot_url", "")
        online = p.get("online", False)

        lines.append("    {")
        lines.append(f'        "{pid}",')
        lines.append(f'        "{name}",')
        lines.append(f'        "{local_name}",')
        lines.append(f'        "{country}",')
        lines.append(f'        "{region}",')
        lines.append(f"        {lat:.4f}f,")
        lines.append(f"        {lon:.4f}f,")
        lines.append(f"        {elev},")
        lines.append(f"        {utc},")
        lines.append(f'        "{cat}",')
        lines.append(f"        0x{color:04X},")
        lines.append(f'        "{desc}",')
        lines.append(f'        "{hl}",')
        lines.append(f'        "{cam_name}",')
        lines.append(f'        "{src}",')
        lines.append(f'        "{url}",')
        lines.append("        1,")
        lines.append(f"        {'true' if online else 'false'}")
        lines.append("    },")

    # 去掉最后一个逗号
    if lines[-1] == "    },":
        lines[-1] = "    }"

    lines.append("};")
    lines.append("")
    lines.append("extern const size_t GLOBAL_PLACES_COUNT = sizeof(GLOBAL_PLACES) / sizeof(GLOBAL_PLACES[0]);")
    cpp_snippet = "\n".join(lines)

    # 1. 写入 data/places_cpp.txt
    with open(OUTPUT_SNIPPET, "w", encoding="utf-8") as f:
        f.write(cpp_snippet + "\n")
    print(f"[v] Saved snippet to {OUTPUT_SNIPPET}")

    # 2. 注入 src/place_marker.cpp
    if not os.path.exists(MARKER_CPP):
        print(f"[!] {MARKER_CPP} not found, skipping injection.")
        return True

    with open(MARKER_CPP, "r", encoding="utf-8") as f:
        marker_src = f.read()

    start_marker = "extern const PlaceLocation GLOBAL_PLACES[] = {"
    end_marker = "extern const size_t GLOBAL_PLACES_COUNT = sizeof(GLOBAL_PLACES) / sizeof(GLOBAL_PLACES[0]);"

    s_idx = marker_src.find(start_marker)
    e_idx = marker_src.find(end_marker)

    if s_idx == -1 or e_idx == -1:
        print("[!] Could not locate injection markers in src/place_marker.cpp")
        return False

    e_idx += len(end_marker)
    updated_cpp = marker_src[:s_idx] + cpp_snippet + marker_src[e_idx:]

    with open(MARKER_CPP, "w", encoding="utf-8") as f:
        f.write(updated_cpp)

    print(f"[v] Successfully injected {len(places)} places into {MARKER_CPP}")
    return True

if __name__ == "__main__":
    success = export_firmware_data()
    sys.exit(0 if success else 1)
