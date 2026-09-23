"""
LiveEarth Official Source Discovery Skeleton
聚焦官方、合法、公开渠道的地点与摄像头线索发现基类。
"""

import abc
import json
from dataclasses import dataclass, asdict
from typing import List, Optional

@dataclass
class DiscoveredCandidate:
    candidate_id: str
    place_name: str
    country: str
    region: str
    source_name: str
    source_url: str
    evidence_url: str
    permission_status: str  # 默认必须为 REVIEW_REQUIRED
    latitude: Optional[float]
    longitude: Optional[float]
    reason: str
    questions: List[str]
    discovered_at: str

class BaseDiscoveryEngine(abc.ABC):
    """发现引擎基础接口"""

    @abc.abstractmethod
    def search_candidates(self, place_query: str) -> List[DiscoveredCandidate]:
        """按地点或组织查询官方公开摄像头候选"""
        pass

    def export_to_review_queue(self, candidates: List[DiscoveredCandidate], queue_path: str):
        """将未经过人工核准的候选项目追加写入待审队列"""
        try:
            with open(queue_path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            data = []

        existing_ids = {item.get("candidate_id") for item in data}
        added_count = 0

        for cand in candidates:
            if cand.candidate_id not in existing_ids:
                data.append(asdict(cand))
                existing_ids.add(cand.candidate_id)
                added_count += 1

        with open(queue_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

        print(f"[Discovery] Added {added_count} candidates to review queue: {queue_path}")

if __name__ == "__main__":
    print("[Discovery] BaseDiscoveryEngine ready.")
