import urllib.request
import ssl

ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

test_urls = [
    ('Wikimedia_Fuji', 'https://upload.wikimedia.org/wikipedia/commons/thumb/6/60/Fuji-san_from_Motosuko.jpg/400px-Fuji-san_from_Motosuko.jpg'),
    ('Wikimedia_GreatWall', 'https://upload.wikimedia.org/wikipedia/commons/thumb/2/23/The_Great_Wall_of_China_at_Jinshanling-edit.jpg/400px-The_Great_Wall_of_China_at_Jinshanling-edit.jpg'),
    ('Wikimedia_Eiffel', 'https://upload.wikimedia.org/wikipedia/commons/thumb/a/a8/Tour_Eiffel_Wikimedia_Commons.jpg/400px-Tour_Eiffel_Wikimedia_Commons.jpg'),
    ('Wikimedia_TimesSquare', 'https://upload.wikimedia.org/wikipedia/commons/thumb/4/47/New_york_times_square-terabass.jpg/400px-New_york_times_square-terabass.jpg'),
    ('Unsplash_Sample', 'https://images.unsplash.com/photo-1506744038136-46273834b3fb?w=400&q=80&fm=jpg'),
]

for name, u in test_urls:
    try:
        req = urllib.request.Request(u, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=5, context=ctx) as r:
            data = r.read()
            sof_type = None
            for i in range(len(data)-1):
                if data[i] == 0xFF and data[i+1] in [0xC0, 0xC1, 0xC2, 0xC3]:
                    sof_type = data[i+1]
                    break
            is_baseline = (sof_type == 0xC0)
            print(f"[{name}] status={r.status}, size={len(data)}, SOF=0x{sof_type:02X}, is_baseline={is_baseline}")
    except Exception as e:
        print(f"[{name}] error: {e}")
