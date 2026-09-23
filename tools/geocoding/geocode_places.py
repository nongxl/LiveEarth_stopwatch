"""
LiveEarth Geolocation & Precision Validation Tool
坐标合法性校验与精度级别定义。
"""

from typing import Tuple, Optional

PRECISION_LEVELS = ["EXACT", "BUILDING", "AREA", "CITY", "APPROXIMATE"]

def validate_coordinates(lat: float, lon: float) -> Tuple[bool, str]:
    """验证经纬度数学范围有效性"""
    if not isinstance(lat, (int, float)) or not isinstance(lon, (int, float)):
        return False, "Coordinates must be numeric"
    if lat < -90.0 or lat > 90.0:
        return False, f"Latitude {lat} out of range [-90, +90]"
    if lon < -180.0 or lon > 180.0:
        return False, f"Longitude {lon} out of range [-180, +180]"
    if abs(lat) < 0.0001 and abs(lon) < 0.0001:
        return False, "Null Island (0, 0) coordinates detected - likely placeholder"
    return True, "Valid"

def classify_precision(source_type: str, has_official_gps: bool) -> str:
    """根据数据源确定坐标精度等级"""
    if has_official_gps:
        return "EXACT"
    if source_type in ["observatory", "building", "airport", "station"]:
        return "BUILDING"
    if source_type in ["mountain", "national_park", "lake"]:
        return "AREA"
    return "CITY"

if __name__ == "__main__":
    valid, msg = validate_coordinates(35.3606, 138.7274)
    print(f"[Geocoding] Test Fuji coordinates: {valid} ({msg})")
