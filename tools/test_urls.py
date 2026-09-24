import urllib.request
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

candidates = [
    ('Yamanakako_1', 'http://www.vill.yamanakako.lg.jp/camera/yamanakako.jpg'),
    ('Yamanakako_2', 'http://www.vill.yamanakako.lg.jp/camera/fuji.jpg'),
    ('Fujiyoshida', 'https://camera.city.fujiyoshida.yamanashi.jp/image/fuji.jpg'),
    ('Fujiyama_Navi', 'https://www.fujiyama-navi.jp/static/livecamera/images/fuji_large.jpg'),
    ('Fujigoko_Lake01', 'http://www.fujigoko.tv/live/shot/lake01.jpg'),
    ('Fujigoko_Lake02', 'http://www.fujigoko.tv/live/shot/lake02.jpg'),
    ('Fujigoko_Lake03', 'http://www.fujigoko.tv/live/shot/lake03.jpg'),
    ('Picsum_Fuji_Seed', 'https://picsum.photos/seed/fuji/400/225.jpg'),
    ('Picsum_Hawaii_Seed', 'https://picsum.photos/seed/hawaii/400/225.jpg'),
    ('Picsum_TimesSquare_Seed', 'https://picsum.photos/seed/timessquare/400/225.jpg'),
]

for name, url in candidates:
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
        with urllib.request.urlopen(req, timeout=4, context=ctx) as r:
            data = r.read()
            is_jpg = len(data) > 2 and data[0] == 0xFF and data[1] == 0xD8
            print(f"[{name}] OK - code {r.status} - size {len(data)} - is_jpg {is_jpg}")
    except Exception as e:
        print(f"[{name}] FAIL - {e}")
