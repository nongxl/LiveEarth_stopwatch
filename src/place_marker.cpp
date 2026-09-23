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
        35.3606f,
        138.7274f,
        "mountain",
        0x47E0 // 翡翠冰雪绿
    },
    {
        "us-kilauea-volcano",
        "Kilauea Volcano",
        "基拉韦厄火山",
        "USA Hawaii",
        19.4069f,
        -155.2833f,
        "volcano",
        0xFA20 // 炽热熔岩橙红
    },
    {
        "us-times-square",
        "Times Square",
        "时代广场",
        "USA NYC",
        40.7580f,
        -73.9855f,
        "city",
        0xF81F // 繁华霓虹洋红
    },
    {
        "cn-shanghai-bund",
        "The Bund",
        "上海外滩",
        "China",
        31.2397f,
        121.4900f,
        "city",
        0x07FF // 东方明珠科技青
    },
    {
        "fr-paris-eiffel",
        "Eiffel Tower",
        "埃菲尔铁塔",
        "France",
        48.8584f,
        2.2945f,
        "landmark",
        0xFFE0 // 浪漫香槟金
    },
    {
        "uk-greenwich",
        "Greenwich Obs",
        "格林威治天文台",
        "UK London",
        51.4769f,
        -0.0005f,
        "observatory",
        0xFE60 // 0度经线皇家琥珀
    },
    {
        "au-sydney-opera",
        "Sydney Opera",
        "悉尼歌剧院",
        "Australia",
        -33.8568f,
        151.2153f,
        "landmark",
        0x3E7F // 南太平洋明蓝
    },
    {
        "eg-giza-pyramids",
        "Pyramids of Giza",
        "吉萨金字塔",
        "Egypt",
        29.9792f,
        31.1342f,
        "landmark",
        0xFDC0 // 撒哈拉金沙
    },
    {
        "br-rio-christ",
        "Christ Redeemer",
        "里约基督像",
        "Brazil",
        -22.9519f,
        -43.2105f,
        "landmark",
        0x7FE0 // 亚马逊雨林翡翠
    },
    {
        "is-reykjavik",
        "Reykjavik",
        "雷克雅未克",
        "Iceland",
        64.1466f,
        -21.9426f,
        "landmark",
        0x57FE // 北极极光蓝
    },
    {
        "za-cape-hope",
        "Cape of Good Hope",
        "好望角",
        "South Africa",
        -34.3568f,
        18.4740f,
        "landmark",
        0x257F // 两洋交汇深海蓝
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

    // 检查是否在视口范围内
    if (local_x < 0 || local_x >= GLOBE_CANVAS_SIZE || local_y < 0 || local_y >= GLOBE_CANVAS_SIZE) {
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
