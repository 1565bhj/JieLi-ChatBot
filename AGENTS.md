# Agent 指南

## 项目形态

这个仓库是杰理 AC79/WL82 SDK 的一个定制化子集，当前核心产品是 `apps/wifi_story_machine`。它不是一个从零搭建的纯业务仓库，而是一套分层固件工程，主要由下面几部分组成：

- `cpu/wl82`、`include_lib`、`cpu/wl82/liba`、`lib` 中的 SDK 平台代码和预编译库
- `apps/common` 中的通用能力源码
- `apps/wifi_story_machine` 中的产品业务代码和板级代码
- `cpu/wl82/tools` 中的资源打包、下载和 OTA 相关工具链

## 基本工作规则

- 优先修改 `apps/wifi_story_machine`，再考虑是否需要改 `apps/common`。
- `cpu/wl82/liba` 及其他预编译库默认视为黑盒依赖，除非已经有充分证据表明问题必须通过源码层修复。
- 遇到行为问题时，不要只盯着第一个找到的 `.c` 文件，先判断真实控制点是不是板级配置、功能宏或资源打包。
- 谨慎修改 `cpu/wl82/tools` 下的文件。资源地址、OTA 布局和后处理脚本之间耦合很紧。
- 不要默认根目录 `Makefile` 的 `all` 目标适合当前这个裁剪后的仓库。

## 建议阅读顺序

开始分析或改动前，建议按下面顺序建立上下文：

1. `apps/wifi_story_machine/app_main.c`
2. `apps/wifi_story_machine/board/wl82/board_config.h`
3. `apps/wifi_story_machine/board/wl82/board_7916A_develop_cfg.h`
4. `apps/wifi_story_machine/include/app_config.h`
5. 你这次实际要修改的功能目录
6. 如果业务层没有实现，再回看 `apps/common`
7. 如果仍解释不通，再判断是否落在预编译库边界

## 关键入口

- `apps/wifi_story_machine/app_main.c`
  系统集成主入口，包含启动流程、任务表、模式选择。
- `apps/wifi_story_machine/board/wl82/board_config.h`
  板型选择和工程配置切换入口。
- `apps/wifi_story_machine/board/wl82/board_7916A_develop_cfg.h`
  板级能力宏、引脚决策、设备使能开关。
- `apps/wifi_story_machine/include/app_config.h`
  产品级功能开关和默认能力配置。

## 构建与验证

推荐的构建入口：

- 在仓库根目录执行：`make ac791n_wifi_story_machine`
- 在板级目录执行：`cd apps/wifi_story_machine/board/wl82` 后再 `make`

常用变体：

- `make clean`
- `make VERBOSE=1`

从板级 `Makefile` 可以看出的工具链假设：

- Windows 默认工具链路径：`C:/JL/pi32/bin`

不要默认这个项目已经具备主机侧单元测试体系。当前验证方式更偏固件工程：

- 编译和链接成功
- 后处理和资源打包成功
- 如果相关，下载文件或 OTA 包生成成功
- 通过板级日志、串口输出或实际产品行为做运行验证

## 嵌入式调试建议

- 调试时先判断问题属于业务层、通用 SDK 层、板级配置层、资源与交付层，还是预编译库黑盒边界。
- 遇到串口协议、AI 行为、唤醒流程、联网流程问题时，优先保留精确日志和触发步骤。
- 遇到板级问题时，记录当前生效的板型宏和功能开关。
- 遇到 OTA 或资源问题时，同时检查 C 侧改动和 `cpu/wl82/tools` 下生成出来的产物。

## 文档约定

- `CONTEXT.md` 用于维护项目共享术语和概念定义。
- `docs/adr/` 用于记录“为什么这样做”的决策，避免后续被随意重新讨论。
- 新增说明尽量简洁，并且要对应真实的维护问题。
