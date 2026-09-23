"""
LiveEarth Editorial Linter Tool
审查地点的人文简介（1-3句，简洁，适合圆形嵌入式屏幕）与亮点清单（3-5项）。
"""

import json
import re
from typing import Dict, Any, List, Tuple

FORBIDDEN_MARKETING_WORDS = [
    "best in the world",
    "unmatched luxury",
    "exclusive offer",
    "book now",
    "guaranteed satisfaction",
    "cheap tickets",
    "top rated hotel"
]

def lint_place_editorial(place: Dict[str, Any]) -> List[str]:
    issues = []
    desc = place.get("description", "").strip()
    highlights = place.get("highlights", [])

    # 1. 检查描述长度与句子数
    if not desc:
        issues.append("Missing description")
    else:
        # 统计大致句子数
        sentences = [s.strip() for s in re.split(r'[.!?。！？]+', desc) if s.strip()]
        if len(sentences) < 1 or len(sentences) > 3:
            issues.append(f"Description must be 1-3 sentences (got {len(sentences)})")
        if len(desc) > 350:
            issues.append(f"Description too long for round display ({len(desc)} chars, max 350)")

        # 检查违禁营销词汇
        for bad_word in FORBIDDEN_MARKETING_WORDS:
            if bad_word in desc.lower():
                issues.append(f"Contains prohibited marketing phrase: '{bad_word}'")

    # 2. 检查 Highlights
    if not isinstance(highlights, list) or len(highlights) < 3 or len(highlights) > 5:
        issues.append(f"Highlights must contain 3-5 items (got {len(highlights) if isinstance(highlights, list) else 0})")
    else:
        for idx, item in enumerate(highlights):
            if len(item) < 3 or len(item) > 60:
                issues.append(f"Highlight item [{idx}] length invalid ({len(item)} chars, 3-60 allowed)")

    return issues

if __name__ == "__main__":
    import os
    target = os.path.join(os.path.dirname(__file__), "../../data/places.json")
    with open(target, "r", encoding="utf-8") as f:
        places = json.load(f)
    print(f"[Editorial] Checking {len(places)} places...")
    all_clean = True
    for p in places:
        errs = lint_place_editorial(p)
        if errs:
            all_clean = False
            print(f"[FAIL] {p.get('id')}: {errs}")
        else:
            print(f"[PASS] {p.get('id')}")
    if all_clean:
        print("[Editorial] All place editorials passed verification.")
