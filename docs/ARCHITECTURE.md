# LiveEarth 系统架构设计 (Architecture)

本文档定义 LiveEarth 的软件分层架构与离线/在线数据处理管线。

---

## 1. 总体架构图

```
+-----------------------------------------------------------------------+
|                              LiveEarth UI                             |
|  [3D Globe Engine]   [Marker Projection]   [Place View]   [Live View] |
+-----------------------------------------------------------------------+
|                           Core Services                               |
|   PlaceManager   |   GestureEngine   |   RenderEngine   | PowerMgr    |
+-----------------------------------------------------------------------+
|                         Camera Abstraction                            |
|       CameraPlayer (Interface)                                        |
|         ├── SnapshotPlayer (JPEG via HTTP)                            |
|         ├── MJPEGPlayer (Chunked Stream)                              |
|         └── HLSPlayer (Future Transcoded Stream)                      |
+-----------------------------------------------------------------------+
|                    Hardware Abstraction Layer (HAL)                   |
|   M5Unified   |   M5GFX (CO5300 AMOLED)   |   Touch / Buttons / RTC   |
+-----------------------------------------------------------------------+
|                       ESP32-S3 Hardware Layer                         |
|   Xtensa Dual-Core LX7   |   8MB PSRAM (OPI)   |   16MB Flash         |
+-----------------------------------------------------------------------+
```

---

## 2. 固件模块职责

### 2.1 硬件抽象与展示层 (HAL & Display)
- 封装 M5Unified，统一管理电源、屏幕背光、RTC 与按键。
- 针对 466×466 圆形 AMOLED，统一维护圆形坐标系：
  - 圆心点位于 $(233, 233)$。
  - 安全可视半径 $R \le 220$ px。
  - 避免在屏幕边缘被表圈遮挡的区域排版重要文字。

### 2.2 3D 地球渲染引擎 (Globe Engine)
- 离线 3D 球体网格计算：地理经纬度 $(\phi, \lambda)$ 到三维笛卡尔坐标 $(x, y, z)$ 的矩阵映射。
- 视角变换（透视/正交投影）、四元数或欧拉角旋转。
- **背向剔除 (Back-face Culling)**：处于地球背面的 Marker 自动隐藏或淡出，仅渲染正对用户视角的标记点。

### 2.3 地点与离线数据管理器 (PlaceManager)
- 遵循 **Offline-First** 原则。
- 固件内置 100+ 个全球精选地点的轻量化索引与核心信息（ID、名称、当地名、坐标、类别、简介、特色亮点）。
- 在无 Wi-Fi 环境下依然可无缝进行 3D 旋转与知识探索。

### 2.4 播放器抽象层 (CameraPlayer)
- 解耦 UI 与具体传输协议。
- 包含生命周期接口：`init()`, `connect()`, `fetchFrame()`, `stop()`, `getStatus()`。

---

## 3. 数据处理管线 (Data Pipeline)

```
[ Discovery (官方发现/线索挖掘) ]
               │
               ▼
[ Candidate Pool (候选池生成) ]
               │
               ▼
[ Geocoding & Deduplication (经纬度解析 & 同实体去重) ]
               │
               ▼
[ Source Verification (源有效性验证 & 权限分析) ]
               │
      ┌────────┴────────┐
      ▼                 ▼
[ REVIEW_REQUIRED ]   [ APPROVED ]
(存入 review_queue)    (通过严格人工审核)
                        │
                        ▼
            [ Editorial & QA Check ]
            (精炼介绍审查 & 全面自动化质检)
                        │
                        ▼
            [ Export to Device (紧凑二进制/JSON) ]
```

- **严密的数据准入标准**：
  任何 `permission_status != "APPROVED"` 的项目均被自动化 QA 脚本无情拦截，确保生产固件合规安全。
