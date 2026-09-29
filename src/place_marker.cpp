#include "place_marker.h"
#include "config.h"
#include <cmath>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

#pragma GCC optimize ("O3,fast-math")

// -------------------------------------------------------------
// 全球代表性地标与公开相机点位数据库 (覆盖各大洲、火山、名山、都会)
// -------------------------------------------------------------
extern const PlaceLocation DEFAULT_GLOBAL_PLACES[] = {
    {
        "hk-victoria-peak",
        "Victoria Peak",
        "太平山顶 (维港全景)",
        "Hong Kong",
        "Hong Kong Island",
        22.2759f,
        114.1455f,
        552,
        8,
        "mountain",
        0x47E0,
        "Panoramic view of world-famous Victoria Harbour skyline from the summit of Victoria Peak.",
        "Victoria Harbour panoramic skyline & financial district",
        "The Peak Summit East",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/vpa/latest_VPA.jpg",
        1,
        true
    },
    {
        "hk-central-pier",
        "Central Pier",
        "中环码头 (维多利亚港)",
        "Hong Kong",
        "Hong Kong Island",
        22.2871f,
        114.1602f,
        8,
        8,
        "city",
        0x07FF,
        "Bustling central waterfront overlooking Star Ferry and ICC skyscraper across the harbour.",
        "Star Ferry routes & Kowloon ICC skyscraper",
        "Central Pier Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/cp1/latest_CP1.jpg",
        1,
        true
    },
    {
        "hk-victoria-dnl",
        "SE Kowloon",
        "东南九龙 (启德邮轮码头)",
        "Hong Kong",
        "Kowloon",
        22.3080f,
        114.1950f,
        35,
        8,
        "city",
        0x3E7F,
        "View over Victoria Harbour shipping channel from Southeast Kowloon.",
        "Harbour fairway traffic & Hong Kong Island east hills",
        "SE Kowloon Fairway",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/dnl/latest_DNL.jpg",
        1,
        true
    },
    {
        "hk-sha-lo-wan",
        "Sha Lo Wan",
        "沙螺湾 (香港国际机场)",
        "Hong Kong",
        "Lantau Island",
        22.2917f,
        113.9011f,
        15,
        8,
        "coast",
        0x07FF,
        "Scenic coastal spot on Lantau overlooking Hong Kong International Airport south runway.",
        "Aircraft taking off against open Pearl River estuary",
        "HKIA South Runway Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/slw/latest_SLW.jpg",
        1,
        true
    },
    {
        "hk-tai-mo-shan",
        "Tai Mo Shan",
        "大帽山 (高山云海)",
        "Hong Kong",
        "New Territories",
        22.4116f,
        114.1234f,
        957,
        8,
        "mountain",
        0x7BEF,
        "Hong Kong's highest mountain peak frequently surrounded by an ethereal sea of clouds.",
        "Rolling sea of clouds over New Territories mountain ranges",
        "Tai Mo Shan Weather Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/tm2/latest_TM2.jpg",
        1,
        true
    },
    {
        "hk-waglan-island",
        "Waglan Island",
        "横澜岛灯塔 (南海前哨)",
        "Hong Kong",
        "Outlying Islands",
        22.1833f,
        114.3014f,
        58,
        8,
        "coast",
        0x0989,
        "Remote southeastern outpost island featuring a century-old historic Victorian lighthouse.",
        "Open South China Sea swells & historic lighthouse",
        "Waglan Lighthouse Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/wgl/latest_WGL.jpg",
        1,
        true
    },
    {
        "hk-cheung-chau",
        "Cheung Chau",
        "长洲岛",
        "Hong Kong",
        "Outlying Islands",
        22.2080f,
        114.0290f,
        18,
        8,
        "coast",
        0x3E7F,
        "Idyllic dumbbell-shaped island known for traditional fishing culture and sandy beaches.",
        "Sheltered fishing junks & South China Sea horizon",
        "Cheung Chau Harbour Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/cch/latest_CCH.jpg",
        1,
        true
    },
    {
        "hk-lamma-island",
        "Lamma Island",
        "南丫岛 (风力发电站)",
        "Hong Kong",
        "Outlying Islands",
        22.2260f,
        114.1160f,
        30,
        8,
        "nature",
        0x2668,
        "Lush bohemian island with tranquil bays and coastal hiking trails.",
        "Verdant coastal hills & open sea channels",
        "Lamma Channel Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/lam/latest_LAM.jpg",
        1,
        true
    },
    {
        "hk-wetland-park",
        "Wetland Park",
        "香港湿地公园",
        "Hong Kong",
        "New Territories",
        22.4680f,
        114.0080f,
        5,
        8,
        "nature",
        0x2668,
        "Ecological conservation area home to migratory waterbirds and mangroves.",
        "Reflective marshlands & Shenzhen skyline distant backdrop",
        "Wetland Reserve Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/wlp/latest_WLP.jpg",
        1,
        true
    },
    {
        "hk-kau-sai-chau",
        "Kau Sai Chau",
        "滘西洲 (西贡地质公园)",
        "Hong Kong",
        "New Territories",
        22.3680f,
        114.3160f,
        45,
        8,
        "nature",
        0x2668,
        "Scenic island in Sai Kung waters featuring rolling coastal green terrain and turquoise coves.",
        "Sai Kung geopark waters & coastal islands",
        "Kau Sai Chau Marine Cam",
        "Hong Kong Observatory",
        "https://www.hko.gov.hk/wxinfo/aws/hko_mica/ks2/latest_KS2.jpg",
        1,
        true
    },
    {
        "jp-mt-fuji-tarobo",
        "Mount Fuji (Tarobo)",
        "富士山 (太郎坊观测所)",
        "Japan",
        "Shizuoka",
        35.3350f,
        138.7880f,
        1300,
        9,
        "mountain",
        0x47E0,
        "Direct unobstructed vista of Mount Fuji's southeastern volcanic slope and summit crater rim.",
        "Snowy conical crater & volcanic ash ridgelines",
        "Tarobo Observatory Cam",
        "Ministry of Land, Infrastructure, Transport and Tourism (MLIT)",
        "https://www.cbr.mlit.go.jp/fujisabo/camera/images/00023.jpg",
        1,
        true
    },
    {
        "jp-mt-fuji-fujinomiya",
        "Mount Fuji (Fujinomiya)",
        "富士山 (富士宫市远眺)",
        "Japan",
        "Shizuoka",
        35.2280f,
        138.6180f,
        150,
        9,
        "mountain",
        0x07FF,
        "Spectacular classic view of Mount Fuji towering over the historic shrines of Fujinomiya.",
        "Symmetrical snow cap towering over Shizuoka",
        "Fujinomiya City View",
        "Ministry of Land, Infrastructure, Transport and Tourism (MLIT)",
        "https://www.cbr.mlit.go.jp/fujisabo/camera/images/00026.jpg",
        1,
        true
    },
    {
        "jp-mt-fuji-fujisawa",
        "Mount Fuji (Obuchi)",
        "富士山 (大渊观测点)",
        "Japan",
        "Shizuoka",
        35.1950f,
        138.6750f,
        310,
        9,
        "mountain",
        0x47E0,
        "Panoramic southwest view of Mount Fuji across tea plantations and foothill forests.",
        "Iconic snow cap with foreground foothill forests",
        "Obuchi Station Cam",
        "Ministry of Land, Infrastructure, Transport and Tourism (MLIT)",
        "https://www.cbr.mlit.go.jp/fujisabo/camera/images/00024.jpg",
        1,
        true
    },
    {
        "de-zugspitze",
        "Zugspitze",
        "楚格峰 (德国最高峰 2962m)",
        "Germany",
        "Bavaria",
        47.4211f,
        10.9853f,
        2962,
        1,
        "mountain",
        0x9FFF,
        "Highest peak in Germany standing at 2,962m above sea level with glacier panoramic views.",
        "High Alpine glacier snowfield & endless sea of peaks",
        "Zugspitze South Ridge",
        "Foto-Webcam.eu",
        "https://www.foto-webcam.eu/webcam/zugspitze-sued/current/400.jpg",
        1,
        true
    },
    {
        "de-neuschwanstein",
        "Neuschwanstein",
        "新天鹅堡 (阿尔卑斯山麓)",
        "Germany",
        "Bavaria",
        47.5576f,
        10.7498f,
        1730,
        1,
        "culture",
        0xFEA0,
        "Iconic 19th-century historicist palace nestled against the Bavarian Alps backdrop.",
        "Tegelberg peak & fairy-tale castle foothills",
        "Tegelberg Alpine Cam",
        "Foto-Webcam.eu",
        "https://www.foto-webcam.eu/webcam/tegelberg/current/400.jpg",
        1,
        true
    },
    {
        "de-koenigssee",
        "Lake Königssee",
        "国王湖 (贝希特斯加登)",
        "Germany",
        "Bavaria",
        47.5689f,
        12.9833f,
        1874,
        1,
        "nature",
        0x0989,
        "Emerald-green fjord-like alpine lake beneath the sheer eastern rock wall of Mount Watzmann.",
        "Emerald waters & vertical limestone precipice",
        "Jennerbahn Peak Cam",
        "Foto-Webcam.eu",
        "https://www.foto-webcam.eu/webcam/jenner/current/400.jpg",
        1,
        true
    },
    {
        "at-innsbruck",
        "Innsbruck Alps",
        "因斯布鲁克 (阿尔卑斯之心)",
        "Austria",
        "Tyrol",
        47.2692f,
        11.4041f,
        850,
        1,
        "city",
        0x07FF,
        "Capital of Tyrol enclosed by dramatic 2,000m limestone mountain walls of the Nordkette.",
        "Tyrolean valley cityscape beneath vertical alpine walls",
        "Innsbruck Valley Cam",
        "Foto-Webcam.eu",
        "https://www.foto-webcam.eu/webcam/innsbruck/current/400.jpg",
        1,
        true
    },
    {
        "at-zell-am-see",
        "Zell am See",
        "滨湖采尔 (高山明珠)",
        "Austria",
        "Salzburg",
        47.3235f,
        12.7968f,
        750,
        1,
        "nature",
        0x3E7F,
        "Glittering crystal-clear alpine lake surrounded by snow-dusted Austrian peaks.",
        "Mirror lake surface reflecting surrounding summits",
        "Zeller See Panorama",
        "Foto-Webcam.eu",
        "https://www.foto-webcam.eu/webcam/zellamsee/current/400.jpg",
        1,
        true
    },
    {
        "de-wank-mountain",
        "Wank Mountain",
        "旺克峰 (全景看台)",
        "Germany",
        "Bavaria",
        47.5097f,
        11.1444f,
        1780,
        1,
        "mountain",
        0x47E0,
        "Sunny panoramic balcony overlooking Garmisch-Partenkirchen and the massive Alpspitz.",
        "360-degree panorama of the Wetterstein range",
        "Wankhaus Summit View",
        "Foto-Webcam.eu",
        "https://www.foto-webcam.eu/webcam/wank/current/400.jpg",
        1,
        true
    },
    {
        "us-yellowstone-washburn",
        "Mount Washburn",
        "黄石国家公园 (沃什伯恩峰)",
        "United States",
        "Wyoming",
        44.7978f,
        -110.4344f,
        3122,
        -7,
        "nature",
        0xFEA0,
        "Panoramic high-elevation fire lookout summit overlooking the vast Yellowstone Caldera basin.",
        "Endless pine forests & steaming geothermal basin",
        "Washburn Summit Cam",
        "National Park Service (NPS)",
        "https://www.nps.gov/webcams-yell/washburn_sw.jpg",
        1,
        true
    },
    {
        "us-yellowstone-mammoth",
        "Mammoth Hot Springs",
        "黄石国家公园 (猛犸温泉)",
        "United States",
        "Wyoming",
        44.9769f,
        -110.7011f,
        2050,
        -7,
        "nature",
        0xFEA0,
        "Ancient geothermal travertine terraces with Electric Peak towering in the backdrop.",
        "Travertine limestone terraces & snow-capped peaks",
        "Electric Peak View",
        "National Park Service (NPS)",
        "https://www.nps.gov/webcams-yell/mammoth_electric.jpg",
        1,
        true
    },
    {
        "us-hawaii-kilauea",
        "Kilauea Volcano",
        "夏威夷基拉韦厄活火山",
        "United States",
        "Hawaii",
        19.4069f,
        -155.2834f,
        1222,
        -10,
        "nature",
        0xF800,
        "One of Earth's most active shield volcanoes, looking directly into Halemaʻumaʻu caldera floor.",
        "Lava lake caldera floor & active volcanic degassing",
        "KWcam Halemaumau",
        "USGS Hawaiian Volcano Observatory",
        "https://volcanoes.usgs.gov/observatories/hvo/cams/KWcam/images/M.jpg",
        1,
        true
    },
};

extern const size_t DEFAULT_GLOBAL_PLACES_COUNT = sizeof(DEFAULT_GLOBAL_PLACES) / sizeof(DEFAULT_GLOBAL_PLACES[0]);

// 活跃地点指针，默认指向 ROM 硬编码数组
const PlaceLocation* GLOBAL_PLACES = DEFAULT_GLOBAL_PLACES;
size_t GLOBAL_PLACES_COUNT = DEFAULT_GLOBAL_PLACES_COUNT;

PlaceMarkerManager place_mgr;

static char* psram_strdup(const char* str) {
    if (!str) return nullptr;
    size_t len = strlen(str) + 1;
    char* p = (char*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) memcpy(p, str, len);
    return p;
}

// -------------------------------------------------------------
// PlaceMarkerManager Implementation
// -------------------------------------------------------------
PlaceMarkerManager::PlaceMarkerManager()
    : pulse_phase(0.0f),
      selected_idx(0),
      last_anim_ms(0),
      is_dynamic(false),
      dynamic_places(nullptr),
      dynamic_count(0),
      dynamic_str_pool(nullptr)
{
}

PlaceMarkerManager::~PlaceMarkerManager() {
    freeDynamicMemory();
}

void PlaceMarkerManager::freeDynamicMemory() {
    if (dynamic_places) {
        for (size_t i = 0; i < dynamic_count; i++) {
            if (dynamic_places[i].id) free((void*)dynamic_places[i].id);
            if (dynamic_places[i].name) free((void*)dynamic_places[i].name);
            if (dynamic_places[i].local_name) free((void*)dynamic_places[i].local_name);
            if (dynamic_places[i].country) free((void*)dynamic_places[i].country);
            if (dynamic_places[i].region) free((void*)dynamic_places[i].region);
            if (dynamic_places[i].category) free((void*)dynamic_places[i].category);
            if (dynamic_places[i].description) free((void*)dynamic_places[i].description);
            if (dynamic_places[i].highlight) free((void*)dynamic_places[i].highlight);
            if (dynamic_places[i].cam_name) free((void*)dynamic_places[i].cam_name);
            if (dynamic_places[i].cam_source) free((void*)dynamic_places[i].cam_source);
            if (dynamic_places[i].snapshot_url) free((void*)dynamic_places[i].snapshot_url);
        }
        free(dynamic_places);
        dynamic_places = nullptr;
    }
    dynamic_count = 0;
    is_dynamic = false;
}

void PlaceMarkerManager::resetToDefault() {
    freeDynamicMemory();
    GLOBAL_PLACES = DEFAULT_GLOBAL_PLACES;
    GLOBAL_PLACES_COUNT = DEFAULT_GLOBAL_PLACES_COUNT;
    if (selected_idx >= (int)GLOBAL_PLACES_COUNT) {
        selected_idx = 0;
    }
    Serial.println("[PlaceMarker] Reset to firmware default places.");
}

bool PlaceMarkerManager::loadFromJson(const char* json_str, size_t json_len) {
    if (!json_str || json_len == 0) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json_str, json_len);
    if (error) {
        Serial.printf("[PlaceMarker] JSON parse failed: %s\n", error.c_str());
        return false;
    }

    if (!doc.is<JsonArray>()) {
        Serial.println("[PlaceMarker] JSON root is not an array!");
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    size_t count = arr.size();
    if (count == 0) {
        Serial.println("[PlaceMarker] JSON array is empty!");
        return false;
    }

    PlaceLocation* new_places = (PlaceLocation*)heap_caps_malloc(
        count * sizeof(PlaceLocation), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (!new_places) {
        Serial.println("[PlaceMarker] Failed to allocate memory in PSRAM for dynamic places!");
        return false;
    }
    memset(new_places, 0, count * sizeof(PlaceLocation));

    for (size_t i = 0; i < count; i++) {
        JsonObject obj = arr[i];
        new_places[i].id = psram_strdup(obj["id"] | "");
        new_places[i].name = psram_strdup(obj["name"] | "");
        new_places[i].local_name = psram_strdup(obj["local_name"] | obj["name"] | "");
        new_places[i].country = psram_strdup(obj["country"] | obj["cntry"] | "");
        new_places[i].region = psram_strdup(obj["region"] | obj["reg"] | "");
        new_places[i].latitude = obj["latitude"] | obj["lat"] | 0.0f;
        new_places[i].longitude = obj["longitude"] | obj["lon"] | 0.0f;
        new_places[i].elevation_m = obj["elevation_m"] | obj["elev"] | 0;
        new_places[i].utc_offset = obj["utc_offset"] | obj["utc"] | 0;
        new_places[i].category = psram_strdup(obj["category"] | obj["cat"] | "nature");

        if (obj["col"].is<const char*>()) {
            new_places[i].theme_color = (uint16_t)strtoul(obj["col"].as<const char*>(), NULL, 0);
        } else {
            new_places[i].theme_color = obj["color"] | obj["col"] | 0x07FF;
        }

        new_places[i].description = psram_strdup(obj["description"] | obj["desc"] | "");
        new_places[i].highlight = psram_strdup(obj["highlight"] | obj["hl"] | "");
        new_places[i].cam_name = psram_strdup(obj["cam_name"] | obj["cam"] | "");
        new_places[i].cam_source = psram_strdup(obj["cam_source"] | obj["src"] | "");
        new_places[i].snapshot_url = psram_strdup(obj["snapshot_url"] | obj["url"] | "");
        new_places[i].cam_count = obj["cam_count"] | 1;
        new_places[i].is_online = obj["is_online"] | obj["live"] | false;
    }

    // 原子安全切换全局指针
    freeDynamicMemory();
    dynamic_places = new_places;
    dynamic_count = count;
    is_dynamic = true;

    GLOBAL_PLACES = dynamic_places;
    GLOBAL_PLACES_COUNT = dynamic_count;
    if (selected_idx >= (int)GLOBAL_PLACES_COUNT) {
        selected_idx = 0;
    }

    Serial.printf("[PlaceMarker] Successfully loaded %u dynamic places into PSRAM!\n", (uint32_t)GLOBAL_PLACES_COUNT);
    return true;
}

bool PlaceMarkerManager::loadFromLittleFS(const char* filepath) {
    if (!LittleFS.exists(filepath)) {
        Serial.printf("[PlaceMarker] LittleFS file not found: %s, using default places.\n", filepath);
        resetToDefault();
        return false;
    }

    File f = LittleFS.open(filepath, "r");
    if (!f) {
        Serial.printf("[PlaceMarker] Failed to open %s\n", filepath);
        resetToDefault();
        return false;
    }

    size_t fsize = f.size();
    if (fsize == 0 || fsize > 256 * 1024) {
        Serial.printf("[PlaceMarker] Invalid file size: %u bytes\n", (uint32_t)fsize);
        f.close();
        resetToDefault();
        return false;
    }

    char* buf = (char*)heap_caps_malloc(fsize + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        Serial.println("[PlaceMarker] PSRAM allocation failed for loading places.json");
        f.close();
        resetToDefault();
        return false;
    }

    size_t read_bytes = f.readBytes(buf, fsize);
    buf[read_bytes] = '\0';
    f.close();

    bool ok = loadFromJson(buf, read_bytes);
    free(buf);

    if (!ok) {
        resetToDefault();
    }
    return ok;
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
// -------------------------------------------------------------
// Phase 3 & Phase 4: 渲染所有正面可见 Marker 并执行动态呼吸光圈
// -------------------------------------------------------------
void PlaceMarkerManager::renderMarkers(M5Canvas& canvas, const GlobeEngine& globe) {
    // 呼吸动态扩散热度：3.0px ~ 6.5px
    float pulse_sine = 0.5f + 0.5f * sinf(pulse_phase);
    float pulse_radius = 3.0f + 3.5f * pulse_sine;

    int selected_in_view = -1;
    int16_t sel_sx = 0, sel_sy = 0;
    float sel_depth = 0.0f;

    // --- 第一遍：渲染所有未选中的微光星标（避免文字重叠，保持地球仪纯净视觉）---
    for (size_t i = 0; i < GLOBAL_PLACES_COUNT; i++) {
        const PlaceLocation& place = GLOBAL_PLACES[i];

        int16_t sx, sy;
        float depth;

        // 球极坐标正交投影与背面遮挡剔除
        if (!globe.projectLatLng(place.latitude, place.longitude, sx, sy, depth)) {
            continue; // 背面遮挡或视野外，剔除
        }

        // 严格剔除背向及极边缘地平线遮挡
        if (depth <= 0.05f) {
            continue;
        }

        if ((int)i == selected_idx) {
            // 记录选中点，留到顶层第二遍绘制
            selected_in_view = (int)i;
            sel_sx = sx;
            sel_sy = sy;
            sel_depth = depth;
            continue;
        }

        uint16_t col = place.theme_color;

        if (place.is_online) {
            // 实时在线摄像头：宇宙级醒目电光绿雷达波纹与发光卫星核心
            int32_t live_r = (int32_t)(pulse_radius + 2.0f);
            canvas.drawCircle(sx, sy, live_r, 0x07E0);
            canvas.drawCircle(sx, sy, live_r + 2, 0x03E0);
            canvas.fillCircle(sx, sy, 3, 0x07E0);
            canvas.drawPixel(sx, sy, 0xFFFF);
            if (depth > 0.40f) {
                canvas.setTextDatum(middle_left);
                canvas.setTextSize(1);
                canvas.setTextColor(0x07E0);
                canvas.drawString("LIVE", sx + live_r + 4, sy);
            }
        } else if (depth > 0.45f) {
            // 正对视野的普通地标：微弱柔和呼吸光晕
            canvas.drawCircle(sx, sy, (int32_t)pulse_radius, col);
            canvas.fillCircle(sx, sy, 2, col);
            canvas.drawPixel(sx, sy, 0xFFFF); // 纯白高光微核心
        } else {
            // 边缘侧视点：自然淡化为 1px/2px 纯净星点，增强三维球体景深
            canvas.fillCircle(sx, sy, 1, col);
        }
    }

    // --- 第二遍：置顶渲染当前选中焦点的 HUD 准心与防遮挡胶囊名牌 ---
    if (selected_in_view >= 0) {
        const PlaceLocation& sel_place = GLOBAL_PLACES[selected_in_view];
        uint16_t col = sel_place.is_online ? 0x07E0 : sel_place.theme_color;

        // 1. 选中项醒目的双层脉冲光环
        canvas.drawCircle(sel_sx, sel_sy, (int32_t)(pulse_radius + 1), col);
        canvas.drawCircle(sel_sx, sel_sy, (int32_t)(pulse_radius + 3), 0xFFFF);
        canvas.fillCircle(sel_sx, sel_sy, 3, col);
        canvas.drawPixel(sel_sx, sel_sy, 0xFFFF);

        // 2. 选中态 HUD 瞄准准心框 (Targeting Brackets)
        int16_t b_len = 3;
        int16_t b_dist = (int16_t)(pulse_radius + 5);

        // 左上角
        canvas.drawLine(sel_sx - b_dist, sel_sy - b_dist, sel_sx - b_dist + b_len, sel_sy - b_dist, 0xFFFF);
        canvas.drawLine(sel_sx - b_dist, sel_sy - b_dist, sel_sx - b_dist, sel_sy - b_dist + b_len, 0xFFFF);
        // 右上角
        canvas.drawLine(sel_sx + b_dist, sel_sy - b_dist, sel_sx + b_dist - b_len, sel_sy - b_dist, 0xFFFF);
        canvas.drawLine(sel_sx + b_dist, sel_sy - b_dist, sel_sx + b_dist, sel_sy - b_dist + b_len, 0xFFFF);
        // 左下角
        canvas.drawLine(sel_sx - b_dist, sel_sy + b_dist, sel_sx - b_dist + b_len, sel_sy + b_dist, 0xFFFF);
        canvas.drawLine(sel_sx - b_dist, sel_sy + b_dist, sel_sx - b_dist, sel_sy + b_dist - b_len, 0xFFFF);
        // 右下角
        canvas.drawLine(sel_sx + b_dist, sel_sy + b_dist, sel_sx + b_dist - b_len, sel_sy + b_dist, 0xFFFF);
        canvas.drawLine(sel_sx + b_dist, sel_sy + b_dist, sel_sx + b_dist, sel_sy + b_dist - b_len, 0xFFFF);

        // 3. 科技防遮挡圆角胶囊名牌 (Smart Pill Badge)
        canvas.setTextDatum(middle_left);
        canvas.setTextSize(1);
        int text_w = canvas.textWidth(sel_place.name) + (sel_place.is_online ? 40 : 0);
        int pill_w = text_w + 14;
        int pill_h = 16;

        // 智能边界避让：若右侧超出画布则翻转至左侧
        bool flip_left = (sel_sx + b_dist + 4 + pill_w > GLOBE_CANVAS_WIDTH - 6);
        int16_t pill_x = flip_left ? (sel_sx - b_dist - 4 - pill_w) : (sel_sx + b_dist + 4);
        int16_t pill_y = sel_sy - (pill_h / 2);

        // 深黑半透明底板 + 霓虹主题色边框
        canvas.fillRoundRect(pill_x, pill_y, pill_w, pill_h, 3, 0x0841); // 深沉科技蓝黑
        canvas.drawRoundRect(pill_x, pill_y, pill_w, pill_h, 3, col);    // 主题色微光边框

        // 胶囊内文字与 LIVE 状态
        if (sel_place.is_online) {
            canvas.setTextDatum(middle_left);
            canvas.setTextColor(0x07E0);
            canvas.drawString("*LIVE", pill_x + 6, sel_sy);
            canvas.setTextColor(0xFFFF);
            canvas.drawString(sel_place.name, pill_x + 44, sel_sy);
        } else {
            canvas.setTextColor(0xFFFF);
            canvas.setTextDatum(middle_center);
            canvas.drawString(sel_place.name, pill_x + (pill_w / 2), sel_sy);
        }
    }
}

// -------------------------------------------------------------
// 触控命中检测 (Hit Test) - 磁吸式就近加权判定
// -------------------------------------------------------------
int PlaceMarkerManager::hitTest(int16_t screen_touch_x, int16_t screen_touch_y, const GlobeEngine& globe, float max_radius_px) {
    // 转换为 Canvas 局部坐标系
    int16_t local_x = screen_touch_x - GLOBE_CANVAS_OFFSET_X;
    int16_t local_y = screen_touch_y - GLOBE_CANVAS_OFFSET_Y;

    // 检查是否在视口范围内 (466x320 全宽画布)
    if (local_x < 0 || local_x >= GLOBE_CANVAS_WIDTH || local_y < 0 || local_y >= GLOBE_CANVAS_HEIGHT) {
        return -1;
    }

    // 扩大手指点击感应半径至 38px
    float effective_radius = (max_radius_px < 38.0f) ? 38.0f : max_radius_px;
    float max_dist_sq = effective_radius * effective_radius;

    float best_score = 999999.0f;
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

        if (dist_sq <= max_dist_sq) {
            // 空间深度磁吸算法：正面(depth接近1.0)得分优于边缘(depth较小)，避免边缘歪点抢先吸附
            float score = dist_sq / (depth * depth);
            if (score < best_score) {
                best_score = score;
                hit_index = (int)i;
            }
        }
    }

    return hit_index;
}
