#!/usr/bin/env python3
import urllib.request
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

candidates = [
    ('USGS Kilauea Volcano M', 'https://volcanoes.usgs.gov/observatories/hvo/cams/K2cam/images/M.jpg'),
    ('USGS Mauna Loa MLcam', 'https://volcanoes.usgs.gov/observatories/hvo/cams/MLcam/images/M.jpg'),
    ('USGS Mount St Helens', 'https://volcanoes.usgs.gov/observatories/cvo/cams/msh/images/M.jpg'),
    ('HKO Victoria Peak (vpk)', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/vpk/latest_HD_VPK.jpg'),
    ('HKO ICC Victoria Harbour', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/ic2/latest_HD_IC2.jpg'),
    ('HKO Tsim Sha Tsui', 'https://www.hko.gov.hk/wxinfo/aws/hko_mica/tst/latest_HD_TST.jpg'),
    ('NOAA GOES Earth Full Disk', 'https://cdn.star.nesdis.noaa.gov/GOES16/ABI/FD/GEOCOLOR/678x678.jpg'),
    ('NOAA Himawari Earth Disk', 'https://cdn.star.nesdis.noaa.gov/HIMAWARI/FD/GEOCOLOR/550x550.jpg'),
]

headers = {'User-Agent': 'Mozilla/5.0'}

for name, url in candidates:
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=8, context=ctx) as r:
            d = r.read()
            is_jpg = d[:2] == b'\xff\xd8'
            print(f"[v] {name:28s}: HTTP {r.status}, {len(d):7d} bytes, Is_JPEG={is_jpg}")
    except Exception as e:
        print(f"[x] {name:28s}: FAILED -> {e}")
