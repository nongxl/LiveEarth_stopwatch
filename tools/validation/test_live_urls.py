#!/usr/bin/env python3
import urllib.request
import ssl
import time

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

# 11 个当前项目中已配置并经过实测连通的官方在线直播快照源
candidates = [
    # --- 中国香港沿海与都会天际线 (香港天文台 HKO) ---
    ('HK_Victoria_Peak', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/vpb/latest_VPB.jpg'),
    ('HK_Tsim_Sha_Tsui', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/cwb/latest_CWB.jpg'),
    ('HK_Cheung_Chau', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/cch/latest_CCH.jpg'),
    ('HK_Central_Waterfront', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/elc/latest_ELC.jpg'),
    ('HK_Sai_Kung_Geopark', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/skg/latest_SKG.jpg'),
    ('HK_Disneyland_Resort', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/dnl/latest_DNL.jpg'),

    # --- 欧洲阿尔卑斯群山与名城 (Foto-Webcam.eu 官方气象风光网) ---
    ('DE_Zugspitze_Alps', 'https://www.foto-webcam.eu/webcam/zugspitze-ost/current/400.jpg'),
    ('AT_Salzburg_Fortress', 'https://www.foto-webcam.eu/webcam/salzburg/current/400.jpg'),
    ('AT_Innsbruck_Nordkette', 'https://www.foto-webcam.eu/webcam/innsbruck/current/400.jpg'),

    # --- 太平洋夏威夷火山 (美国地质调查局 USGS 监测网) ---
    ('US_Kilauea_Caldera', 'https://volcanoes.usgs.gov/observatories/hvo/cams/K2cam/images/M.jpg'),
    ('US_Mauna_Loa_Summit', 'https://volcanoes.usgs.gov/observatories/hvo/cams/MLcam/images/M.jpg'),
]

headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'}

print("Validating 11 verified live camera streams...")
for name, url in candidates:
    try:
        t0 = time.time()
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=6, context=ctx) as r:
            d = r.read()
            dt = int((time.time() - t0) * 1000)
            is_jpg = len(d) > 4 and d[0] == 0xFF and d[1] == 0xD8
            status_tag = "[PASS]" if (is_jpg and r.status == 200) else "[WARN]"
            print(f"{status_tag} {name:24s}: HTTP {r.status} | {len(d)//1024:3d} KB in {dt:4d}ms | JPEG={is_jpg}")
    except Exception as e:
        print(f"[FAIL] {name:24s}: {e}")
