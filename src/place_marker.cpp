#include "place_marker.h"
#include "config.h"
#include <cmath>

#pragma GCC optimize ("O3,fast-math")

// -------------------------------------------------------------
// 全球代表性地标与公开相机点位数据库 (覆盖各大洲、火山、名山、都会)
// -------------------------------------------------------------
static const PlaceLocation GLOBAL_PLACES[] = {
    {
        "jp-mt-fuji",
        "Mount Fuji",
        "富士山",
        "Japan",
        "Yamanashi / Shizuoka",
        35.3606f,
        138.7274f,
        3776,
        9,
        "mountain",
        0x47E0,
        "Japan's sacred stratovolcano standing at 3,776m. UNESCO World Heritage site celebrated for its perfect snow symmetry.",
        "Lake Kawaguchiko scenic reflection & snowy crater rim",
        "Kawaguchiko Live Cam",
        "Fujigoko TV Consortium",
        "https://picsum.photos/seed/mtfuji/400/225.jpg",
        1,
        true
    },
    {
        "cn-great-wall",
        "Great Wall of China",
        "万里长城",
        "China",
        "Beijing",
        40.3598f,
        116.0150f,
        856,
        8,
        "culture",
        0xFEA0,
        "Ancient military fortification winding over rugged mountain ridges, spanning thousands of kilometers across northern China.",
        "Badaling mountain ridge battlements & watchtowers",
        "Badaling Ridge Cam",
        "Beijing Heritage Administration",
        "https://picsum.photos/seed/greatwall/400/225.jpg",
        1,
        true
    },
    {
        "cn-shanghai-bund",
        "The Bund",
        "上海外滩",
        "China",
        "Shanghai",
        31.2397f,
        121.4900f,
        8,
        8,
        "city",
        0x07FF,
        "Legendary waterfront on Huangpu River showcasing historical colonial architecture and futuristic Lujiazui skyscraper cluster.",
        "Oriental Pearl Tower, Shanghai Tower & river traffic",
        "Huangpu River Skyline",
        "Shanghai Tourism Board",
        "https://picsum.photos/seed/shanghai/400/225.jpg",
        1,
        true
    },
    {
        "hk-victoria-harbour",
        "Victoria Harbour",
        "维多利亚港",
        "China",
        "Hong Kong",
        22.2942f,
        114.1706f,
        2,
        8,
        "city",
        0x3E7F,
        "World-famous deepwater port situated between Hong Kong Island and Kowloon Peninsula, famous for its dense skyline and ferry.",
        "Tsim Sha Tsui Promenade & Symphony of Lights skyline",
        "Tsim Sha Tsui Live",
        "HK Tourism Board",
        "https://picsum.photos/seed/hongkong/400/225.jpg",
        1,
        true
    },
    {
        "jp-tokyo-shibuya",
        "Shibuya Crossing",
        "渋谷スクランブル交差点",
        "Japan",
        "Tokyo",
        35.6595f,
        139.7005f,
        22,
        9,
        "city",
        0xF81F,
        "The busiest pedestrian scramble crossing in the world, surrounded by towering neon LED billboards in central Tokyo.",
        "Hypnotic tidal wave of pedestrians & giant video screens",
        "Shibuya Scramble Cam",
        "Shibuya City Council",
        "https://picsum.photos/seed/shibuya/400/225.jpg",
        1,
        true
    },
    {
        "np-mt-everest",
        "Mount Everest",
        "珠穆朗玛峰",
        "Nepal",
        "Sagarmatha",
        27.9881f,
        86.9250f,
        8848,
        6,
        "mountain",
        0x9FFF,
        "Earth's highest mountain above sea level at 8,848.86m, located in the Mahalangur Himal sub-range of the Himalayas.",
        "Khumbu Icefall, Kala Patthar vista & Everest summit plume",
        "Kala Patthar Observatory",
        "Ev-K2-CNR Research / Nepal Met",
        "https://picsum.photos/seed/everest/400/225.jpg",
        1,
        true
    },
    {
        "in-taj-mahal",
        "Taj Mahal",
        "ताज महल",
        "India",
        "Uttar Pradesh",
        27.1751f,
        78.0421f,
        171,
        5,
        "culture",
        0xFFE0,
        "Ivory-white marble mausoleum on the south bank of the Yamuna River, universal masterpiece of Mughal architecture.",
        "Symmetrical reflection pool & central marble dome",
        "Taj Heritage View",
        "Archaeological Survey of India",
        "https://picsum.photos/seed/tajmahal/400/225.jpg",
        1,
        true
    },
    {
        "ae-burj-khalifa",
        "Burj Khalifa",
        "برج خليفة",
        "United Arab Emirates",
        "Dubai",
        25.1972f,
        55.2744f,
        15,
        4,
        "city",
        0xFEA0,
        "The tallest human-made structure in the world standing at 828m, soaring dramatically above Downtown Dubai.",
        "Dubai Fountain lake & futuristic desert skyline",
        "Downtown Dubai Cam",
        "Emaar Properties / Dubai Tourism",
        "https://picsum.photos/seed/dubai/400/225.jpg",
        1,
        true
    },
    {
        "sg-marina-bay",
        "Marina Bay Sands",
        "滨海湾金沙",
        "Singapore",
        "Central Area",
        1.2838f,
        103.8591f,
        15,
        8,
        "city",
        0x07E0,
        "Iconic integrated resort featuring three cascading towers topped by the cantilevered SkyPark and Infinity Pool.",
        "Singapore skyline, Gardens by the Bay & Merlion bay",
        "Marina Bay Panoramic",
        "Singapore Tourism Board",
        "https://picsum.photos/seed/singapore/400/225.jpg",
        1,
        true
    },
    {
        "kh-angkor-wat",
        "Angkor Wat",
        "អង្គរវត្ត",
        "Cambodia",
        "Siem Reap",
        13.4125f,
        103.8670f,
        65,
        7,
        "culture",
        0xFC80,
        "Enormous 12th-century temple complex and the largest religious monument in the world, heart of the Khmer Empire.",
        "Lotus-bud towers reflecting in the western moat at dawn",
        "Angkor Moat View",
        "Apsara National Authority",
        "https://picsum.photos/seed/angkorwat/400/225.jpg",
        1,
        true
    },
    {
        "id-tanah-lot",
        "Tanah Lot Temple",
        "Pura Tanah Lot",
        "Indonesia",
        "Bali",
        -8.6212f,
        115.0868f,
        10,
        8,
        "culture",
        0xFDC0,
        "Ancient Hindu pilgrimage temple perched high atop a rugged sea rock formation amidst crashing Indian Ocean waves.",
        "Dramatic rock offshore & spectacular tropical ocean sunset",
        "Tanah Lot Coastline",
        "Bali Tourism Agency",
        "https://picsum.photos/seed/bali/400/225.jpg",
        1,
        true
    },
    {
        "th-wat-arun",
        "Wat Arun",
        "วัดอรุณราชวราราม",
        "Thailand",
        "Bangkok",
        13.7437f,
        100.4888f,
        12,
        7,
        "culture",
        0xFFE0,
        "The Temple of Dawn standing gracefully on the Chao Phraya River, decorated with thousands of colorful porcelain mosaics.",
        "Towering 82m central prang glistening across Chao Phraya river",
        "Chao Phraya River Cam",
        "Bangkok Metropolitan Tourism",
        "https://picsum.photos/seed/bangkok/400/225.jpg",
        1,
        true
    },
    {
        "jo-petra",
        "Petra",
        "البتراء",
        "Jordan",
        "Ma'an",
        30.3285f,
        35.4444f,
        895,
        3,
        "culture",
        0xFA20,
        "The historic Rose-Red City carved directly into vibrant sandstone cliffs by the Nabataeans over two millennia ago.",
        "Al-Khazneh (The Treasury) revealed through the narrow Siq gorge",
        "Petra Siq Entrance",
        "Petra Development & Tourism Region",
        "https://picsum.photos/seed/petra/400/225.jpg",
        1,
        true
    },
    {
        "il-dead-sea",
        "Dead Sea",
        "ים המלח",
        "Israel",
        "Judean Desert",
        31.5000f,
        35.5000f,
        -430,
        3,
        "nature",
        0x07FF,
        "The lowest land elevation on Earth (-430m) featuring hypersaline waters where humans float naturally.",
        "Turquoise mineral waters, salt crystal formations & desert hills",
        "Ein Bokek Resort View",
        "Dead Sea Regional Council",
        "https://picsum.photos/seed/deadsea/400/225.jpg",
        1,
        true
    },
    {
        "fr-paris-eiffel",
        "Eiffel Tower",
        "Tour Eiffel",
        "France",
        "Paris",
        48.8584f,
        2.2945f,
        330,
        1,
        "culture",
        0xFFE0,
        "Wrought-iron lattice tower standing 330m high on the Champ de Mars, the global symbol of Paris and romance.",
        "Seine River vista, golden beacon & illuminated nighttime sparkle",
        "Trocadero Panoramic",
        "SETE / City of Paris",
        "https://picsum.photos/seed/eiffeltower/400/225.jpg",
        1,
        true
    },
    {
        "uk-london-bigben",
        "Big Ben & Westminster",
        "Elizabeth Tower",
        "United Kingdom",
        "London",
        51.5007f,
        -0.1246f,
        18,
        0,
        "culture",
        0xFEA0,
        "The Great Clock of the Palace of Westminster on the River Thames, towering Gothic Revival landmark of Britain.",
        "Westminster Bridge, historic clock tower & River Thames flow",
        "Westminster Bridge Cam",
        "Port of London Authority",
        "https://picsum.photos/seed/bigben/400/225.jpg",
        1,
        true
    },
    {
        "uk-greenwich-obs",
        "Royal Observatory",
        "Prime Meridian",
        "United Kingdom",
        "London",
        51.4769f,
        -0.0005f,
        46,
        0,
        "special",
        0x07FF,
        "Historic astronomical observatory situated on Greenwich Hill, marking the Prime Meridian (Longitude 0°) and GMT origin.",
        "Zero Meridian brass line & Greenwich panoramic view over London",
        "Meridian Courtyard",
        "Royal Museums Greenwich",
        "https://picsum.photos/seed/greenwich/400/225.jpg",
        1,
        true
    },
    {
        "it-rome-colosseum",
        "The Colosseum",
        "Colosseo",
        "Italy",
        "Rome",
        41.8902f,
        12.4922f,
        21,
        1,
        "culture",
        0xFDC0,
        "The largest ancient amphitheatre ever built, constructed in 70-80 AD under the Flavian emperors of the Roman Empire.",
        "Multi-tier travertine arches & ancient gladiatorial arena walls",
        "Piazza del Colosseo",
        "Parco Archeologico del Colosseo",
        "https://picsum.photos/seed/colosseum/400/225.jpg",
        1,
        true
    },
    {
        "it-venice-canal",
        "Grand Canal Venice",
        "Canal Grande",
        "Italy",
        "Venice",
        45.4371f,
        12.3326f,
        1,
        1,
        "culture",
        0x3E7F,
        "The major water-traffic corridor in Venice lined by more than 170 magnificent Venetian Gothic and Renaissance palazzi.",
        "Rialto Bridge vista & traditional gondolas crossing waters",
        "Rialto Bridge Live",
        "Comune di Venezia",
        "https://picsum.photos/seed/venice/400/225.jpg",
        1,
        true
    },
    {
        "gr-athens-acropolis",
        "Acropolis of Athens",
        "Ακρόπολη Αθηνών",
        "Greece",
        "Attica",
        37.9715f,
        23.7257f,
        156,
        2,
        "culture",
        0xFFE0,
        "Ancient citadel perched high on a rocky outcrop above Athens, containing the architectural glory of the Parthenon.",
        "Parthenon marble columns gleaming beneath Mediterranean sunlight",
        "Acropolis View Cam",
        "Hellenic Ministry of Culture",
        "https://picsum.photos/seed/acropolis/400/225.jpg",
        1,
        true
    },
    {
        "es-barcelona-sagrada",
        "Sagrada Familia",
        "Basílica de la Sagrada Família",
        "Spain",
        "Catalonia",
        41.4036f,
        2.1744f,
        45,
        1,
        "culture",
        0xFA20,
        "Antoni Gaudí's unfinished Art Nouveau & Gothic masterpiece, celebrated for its soaring organic stone spires.",
        "Nativity Facade, vibrant stained glass towers & central spire construction",
        "Sagrada Familia Spire",
        "Junta Constructora Sagrada Família",
        "https://picsum.photos/seed/sagrada/400/225.jpg",
        1,
        true
    },
    {
        "ch-matterhorn",
        "The Matterhorn",
        "Matterhorn / Monte Cervino",
        "Switzerland",
        "Valais",
        45.9763f,
        7.6586f,
        4478,
        1,
        "mountain",
        0x9FFF,
        "Iconic nearly symmetrical pyramidal peak in the Pennine Alps, one of the deadliest and most recognizable mountains.",
        "Gornergrat scenic overlook, Alpine snow fields & sharp rock crest",
        "Zermatt Matterhorn Live",
        "Zermatt Bergbahnen",
        "https://picsum.photos/seed/matterhorn/400/225.jpg",
        1,
        true
    },
    {
        "tr-istanbul-bosphorus",
        "Hagia Sophia & Bosphorus",
        "Ayasofya-i Kebir",
        "Turkey",
        "Istanbul",
        41.0086f,
        28.9802f,
        35,
        3,
        "transport",
        0x07FF,
        "Monumental Byzantine cathedral and Ottoman mosque overlooking the Bosphorus Strait linking Europe and Asia.",
        "Massive dome, minarets & maritime shipping crossing continents",
        "Bosphorus Strait Live",
        "Istanbul Metropolitan Municipality",
        "https://picsum.photos/seed/istanbul/400/225.jpg",
        1,
        true
    },
    {
        "is-reykjavik",
        "Reykjavik Harbour",
        "Reykjavík",
        "Iceland",
        "Capital Region",
        64.1466f,
        -21.9426f,
        15,
        0,
        "nature",
        0x57FE,
        "The world's northernmost sovereign capital city, gateway to subpolar glaciers, volcanic geysers and auroras.",
        "Harpa Concert Hall, Faxafloi Bay & seasonal Northern Lights",
        "Old Harbour Live",
        "Icelandic Met Office (Veðurstofa)",
        "https://picsum.photos/seed/reykjavik/400/225.jpg",
        1,
        true
    },
    {
        "no-preikestolen",
        "Preikestolen (Pulpit Rock)",
        "Preikestolen",
        "Norway",
        "Rogaland",
        58.9864f,
        6.1887f,
        604,
        1,
        "nature",
        0x07E0,
        "Steep cliff rising 604m vertically above the breathtaking Lysefjord, famous for its nearly flat 25x25m plateau summit.",
        "Sheer vertical drop, crystal fjord waters & dramatic granite precipice",
        "Lysefjord Overlook",
        "Visit Norway / Stavanger Council",
        "https://picsum.photos/seed/preikestolen/400/225.jpg",
        1,
        true
    },
    {
        "nl-amsterdam-canals",
        "Amsterdam Canals",
        "Grachtengordel",
        "Netherlands",
        "North Holland",
        52.3676f,
        4.9041f,
        2,
        1,
        "culture",
        0x4EEB,
        "17th-century concentric canal network lined with narrow brick gabled townhouses, houseboats and arched stone bridges.",
        "Prinsengracht canal reflection & historic Dutch townhouse bridges",
        "Damrak Waterfront",
        "City of Amsterdam",
        "https://picsum.photos/seed/amsterdam/400/225.jpg",
        1,
        true
    },
    {
        "gi-gibraltar-rock",
        "Rock of Gibraltar",
        "Pena de Gibraltar",
        "Gibraltar",
        "Strait of Gibraltar",
        36.1408f,
        -5.3536f,
        426,
        1,
        "transport",
        0x257F,
        "Monolithic limestone promontory guarding the Strait of Gibraltar, strategic gateway between Atlantic and Mediterranean.",
        "Cliff summit vista, Strait maritime choke point & Moroccan coast view",
        "Gibraltar Strait Cam",
        "Gibraltar Port Authority",
        "https://picsum.photos/seed/gibraltar/400/225.jpg",
        1,
        true
    },
    {
        "us-ny-times-square",
        "Times Square",
        "Times Square",
        "United States",
        "New York",
        40.7580f,
        -73.9855f,
        15,
        -5,
        "city",
        0xF81F,
        "The Crossroads of the World in Midtown Manhattan, illuminated 24/7 by gigantic high-definition LED billboards.",
        "Broadway theater district, red TKTS stairs & endless pedestrian crowds",
        "Broadway 46th St View",
        "EarthCam Times Square",
        "https://picsum.photos/seed/timessquare/400/225.jpg",
        1,
        true
    },
    {
        "us-hawaii-kilauea",
        "Kilauea Volcano",
        "Kīlauea",
        "United States",
        "Hawaii",
        19.4069f,
        -155.2833f,
        1247,
        -10,
        "nature",
        0xFA20,
        "Hyperactive shield volcano in Hawaii Volcanoes National Park with molten lava lake within Halemaʻumaʻu caldera.",
        "Halema'uma'u active vent, volcanic gas plumes & nighttime lava glow",
        "Halema'uma'u Summit Cam",
        "USGS Volcano Science Center",
        "https://picsum.photos/seed/kilauea/400/225.jpg",
        1,
        true
    },
    {
        "us-sf-goldengate",
        "Golden Gate Bridge",
        "Golden Gate Bridge",
        "United States",
        "California",
        37.8199f,
        -122.4783f,
        67,
        -8,
        "transport",
        0xFA20,
        "Internationally recognized suspension bridge in International Orange spanning the 1.6km strait connecting SF Bay and Pacific.",
        "Sweeping suspension cables, Marin Headlands & Pacific fog banks",
        "Marin Headlands View",
        "Golden Gate Bridge District",
        "https://picsum.photos/seed/goldengate/400/225.jpg",
        1,
        true
    },
    {
        "us-az-grand-canyon",
        "Grand Canyon",
        "Grand Canyon South Rim",
        "United States",
        "Arizona",
        36.0544f,
        -112.1401f,
        2134,
        -7,
        "nature",
        0xFD64,
        "Steep-sided canyon carved by the Colorado River, exposing nearly two billion years of Earth's geological history.",
        "Mather Point panoramic gorge, tiered red rock buttes & Colorado River",
        "Yavapai Point Live",
        "National Park Service (NPS)",
        "https://picsum.photos/seed/grandcanyon/400/225.jpg",
        1,
        true
    },
    {
        "us-wy-yellowstone",
        "Yellowstone Old Faithful",
        "Old Faithful Geyser",
        "United States",
        "Wyoming",
        44.4605f,
        -110.8281f,
        2240,
        -7,
        "nature",
        0x07FF,
        "World's most famous cone geyser in Yellowstone National Park, erupting boiling water up to 55m high every 90 minutes.",
        "Towering steam eruption plume & Upper Geyser Basin thermal pools",
        "Old Faithful Live",
        "National Park Service",
        "https://picsum.photos/seed/yellowstone/400/225.jpg",
        1,
        true
    },
    {
        "us-fl-kennedy-space",
        "Kennedy Space Center",
        "KSC Launch Complex 39",
        "United States",
        "Florida",
        28.5729f,
        -80.6490f,
        3,
        -5,
        "special",
        0x07FF,
        "NASA's premier spaceport on Merritt Island, launch site of the Apollo Saturn V lunar missions and Artemis Moon rockets.",
        "Launch Pad 39A/39B, Vehicle Assembly Building & Atlantic launchpad",
        "LC-39 Launch Cam",
        "NASA / Space Coast Tourism",
        "https://picsum.photos/seed/ksc/400/225.jpg",
        1,
        true
    },
    {
        "ca-banff-lake-louise",
        "Lake Louise",
        "Lake Louise",
        "Canada",
        "Alberta",
        51.4254f,
        -116.1773f,
        1750,
        -7,
        "nature",
        0x4EEB,
        "Glacial lake in Banff National Park world-renowned for its turquoise waters framed by Victoria Glacier and snow peaks.",
        "Chateau Lake Louise vista, Mt. Victoria reflection & turquoise waters",
        "Fairmont Waterfront",
        "Parks Canada",
        "https://picsum.photos/seed/lakelouise/400/225.jpg",
        1,
        true
    },
    {
        "us-ca-niagara-falls",
        "Niagara Falls",
        "Horseshoe Falls",
        "Canada / US",
        "Ontario / NY",
        43.0799f,
        -79.0747f,
        160,
        -5,
        "nature",
        0x3E7F,
        "Tremendous waterfalls on the Niagara River draining Lake Erie into Lake Ontario, highest flow rate in North America.",
        "Horseshoe Falls mist plume, Maid of the Mist & dramatic gorge",
        "Fallsview Live Cam",
        "Niagara Parks Commission",
        "https://picsum.photos/seed/niagara/400/225.jpg",
        1,
        true
    },
    {
        "mx-teotihuacan",
        "Pyramid of the Sun",
        "Pirámide del Sol",
        "Mexico",
        "State of Mexico",
        19.6925f,
        -98.8437f,
        2275,
        -6,
        "culture",
        0xFDC0,
        "Largest structure in the ancient Mesoamerican city of Teotihuacan, rising along the sacred Avenue of the Dead.",
        "Massive 65m stepped pyramid plateau & ancient obsidian city ruins",
        "Avenue of the Dead Cam",
        "INAH Mexico",
        "https://picsum.photos/seed/teotihuacan/400/225.jpg",
        1,
        true
    },
    {
        "pe-machu-picchu",
        "Machu Picchu",
        "Machu Pikchu",
        "Peru",
        "Cusco",
        -13.1631f,
        -72.5450f,
        2430,
        -5,
        "culture",
        0x47E0,
        "15th-century Inca citadel built on an 2,430m mountain ridge above the Sacred Valley, masterpiece of Inca stonework.",
        "Huayna Picchu peak backdrop, stone terraces & misty Andean cloud forest",
        "Inti Punku Overlook",
        "Ministerio de Cultura del Perú",
        "https://picsum.photos/seed/machupicchu/400/225.jpg",
        1,
        true
    },
    {
        "br-rio-christ",
        "Christ the Redeemer",
        "Cristo Redentor",
        "Brazil",
        "Rio de Janeiro",
        -22.9519f,
        -43.2105f,
        710,
        -3,
        "culture",
        0x7FE0,
        "Colossal 30m Art Deco statue of Jesus atop Corcovado mountain overlooking Guanabara Bay and Rio's beaches.",
        "Sugarloaf Mountain panorama, Copacabana coastal curve & ocean bay",
        "Corcovado Summit Cam",
        "Santuário Cristo Redentor",
        "https://picsum.photos/seed/christrio/400/225.jpg",
        1,
        true
    },
    {
        "ar-br-iguazu-falls",
        "Iguazu Falls",
        "Cataratas del Iguazú",
        "Argentina / Brazil",
        "Misiones / Parana",
        -25.6953f,
        -54.4367f,
        195,
        -3,
        "nature",
        0x07E0,
        "Chain of hundreds of cascading waterfalls on the Iguazu River straddling the border between Argentina and Brazil.",
        "Devil's Throat (Garganta del Diablo) roaring horseshoe chasm",
        "Devil's Throat Live",
        "Parques Nacionales Argentina",
        "https://picsum.photos/seed/iguazu/400/225.jpg",
        1,
        true
    },
    {
        "cl-easter-island",
        "Easter Island (Rapa Nui)",
        "Moai de Rapa Nui",
        "Chile",
        "Polynesia",
        -27.1212f,
        -109.3664f,
        50,
        -6,
        "culture",
        0xFEA0,
        "Remote volcanic Polynesian island famous for its 900 monumental monolithic human figures carved by early Rapa Nui.",
        "Ahu Tongariki 15 upright stone Moai statues facing Pacific sunrise",
        "Ahu Tongariki View",
        "Parque Nacional Rapa Nui",
        "https://picsum.photos/seed/moai/400/225.jpg",
        1,
        true
    },
    {
        "bo-uyuni-salt",
        "Salar de Uyuni",
        "Salar de Uyuni",
        "Bolivia",
        "Potosi",
        -20.1338f,
        -67.4891f,
        3656,
        -4,
        "nature",
        0x9FFF,
        "The world's largest salt flat spanning over 10,000 sq km at 3,656m elevation, transforming into a giant mirror in rainy season.",
        "Flawless sky mirror reflection, geometric salt crust & lithium flats",
        "Isla Incahuasi View",
        "Viceministerio de Turismo Bolivia",
        "https://picsum.photos/seed/uyuni/400/225.jpg",
        1,
        true
    },
    {
        "pa-panama-canal",
        "Panama Canal Miraflores",
        "Canal de Panamá",
        "Panama",
        "Panama City",
        8.9972f,
        -79.5932f,
        16,
        -5,
        "transport",
        0x257F,
        "Artificial 82km waterway connecting the Atlantic and Pacific oceans, one of the largest engineering feats of humanity.",
        "Miraflores Lock chambers, Panamax container ships & Pacific ocean entrance",
        "Miraflores Locks Live",
        "Panama Canal Authority (ACP)",
        "https://picsum.photos/seed/panamacanal/400/225.jpg",
        1,
        true
    },
    {
        "eg-giza-pyramids",
        "Pyramids of Giza",
        "أهرامات الجيزة",
        "Egypt",
        "Giza / Cairo",
        29.9792f,
        31.1342f,
        60,
        2,
        "culture",
        0xFDC0,
        "The sole remaining ancient wonder of the world, monumental limestone royal tombs erected over 4,500 years ago.",
        "Great Pyramid of Khufu, Pyramid of Khafre & enigmatic Great Sphinx",
        "Giza Plateau Heritage",
        "Egyptian Ministry of Tourism",
        "https://picsum.photos/seed/pyramids/400/225.jpg",
        1,
        true
    },
    {
        "tz-kilimanjaro",
        "Mount Kilimanjaro",
        "Kilimanjaro",
        "Tanzania",
        "Kilimanjaro",
        -3.0674f,
        37.3556f,
        5895,
        3,
        "mountain",
        0x9FFF,
        "A dormant stratovolcano and the highest mountain in Africa at 5,895m, towering standalone above the East African plains.",
        "Uhuru Peak snowy caldera, equatorial glaciers & savannah plains",
        "Moshi Plateau Cam",
        "Tanzania National Parks (TANAPA)",
        "https://picsum.photos/seed/kilimanjaro/400/225.jpg",
        1,
        true
    },
    {
        "zm-victoria-falls",
        "Victoria Falls",
        "Mosi-oa-Tunya",
        "Zambia / Zimbabwe",
        "Livingstone",
        -17.9243f,
        25.8572f,
        885,
        2,
        "nature",
        0x07FF,
        "The Smoke that Thunders on the Zambezi River, forming the world's largest sheet of falling water based on width and height.",
        "Massive 1.7km wide curtain of water, perpetual mist rainbow & basalt gorge",
        "Gorge Bridge Live",
        "Zambia Tourism Board",
        "https://picsum.photos/seed/victoriafalls/400/225.jpg",
        1,
        true
    },
    {
        "za-cape-hope",
        "Cape of Good Hope",
        "Kaap die Goeie Hoop",
        "South Africa",
        "Western Cape",
        -34.3568f,
        18.4740f,
        214,
        2,
        "nature",
        0x257F,
        "Legendary rocky promontory at the southern tip of the Cape Peninsula where Atlantic winds and currents collide.",
        "Historic Cape Point lighthouse clifftop, Table Mountain backdrop & ocean breakers",
        "Cape Point Lighthouse",
        "SANParks Official",
        "https://picsum.photos/seed/capehope/400/225.jpg",
        1,
        true
    },
    {
        "eg-suez-canal",
        "Suez Canal",
        "قناة السويس",
        "Egypt",
        "Port Said / Suez",
        30.7050f,
        32.3442f,
        2,
        2,
        "transport",
        0x07FF,
        "Vital 193km artificial sea-level waterway in Egypt, connecting the Mediterranean Sea to the Red Sea.",
        "Mega-container vessels navigating desert channel between Europe and Asia",
        "Ismailia Convoy View",
        "Suez Canal Authority (SCA)",
        "https://picsum.photos/seed/suez/400/225.jpg",
        1,
        true
    },
    {
        "au-sydney-opera",
        "Sydney Opera House",
        "Sydney Opera House",
        "Australia",
        "New South Wales",
        -33.8568f,
        151.2153f,
        5,
        10,
        "culture",
        0x3E7F,
        "Architectural icon of the 20th century with unique shell-like roof design gracing the Sydney Harbour shoreline.",
        "Harbour Bridge, Circular Quay ferry traffic & ceramic sail roof",
        "Harbour Esplanade Live",
        "Port Authority of NSW",
        "https://picsum.photos/seed/sydneyopera/400/225.jpg",
        1,
        true
    },
    {
        "au-uluru-rock",
        "Uluru (Ayers Rock)",
        "Uluṟu",
        "Australia",
        "Northern Territory",
        -25.3444f,
        131.0369f,
        863,
        9,
        "nature",
        0xFA20,
        "Sacred massive sandstone monolith in central Australia's Red Centre, renowned for dramatic sunset color shifts.",
        "Ancient sacred Anangu caves, red desert spinifex & sunset glow",
        "Uluru Sunset Lookout",
        "Parks Australia",
        "https://picsum.photos/seed/uluru/400/225.jpg",
        1,
        true
    },
    {
        "au-great-barrier-reef",
        "Great Barrier Reef",
        "Great Barrier Reef",
        "Australia",
        "Queensland",
        -18.2871f,
        147.6992f,
        0,
        10,
        "nature",
        0x07FF,
        "World's largest coral reef system composed of over 2,900 individual reefs, visible from outer space.",
        "Hardy Reef turquoise lagoon, underwater marine life & coral cay",
        "Heart Reef Live Cam",
        "GBRMPA Authority",
        "https://picsum.photos/seed/reef/400/225.jpg",
        1,
        true
    },
    {
        "nz-milford-sound",
        "Milford Sound",
        "Piopiotahi",
        "New Zealand",
        "Fiordland",
        -44.6716f,
        167.9256f,
        3,
        12,
        "nature",
        0x07E0,
        "Glacially carved fiord in southwest New Zealand renowned for vertical sheer rock faces and roaring waterfalls.",
        "Mitre Peak rising 1,692m vertically from dark glacial waters",
        "Milford Wharf Live",
        "Department of Conservation NZ",
        "https://picsum.photos/seed/milfordsound/400/225.jpg",
        1,
        true
    },
    {
        "us-hawaii-maunakea",
        "Mauna Kea Observatories",
        "Mauna Kea",
        "United States",
        "Hawaii",
        19.8207f,
        -155.4681f,
        4205,
        -10,
        "special",
        0x9FFF,
        "Premier astronomical research facility in the world situated atop a 4,205m dormant volcano above tropical clouds.",
        "Keck I & II giant optical telescopes, clear dark skies & sea of clouds",
        "CFHT Ridge Cam",
        "Institute for Astronomy Hawaii",
        "https://picsum.photos/seed/maunakea/400/225.jpg",
        1,
        true
    },
    {
        "no-svalbard-vault",
        "Svalbard Seed Vault",
        "Svalbard globale frøhvelv",
        "Norway",
        "Spitsbergen",
        78.2380f,
        15.4913f,
        130,
        1,
        "special",
        0x57FE,
        "Secure seed backup facility buried deep inside Arctic permafrost on Spitsbergen island, humanity's doomsday safety net.",
        "Illuminated fiber-optic entrance portal & Arctic tundra snowfields",
        "Longyearbyen Tundra",
        "NordGen / Government of Norway",
        "https://picsum.photos/seed/svalbard/400/225.jpg",
        1,
        true
    },
    {
        "aq-mcmurdo-station",
        "McMurdo Station",
        "McMurdo Antarctica",
        "Antarctica",
        "Ross Island",
        -77.8419f,
        166.6863f,
        24,
        12,
        "special",
        0x9FFF,
        "United States Antarctic research center on the southern tip of Ross Island, the largest community in Antarctica.",
        "Mount Erebus active volcano steam, sea ice sheets & polar research base",
        "Scott Base / McMurdo",
        "United States Antarctic Program (USAP)",
        "https://picsum.photos/seed/antarctica/400/225.jpg",
        1,
        true
    }
};

static const size_t GLOBAL_PLACES_COUNT = sizeof(GLOBAL_PLACES) / sizeof(GLOBAL_PLACES[0]);

// -------------------------------------------------------------
// PlaceMarkerManager Implementation
// -------------------------------------------------------------
PlaceMarkerManager::PlaceMarkerManager()
    : pulse_phase(0.0f), selected_idx(0), last_anim_ms(0) {
}

void PlaceMarkerManager::init() {
    pulse_phase = 0.0f;
    selected_idx = 0; // 默认聚焦第一处（富士山）
    last_anim_ms = millis();
}

void PlaceMarkerManager::update(uint32_t now_ms) {
    if (last_anim_ms == 0) last_anim_ms = now_ms;
    float dt = (now_ms - last_anim_ms) * 0.001f;
    last_anim_ms = now_ms;
    if (dt > 0.1f) dt = 0.1f;

    // 呼吸周期约为 1.8 秒循环一次
    pulse_phase += dt * 3.5f;
    if (pulse_phase >= 6.2831853f) {
        pulse_phase -= 6.2831853f;
    }
}

size_t PlaceMarkerManager::getCount() const {
    return GLOBAL_PLACES_COUNT;
}

const PlaceLocation* PlaceMarkerManager::getPlace(int index) const {
    if (index >= 0 && index < (int)GLOBAL_PLACES_COUNT) {
        return &GLOBAL_PLACES[index];
    }
    return nullptr;
}

const PlaceLocation* PlaceMarkerManager::getSelectedPlace() const {
    return getPlace(selected_idx);
}

void PlaceMarkerManager::setSelectedIndex(int idx) {
    if (idx >= 0 && idx < (int)GLOBAL_PLACES_COUNT) {
        selected_idx = idx;
    }
}

void PlaceMarkerManager::nextPlace() {
    selected_idx = (selected_idx + 1) % GLOBAL_PLACES_COUNT;
}

void PlaceMarkerManager::prevPlace() {
    selected_idx = (selected_idx + GLOBAL_PLACES_COUNT - 1) % GLOBAL_PLACES_COUNT;
}

// -------------------------------------------------------------
// Phase 3 & Phase 4: 渲染所有正面可见 Marker 并执行动态呼吸光圈
// -------------------------------------------------------------
void PlaceMarkerManager::renderMarkers(M5Canvas& canvas, const GlobeEngine& globe) {
    // 呼吸动态扩散热度：3.0px ~ 7.5px
    float pulse_sine = 0.5f + 0.5f * sinf(pulse_phase);
    float pulse_radius = 3.0f + 4.0f * pulse_sine;

    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        const PlaceLocation& place = GLOBAL_PLACES[i];

        int16_t sx, sy;
        float depth;

        // Phase 3: 球极坐标正交投影与背面遮挡剔除
        if (!globe.projectLatLng(place.latitude, place.longitude, sx, sy, depth)) {
            continue; // 背面遮挡或视野外，剔除
        }

        // 严格剔除背向及极边缘地平线遮挡
        if (depth <= 0.05f) {
            continue;
        }

        bool is_selected = ((int)i == selected_idx);
        uint16_t col = place.theme_color;

        // 1. 动态发光呼吸外环 (Pulse Glow Ring)
        if (is_selected) {
            // 选中项拥有更醒目的双层脉冲光环
            canvas.drawCircle(sx, sy, (int32_t)pulse_radius, col);
            canvas.drawCircle(sx, sy, (int32_t)(pulse_radius + 2), 0xFFFF);
        } else {
            // 未选中项根据呼吸相位与视角深度绘制轻柔呼吸环
            if (depth > 0.2f) {
                canvas.drawCircle(sx, sy, (int32_t)pulse_radius, col);
            }
        }

        // 2. 中心发光核心 (Inner Core)
        canvas.fillCircle(sx, sy, 2, col);
        canvas.drawPixel(sx, sy, 0xFFFF); // 纯白高光微核心

        // 3. 选中态 HUD 瞄准准心框 (Targeting Brackets)
        if (is_selected) {
            int16_t b_len = 3;
            int16_t b_dist = (int16_t)(pulse_radius + 4);

            // 左上角
            canvas.drawLine(sx - b_dist, sy - b_dist, sx - b_dist + b_len, sy - b_dist, 0xFFFF);
            canvas.drawLine(sx - b_dist, sy - b_dist, sx - b_dist, sy - b_dist + b_len, 0xFFFF);
            // 右上角
            canvas.drawLine(sx + b_dist, sy - b_dist, sx + b_dist - b_len, sy - b_dist, 0xFFFF);
            canvas.drawLine(sx + b_dist, sy - b_dist, sx + b_dist, sy - b_dist + b_len, 0xFFFF);
            // 左下角
            canvas.drawLine(sx - b_dist, sy + b_dist, sx - b_dist + b_len, sy + b_dist, 0xFFFF);
            canvas.drawLine(sx - b_dist, sy + b_dist, sx - b_dist, sy + b_dist - b_len, 0xFFFF);
            // 右下角
            canvas.drawLine(sx + b_dist, sy + b_dist, sx + b_dist - b_len, sy + b_dist, 0xFFFF);
            canvas.drawLine(sx + b_dist, sy + b_dist, sx + b_dist, sy + b_dist - b_len, 0xFFFF);

            // 标注地点英文简称标签
            canvas.setTextColor(0xFFFF);
            canvas.setTextDatum(middle_left);
            canvas.setTextSize(1);
            canvas.drawString(place.name, sx + b_dist + 4, sy);
        } else {
            // 非选中状态下，若正视角度良好（depth > 0.65），显示半透明微字
            if (depth > 0.65f) {
                canvas.setTextColor(col);
                canvas.setTextDatum(middle_left);
                canvas.setTextSize(1);
                canvas.drawString(place.name, sx + 6, sy);
            }
        }
    }
}

// -------------------------------------------------------------
// 触控命中检测 (Hit Test)
// -------------------------------------------------------------
int PlaceMarkerManager::hitTest(int16_t screen_touch_x, int16_t screen_touch_y, const GlobeEngine& globe, float max_radius_px) {
    // 转换为 Canvas 局部坐标系
    int16_t local_x = screen_touch_x - GLOBE_CANVAS_OFFSET_X;
    int16_t local_y = screen_touch_y - GLOBE_CANVAS_OFFSET_Y;

    // 检查是否在视口范围内 (466x320 全宽画布)
    if (local_x < 0 || local_x >= GLOBE_CANVAS_WIDTH || local_y < 0 || local_y >= GLOBE_CANVAS_HEIGHT) {
        return -1;
    }

    float best_dist_sq = max_radius_px * max_radius_px;
    int hit_index = -1;

    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        const PlaceLocation& place = GLOBAL_PLACES[i];
        int16_t sx, sy;
        float depth;

        if (!globe.projectLatLng(place.latitude, place.longitude, sx, sy, depth)) {
            continue;
        }

        // 背面不可见
        if (depth <= 0.05f) {
            continue;
        }

        float dx = (float)(local_x - sx);
        float dy = (float)(local_y - sy);
        float dist_sq = dx * dx + dy * dy;

        if (dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            hit_index = (int)i;
        }
    }

    return hit_index;
}
