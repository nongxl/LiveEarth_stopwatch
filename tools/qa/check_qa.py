#!/usr/bin/env python3
"""
LiveEarth Automated Quality Assurance (QA) Tool
严格按照《docs/需求.md》第 21 条（QA）规范编写：
- 验证 JSON 格式完整性
- 检查重复 ID
- 检查重复或越界的经纬度坐标 [-90, 90], [-180, 180]
- 检查孤儿摄像头（orphan camera）与缺失地点
- 检查来源与证据 URL、合规权限状态
- 严格规则：任何生产上线摄像头，permission_status != 'APPROVED' 必须 FAIL QA！
"""

import json
import os
import sys

DATA_DIR = os.path.join(os.path.dirname(__file__), "../../data")

def check_qa():
    print("=" * 60)
    print(" LiveEarth Quality Assurance (QA) Automated Checker")
    print("=" * 60)

    errors = []
    warnings = []

    # 1. 检查 places.json
    places_file = os.path.join(DATA_DIR, "places.json")
    if not os.path.exists(places_file):
        errors.append(f"Missing essential file: {places_file}")
        return errors, warnings

    with open(places_file, "r", encoding="utf-8") as f:
        try:
            places = json.load(f)
        except Exception as e:
            errors.append(f"Invalid JSON in {places_file}: {e}")
            return errors, warnings

    print(f"[*] Checking places dataset ({len(places)} items)...")
    place_ids = set()
    place_coords = set()

    required_place_fields = [
        "id", "name", "local_name", "country", "region"
    ]

    for p in places:
        pid = p.get("id")
        if not pid:
            errors.append(f"Place without 'id': {p}")
            continue

        if pid in place_ids:
            errors.append(f"Duplicate place ID: {pid}")
        place_ids.add(pid)

        # 检查必填字段
        for field in required_place_fields:
            if field not in p:
                errors.append(f"Place '{pid}' missing field '{field}'")

        # 检查经纬度合法性 (兼容 lat/latitude, lon/longitude)
        lat = p.get("lat") if "lat" in p else p.get("latitude")
        lon = p.get("lon") if "lon" in p else p.get("longitude")
        if lat is None or not (-90.0 <= lat <= 90.0):
            errors.append(f"Place '{pid}' invalid latitude: {lat}")
        if lon is None or not (-180.0 <= lon <= 180.0):
            errors.append(f"Place '{pid}' invalid longitude: {lon}")

        if lat is not None and lon is not None:
            # 检查坐标重复（精度到小数点后3位，约110米范围内）
            coord_key = (round(lat, 3), round(lon, 3))
            if coord_key in place_coords:
                warnings.append(f"Place '{pid}' very close coordinates to another place: {coord_key}")
            place_coords.add(coord_key)

        # 检查色值 (兼容 color / theme_color)
        tc = p.get("color") if "color" in p else p.get("theme_color")
        if tc is not None and not (0 <= tc <= 0xFFFF):
            errors.append(f"Place '{pid}' theme_color out of 16-bit range: {tc}")

    print(f"    -> Verified {len(place_ids)} unique places.")

    # 2. 检查 cameras.json
    cameras_file = os.path.join(DATA_DIR, "cameras.json")
    if os.path.exists(cameras_file):
        with open(cameras_file, "r", encoding="utf-8") as f:
            try:
                cameras = json.load(f)
            except Exception as e:
                errors.append(f"Invalid JSON in {cameras_file}: {e}")
                cameras = []

        print(f"[*] Checking cameras dataset ({len(cameras)} items)...")
        camera_ids = set()

        for c in cameras:
            cid = c.get("id")
            if not cid:
                errors.append(f"Camera without 'id': {c}")
                continue

            if cid in camera_ids:
                errors.append(f"Duplicate camera ID: {cid}")
            camera_ids.add(cid)

            # 检查孤儿摄像头
            parent_place = c.get("place_id")
            if parent_place not in place_ids:
                errors.append(f"Orphan camera '{cid}': place_id '{parent_place}' does not exist in places.json")

            # 检查许可状态与证据
            perm = c.get("permission_status")
            if not perm:
                errors.append(f"Camera '{cid}' missing 'permission_status'")
            elif c.get("status") == "ONLINE" and perm != "APPROVED":
                # 《需求.md》硬性规范：任何 production camera，permission_status != APPROVED 必须 FAIL QA！
                errors.append(f"CRITICAL: Camera '{cid}' is ONLINE but permission_status is '{perm}' (must be 'APPROVED')")

            if not c.get("evidence_url"):
                warnings.append(f"Camera '{cid}' missing 'evidence_url'")
            if not c.get("source_url"):
                warnings.append(f"Camera '{cid}' missing 'source_url'")

    # 3. 检查 review_queue.json
    queue_file = os.path.join(DATA_DIR, "review_queue.json")
    if os.path.exists(queue_file):
        with open(queue_file, "r", encoding="utf-8") as f:
            try:
                queue = json.load(f)
            except Exception as e:
                errors.append(f"Invalid JSON in {queue_file}: {e}")
                queue = []

        print(f"[*] Checking review queue ({len(queue)} items)...")
        cand_ids = set()
        for q in queue:
            qid = q.get("candidate_id")
            if not qid:
                errors.append(f"Queue item without 'candidate_id': {q}")
                continue
            if qid in cand_ids:
                errors.append(f"Duplicate candidate_id in queue: {qid}")
            cand_ids.add(qid)

            if not q.get("questions") or len(q.get("questions")) == 0:
                warnings.append(f"Candidate '{qid}' has no audit questions specified")

    print("-" * 60)
    print(f"QA RESULT: {len(errors)} Errors, {len(warnings)} Warnings")
    print("-" * 60)

    if warnings:
        print("[!] Warnings:")
        for w in warnings:
            print(f"  - {w}")

    if errors:
        print("[x] FAIL - Errors:")
        for err in errors:
            print(f"  - {err}")
        return False

    print("[v] PASS - All datasets adhere strictly to LiveEarth Quality & Legal Standards.")
    return True

if __name__ == "__main__":
    success = check_qa()
    sys.exit(0 if success else 1)
