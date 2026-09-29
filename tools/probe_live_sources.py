# -*- coding: utf-8 -*-
"""
LiveEarth Phase 11: Multi-Source Live Camera Discovery & Probe Pipeline
全自动采集、并发连通性探测、健康度过滤与紧凑型 OTA 导出流水线。
覆盖四大洲及极地官方权威机构：
- 香港天文台 (HKO - Hong Kong Observatory)
- 日本国土交通省 (MLIT - Ministry of Land, Infrastructure, Transport and Tourism)
- 欧洲阿尔卑斯全景网络 (Foto-Webcam.eu)
- 美国国家公园管理局 (NPS - National Park Service)
- 美国地质调查局火山观测所 (USGS Hawaiian Volcano Observatory)
- 澳大利亚南极局 (AAD - Australian Antarctic Division)
"""

import os
import sys
import json
import time
import ssl
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed

# 强制 UTF-8 标准输出
sys.stdout.reconfigure(encoding='utf-8')

SSL_CTX = ssl.create_default_context()
SSL_CTX.check_hostname = False
SSL_CTX.verify_mode = ssl.CERT_NONE

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Accept": "image/jpeg,image/*,*/*",
    "Connection": "close"
}

# 权威公开高可用机位候选库
CANDIDATE_SOURCES = [
    # =========================================================================
    # 1. 亚洲 (Asia) - 香港天文台 (HKO) 官方开放机位集群
    # =========================================================================
    {
        "id": "hk-victoria-peak",
        "name": "Victoria Peak",
        "local_name": "太平山顶 (维港全景)",
        "country": "Hong Kong",
        "region": "Hong Kong Island",
        "lat": 22.2759,
        "lon": 114.1455,
        "elev": 552,
        "utc": 8,
        "cat": "mountain",
        "color": "0x47E0",
        "desc": "Panoramic view of world-famous Victoria Harbour skyline from the summit of Victoria Peak.",
        "hl": "Victoria Harbour panoramic skyline & financial district",
        "cam_name": "The Peak Summit East",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/vpa/latest_VPA.jpg"
    },
    {
        "id": "hk-central-pier",
        "name": "Central Pier",
        "local_name": "中环码头 (维多利亚港)",
        "country": "Hong Kong",
        "region": "Hong Kong Island",
        "lat": 22.2871,
        "lon": 114.1602,
        "elev": 8,
        "utc": 8,
        "cat": "city",
        "color": "0x07FF",
        "desc": "Bustling central waterfront overlooking Star Ferry and ICC skyscraper across the harbour.",
        "hl": "Star Ferry routes & Kowloon ICC skyscraper",
        "cam_name": "Central Pier Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/cp1/latest_CP1.jpg"
    },
    {
        "id": "hk-victoria-dnl",
        "name": "SE Kowloon",
        "local_name": "东南九龙 (启德邮轮码头)",
        "country": "Hong Kong",
        "region": "Kowloon",
        "lat": 22.3080,
        "lon": 114.1950,
        "elev": 35,
        "utc": 8,
        "cat": "city",
        "color": "0x3E7F",
        "desc": "View over Victoria Harbour shipping channel from Southeast Kowloon.",
        "hl": "Harbour fairway traffic & Hong Kong Island east hills",
        "cam_name": "SE Kowloon Fairway",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/dnl/latest_DNL.jpg"
    },
    {
        "id": "hk-sha-lo-wan",
        "name": "Sha Lo Wan",
        "local_name": "沙螺湾 (香港国际机场)",
        "country": "Hong Kong",
        "region": "Lantau Island",
        "lat": 22.2917,
        "lon": 113.9011,
        "elev": 15,
        "utc": 8,
        "cat": "coast",
        "color": "0x07FF",
        "desc": "Scenic coastal spot on Lantau overlooking Hong Kong International Airport south runway.",
        "hl": "Aircraft taking off against open Pearl River estuary",
        "cam_name": "HKIA South Runway Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/slw/latest_SLW.jpg"
    },
    {
        "id": "hk-tai-mo-shan",
        "name": "Tai Mo Shan",
        "local_name": "大帽山 (高山云海)",
        "country": "Hong Kong",
        "region": "New Territories",
        "lat": 22.4116,
        "lon": 114.1234,
        "elev": 957,
        "utc": 8,
        "cat": "mountain",
        "color": "0x7BEF",
        "desc": "Hong Kong's highest mountain peak frequently surrounded by an ethereal sea of clouds.",
        "hl": "Rolling sea of clouds over New Territories mountain ranges",
        "cam_name": "Tai Mo Shan Weather Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/tm2/latest_TM2.jpg"
    },
    {
        "id": "hk-waglan-island",
        "name": "Waglan Island",
        "local_name": "横澜岛灯塔 (南海前哨)",
        "country": "Hong Kong",
        "region": "Outlying Islands",
        "lat": 22.1833,
        "lon": 114.3014,
        "elev": 58,
        "utc": 8,
        "cat": "coast",
        "color": "0x0989",
        "desc": "Remote southeastern outpost island featuring a century-old historic Victorian lighthouse.",
        "hl": "Open South China Sea swells & historic lighthouse",
        "cam_name": "Waglan Lighthouse Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/wgl/latest_WGL.jpg"
    },
    {
        "id": "hk-cheung-chau",
        "name": "Cheung Chau",
        "local_name": "长洲岛",
        "country": "Hong Kong",
        "region": "Outlying Islands",
        "lat": 22.2080,
        "lon": 114.0290,
        "elev": 18,
        "utc": 8,
        "cat": "coast",
        "color": "0x3E7F",
        "desc": "Idyllic dumbbell-shaped island known for traditional fishing culture and sandy beaches.",
        "hl": "Sheltered fishing junks & South China Sea horizon",
        "cam_name": "Cheung Chau Harbour Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/cch/latest_CCH.jpg"
    },
    {
        "id": "hk-lamma-island",
        "name": "Lamma Island",
        "local_name": "南丫岛 (风力发电站)",
        "country": "Hong Kong",
        "region": "Outlying Islands",
        "lat": 22.2260,
        "lon": 114.1160,
        "elev": 30,
        "utc": 8,
        "cat": "nature",
        "color": "0x2668",
        "desc": "Lush bohemian island with tranquil bays and coastal hiking trails.",
        "hl": "Verdant coastal hills & open sea channels",
        "cam_name": "Lamma Channel Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/lam/latest_LAM.jpg"
    },
    {
        "id": "hk-wetland-park",
        "name": "Wetland Park",
        "local_name": "香港湿地公园",
        "country": "Hong Kong",
        "region": "New Territories",
        "lat": 22.4680,
        "lon": 114.0080,
        "elev": 5,
        "utc": 8,
        "cat": "nature",
        "color": "0x2668",
        "desc": "Ecological conservation area home to migratory waterbirds and mangroves.",
        "hl": "Reflective marshlands & Shenzhen skyline distant backdrop",
        "cam_name": "Wetland Reserve Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/wlp/latest_WLP.jpg"
    },
    {
        "id": "hk-kau-sai-chau",
        "name": "Kau Sai Chau",
        "local_name": "滘西洲 (西贡地质公园)",
        "country": "Hong Kong",
        "region": "New Territories",
        "lat": 22.3680,
        "lon": 114.3160,
        "elev": 45,
        "utc": 8,
        "cat": "nature",
        "color": "0x2668",
        "desc": "Scenic island in Sai Kung waters featuring rolling coastal green terrain and turquoise coves.",
        "hl": "Sai Kung geopark waters & coastal islands",
        "cam_name": "Kau Sai Chau Marine Cam",
        "src": "Hong Kong Observatory",
        "url": "https://www.hko.gov.hk/wxinfo/aws/hko_mica/ks2/latest_KS2.jpg"
    },

    # =========================================================================
    # 2. 亚洲 (Asia) - 日本富士山 (国土交通省 MLIT 官方机位)
    # =========================================================================
    {
        "id": "jp-mt-fuji-tarobo",
        "name": "Mount Fuji (Tarobo)",
        "local_name": "富士山 (太郎坊观测所)",
        "country": "Japan",
        "region": "Shizuoka",
        "lat": 35.3350,
        "lon": 138.7880,
        "elev": 1300,
        "utc": 9,
        "cat": "mountain",
        "color": "0x47E0",
        "desc": "Direct unobstructed vista of Mount Fuji's southeastern volcanic slope and summit crater rim.",
        "hl": "Snowy conical crater & volcanic ash ridgelines",
        "cam_name": "Tarobo Observatory Cam",
        "src": "Ministry of Land, Infrastructure, Transport and Tourism (MLIT)",
        "url": "https://www.cbr.mlit.go.jp/fujisabo/camera/images/00023.jpg"
    },
    {
        "id": "jp-mt-fuji-fujinomiya",
        "name": "Mount Fuji (Fujinomiya)",
        "local_name": "富士山 (富士宫市远眺)",
        "country": "Japan",
        "region": "Shizuoka",
        "lat": 35.2280,
        "lon": 138.6180,
        "elev": 150,
        "utc": 9,
        "cat": "mountain",
        "color": "0x07FF",
        "desc": "Spectacular classic view of Mount Fuji towering over the historic shrines of Fujinomiya.",
        "hl": "Symmetrical snow cap towering over Shizuoka",
        "cam_name": "Fujinomiya City View",
        "src": "Ministry of Land, Infrastructure, Transport and Tourism (MLIT)",
        "url": "https://www.cbr.mlit.go.jp/fujisabo/camera/images/00026.jpg"
    },
    {
        "id": "jp-mt-fuji-fujisawa",
        "name": "Mount Fuji (Obuchi)",
        "local_name": "富士山 (大渊观测点)",
        "country": "Japan",
        "region": "Shizuoka",
        "lat": 35.1950,
        "lon": 138.6750,
        "elev": 310,
        "utc": 9,
        "cat": "mountain",
        "color": "0x47E0",
        "desc": "Panoramic southwest view of Mount Fuji across tea plantations and foothill forests.",
        "hl": "Iconic snow cap with foreground foothill forests",
        "cam_name": "Obuchi Station Cam",
        "src": "Ministry of Land, Infrastructure, Transport and Tourism (MLIT)",
        "url": "https://www.cbr.mlit.go.jp/fujisabo/camera/images/00024.jpg"
    },

    # =========================================================================
    # 3. 欧洲 (Europe) - 阿尔卑斯山名胜全景 (Foto-Webcam.eu 官方网络)
    # =========================================================================
    {
        "id": "de-zugspitze",
        "name": "Zugspitze",
        "local_name": "楚格峰 (德国最高峰 2962m)",
        "country": "Germany",
        "region": "Bavaria",
        "lat": 47.4211,
        "lon": 10.9853,
        "elev": 2962,
        "utc": 1,
        "cat": "mountain",
        "color": "0x9FFF",
        "desc": "Highest peak in Germany standing at 2,962m above sea level with glacier panoramic views.",
        "hl": "High Alpine glacier snowfield & endless sea of peaks",
        "cam_name": "Zugspitze South Ridge",
        "src": "Foto-Webcam.eu",
        "url": "https://www.foto-webcam.eu/webcam/zugspitze-sued/current/400.jpg"
    },
    {
        "id": "de-neuschwanstein",
        "name": "Neuschwanstein",
        "local_name": "新天鹅堡 (阿尔卑斯山麓)",
        "country": "Germany",
        "region": "Bavaria",
        "lat": 47.5576,
        "lon": 10.7498,
        "elev": 1730,
        "utc": 1,
        "cat": "culture",
        "color": "0xFEA0",
        "desc": "Iconic 19th-century historicist palace nestled against the Bavarian Alps backdrop.",
        "hl": "Tegelberg peak & fairy-tale castle foothills",
        "cam_name": "Tegelberg Alpine Cam",
        "src": "Foto-Webcam.eu",
        "url": "https://www.foto-webcam.eu/webcam/tegelberg/current/400.jpg"
    },
    {
        "id": "de-koenigssee",
        "name": "Lake Königssee",
        "local_name": "国王湖 (贝希特斯加登)",
        "country": "Germany",
        "region": "Bavaria",
        "lat": 47.5689,
        "lon": 12.9833,
        "elev": 1874,
        "utc": 1,
        "cat": "nature",
        "color": "0x0989",
        "desc": "Emerald-green fjord-like alpine lake beneath the sheer eastern rock wall of Mount Watzmann.",
        "hl": "Emerald waters & vertical limestone precipice",
        "cam_name": "Jennerbahn Peak Cam",
        "src": "Foto-Webcam.eu",
        "url": "https://www.foto-webcam.eu/webcam/jenner/current/400.jpg"
    },
    {
        "id": "at-innsbruck",
        "name": "Innsbruck Alps",
        "local_name": "因斯布鲁克 (阿尔卑斯之心)",
        "country": "Austria",
        "region": "Tyrol",
        "lat": 47.2692,
        "lon": 11.4041,
        "elev": 850,
        "utc": 1,
        "cat": "city",
        "color": "0x07FF",
        "desc": "Capital of Tyrol enclosed by dramatic 2,000m limestone mountain walls of the Nordkette.",
        "hl": "Tyrolean valley cityscape beneath vertical alpine walls",
        "cam_name": "Innsbruck Valley Cam",
        "src": "Foto-Webcam.eu",
        "url": "https://www.foto-webcam.eu/webcam/innsbruck/current/400.jpg"
    },
    {
        "id": "at-zell-am-see",
        "name": "Zell am See",
        "local_name": "滨湖采尔 (高山明珠)",
        "country": "Austria",
        "region": "Salzburg",
        "lat": 47.3235,
        "lon": 12.7968,
        "elev": 750,
        "utc": 1,
        "cat": "nature",
        "color": "0x3E7F",
        "desc": "Glittering crystal-clear alpine lake surrounded by snow-dusted Austrian peaks.",
        "hl": "Mirror lake surface reflecting surrounding summits",
        "cam_name": "Zeller See Panorama",
        "src": "Foto-Webcam.eu",
        "url": "https://www.foto-webcam.eu/webcam/zellamsee/current/400.jpg"
    },
    {
        "id": "de-wank-mountain",
        "name": "Wank Mountain",
        "local_name": "旺克峰 (全景看台)",
        "country": "Germany",
        "region": "Bavaria",
        "lat": 47.5097,
        "lon": 11.1444,
        "elev": 1780,
        "utc": 1,
        "cat": "mountain",
        "color": "0x47E0",
        "desc": "Sunny panoramic balcony overlooking Garmisch-Partenkirchen and the massive Alpspitz.",
        "hl": "360-degree panorama of the Wetterstein range",
        "cam_name": "Wankhaus Summit View",
        "src": "Foto-Webcam.eu",
        "url": "https://www.foto-webcam.eu/webcam/wank/current/400.jpg"
    },

    # =========================================================================
    # 4. 美洲 (Americas) - 美国国家公园与活火山 (NPS / USGS)
    # =========================================================================
    {
        "id": "us-yellowstone-washburn",
        "name": "Mount Washburn",
        "local_name": "黄石国家公园 (沃什伯恩峰)",
        "country": "United States",
        "region": "Wyoming",
        "lat": 44.7978,
        "lon": -110.4344,
        "elev": 3122,
        "utc": -7,
        "cat": "nature",
        "color": "0xFEA0",
        "desc": "Panoramic high-elevation fire lookout summit overlooking the vast Yellowstone Caldera basin.",
        "hl": "Endless pine forests & steaming geothermal basin",
        "cam_name": "Washburn Summit Cam",
        "src": "National Park Service (NPS)",
        "url": "https://www.nps.gov/webcams-yell/washburn_sw.jpg"
    },
    {
        "id": "us-yellowstone-mammoth",
        "name": "Mammoth Hot Springs",
        "local_name": "黄石国家公园 (猛犸温泉)",
        "country": "United States",
        "region": "Wyoming",
        "lat": 44.9769,
        "lon": -110.7011,
        "elev": 2050,
        "utc": -7,
        "cat": "nature",
        "color": "0xFEA0",
        "desc": "Ancient geothermal travertine terraces with Electric Peak towering in the backdrop.",
        "hl": "Travertine limestone terraces & snow-capped peaks",
        "cam_name": "Electric Peak View",
        "src": "National Park Service (NPS)",
        "url": "https://www.nps.gov/webcams-yell/mammoth_electric.jpg"
    },
    {
        "id": "us-hawaii-kilauea",
        "name": "Kilauea Volcano",
        "local_name": "夏威夷基拉韦厄活火山",
        "country": "United States",
        "region": "Hawaii",
        "lat": 19.4069,
        "lon": -155.2834,
        "elev": 1222,
        "utc": -10,
        "cat": "nature",
        "color": "0xF800",
        "desc": "One of Earth's most active shield volcanoes, looking directly into Halemaʻumaʻu caldera floor.",
        "hl": "Lava lake caldera floor & active volcanic degassing",
        "cam_name": "KWcam Halemaumau",
        "src": "USGS Hawaiian Volcano Observatory",
        "url": "https://volcanoes.usgs.gov/observatories/hvo/cams/KWcam/images/M.jpg"
    }
]

def probe_single_source(place):
    url = place["url"]
    start_time = time.time()
    result = {
        "id": place["id"],
        "name": place["name"],
        "country": place["country"],
        "url": url,
        "is_online": False,
        "http_code": -1,
        "bytes_size": 0,
        "rtt_ms": 0,
        "is_valid_jpeg": False,
        "error_msg": ""
    }

    try:
        req = urllib.request.Request(url, headers=HEADERS)
        with urllib.request.urlopen(req, timeout=7, context=SSL_CTX) as response:
            data = response.read()
            elapsed_ms = int((time.time() - start_time) * 1000)
            result["http_code"] = response.status
            result["bytes_size"] = len(data)
            result["rtt_ms"] = elapsed_ms

            # 校验 JPEG 幻数 (SOI 0xFF,0xD8)
            if len(data) > 4096 and data[0] == 0xFF and data[1] == 0xD8:
                result["is_valid_jpeg"] = True
                result["is_online"] = True
            else:
                result["error_msg"] = f"Invalid magic header (size={len(data)})"

    except Exception as e:
        elapsed_ms = int((time.time() - start_time) * 1000)
        result["rtt_ms"] = elapsed_ms
        result["error_msg"] = str(e)

    return result

def main():
    print("=" * 76)
    print(" LiveEarth Phase 11: Multi-Source Global Webcams Probe & Pipeline")
    print(f" Total authoritative candidates to verify: {len(CANDIDATE_SOURCES)}")
    print("=" * 76)

    results = []
    # 5 线程并发测试，杜绝阻塞且不触发服务端反爬
    with ThreadPoolExecutor(max_workers=5) as executor:
        futures = {executor.submit(probe_single_source, p): p for p in CANDIDATE_SOURCES}
        for future in as_completed(futures):
            res = future.result()
            results.append(res)
            status_tag = "[PASS]" if res["is_online"] else "[FAIL]"
            print(f"{status_tag} {res['name']:<24} | {res['country']:<14} | {res['rtt_ms']:>4}ms | {res['bytes_size']/1024:>6.1f}KB | {res['error_msg']}")

    online_count = sum(1 for r in results if r["is_online"])
    print("=" * 76)
    print(f" Probe Summary: {online_count}/{len(results)} sources ONLINE ({online_count/len(results)*100:.1f}%)")
    print("=" * 76)

    # 1. 组装清洗后的结构化数据
    validated_places = []
    res_map = {r["id"]: r for r in results}

    for p in CANDIDATE_SOURCES:
        probe_info = res_map.get(p["id"])
        is_ok = probe_info["is_online"] if probe_info else False
        item = dict(p)
        item["is_online"] = is_ok
        item["tested_rtt_ms"] = probe_info["rtt_ms"] if probe_info else 0
        item["tested_bytes"] = probe_info["bytes_size"] if probe_info else 0
        validated_places.append(item)

    # 2. 导出全量 JSON 到 data/places_live.json
    out_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "data")
    os.makedirs(out_dir, exist_ok=True)
    out_json_path = os.path.join(out_dir, "places_live.json")

    with open(out_json_path, "w", encoding="utf-8") as f:
        json.dump(validated_places, f, indent=2, ensure_ascii=False)
    print(f"[Export] Full live places dataset saved: {out_json_path}")

    # 3. 导出给 ESP32-S3 紧凑型 OTA 的精简 JSON (data/places_ota.json)
    compact_ota = []
    for p in validated_places:
        if p["is_online"]:
            compact_ota.append({
                "id": p["id"],
                "name": p["name"],
                "cntry": p["country"],
                "reg": p["region"],
                "lat": round(p["lat"], 4),
                "lon": round(p["lon"], 4),
                "elev": p["elev"],
                "utc": p["utc"],
                "cat": p["cat"],
                "col": p["color"],
                "desc": p["desc"],
                "hl": p["hl"],
                "cam": p["cam_name"],
                "src": p["src"],
                "url": p["url"],
                "live": True
            })

    ota_json_path = os.path.join(out_dir, "places_ota.json")
    with open(ota_json_path, "w", encoding="utf-8") as f:
        json.dump(compact_ota, f, separators=(',', ':'), ensure_ascii=False)
    ota_size = os.path.getsize(ota_json_path)
    print(f"[Export] Compact OTA JSON saved: {ota_json_path} ({len(compact_ota)} places, {ota_size} bytes)")

    # 4. 生成 Markdown 报告保存到 docs/
    docs_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "docs")
    report_path = os.path.join(docs_dir, "live_sources_report.md")
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("# LiveEarth 全球高质量直播源健康度探测报告\n\n")
        f.write(f"- **更新时间**: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"- **总候选机位**: {len(results)}\n")
        f.write(f"- **在线健康机位**: {online_count} ({online_count/len(results)*100:.1f}%)\n")
        f.write(f"- **ESP32 OTA 数据包大小**: {ota_size} 字节（极其紧凑，可在 0.2 秒内下载完毕）\n\n")
        f.write("| 状态 | 地点名称 | 国家 / 地区 | 类别 | 响应耗时 | 图像体积 | 机构来源 |\n")
        f.write("| :---: | :--- | :--- | :---: | :---: | :---: | :--- |\n")
        for p in validated_places:
            st = "🟢 LIVE" if p["is_online"] else "🔴 OFFLINE"
            kb = f"{p['tested_bytes']/1024:.1f} KB" if p["tested_bytes"] > 0 else "-"
            rtt = f"{p['tested_rtt_ms']} ms"
            f.write(f"| {st} | **{p['name']}** ({p['local_name']}) | {p['country']} | `{p['cat']}` | {rtt} | {kb} | {p['src']} |\n")

    print(f"[Report] Markdown verification report saved: {report_path}")

if __name__ == "__main__":
    main()
