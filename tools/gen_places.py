# 52 Global Curated Places Generator
import json

places = [
    # --- 亚洲 & 太平洋 (Asia & Pacific) ---
    {
        "id": "jp-mt-fuji", "name": "Mount Fuji", "local_name": "富士山", "country": "Japan", "region": "Yamanashi / Shizuoka",
        "lat": 35.3606, "lon": 138.7274, "elev": 3776, "utc": 9, "cat": "mountain", "color": 0x47E0,
        "desc": "Japan's sacred stratovolcano standing at 3,776m. UNESCO World Heritage site celebrated for its perfect snow symmetry.",
        "hl": "Lake Kawaguchiko scenic reflection & snowy crater rim", "cam_name": "Kawaguchiko Live Cam", "src": "Fujigoko TV Consortium",
        "url": "https://picsum.photos/seed/mtfuji/400/225.jpg"
    },
    {
        "id": "cn-great-wall", "name": "Great Wall of China", "local_name": "万里长城", "country": "China", "region": "Beijing",
        "lat": 40.3598, "lon": 116.0150, "elev": 856, "utc": 8, "cat": "culture", "color": 0xFEA0,
        "desc": "Ancient military fortification winding over rugged mountain ridges, spanning thousands of kilometers across northern China.",
        "hl": "Badaling mountain ridge battlements & watchtowers", "cam_name": "Badaling Ridge Cam", "src": "Beijing Heritage Administration",
        "url": "https://picsum.photos/seed/greatwall/400/225.jpg"
    },
    {
        "id": "cn-shanghai-bund", "name": "The Bund", "local_name": "上海外滩", "country": "China", "region": "Shanghai",
        "lat": 31.2397, "lon": 121.4900, "elev": 8, "utc": 8, "cat": "city", "color": 0x07FF,
        "desc": "Legendary waterfront on Huangpu River showcasing historical colonial architecture and futuristic Lujiazui skyscraper cluster.",
        "hl": "Oriental Pearl Tower, Shanghai Tower & river traffic", "cam_name": "Huangpu River Skyline", "src": "Shanghai Tourism Board",
        "url": "https://picsum.photos/seed/shanghai/400/225.jpg"
    },
    {
        "id": "hk-victoria-harbour", "name": "Victoria Harbour", "local_name": "维多利亚港", "country": "China", "region": "Hong Kong",
        "lat": 22.2942, "lon": 114.1706, "elev": 2, "utc": 8, "cat": "city", "color": 0x3E7F,
        "desc": "World-famous deepwater port situated between Hong Kong Island and Kowloon Peninsula, famous for its dense skyline and ferry.",
        "hl": "Tsim Sha Tsui Promenade & Symphony of Lights skyline", "cam_name": "Tsim Sha Tsui Live", "src": "HK Tourism Board",
        "url": "https://picsum.photos/seed/hongkong/400/225.jpg"
    },
    {
        "id": "jp-tokyo-shibuya", "name": "Shibuya Crossing", "local_name": "渋谷スクランブル交差点", "country": "Japan", "region": "Tokyo",
        "lat": 35.6595, "lon": 139.7005, "elev": 22, "utc": 9, "cat": "city", "color": 0xF81F,
        "desc": "The busiest pedestrian scramble crossing in the world, surrounded by towering neon LED billboards in central Tokyo.",
        "hl": "Hypnotic tidal wave of pedestrians & giant video screens", "cam_name": "Shibuya Scramble Cam", "src": "Shibuya City Council",
        "url": "https://picsum.photos/seed/shibuya/400/225.jpg"
    },
    {
        "id": "np-mt-everest", "name": "Mount Everest", "local_name": "珠穆朗玛峰", "country": "Nepal", "region": "Sagarmatha",
        "lat": 27.9881, "lon": 86.9250, "elev": 8848, "utc": 6, "cat": "mountain", "color": 0x9FFF,
        "desc": "Earth's highest mountain above sea level at 8,848.86m, located in the Mahalangur Himal sub-range of the Himalayas.",
        "hl": "Khumbu Icefall, Kala Patthar vista & Everest summit plume", "cam_name": "Kala Patthar Observatory", "src": "Ev-K2-CNR Research / Nepal Met",
        "url": "https://picsum.photos/seed/everest/400/225.jpg"
    },
    {
        "id": "in-taj-mahal", "name": "Taj Mahal", "local_name": "ताज महल", "country": "India", "region": "Uttar Pradesh",
        "lat": 27.1751, "lon": 78.0421, "elev": 171, "utc": 5, "cat": "culture", "color": 0xFFE0,
        "desc": "Ivory-white marble mausoleum on the south bank of the Yamuna River, universal masterpiece of Mughal architecture.",
        "hl": "Symmetrical reflection pool & central marble dome", "cam_name": "Taj Heritage View", "src": "Archaeological Survey of India",
        "url": "https://picsum.photos/seed/tajmahal/400/225.jpg"
    },
    {
        "id": "ae-burj-khalifa", "name": "Burj Khalifa", "local_name": "برج خليفة", "country": "United Arab Emirates", "region": "Dubai",
        "lat": 25.1972, "lon": 55.2744, "elev": 15, "utc": 4, "cat": "city", "color": 0xFEA0,
        "desc": "The tallest human-made structure in the world standing at 828m, soaring dramatically above Downtown Dubai.",
        "hl": "Dubai Fountain lake & futuristic desert skyline", "cam_name": "Downtown Dubai Cam", "src": "Emaar Properties / Dubai Tourism",
        "url": "https://picsum.photos/seed/dubai/400/225.jpg"
    },
    {
        "id": "sg-marina-bay", "name": "Marina Bay Sands", "local_name": "滨海湾金沙", "country": "Singapore", "region": "Central Area",
        "lat": 1.2838, "lon": 103.8591, "elev": 15, "utc": 8, "cat": "city", "color": 0x07E0,
        "desc": "Iconic integrated resort featuring three cascading towers topped by the cantilevered SkyPark and Infinity Pool.",
        "hl": "Singapore skyline, Gardens by the Bay & Merlion bay", "cam_name": "Marina Bay Panoramic", "src": "Singapore Tourism Board",
        "url": "https://picsum.photos/seed/singapore/400/225.jpg"
    },
    {
        "id": "kh-angkor-wat", "name": "Angkor Wat", "local_name": "អង្គរវត្ត", "country": "Cambodia", "region": "Siem Reap",
        "lat": 13.4125, "lon": 103.8670, "elev": 65, "utc": 7, "cat": "culture", "color": 0xFC80,
        "desc": "Enormous 12th-century temple complex and the largest religious monument in the world, heart of the Khmer Empire.",
        "hl": "Lotus-bud towers reflecting in the western moat at dawn", "cam_name": "Angkor Moat View", "src": "Apsara National Authority",
        "url": "https://picsum.photos/seed/angkorwat/400/225.jpg"
    },
    {
        "id": "id-tanah-lot", "name": "Tanah Lot Temple", "local_name": "Pura Tanah Lot", "country": "Indonesia", "region": "Bali",
        "lat": -8.6212, "lon": 115.0868, "elev": 10, "utc": 8, "cat": "culture", "color": 0xFDC0,
        "desc": "Ancient Hindu pilgrimage temple perched high atop a rugged sea rock formation amidst crashing Indian Ocean waves.",
        "hl": "Dramatic rock offshore & spectacular tropical ocean sunset", "cam_name": "Tanah Lot Coastline", "src": "Bali Tourism Agency",
        "url": "https://picsum.photos/seed/bali/400/225.jpg"
    },
    {
        "id": "th-wat-arun", "name": "Wat Arun", "local_name": "วัดอรุณราชวราราม", "country": "Thailand", "region": "Bangkok",
        "lat": 13.7437, "lon": 100.4888, "elev": 12, "utc": 7, "cat": "culture", "color": 0xFFE0,
        "desc": "The Temple of Dawn standing gracefully on the Chao Phraya River, decorated with thousands of colorful porcelain mosaics.",
        "hl": "Towering 82m central prang glistening across Chao Phraya river", "cam_name": "Chao Phraya River Cam", "src": "Bangkok Metropolitan Tourism",
        "url": "https://picsum.photos/seed/bangkok/400/225.jpg"
    },
    {
        "id": "jo-petra", "name": "Petra", "local_name": "البتراء", "country": "Jordan", "region": "Ma'an",
        "lat": 30.3285, "lon": 35.4444, "elev": 895, "utc": 3, "cat": "culture", "color": 0xFA20,
        "desc": "The historic Rose-Red City carved directly into vibrant sandstone cliffs by the Nabataeans over two millennia ago.",
        "hl": "Al-Khazneh (The Treasury) revealed through the narrow Siq gorge", "cam_name": "Petra Siq Entrance", "src": "Petra Development & Tourism Region",
        "url": "https://picsum.photos/seed/petra/400/225.jpg"
    },
    {
        "id": "il-dead-sea", "name": "Dead Sea", "local_name": "ים המלח", "country": "Israel", "region": "Judean Desert",
        "lat": 31.5000, "lon": 35.5000, "elev": -430, "utc": 3, "cat": "nature", "color": 0x07FF,
        "desc": "The lowest land elevation on Earth (-430m) featuring hypersaline waters where humans float naturally.",
        "hl": "Turquoise mineral waters, salt crystal formations & desert hills", "cam_name": "Ein Bokek Resort View", "src": "Dead Sea Regional Council",
        "url": "https://picsum.photos/seed/deadsea/400/225.jpg"
    },

    # --- 欧洲 & 地中海 (Europe & Mediterranean) ---
    {
        "id": "fr-paris-eiffel", "name": "Eiffel Tower", "local_name": "Tour Eiffel", "country": "France", "region": "Paris",
        "lat": 48.8584, "lon": 2.2945, "elev": 330, "utc": 1, "cat": "culture", "color": 0xFFE0,
        "desc": "Wrought-iron lattice tower standing 330m high on the Champ de Mars, the global symbol of Paris and romance.",
        "hl": "Seine River vista, golden beacon & illuminated nighttime sparkle", "cam_name": "Trocadero Panoramic", "src": "SETE / City of Paris",
        "url": "https://picsum.photos/seed/eiffeltower/400/225.jpg"
    },
    {
        "id": "uk-london-bigben", "name": "Big Ben & Westminster", "local_name": "Elizabeth Tower", "country": "United Kingdom", "region": "London",
        "lat": 51.5007, "lon": -0.1246, "elev": 18, "utc": 0, "cat": "culture", "color": 0xFEA0,
        "desc": "The Great Clock of the Palace of Westminster on the River Thames, towering Gothic Revival landmark of Britain.",
        "hl": "Westminster Bridge, historic clock tower & River Thames flow", "cam_name": "Westminster Bridge Cam", "src": "Port of London Authority",
        "url": "https://picsum.photos/seed/bigben/400/225.jpg"
    },
    {
        "id": "uk-greenwich-obs", "name": "Royal Observatory", "local_name": "Prime Meridian", "country": "United Kingdom", "region": "London",
        "lat": 51.4769, "lon": -0.0005, "elev": 46, "utc": 0, "cat": "special", "color": 0x07FF,
        "desc": "Historic astronomical observatory situated on Greenwich Hill, marking the Prime Meridian (Longitude 0°) and GMT origin.",
        "hl": "Zero Meridian brass line & Greenwich panoramic view over London", "cam_name": "Meridian Courtyard", "src": "Royal Museums Greenwich",
        "url": "https://picsum.photos/seed/greenwich/400/225.jpg"
    },
    {
        "id": "it-rome-colosseum", "name": "The Colosseum", "local_name": "Colosseo", "country": "Italy", "region": "Rome",
        "lat": 41.8902, "lon": 12.4922, "elev": 21, "utc": 1, "cat": "culture", "color": 0xFDC0,
        "desc": "The largest ancient amphitheatre ever built, constructed in 70-80 AD under the Flavian emperors of the Roman Empire.",
        "hl": "Multi-tier travertine arches & ancient gladiatorial arena walls", "cam_name": "Piazza del Colosseo", "src": "Parco Archeologico del Colosseo",
        "url": "https://picsum.photos/seed/colosseum/400/225.jpg"
    },
    {
        "id": "it-venice-canal", "name": "Grand Canal Venice", "local_name": "Canal Grande", "country": "Italy", "region": "Venice",
        "lat": 45.4371, "lon": 12.3326, "elev": 1, "utc": 1, "cat": "culture", "color": 0x3E7F,
        "desc": "The major water-traffic corridor in Venice lined by more than 170 magnificent Venetian Gothic and Renaissance palazzi.",
        "hl": "Rialto Bridge vista & traditional gondolas crossing waters", "cam_name": "Rialto Bridge Live", "src": "Comune di Venezia",
        "url": "https://picsum.photos/seed/venice/400/225.jpg"
    },
    {
        "id": "gr-athens-acropolis", "name": "Acropolis of Athens", "local_name": "Ακρόπολη Αθηνών", "country": "Greece", "region": "Attica",
        "lat": 37.9715, "lon": 23.7257, "elev": 156, "utc": 2, "cat": "culture", "color": 0xFFE0,
        "desc": "Ancient citadel perched high on a rocky outcrop above Athens, containing the architectural glory of the Parthenon.",
        "hl": "Parthenon marble columns gleaming beneath Mediterranean sunlight", "cam_name": "Acropolis View Cam", "src": "Hellenic Ministry of Culture",
        "url": "https://picsum.photos/seed/acropolis/400/225.jpg"
    },
    {
        "id": "es-barcelona-sagrada", "name": "Sagrada Familia", "local_name": "Basílica de la Sagrada Família", "country": "Spain", "region": "Catalonia",
        "lat": 41.4036, "lon": 2.1744, "elev": 45, "utc": 1, "cat": "culture", "color": 0xFA20,
        "desc": "Antoni Gaudí's unfinished Art Nouveau & Gothic masterpiece, celebrated for its soaring organic stone spires.",
        "hl": "Nativity Facade, vibrant stained glass towers & central spire construction", "cam_name": "Sagrada Familia Spire", "src": "Junta Constructora Sagrada Família",
        "url": "https://picsum.photos/seed/sagrada/400/225.jpg"
    },
    {
        "id": "ch-matterhorn", "name": "The Matterhorn", "local_name": "Matterhorn / Monte Cervino", "country": "Switzerland", "region": "Valais",
        "lat": 45.9763, "lon": 7.6586, "elev": 4478, "utc": 1, "cat": "mountain", "color": 0x9FFF,
        "desc": "Iconic nearly symmetrical pyramidal peak in the Pennine Alps, one of the deadliest and most recognizable mountains.",
        "hl": "Gornergrat scenic overlook, Alpine snow fields & sharp rock crest", "cam_name": "Zermatt Matterhorn Live", "src": "Zermatt Bergbahnen",
        "url": "https://picsum.photos/seed/matterhorn/400/225.jpg"
    },
    {
        "id": "tr-istanbul-bosphorus", "name": "Hagia Sophia & Bosphorus", "local_name": "Ayasofya-i Kebir", "country": "Turkey", "region": "Istanbul",
        "lat": 41.0086, "lon": 28.9802, "elev": 35, "utc": 3, "cat": "transport", "color": 0x07FF,
        "desc": "Monumental Byzantine cathedral and Ottoman mosque overlooking the Bosphorus Strait linking Europe and Asia.",
        "hl": "Massive dome, minarets & maritime shipping crossing continents", "cam_name": "Bosphorus Strait Live", "src": "Istanbul Metropolitan Municipality",
        "url": "https://picsum.photos/seed/istanbul/400/225.jpg"
    },
    {
        "id": "is-reykjavik", "name": "Reykjavik Harbour", "local_name": "Reykjavík", "country": "Iceland", "region": "Capital Region",
        "lat": 64.1466, "lon": -21.9426, "elev": 15, "utc": 0, "cat": "nature", "color": 0x57FE,
        "desc": "The world's northernmost sovereign capital city, gateway to subpolar glaciers, volcanic geysers and auroras.",
        "hl": "Harpa Concert Hall, Faxafloi Bay & seasonal Northern Lights", "cam_name": "Old Harbour Live", "src": "Icelandic Met Office (Veðurstofa)",
        "url": "https://picsum.photos/seed/reykjavik/400/225.jpg"
    },
    {
        "id": "no-preikestolen", "name": "Preikestolen (Pulpit Rock)", "local_name": "Preikestolen", "country": "Norway", "region": "Rogaland",
        "lat": 58.9864, "lon": 6.1887, "elev": 604, "utc": 1, "cat": "nature", "color": 0x07E0,
        "desc": "Steep cliff rising 604m vertically above the breathtaking Lysefjord, famous for its nearly flat 25x25m plateau summit.",
        "hl": "Sheer vertical drop, crystal fjord waters & dramatic granite precipice", "cam_name": "Lysefjord Overlook", "src": "Visit Norway / Stavanger Council",
        "url": "https://picsum.photos/seed/preikestolen/400/225.jpg"
    },
    {
        "id": "nl-amsterdam-canals", "name": "Amsterdam Canals", "local_name": "Grachtengordel", "country": "Netherlands", "region": "North Holland",
        "lat": 52.3676, "lon": 4.9041, "elev": 2, "utc": 1, "cat": "culture", "color": 0x4EEB,
        "desc": "17th-century concentric canal network lined with narrow brick gabled townhouses, houseboats and arched stone bridges.",
        "hl": "Prinsengracht canal reflection & historic Dutch townhouse bridges", "cam_name": "Damrak Waterfront", "src": "City of Amsterdam",
        "url": "https://picsum.photos/seed/amsterdam/400/225.jpg"
    },
    {
        "id": "gi-gibraltar-rock", "name": "Rock of Gibraltar", "local_name": "Pena de Gibraltar", "country": "Gibraltar", "region": "Strait of Gibraltar",
        "lat": 36.1408, "lon": -5.3536, "elev": 426, "utc": 1, "cat": "transport", "color": 0x257F,
        "desc": "Monolithic limestone promontory guarding the Strait of Gibraltar, strategic gateway between Atlantic and Mediterranean.",
        "hl": "Cliff summit vista, Strait maritime choke point & Moroccan coast view", "cam_name": "Gibraltar Strait Cam", "src": "Gibraltar Port Authority",
        "url": "https://picsum.photos/seed/gibraltar/400/225.jpg"
    },

    # --- 北美洲 (North America) ---
    {
        "id": "us-ny-times-square", "name": "Times Square", "local_name": "Times Square", "country": "United States", "region": "New York",
        "lat": 40.7580, "lon": -73.9855, "elev": 15, "utc": -5, "cat": "city", "color": 0xF81F,
        "desc": "The Crossroads of the World in Midtown Manhattan, illuminated 24/7 by gigantic high-definition LED billboards.",
        "hl": "Broadway theater district, red TKTS stairs & endless pedestrian crowds", "cam_name": "Broadway 46th St View", "src": "EarthCam Times Square",
        "url": "https://picsum.photos/seed/timessquare/400/225.jpg"
    },
    {
        "id": "us-hawaii-kilauea", "name": "Kilauea Volcano", "local_name": "Kīlauea", "country": "United States", "region": "Hawaii",
        "lat": 19.4069, "lon": -155.2833, "elev": 1247, "utc": -10, "cat": "nature", "color": 0xFA20,
        "desc": "Hyperactive shield volcano in Hawaii Volcanoes National Park with molten lava lake within Halemaʻumaʻu caldera.",
        "hl": "Halema'uma'u active vent, volcanic gas plumes & nighttime lava glow", "cam_name": "Halema'uma'u Summit Cam", "src": "USGS Volcano Science Center",
        "url": "https://picsum.photos/seed/kilauea/400/225.jpg"
    },
    {
        "id": "us-sf-goldengate", "name": "Golden Gate Bridge", "local_name": "Golden Gate Bridge", "country": "United States", "region": "California",
        "lat": 37.8199, "lon": -122.4783, "elev": 67, "utc": -8, "cat": "transport", "color": 0xFA20,
        "desc": "Internationally recognized suspension bridge in International Orange spanning the 1.6km strait connecting SF Bay and Pacific.",
        "hl": "Sweeping suspension cables, Marin Headlands & Pacific fog banks", "cam_name": "Marin Headlands View", "src": "Golden Gate Bridge District",
        "url": "https://picsum.photos/seed/goldengate/400/225.jpg"
    },
    {
        "id": "us-az-grand-canyon", "name": "Grand Canyon", "local_name": "Grand Canyon South Rim", "country": "United States", "region": "Arizona",
        "lat": 36.0544, "lon": -112.1401, "elev": 2134, "utc": -7, "cat": "nature", "color": 0xFD64,
        "desc": "Steep-sided canyon carved by the Colorado River, exposing nearly two billion years of Earth's geological history.",
        "hl": "Mather Point panoramic gorge, tiered red rock buttes & Colorado River", "cam_name": "Yavapai Point Live", "src": "National Park Service (NPS)",
        "url": "https://picsum.photos/seed/grandcanyon/400/225.jpg"
    },
    {
        "id": "us-wy-yellowstone", "name": "Yellowstone Old Faithful", "local_name": "Old Faithful Geyser", "country": "United States", "region": "Wyoming",
        "lat": 44.4605, "lon": -110.8281, "elev": 2240, "utc": -7, "cat": "nature", "color": 0x07FF,
        "desc": "World's most famous cone geyser in Yellowstone National Park, erupting boiling water up to 55m high every 90 minutes.",
        "hl": "Towering steam eruption plume & Upper Geyser Basin thermal pools", "cam_name": "Old Faithful Live", "src": "National Park Service",
        "url": "https://picsum.photos/seed/yellowstone/400/225.jpg"
    },
    {
        "id": "us-fl-kennedy-space", "name": "Kennedy Space Center", "local_name": "KSC Launch Complex 39", "country": "United States", "region": "Florida",
        "lat": 28.5729, "lon": -80.6490, "elev": 3, "utc": -5, "cat": "special", "color": 0x07FF,
        "desc": "NASA's premier spaceport on Merritt Island, launch site of the Apollo Saturn V lunar missions and Artemis Moon rockets.",
        "hl": "Launch Pad 39A/39B, Vehicle Assembly Building & Atlantic launchpad", "cam_name": "LC-39 Launch Cam", "src": "NASA / Space Coast Tourism",
        "url": "https://picsum.photos/seed/ksc/400/225.jpg"
    },
    {
        "id": "ca-banff-lake-louise", "name": "Lake Louise", "local_name": "Lake Louise", "country": "Canada", "region": "Alberta",
        "lat": 51.4254, "lon": -116.1773, "elev": 1750, "utc": -7, "cat": "nature", "color": 0x4EEB,
        "desc": "Glacial lake in Banff National Park world-renowned for its turquoise waters framed by Victoria Glacier and snow peaks.",
        "hl": "Chateau Lake Louise vista, Mt. Victoria reflection & turquoise waters", "cam_name": "Fairmont Waterfront", "src": "Parks Canada",
        "url": "https://picsum.photos/seed/lakelouise/400/225.jpg"
    },
    {
        "id": "us-ca-niagara-falls", "name": "Niagara Falls", "local_name": "Horseshoe Falls", "country": "Canada / US", "region": "Ontario / NY",
        "lat": 43.0799, "lon": -79.0747, "elev": 160, "utc": -5, "cat": "nature", "color": 0x3E7F,
        "desc": "Tremendous waterfalls on the Niagara River draining Lake Erie into Lake Ontario, highest flow rate in North America.",
        "hl": "Horseshoe Falls mist plume, Maid of the Mist & dramatic gorge", "cam_name": "Fallsview Live Cam", "src": "Niagara Parks Commission",
        "url": "https://picsum.photos/seed/niagara/400/225.jpg"
    },
    {
        "id": "mx-teotihuacan", "name": "Pyramid of the Sun", "local_name": "Pirámide del Sol", "country": "Mexico", "region": "State of Mexico",
        "lat": 19.6925, "lon": -98.8437, "elev": 2275, "utc": -6, "cat": "culture", "color": 0xFDC0,
        "desc": "Largest structure in the ancient Mesoamerican city of Teotihuacan, rising along the sacred Avenue of the Dead.",
        "hl": "Massive 65m stepped pyramid plateau & ancient obsidian city ruins", "cam_name": "Avenue of the Dead Cam", "src": "INAH Mexico",
        "url": "https://picsum.photos/seed/teotihuacan/400/225.jpg"
    },

    # --- 南美洲 (South America) ---
    {
        "id": "pe-machu-picchu", "name": "Machu Picchu", "local_name": "Machu Pikchu", "country": "Peru", "region": "Cusco",
        "lat": -13.1631, "lon": -72.5450, "elev": 2430, "utc": -5, "cat": "culture", "color": 0x47E0,
        "desc": "15th-century Inca citadel built on an 2,430m mountain ridge above the Sacred Valley, masterpiece of Inca stonework.",
        "hl": "Huayna Picchu peak backdrop, stone terraces & misty Andean cloud forest", "cam_name": "Inti Punku Overlook", "src": "Ministerio de Cultura del Perú",
        "url": "https://picsum.photos/seed/machupicchu/400/225.jpg"
    },
    {
        "id": "br-rio-christ", "name": "Christ the Redeemer", "local_name": "Cristo Redentor", "country": "Brazil", "region": "Rio de Janeiro",
        "lat": -22.9519, "lon": -43.2105, "elev": 710, "utc": -3, "cat": "culture", "color": 0x7FE0,
        "desc": "Colossal 30m Art Deco statue of Jesus atop Corcovado mountain overlooking Guanabara Bay and Rio's beaches.",
        "hl": "Sugarloaf Mountain panorama, Copacabana coastal curve & ocean bay", "cam_name": "Corcovado Summit Cam", "src": "Santuário Cristo Redentor",
        "url": "https://picsum.photos/seed/christrio/400/225.jpg"
    },
    {
        "id": "ar-br-iguazu-falls", "name": "Iguazu Falls", "local_name": "Cataratas del Iguazú", "country": "Argentina / Brazil", "region": "Misiones / Parana",
        "lat": -25.6953, "lon": -54.4367, "elev": 195, "utc": -3, "cat": "nature", "color": 0x07E0,
        "desc": "Chain of hundreds of cascading waterfalls on the Iguazu River straddling the border between Argentina and Brazil.",
        "hl": "Devil's Throat (Garganta del Diablo) roaring horseshoe chasm", "cam_name": "Devil's Throat Live", "src": "Parques Nacionales Argentina",
        "url": "https://picsum.photos/seed/iguazu/400/225.jpg"
    },
    {
        "id": "cl-easter-island", "name": "Easter Island (Rapa Nui)", "local_name": "Moai de Rapa Nui", "country": "Chile", "region": "Polynesia",
        "lat": -27.1212, "lon": -109.3664, "elev": 50, "utc": -6, "cat": "culture", "color": 0xFEA0,
        "desc": "Remote volcanic Polynesian island famous for its 900 monumental monolithic human figures carved by early Rapa Nui.",
        "hl": "Ahu Tongariki 15 upright stone Moai statues facing Pacific sunrise", "cam_name": "Ahu Tongariki View", "src": "Parque Nacional Rapa Nui",
        "url": "https://picsum.photos/seed/moai/400/225.jpg"
    },
    {
        "id": "bo-uyuni-salt", "name": "Salar de Uyuni", "local_name": "Salar de Uyuni", "country": "Bolivia", "region": "Potosi",
        "lat": -20.1338, "lon": -67.4891, "elev": 3656, "utc": -4, "cat": "nature", "color": 0x9FFF,
        "desc": "The world's largest salt flat spanning over 10,000 sq km at 3,656m elevation, transforming into a giant mirror in rainy season.",
        "hl": "Flawless sky mirror reflection, geometric salt crust & lithium flats", "cam_name": "Isla Incahuasi View", "src": "Viceministerio de Turismo Bolivia",
        "url": "https://picsum.photos/seed/uyuni/400/225.jpg"
    },
    {
        "id": "pa-panama-canal", "name": "Panama Canal Miraflores", "local_name": "Canal de Panamá", "country": "Panama", "region": "Panama City",
        "lat": 8.9972, "lon": -79.5932, "elev": 16, "utc": -5, "cat": "transport", "color": 0x257F,
        "desc": "Artificial 82km waterway connecting the Atlantic and Pacific oceans, one of the largest engineering feats of humanity.",
        "hl": "Miraflores Lock chambers, Panamax container ships & Pacific ocean entrance", "cam_name": "Miraflores Locks Live", "src": "Panama Canal Authority (ACP)",
        "url": "https://picsum.photos/seed/panamacanal/400/225.jpg"
    },

    # --- 非洲 (Africa) ---
    {
        "id": "eg-giza-pyramids", "name": "Pyramids of Giza", "local_name": "أهرامات الجيزة", "country": "Egypt", "region": "Giza / Cairo",
        "lat": 29.9792, "lon": 31.1342, "elev": 60, "utc": 2, "cat": "culture", "color": 0xFDC0,
        "desc": "The sole remaining ancient wonder of the world, monumental limestone royal tombs erected over 4,500 years ago.",
        "hl": "Great Pyramid of Khufu, Pyramid of Khafre & enigmatic Great Sphinx", "cam_name": "Giza Plateau Heritage", "src": "Egyptian Ministry of Tourism",
        "url": "https://picsum.photos/seed/pyramids/400/225.jpg"
    },
    {
        "id": "tz-kilimanjaro", "name": "Mount Kilimanjaro", "local_name": "Kilimanjaro", "country": "Tanzania", "region": "Kilimanjaro",
        "lat": -3.0674, "lon": 37.3556, "elev": 5895, "utc": 3, "cat": "mountain", "color": 0x9FFF,
        "desc": "A dormant stratovolcano and the highest mountain in Africa at 5,895m, towering standalone above the East African plains.",
        "hl": "Uhuru Peak snowy caldera, equatorial glaciers & savannah plains", "cam_name": "Moshi Plateau Cam", "src": "Tanzania National Parks (TANAPA)",
        "url": "https://picsum.photos/seed/kilimanjaro/400/225.jpg"
    },
    {
        "id": "zm-victoria-falls", "name": "Victoria Falls", "local_name": "Mosi-oa-Tunya", "country": "Zambia / Zimbabwe", "region": "Livingstone",
        "lat": -17.9243, "lon": 25.8572, "elev": 885, "utc": 2, "cat": "nature", "color": 0x07FF,
        "desc": "The Smoke that Thunders on the Zambezi River, forming the world's largest sheet of falling water based on width and height.",
        "hl": "Massive 1.7km wide curtain of water, perpetual mist rainbow & basalt gorge", "cam_name": "Gorge Bridge Live", "src": "Zambia Tourism Board",
        "url": "https://picsum.photos/seed/victoriafalls/400/225.jpg"
    },
    {
        "id": "za-cape-hope", "name": "Cape of Good Hope", "local_name": "Kaap die Goeie Hoop", "country": "South Africa", "region": "Western Cape",
        "lat": -34.3568, "lon": 18.4740, "elev": 214, "utc": 2, "cat": "nature", "color": 0x257F,
        "desc": "Legendary rocky promontory at the southern tip of the Cape Peninsula where Atlantic winds and currents collide.",
        "hl": "Historic Cape Point lighthouse clifftop, Table Mountain backdrop & ocean breakers", "cam_name": "Cape Point Lighthouse", "src": "SANParks Official",
        "url": "https://picsum.photos/seed/capehope/400/225.jpg"
    },
    {
        "id": "eg-suez-canal", "name": "Suez Canal", "local_name": "قناة السويس", "country": "Egypt", "region": "Port Said / Suez",
        "lat": 30.7050, "lon": 32.3442, "elev": 2, "utc": 2, "cat": "transport", "color": 0x07FF,
        "desc": "Vital 193km artificial sea-level waterway in Egypt, connecting the Mediterranean Sea to the Red Sea.",
        "hl": "Mega-container vessels navigating desert channel between Europe and Asia", "cam_name": "Ismailia Convoy View", "src": "Suez Canal Authority (SCA)",
        "url": "https://picsum.photos/seed/suez/400/225.jpg"
    },

    # --- 大洋洲 & 极地 (Oceania & Polar) ---
    {
        "id": "au-sydney-opera", "name": "Sydney Opera House", "local_name": "Sydney Opera House", "country": "Australia", "region": "New South Wales",
        "lat": -33.8568, "lon": 151.2153, "elev": 5, "utc": 10, "cat": "culture", "color": 0x3E7F,
        "desc": "Architectural icon of the 20th century with unique shell-like roof design gracing the Sydney Harbour shoreline.",
        "hl": "Harbour Bridge, Circular Quay ferry traffic & ceramic sail roof", "cam_name": "Harbour Esplanade Live", "src": "Port Authority of NSW",
        "url": "https://picsum.photos/seed/sydneyopera/400/225.jpg"
    },
    {
        "id": "au-uluru-rock", "name": "Uluru (Ayers Rock)", "local_name": "Uluṟu", "country": "Australia", "region": "Northern Territory",
        "lat": -25.3444, "lon": 131.0369, "elev": 863, "utc": 9, "cat": "nature", "color": 0xFA20,
        "desc": "Sacred massive sandstone monolith in central Australia's Red Centre, renowned for dramatic sunset color shifts.",
        "hl": "Ancient sacred Anangu caves, red desert spinifex & sunset glow", "cam_name": "Uluru Sunset Lookout", "src": "Parks Australia",
        "url": "https://picsum.photos/seed/uluru/400/225.jpg"
    },
    {
        "id": "au-great-barrier-reef", "name": "Great Barrier Reef", "local_name": "Great Barrier Reef", "country": "Australia", "region": "Queensland",
        "lat": -18.2871, "lon": 147.6992, "elev": 0, "utc": 10, "cat": "nature", "color": 0x07FF,
        "desc": "World's largest coral reef system composed of over 2,900 individual reefs, visible from outer space.",
        "hl": "Hardy Reef turquoise lagoon, underwater marine life & coral cay", "cam_name": "Heart Reef Live Cam", "src": "GBRMPA Authority",
        "url": "https://picsum.photos/seed/reef/400/225.jpg"
    },
    {
        "id": "nz-milford-sound", "name": "Milford Sound", "local_name": "Piopiotahi", "country": "New Zealand", "region": "Fiordland",
        "lat": -44.6716, "lon": 167.9256, "elev": 3, "utc": 12, "cat": "nature", "color": 0x07E0,
        "desc": "Glacially carved fiord in southwest New Zealand renowned for vertical sheer rock faces and roaring waterfalls.",
        "hl": "Mitre Peak rising 1,692m vertically from dark glacial waters", "cam_name": "Milford Wharf Live", "src": "Department of Conservation NZ",
        "url": "https://picsum.photos/seed/milfordsound/400/225.jpg"
    },
    {
        "id": "us-hawaii-maunakea", "name": "Mauna Kea Observatories", "local_name": "Mauna Kea", "country": "United States", "region": "Hawaii",
        "lat": 19.8207, "lon": -155.4681, "elev": 4205, "utc": -10, "cat": "special", "color": 0x9FFF,
        "desc": "Premier astronomical research facility in the world situated atop a 4,205m dormant volcano above tropical clouds.",
        "hl": "Keck I & II giant optical telescopes, clear dark skies & sea of clouds", "cam_name": "CFHT Ridge Cam", "src": "Institute for Astronomy Hawaii",
        "url": "https://picsum.photos/seed/maunakea/400/225.jpg"
    },
    {
        "id": "no-svalbard-vault", "name": "Svalbard Seed Vault", "local_name": "Svalbard globale frøhvelv", "country": "Norway", "region": "Spitsbergen",
        "lat": 78.2380, "lon": 15.4913, "elev": 130, "utc": 1, "cat": "special", "color": 0x57FE,
        "desc": "Secure seed backup facility buried deep inside Arctic permafrost on Spitsbergen island, humanity's doomsday safety net.",
        "hl": "Illuminated fiber-optic entrance portal & Arctic tundra snowfields", "cam_name": "Longyearbyen Tundra", "src": "NordGen / Government of Norway",
        "url": "https://picsum.photos/seed/svalbard/400/225.jpg"
    },
    {
        "id": "aq-mcmurdo-station", "name": "McMurdo Station", "local_name": "McMurdo Antarctica", "country": "Antarctica", "region": "Ross Island",
        "lat": -77.8419, "lon": 166.6863, "elev": 24, "utc": 12, "cat": "special", "color": 0x9FFF,
        "desc": "United States Antarctic research center on the southern tip of Ross Island, the largest community in Antarctica.",
        "hl": "Mount Erebus active volcano steam, sea ice sheets & polar research base", "cam_name": "Scott Base / McMurdo", "src": "United States Antarctic Program (USAP)",
        "url": "https://picsum.photos/seed/antarctica/400/225.jpg"
    }
]

print(f"Total curated places: {len(places)}")

# Generate C++ array
lines = []
lines.append("static const PlaceLocation GLOBAL_PLACES[] = {")
for p in places:
    lines.append("    {")
    lines.append(f'        "{p["id"]}",')
    lines.append(f'        "{p["name"]}",')
    lines.append(f'        "{p["local_name"]}",')
    lines.append(f'        "{p["country"]}",')
    lines.append(f'        "{p["region"]}",')
    lines.append(f'        {p["lat"]:.4f}f,')
    lines.append(f'        {p["lon"]:.4f}f,')
    lines.append(f'        {p["elev"]},')
    lines.append(f'        {p["utc"]},')
    lines.append(f'        "{p["cat"]}",')
    lines.append(f'        0x{p["color"]:04X},')
    lines.append(f'        "{p["desc"]}",')
    lines.append(f'        "{p["hl"]}",')
    lines.append(f'        "{p["cam_name"]}",')
    lines.append(f'        "{p["src"]}",')
    lines.append(f'        "{p["url"]}",')
    lines.append('        1,')
    lines.append('        true')
    lines.append("    },")

lines[-1] = "    }"  # remove last comma
lines.append("};")
lines.append("")
lines.append("static const size_t GLOBAL_PLACES_COUNT = sizeof(GLOBAL_PLACES) / sizeof(GLOBAL_PLACES[0]);")

cpp_content = "\n".join(lines)
with open("data/places_cpp.txt", "w", encoding="utf-8") as f:
    f.write(cpp_content)

print("Saved to data/places_cpp.txt")
