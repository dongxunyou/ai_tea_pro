# STM32N6 双模型茶叶识别终端

基于 STM32N647（ATK-CNN647B）的端侧 AI 终端：在**同一块板**上部署两个独立 NPU 权重，通过 LVGL 界面二选一运行——**茶叶分类**（4 类 + 非茶拒识）与**茶叶包装检测**（5 类）。

![preview](preview.png)

## 功能特性

- **茶叶分类**：红茶 / 绿茶 / 青柑普洱 / 非茶拒识，输入 320×320，INT8 QDQ 量化；验证集 top-1 1.0，量化零掉点
- **包装检测**：5 类茶叶包装，输入 256×256；INT8 量化后 mAP@0.5 0.93 → 0.89
- **双权重同板**：分类 @ `0x70A00000`、检测 @ `0x70D00000`（片外 XSPI2 Flash，互不重叠）
- **UI 二选一**：模式切换按钮、识别进度条、类别 / 置信度 / 检测框联动显示
- **识别策略**：5 帧突发投票 + 类条件温度校准 + 拒识阈值
- **性能**：分类 NPU 单帧 **30 ms**，检测 **70 ms**（STM32N6 NPU @1 GHz）

## 硬件平台

| 项目 | 说明 |
| --- | --- |
| 主控 | STM32N647（Cortex-M55 + Neural-ART NPU） |
| 开发板 | ATK-CNN647B |
| 摄像头 | OV5640（DCMIPP） |
| 显示 | LCD + 电容触摸（LVGL v9） |
| 权重存储 | 片外 XSPI2 Flash（双权重分区） |

## 技术栈

`C` · `STM32 HAL` · `FreeRTOS` · `LVGL v9` · `ST Edge AI Core` · `LL_ATON` · `YOLOv8` · `PyTorch / Ultralytics` · `ONNX QDQ` · `CMake + Ninja + arm-none-eabi-gcc`

## 系统架构

```
                 ┌──────────────────────────────┐
                 │        STM32N647 (NPU)       │
                 │  ┌────────┐   ┌───────────┐  │
   OV5640 ──────▶│  │ CAM/DCMIPP│ │ Neural-ART │  │
                 │  └────────┘   │  NPU       │  │
                 │       │       └─────┬─────┘  │
                 │       ▼             ▼        │
                 │  ┌──────────────────────┐    │
                 │  │ FreeRTOS + LL_ATON   │    │
                 │  │ 分类 @0x70A00000     │    │
                 │  │ 检测 @0x70D00000     │    │
                 │  └──────────┬───────────┘    │
                 │             ▼                │
                 │      LVGL v9 识别界面         │
                 └──────────────────────────────┘
```

## 模型与部署

1. **训练**：YOLOv8n-cls（4 类 + 非茶负类，320×320）与 YOLOv8n 检测（5 类，256×256）
2. **量化**：INT8 QDQ（ONNX），分类零掉点，检测 mAP@0.5 0.93 → 0.89
3. **转换**：ST Edge AI Core 生成 Neural-ART 网络代码 + LL_ATON 运行时
4. **板端部署**：双权重分区、双网络分时复用 NPU、LVGL 交互

### 关键工程点

- **双权重地址绑定**：ST Edge AI 生成的权重 blob 与 mpool 基址绑定（内部绝对地址写死），不能直接换烧录地址复用，必须用对应 mpool 重新生成
- **激活区隔离**：检测网络初始激活区落在 `cpuRAM2 (0x34100000–0x34200000)`，与固件 `.bss`（结束于 `0x341199C0`）和栈（`0x341FF800–0x34200000`）重叠；通过修改 mpool 禁用 cpuRAM2，使激活区迁移到 `npuRAM3–5 (0x34200000+)`，与固件完全隔离
- **INT8 输入预处理**：检测网络输入为 INT8（scale = 1/255，zero point = -128），预处理执行 `pixel - 128`，不能直接填 uint8
- **平台级问题**：AXISRAM 默认断电、RISAF 防火墙拦截 NPU 读 XSPI2 权重、WFE 时钟门控、CACHEAXI 缓存未使能、双网络激活区重叠

## 目录结构

```
.
├── Appli/              # 应用层（main、FreeRTOS 任务、LVGL UI、AI 推理封装）
├── Drivers/            # STM32 HAL / BSP 驱动
├── Middlewares/        # FreeRTOS、LVGL、ST AI（LL_ATON）等
├── ExtMemLoader/       # 外部 Flash 权重烧录 / 网络模型
├── training/           # 训练、量化、ST Edge AI 转换脚本
├── tools/              # 辅助脚本
├── CMakeLists.txt      # 顶层构建
└── CMakePresets.json
```

## 构建

依赖：`arm-none-eabi-gcc`、`CMake`、`Ninja`、STM32Cube 相关包。

```bash
cmake -B Appli/build -S Appli
cmake --build Appli/build
```

产物：`Appli/build/01led_Appli.elf`

## 烧录

1. 使用 STM32CubeProgrammer + ATK 外部 Flash Loader 烧录固件
2. 分类权重烧录到 `0x70A00000`，检测权重烧录到 `0x70D00000`
3. 烧录后读回校验权重前 4 KB，确认与生成的 `.xSPI2.raw` 一致

## 演示

- 识别界面：400×400 相机预览、模式二选一、置信度、检测框、进度条
- 分类：红茶 / 绿茶 / 青柑普洱 / 未识别
- 检测：5 类茶叶包装 + 检测框

## License

MIT
