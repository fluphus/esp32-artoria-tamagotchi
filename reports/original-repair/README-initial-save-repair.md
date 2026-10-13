# 原版 Artoria Tamagotchi：存档与串门维修交付

日期：2026-10-12。基线：`448dbe158595c1db9f1d2fc37caa15810c53de1c`。

本交付是**未拆分引擎的原版**修复，位于独立工作树 `/home/omni/.codex/worktrees/original-save-repairs/esp32-artoria-tamagotchi`，分支 `codex/original-save-repairs`。当前引擎重构工作目录未修改，未将引擎、内容包或v10格式移入原版。修复、测试及报告留在该工作树，未提交或推送。

可移植补丁：[original-save-repairs.patch](original-save-repairs-initial.patch)，111759字节，SHA256 `87e6fe7836b9edc8fdc0c70d52d9bacf67278ba518d8fdf45ec8b882ca870da3`。包含生产修改、主机测试、目标夹具及其生成工具；报告/日志独立交付，不嵌入补丁。已在干净原始提交的临时本地克隆中执行 apply --check、实际应用及完整离线回归，全部退出0，见[验证记录](evidence/portable-patch-verification.txt)。

## 已修复

| 原有问题 | 原版修复 | 回归位置/结果 |
| --- | --- | --- |
| 唯一有效访客导致主人槽重入轮转 | 串门加载/全槽回退排除冻结主人，重建访客对；每次写入另行排除主人，不依赖其记录有效 | `startupAndUnknown`：主人坏、槽1唯一访客、槽2空或坏，连续保存不改主人；PASS |
| 只有附属/损坏数据被当新游戏 | hasSave检查原始键，启动共用loadStartup；加载不成功进入恢复锁，拒绝宠物/图鉴/设备自动写入 | `startupAndUnknown`：旧gallery/dev_state合法/坏长度/非法bool，未知v10附属键；PASS |
| 未知格式记录按空槽覆盖 | 写目标仅可是真空槽或验证有效的本版记录；未知版本、损坏、v10键均保留 | `startupAndUnknown`：混合有效槽与未知槽，两种位置连续保存；PASS |
| 完整导入/仅宠物串门部分失败假成功 | main的COMMIT调用同一生产commitImport，候选RAM、逐项返回值、持久repair_guard，全部成功才提交和输出成功 | `workflows`：gallery/slot_cfg/dev_state/最终头、数据、图鉴/完成标记失败；PASS |
| 结束串门恢复主人部分失败仍清串门 | 串口reset和按钮销毁共用restoreOwner；主人时间补齐候选写另一活动槽，完成前原主人槽持续冻结；最后清标记、提交RAM/日志 | `workflows`：slot_cfg/dev_state/最终头、数据、图鉴/完成标记/主人坏档；PASS |
| 主人不可读时强制丢弃，及旧提示 | 取消破坏性fallback，报告owner_unavailable/需要恢复，保留串门、原主人记录 | `workflows`：失败前后、重启后保护；PASS |
| 串口占位后端缺AnimState声明 | 类型include移到两后端共用范围，原宏支持编译配置覆盖 | `original-serial`实际构建；PASS |

既有显式erase命令也检查返回值，失败不再打印Erased；主机模拟erase失败确认原数据/恢复锁保持，再验证明确删除后的恢复路径。

原版图鉴使用连续0..11编号，其索引转换在原内容规模内成立；没有迁入为Sprout稳定ID做的修正，也没有增加第二内容包。epoch0哨兵、倒退时间、终态惩罚等涉及玩法政策的旧语义未改。

## 最小生产边界

- `src/core/save_manager.cpp/.h`：原v9管理器修补，活动对返回SaveResult、坏/未知数据保护、恢复锁；没有替换原版存档为新引擎格式。
- `src/core/save_io.cpp/.h`：仅注入NVS存储与日志。固件仍装配Preferences/Serial，主机注入内存存储和失败；无虚函数、无Arduino环境仿真。
- `src/core/save_workflows.cpp/.h`：从main移动原时间对齐函数，生产导入/恢复/启动流程同时供main和主机调用。主机没有复制持久化流程。
- `src/main.cpp`：两个恢复入口和COMMIT调用上述流程，失败不提前退出保护/报成功；时钟在COMMIT成功后应用。
- `src/display/display_renderer.cpp`、`display_config.h`、`platformio.ini`：仅修原占位后端构建路径。

`check_original.py`逐字节核对基线中的其他文件：游戏状态、枚举、食物表、规则、资源和SaveManager主机串口工具都保持原样；原项目六个既有文件为本次允许修改范围。

## v9与故障保护证据

PetState56、SaveHeader16、Gallery12、DeviceState12；仍用原NVS hdr/dat/gal、gallery、dev_state、slot_cfg键、原校验算法和72/84字节串口备份。正常READY/RECV/COMMIT/EXPORT关键字和成功标识保留。没有改变结构体字段、枚举值或格式版本。

八份旧格式夹具来自原Git头文件及ESP32-S3 Xtensa编译器，生成器随补丁提供，本轮已重新生成。`check_original.py`在当前ESP32-S3目标编译器上验证全部尺寸和字段偏移；生产SaveManager读取/导出目标夹具，头/宠物/附带图鉴逐字节一致。**不是用主机结构体转储证明旧ESP32布局，不是真实设备旧档**；不声称覆盖所有历史padding和编译选项。

新增 `repair_guard` 为6字节有界标记：0xa7/status/frozenOwner/version1/原checksum算法前4字节LE校验。操作开始先写pending，最终成功后写completed；非法长度/身份/版本/校验一律进入恢复保护。它不改变宠物格式，也不是多键事务或自动回滚。

恢复主人时，v9分键写入不适合覆盖唯一冻结主人：本补丁保持原主人槽（例槽0 HP50）原始hdr/dat/gal直到完成，把时间对齐后的主人写到另一个活动槽（例槽1），以高于所有现存槽的sequence成为最新档。正常完成后解除冻结，后续两槽轮转恢复原规则。原主人不可读或另一活动槽未知/损坏时宁可拒绝恢复，不用未知数据作空槽。

## 本轮实际离线验收

从修复工作树根执行：

| 命令 | 结果 | 退出码/日志 |
| --- | --- | --- |
| `python3 tests/host/run_all.py` | 目标ABI/文件身份、1684存档断言、35玩法轨迹一致 | 0，[完整日志](evidence/original-repair-all.log) |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python3 tests/host/run_all.py` | 同套生产检查，无ASan/UBSan诊断 | 0，[完整日志](evidence/original-repair-sanitizers.log) |
| `python3 tools/repair/generate_v9_fixtures.py` | 原Git头+Xtensa目标工具链重新生成旧格式样本 | 0，[来源/布局/哈希](evidence/original-repair-fixtures.log) |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e original-serial` | SSD1351和串口占位均SUCCESS | 0，[完整日志](evidence/original-repair-builds.log) |
| `python3 -m py_compile tests/host/*.py tools/repair/generate_v9_fixtures.py` | Python语法检查 | 0 |
| `git diff --check` | 无空白错误 | 0 |
| 临时干净原始克隆中apply检查、应用补丁、run_all | 补丁独立可用，1684/35通过 | 0，[验证记录](evidence/portable-patch-verification.txt) |

两个生产导入分支分别在11个持久化完成断点模拟掉电，恢复主人在7个断点模拟掉电，共29个断点。存储回调只在实际单键写成功后抛出测试异常，再以全新SaveManager走init/loadStartup及普通写入拒绝；不是只测试codec。除最后完成标记外均进入Recovery，最后标记完成后加载正确宠物；恢复主人不会加载HP71/82为普通主人。

故障矩阵同时捕获实际生产日志、RAM及NVS设备标志；RAM保持原值，无restored/Visit started假成功，原主人逐字节不变。正常结束后多次保存/重启均保持主人50。三条最终数据写失败路径另验证导出原主人、显式erase（含失败注入）、重启新游戏判断、完整导入保留主人、再重启恢复主人50。

35条玩法轨迹执行原版真实feeding/evolution/seriousness，原始Git与修补后比较所有宠物字段及RNG调用数。仅给这三个旧规则提供日志/随机声明shim；新存档/恢复测试完全不依赖Arduino shim。不是宣称所有玩法状态穷举。

| 构建 | Flash字节 | 静态RAM字节 |
| --- | ---: | ---: |
| 原版修补后SSD1351 | 3725677 | 21944 |
| 原版修补后串口占位 | 1664013 | 21712 |

SSD1351相对P0原版3724197/21904增加1480字节Flash、40字节静态RAM。默认固件做过新工作树初次完整编译，最后两后端数据来自最终增量构建；没有把静态RAM等同于运行时峰值/栈测量。

## 手动恢复及限制

发生操作中断后不自动清档或回滚。先使用既有 `SAVE_EXPORT <slot>` 命令导出保留的主人/有效槽；确认备份后才显式erase，重启后通过既有完整导入恢复。上述生产路径已在内存存储中验证，**本轮没有在设备执行**。主人原记录已损坏的情况不能凭空恢复，但损坏字节保留，不擅自重置访客来掩盖错误。

- 未操作真实设备，未烧录、擦除或串口导入；屏幕/按键/UART、芯片NVS掉电及动态栈峰值均未执行。
- v9本身仍为头/数据/图鉴多键存储，普通轮转保存不提供跨键原子事务；本轮以恢复标记保护导入/结束串门，不声称所有普通保存都原子化。故障测试依赖单键原子替换契约。
- 原版主机串口工具文件保持原样，格式兼容通过源码身份、目标夹具和生产流程验证，未声称真实UART往返通过。
- 没有迁入引擎重构，没有重新设计玩法；没有声称修复原项目所有历史问题。

本交付结论限于上述原有缺陷的原版修复和离线回归，不替代最终验收模型或实机验收。
