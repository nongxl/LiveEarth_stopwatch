# LiveEarth Technical Risks & Architecture Decisions

本文件按照《docs/需求.md》第 25 条（IMPORTANT DEVELOPMENT RULE）规范维护，记录系统开发中遇到的关键技术不确定性、影响、候选方案、当前方案及验证状态。

---

## 1. SSL/TLS 握手耗时与 PSRAM 内存开销

- **问题描述**：
  ESP32-S3 访问现代 HTTPS 摄像头/快照源时，每个 TLS 握手（RSA 2048/4096、ECC 证书链）需要消耗 35KB~65KB 动态堆内存，且单次握手在 ESP32 上耗时约 800ms~2200ms。若在主渲染线程发起，会导致界面严重丢帧或卡死。
- **潜在影响**：
  若频繁切换地点或短周期轮询，SRAM 极易碎裂；网络延迟直接影响 50FPS 触控渲染流畅度。
- **候选方案**：
  1. 方案 A：在主线程直接调用 HTTPClient（已证实不可行，会卡死屏幕刷新）；
  2. 方案 B：专属 FreeRTOS 网络下载任务 + PSRAM 堆缓冲 + 双缓冲 Canvas（当前方案）；
  3. 方案 C：自建边缘反向代理，将 HTTPS 卸载为轻量 HTTP（需外部服务器支持，不符合无服务器单机运行初衷）。
- **当前方案**：
  - 采用 **方案 B**：利用 `xTaskCreatePinnedToCore` 将 TLS 下载任务固定在 Core 0，UI 与 3D 渲染固定在 Core 1；
  - `stb_image` 解码缓冲使用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` 全部分配在 8MB PSRAM；
  - 采用一次解码入 Canvas 双缓冲机制（Preview 240x135 / Fullscreen 406x228），重绘时直接从 PSRAM blit，维持 50+ FPS。
- **验证状态**：
  - [x] USGS 基拉韦厄火山 HTTPS 快照在 Core 0 异步抓帧成功，主屏 3D 地球仪旋转丝滑无卡顿。

---

## 2. 真实公开 Webcam 的授权与可重发布许可边界

- **问题描述**：
  “公开网页上可见”（Publicly Viewable）不等于“允许在第三方设备或独立硬件中抓帧再分发”（Redistribution Permitted）。第三方摄像头聚合平台（如 Windy、WebcamTaxi 等）可能附带专有水印、防盗链 Referer 检查或封锁外部 User-Agent。
- **潜在影响**：
  直接拉取未经许可的商业流存在合规与版权风险；未经鉴权的 IP Camera 存在隐私侵犯风险。
- **候选方案**：
  1. 方案 A：爬取第三方聚合站流（严令禁止，违反项目哲学第 2 条）；
  2. 方案 B：建立合规分级体系（Tier A 官方政府/科研/国家公园优先，其余入 Review Queue 人工审核）；
  3. 方案 C：未通过审核的地点全部显示为假图（违反项目哲学第 0 条，已彻底废除）。
- **当前方案**：
  - 严格执行 **PLACE FIRST, CAMERA SECOND**；
  - 固件先收录权威验证的 100 处地标；已确认 Public Domain 许可的源（如 USGS）标为 `is_online: true`；
  - 其余流未明确签署或取得开放授权前，统一标为 `is_online: false`，UI 呈现 **Official Offline Dossier（官方典藏档案卡）**，候选源全部推入 `data/review_queue.json` 进行审查。
- **验证状态**：
  - [x] 100 个地点已入库，未授权流诚实进入审核队列，杜绝假图。

---

## 3. JPEG 硬件与软件解码尺寸与 PSRAM 溢出风险

- **问题描述**：
  部分官方气象/火山摄像头输出的原始图像为 1080P 或 4K 超高清 JPEG。若直接解码整张 4K 图片，解压后的 RGB888 位图需要 3840×2160×3 ≈ 24.8MB 内存，将瞬间耗尽 8MB PSRAM 导致 OOM 重启。
- **潜在影响**：
  设备在尝试载入特定高清摄像头时触发 ESP-IDF Guru Meditation 崩溃重启。
- **候选方案**：
  1. 方案 A：完整载入解码（不可行，超出 8MB PSRAM 物理上限）；
  2. 方案 B：限制最大下载字节数，只拉取官方缩略图或使用分块流式解码器；
  3. 方案 C：在 `SnapshotPlayer` 中设置 `SNAPSHOT_MAX_BUF_SIZE = 128KB`，且在下载前检查 Content-Length，仅接受适配尺寸。
- **当前方案**：
  - 接收缓冲硬上限设为 128KB；超过该大小的单张过大快照直接熔断；
  - 优先选用官方提供的 400x225、640x360 或 720P 尺寸的 public snapshot 接口。
- **验证状态**：
  - [x] USGS KWcam 640x480 快照解码稳定，PSRAM 消耗低于 1MB，内存安全。

---

## 4. YouTube Live / HLS 协议在微控制器上的处理局限

- **问题描述**：
  全球大量城市地标（如涩谷十字路口、时代广场）的官方直播采用 YouTube Live 或复杂加密 HLS (m3u8 + fMP4 / TS)。ESP32-S3 无硬件 H.264/H.265 解码器，纯软解 30FPS 视频会导致 CPU 100% 并严重发烫。
- **潜在影响**：
  无法在单机 MCU 上直接解析高码率 HLS 视频流。
- **候选方案**：
  1. 方案 A：在固件中强行编译软解 ffmpeg（算力无法支撑，不可行）；
  2. 方案 B：针对此类地点，优先寻找其官方运维方开放的定期静态快照图（Snapshot HTTP endpoint）；
  3. 方案 C：可选轻量级中间转码网关（Gateway Agent），由服务器将 HLS 抓取关键帧并输出给设备。
- **当前方案**：
  - 目前采用 **方案 B**，在数据管线中优先发掘官方机构的 Snapshot/Timelapse 端点；无法提供直接端点的移入 `review_queue.json`，在固件端保持典藏档案模式。
- **验证状态**：
  - [x] 架构上由 `CameraPlayer` 虚基类提供插件式扩展，UI 与底层解码协议彻底解耦。

---

## 5. M5Stack Stopwatch 触控芯片（CST816S）单点触控硬件限制

- **问题描述**：
  M5Stack Stopwatch (1.75" 466x466 圆屏) 搭载的触控 IC 为 **Hynitron CST816S / CST820**。通过查阅 M5GFX 驱动源码（`Touch_CSTxxx.inl`），该芯片硬件寄存器只上报单点坐标（Single Point），当两个手指同时触摸时，驱动会视作噪点重试甚至丢弃返回 0，因此软件层 `M5.Touch.getCount() >= 2` 永远无法满足。
- **潜在影响**：
  移动端经典的“双指掐捏”（Pinch-to-zoom）在物理硬件上无法被触发。
- **候选方案**：
  1. 方案 A：强行等待多指支持（不可行，硬件芯片物理不支持）；
  2. 方案 B：屏幕右侧边缘弧度滑轨缩放（Edge Arc Zoom Slider）；
  3. 方案 C：双击并按住垂直拖动缩放（Double-Tap-and-Drag，Google Maps 经典单手手势）；
  4. 方案 D：双击地球仪空白处快速在 1.0x 全景与 1.5x 特写之间切换。
- **当前方案**：
  - 综合集成 **方案 B + C + D**，打造专属于 1.75 寸圆形智能手表的单手人机工学交互：
    - **右侧边缘滑轨**：屏幕右侧边缘（X > 395）上下滑动即无级连续缩放（0.65x ~ 2.2x），并有发光弧形标尺和动态倍率徽章；
    - **双击拖拽**：在屏幕任意位置快速双击并不松手，上下拖动即可单手缩放；
    - **双击空白**：轻敲两次快速在 1.0x 与 1.5x 之间平滑切换；
    - 静止状态下在右侧边缘绘制微光小把柄 `[+] [-]`，与左边缘的抽屉把手 `[>]` 左右对称。
- **验证状态**：
  - [x] 固件编译通过，彻底绕过多点触控硬件物理限制，单手单指操作丝滑顺手。
