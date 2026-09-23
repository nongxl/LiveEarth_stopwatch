"""
LiveEarth Master QA Validation Gate
严格检查所有数据完整性、合规性与一致性。
任何未经 APPROVED 审核批准的生产摄像头将被直接判定为 QA 失败。
"""

import os
import sys
import json

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
DATA_DIR = os.path.join(ROOT_DIR, "data")

def run_qa_check() -> bool:
    print("=" * 60)
    print(" LiveEarth Master QA Validation Suite")
    print("=" * 60)

    has_errors = False

    # 1. 检查各 JSON 文件的格式与有效性
    files_to_check = {
        "categories": os.path.join(DATA_DIR, "categories.json"),
        "sources": os.path.join(DATA_DIR, "sources.json"),
        "places": os.path.join(DATA_DIR, "places.json"),
        "cameras": os.path.join(DATA_DIR, "cameras.json"),
        "review_queue": os.path.join(DATA_DIR, "review_queue.json")
    }

    loaded_data = {}
    for name, filepath in files_to_check.items():
        if not os.path.exists(filepath):
            print(f"[ERROR] Missing required file: {filepath}")
            has_errors = True
            continue
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                loaded_data[name] = json.load(f)
            print(f"[OK] Parsed {name}.json ({len(loaded_data[name])} records)")
        except Exception as e:
            print(f"[ERROR] Invalid JSON in {name}.json: {e}")
            has_errors = True

    if has_errors:
        print("[FAIL] JSON syntax errors detected. Aborting deeper check.")
        return False

    places = loaded_data["places"]
    cameras = loaded_data["cameras"]
    categories = {c["id"] for c in loaded_data["categories"]}

    # 2. 地点校验
    place_ids = set()
    place_coords = set()
    for p in places:
        pid = p.get("id")
        if not pid:
            print("[ERROR] Place missing ID")
            has_errors = True
            continue
        if pid in place_ids:
            print(f"[ERROR] Duplicate place ID: {pid}")
            has_errors = True
        place_ids.add(pid)

        # 类别检查
        cat = p.get("category")
        if cat not in categories:
            print(f"[ERROR] Place {pid} references unknown category: {cat}")
            has_errors = True

        # 坐标有效性
        lat, lon = p.get("latitude"), p.get("longitude")
        if not isinstance(lat, (int, float)) or lat < -90 or lat > 90:
            print(f"[ERROR] Place {pid} invalid latitude: {lat}")
            has_errors = True
        if not isinstance(lon, (int, float)) or lon < -180 or lon > 180:
            print(f"[ERROR] Place {pid} invalid longitude: {lon}")
            has_errors = True

        coord_key = (round(lat, 4), round(lon, 4))
        if coord_key in place_coords:
            print(f"[WARN] Place {pid} near identical coordinates to another place: {coord_key}")
        place_coords.add(coord_key)

    # 3. 摄像头与权限校验
    cam_ids = set()
    linked_cam_ids = set()
    for p in places:
        for cid in p.get("cameras", []):
            linked_cam_ids.add(cid)

    for cam in cameras:
        cid = cam.get("id")
        if not cid:
            print("[ERROR] Camera missing ID")
            has_errors = True
            continue
        if cid in cam_ids:
            print(f"[ERROR] Duplicate camera ID: {cid}")
            has_errors = True
        cam_ids.add(cid)

        # 必须关联已存在的 Place
        pid = cam.get("place_id")
        if pid not in place_ids:
            print(f"[ERROR] Orphan camera {cid} references non-existent place {pid}")
            has_errors = True

        # 核心合规硬性检查：生产环境摄像头权限必须是 APPROVED
        perm = cam.get("permission_status")
        if perm != "APPROVED":
            print(f"[CRITICAL] Camera {cid} permission_status is '{perm}' (ONLY 'APPROVED' permitted in production!)")
            has_errors = True

        # 必须具备完整存证依据
        if not cam.get("evidence_url"):
            print(f"[ERROR] Camera {cid} missing evidence_url")
            has_errors = True
        if not cam.get("permission_basis"):
            print(f"[ERROR] Camera {cid} missing permission_basis")
            has_errors = True
        if not cam.get("source_name") or not cam.get("source_url"):
            print(f"[ERROR] Camera {cid} missing source provenance metadata")
            has_errors = True

    # 4. 检查是否有未挂靠的孤立摄像头引用
    for cid in linked_cam_ids:
        if cid not in cam_ids:
            print(f"[ERROR] Place links to non-existent camera ID: {cid}")
            has_errors = True

    print("-" * 60)
    if has_errors:
        print("[FAIL] QA CHECK FAILED: Please fix above errors before deployment.")
        return False
    else:
        print("[PASS] QA CHECK PASSED: All places, cameras and permission evidence are valid.")
        return True

if __name__ == "__main__":
    success = run_qa_check()
    sys.exit(0 if success else 1)
