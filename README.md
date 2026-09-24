# LiveEarth (M5Stack Stopwatch)

> 一个运行在 **M5Stack Stopwatch** 上的互动式全球合法公开 Webcam 3D 地球仪探索固件与数据工程。

---

## 🌍 核心设计理念 (Core Idea)

传统摄像头应用往往是冗长枯燥的列表或简单监控墙。LiveEarth 的核心体验是：
**旋转地球 → 发现地点 → 点击地点 → 查看地点介绍 → 实时进入 Webcam**

- **Place First, Camera Second**：首先关注“这是一个什么样的地方”，其次才是“通过哪一台摄像头观察”。
- **圆形构图设计**：深度适配 M5Stack Stopwatch 的 1.75" 466×466 圆形 AMOLED 屏幕，绝非生硬裁切矩形 UI。
- **离线优先 (Offline-First)**：即使没有连接网络，3D 地球、各大陆轮廓、地理兴趣点、丰富的人文介绍依然完全可用。

---

## ⚖️ 法律与合规准则 (Legal Data Policy)

本项目只收录合法、公开、授权明确的实时网络摄像头：
- 严格禁止：Shodan / Censys 扫描结果、私人 CCTV、默认密码设备、暴露的未授权 RTSP。
- 来源优先级：
  - **Tier A**：政府机关、国家公园、官方旅游局、气象台、天文台、科研机构、机场港口等官方机构。
  - **Tier B**：明确声明为 Public Webcam 的专业运营机构。
  - **Tier C**：具备明确开放数据许可（Open Data License）的来源。
  - **Tier D**：第三方聚合平台仅作为线索发现渠道，必须追溯到官方源并人工审核。
- 权限状态严格分为：`APPROVED`、`REVIEW_REQUIRED`、`REJECTED`、`EXPIRED`。仅 `APPROVED` 数据可输出至嵌入式端。

---

## 🛠 硬件规格 (Target Device)

- **设备型号**：M5Stack Stopwatch
- **主控芯片**：ESP32-S3 (双核 Xtensa LX7 @ 240MHz)
- **存储规格**：16MB Flash, 8MB PSRAM (Octal SPI)
- **显示屏幕**：1.75 英寸圆形 AMOLED，分辨率 **466 × 466**，驱动芯片 **CO5300**
- **交互外设**：高灵敏电容触摸、物理按键、BMI270 六轴 IMU、振动马达
- **电源管理**：M5PM1 电源芯片 + M5IOE1 IO扩展

---

## 🗺 研发路线图 (Phases)

| 阶段 | 目标 | 状态 |
| :--- | :--- | :--- |
| **Phase 1** | 硬件基础底座（M5Unified/M5GFX/Touch/Buttons/RAM/Flash 监测） | **已完成** |
| **Phase 2** | 3D 地球渲染引擎（466全宽视口、昼夜晨昏渐变、城市夜光火点） | **已完成** |
| **Phase 3** | 地理坐标球极投影与视线遮挡剔除 | **已完成** |
| **Phase 4** | 地理兴趣点 Markers 动态渲染、波纹脉冲与高亮 | **已完成** |
| **Phase 5** | 触控手势旋转地球与缩放、平滑自转飞掠动画、Marker 命中选取 | **已完成** |
| **Phase 6** | 地点详情视图（下方大幅避让、中等卡片窗口、16:9 黄金内接沉浸播放） | **已完成** |
| **Phase 7** | 全球精选地点离线数据库扩充 | 待扩充 |
| **Phase 8** | 快照播放器 (SnapshotPlayer) 硬件 JPEG 解码与自适应渲染 | **已完成** |
| **Phase 9** | 摄像头多协议播放抽象层 (CameraPlayer) | 待开发 |
| **Phase 10**| Wi-Fi 网络连接管理、异步非阻塞抓帧与定时刷新 | **已完成** |
| **Phase 11**| 固件 OTA 与数据动态增量更新机制 | 待开发 |

---

## 📁 目录结构

```text
LiveEarth_stopwatch/
├── .pio/               # PlatformIO 编译构建缓存
├── data/               # 地理、摄像头与审核数据集
│   ├── categories.json # 7大核心分类体系
│   ├── sources.json    # 数据源元数据与归属权
│   ├── places.json     # 全球地点规范数据
│   ├── cameras.json    # 摄像头数据
│   ├── review_queue.json # 人工审核队列
│   └── schema/         # JSON Schema 验证文件
├── docs/               # 架构与技术风险文档
│   ├── 需求.md         # 原始需求说明书
│   ├── TECHNICAL_RISKS.md # 嵌入式技术风险评估
│   └── ARCHITECTURE.md # 软硬件分层架构
├── src/                # 固件源码 (C++/Arduino)
│   ├── main.cpp        # 固件主入口与系统自检
│   └── config.h        # 屏幕几何与系统配置
├── tools/              # 数据挖掘与自动化测试管线
│   ├── discovery/      # 官方公开源发现工具
│   ├── validation/     # URL 与流可用性验证
│   ├── geocoding/      # 坐标解析与精度定标
│   ├── editorial/      # 人文描述规范审查
│   ├── export/         # 固件轻量化数据打包导出
│   └── qa/             # 质量门禁校验 (QA Check)
└── platformio.ini      # 嵌入式工程构建配置
```

---

## 🚀 快速上手

### 固件构建与烧录
```powershell
# 编译固件
pio run -e m5stack-stopwatch

# 烧录到设备
pio run -e m5stack-stopwatch -t upload

# 监控串口输出 (115200 波特率)
pio device monitor -b 115200
```

### 数据管线自检
```powershell
python tools/qa/qa_check.py
```
