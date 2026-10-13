# 原版 Artoria Tamagotchi：存档维修及补充缺陷修复

> 2026-10-13 发布补充：三语README已按实际Adafruit显示库、original-serial构建环境、按键选择、设时与恢复流程修正，并链接独立引擎仓库。既有11个生产文件的修复范围不变；身份检查另外允许3份README文档变化，仍严格验证原规则、资源及v9布局。`package_repairs.py`排除reports/以避免发布后把报告/补丁递归打包。发布前完整`run_all.py`及干净原始克隆apply/check/完整回归均退出0，见`evidence/publish-offline-regression.log`和`evidence/additional-portable-patch.log`。本文其余“未提交/推送”描述保留开发时状态，远端发布状态以Git提交为准。本次仍未操作设备。

日期：2026-10-12。原始基线 `448dbe158595c1db9f1d2fc37caa15810c53de1c`。

交付位于 `/home/omni/.codex/worktrees/original-save-repairs/esp32-artoria-tamagotchi`，分支 `codex/original-save-repairs`。这是保留原始 Artoria 游戏及 v9 存档的独立维修版，**没有移入新引擎、第二内容包或 v10**。引擎生产源码未修改；未提交、推送或操作真实设备。

本轮审查列出的10组遗留问题已完成定向修复和离线回归，并修复补测发现的人工恢复命令被等待设时拦截问题。前批存档维修一并保留。结论限于下表已测范围，不声称所有历史缺陷或实机验收已经通过。

## 本轮问题、修复与生产路径证据

| 问题 | 实际修复及测试位置 | 状态 |
|---|---|---|
| 三键组合松开误操作销毁确认 | ButtonDriver确认组合后抑制普通事件直到全部松开；`destroyRelease`用真实GPIO→消抖→输入映射→菜单→实际main回调，松键不确认；再单击取消、重新组合后L/M明确确认，仅销毁一次。 | PASS |
| 设时写失败仍提示正常运行 | UI确认和SET_TIME共用main内的`confirmTime`，复用原版repair_guard保护图鉴、设备态、宠物及完成标记；所有必要写入成功后才清等待标志/提示成功。失败恢复宠物/设备/图鉴/时钟RAM，清演出并锁定规则/自动保存。`initialTimeFailures/firstTimeSetup/normalSetTime`覆盖新游戏及已有宠物、各关键写失败；`timeInterruptions`覆盖7个成功持久写点后的重启。 | PASS |
| NVS打开失败走新游戏 | loadStartup先检查initialized；init失败保持恢复资格、禁止自动保存。`storageBoundaries`经过实际setup/loop/设时回调验证已有数据不变、无New game或正常运行提示。 | PASS |
| Python不完整导出假成功 | 实际cmd_export核对BEGIN/END长度和槽位后才写文件。`tool_repair_test.py`验证短数据/结束槽不符拒绝，已有主机文件保留。 | PASS |
| millis累计一圈日历倒退 | TimeManager每次累计整秒并滚动锚点，保留余毫秒。`clockRollover/clockFraction`验证50步×100000000ms、每步准确100000秒及999/1000ms边界。 | PASS |
| 显示hold/toast跨回绕提前消失 | 用int32截止差比较；`displayRollover`经过实际DisplayManager验证跨UINT32_MAX的结果页停留和toast开始/到期。 | PASS |
| 存档sequence回绕读旧档 | MAX−1允许最后一次写到MAX；0仅作内部耗尽哨兵，此后保存/forceNewest导入明确拒绝、停止自动保存资格。`storageBoundaries`验证原字节不变、重启仍读取最新HP73。v9编码不变。 | PASS |
| Python超时半行丢失 | _read_line按连接保留半行，完整换行才解析；响应行容量4096。实际Python函数及生产协议桥接都分片响应。 | PASS |
| 串口会话/半行长驻、超长前缀被执行、无偏移重试重复追加 | 实际main接收器整行溢出丢弃并ABORT；半行/导入30秒无活动超时；可选@offset对同偏移相同分包幂等，冲突拒绝。Python按offset=1能力使用偏移及等量ACK；连接旧无偏移固件时不自动重发。`lineProtocol/protocolCompletion`覆盖分片、超时、超长、重复包、非法HEX、未完成导入、ABORT、重复COMMIT；实际Python完整/仅宠物导入各丢一次ACK后安全重试成功。 | PASS |
| 年龄/离线容量 | 日结年龄65535先拒绝；离线支持上限明确为366日，并在执行前检查年龄和epoch容量，超限拒绝、不截断、不落盘。`capacity/offlineLimit`验证65535、367日拒绝及366日实际逐日结算、保存。 | PASS（有界） |
| 补测：恢复锁/等待设时拦截erase，人工恢复无法闭环 | 既有明确`erase`命令移到等待限制之前；成功才重置设备RAM/导入会话。恢复锁仍阻止普通游戏命令和主循环规则。`manualRecovery`实际调用SAVE_EXPORT取回84字节→明确erase→实际分包导入/COMMIT→重启加载主人HP50。`visitTimeFailure`另验证访客离线设时失败后主人原始槽不变、RAM回退、loop禁止继续结算。 | PASS |

`confirmTime`是对原版已有流程的小范围共用，不是引擎装配层；没有给所有模块建立接口。测试直接包含交付的`src/main.cpp`，实际菜单、时间、显示、资源、队列、规则、保存管理器和回调均参与。外围仅替换millis/GPIO/UART/NVS与未测试的电源硬件通知；ESP sleep/RTC声明用于离线链接，不能算实机深睡测试。

设时中断前六个点重启为Recovery，完成标记落盘后加载完整结果。首次guard写入本身失败时，本轮RAM锁定；重启可以重新加载未改的原档，空设备仍为空设备，不伪造pending已落盘。发生后续失败不自动撤销标记或清档，只允许取回备份及明确恢复。

原版分钟精度、倒退时间/epoch0哨兵、日结离线算法等正常支持范围内的语义保留。非等待模式SET_TIME仍保留原设备态的请求秒值，而模拟时钟按原接口取分钟精度，有针对性断言。366日是本轮明确的资源支持上限；超过时提示失败，不悄悄裁剪。真实看门狗和耗时不能由主机结果证明。

## 实际命令、退出码及结果

工作树根执行。g++16.2.1、Python3.14.7、PlatformIO6.2.0；实际依赖清单见[evidence/additional-environment.log](evidence/additional-environment.log)。

| 命令 | 退出码 | 证据 |
|---|---:|---|
| `python tests/host/run_application_repair.py`（修复前六组测试） | 1 | [additional-before.log](evidence/additional-before.log)：六组均FAIL，生产调用路径复现 |
| `python tests/host/run_all.py` | 0 | [additional-all.log](evidence/additional-all.log)：目标ABI/8份夹具/未授权文件身份、原版存档1684断言、35条正常玩法轨迹一致、应用437断言、Python协议11断言 |
| `HOST_TEST_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer -g' python tests/host/run_all.py` | 0 | [additional-sanitizers.log](evidence/additional-sanitizers.log)：同套通过，无ASan/UBSan诊断 |
| `/tmp/artoria-p0/venv/bin/pio run -e esp32-s3-devkitc-1 -e original-serial` | 0 | [additional-builds.log](evidence/additional-builds.log)：两后端SUCCESS；增量构建，未声称本轮clean构建 |
| `python tools/repair/package_repairs.py` | 0 | [additional-portable-patch.log](evidence/additional-portable-patch.log)：干净原始克隆apply --check、实际应用、完整run_all及交付文件身份均通过 |
| `/tmp/artoria-p0/venv/bin/pio pkg list` | 0 | [additional-environment.log](evidence/additional-environment.log) |
| `git diff --check` | 0 | 无空白错误 |

366日离线的普通主机运行13ms、sanitizer运行45ms，见上述日志；测试只说明当前主机样例耗时，不替代ESP32看门狗/堆/栈测量。

| 构建 | Flash字节 | 静态RAM字节 |
|---|---:|---:|
| 原版修补后SSD1351 | 3726857 | 21960 |
| 原版修补后串口占位 | 1665189 | 21728 |

## v9及前批维修保护

PetState56、SaveHeader16、Gallery12、DeviceState12；枚举、字段顺序、原校验、原NVS hdr/dat/gal/gallery/dev_state/slot_cfg及72/84字节备份均保留。布局通过原Git头及当前Xtensa目标编译器assert验证；生产读取/导出8份既有目标工具链夹具仍逐字节一致。没有用主机结构体转储替代目标ABI证据，没有真实设备旧档证据。

`check_original.py`现在允许11个既有文件的明确修复，而非原先6个；新允许的5个文件是TimeManager、DisplayManager、ButtonDriver源/头及Python工具，均由本轮具体行为测试接替“必须原样”的旧身份要求。游戏状态、枚举、食物表、feeding/evolution/seriousness和资源仍逐字节等于原基线；正常规则35条全字段/RNG轨迹保持一致。不能仅更新白名单证明兼容，ABI/夹具、旧存档生产测试及真实应用测试共同提供证据。

前批主人冻结/唯一访客回退、附属与未知数据保护、导入失败、恢复主人、擦除错误处理继续通过1684断言，含29个导入/恢复持久化中断点。原v9分键普通保存仍不是跨键事务；本次guard保护的是明确的导入、主人恢复及设时流程。

前批交付作为历史保留：[README-initial-save-repair.md](README-initial-save-repair.md)及[旧补丁](original-save-repairs-initial.patch)。其中“Python工具未改”和旧构建数字仅适用于该历史版本，不能用于当前交付。

## 补丁与人工恢复

当前完整补丁：[original-save-repairs.patch](original-save-repairs.patch)。205601字节，SHA256 `2e355058c4556342fc54f615fd399db80e301aeff1b46964ab6ef32764b79c06`（见[additional-patch-receipt.json](evidence/additional-patch-receipt.json)）；包含本轮及前批生产修复、测试、目标夹具、工具和三语README修订，不包含reports/报告、缓存或本机配置。使用临时Git索引生成，未改变用户真实索引。已在完整原始提交的独立临时克隆中应用并执行全部离线检查，临时目录随后删除。

恢复锁存在时，不通过普通设时/重启自动清理。使用更新后的工具取回有效主人槽（槽号按实际冻结位置选择）：

```sh
python SaveManager/save_manager.py --port PORT export --slot 0 --out owner.bin
# 核实备份后，在串口明确发送 erase（会删除NVS存档）；失败不会提示Erased。
# 随后可重启，再完整导入备份；本轮测试也覆盖不重启直接导入。
python SaveManager/save_manager.py --port PORT import --slot 1 --in owner.bin
```

以上真实设备命令本轮未执行。普通启动不会自动擦除。新工具支持旧协议，旧无偏移固件丢ACK时明确失败而不重复追加；旧版未更新工具自身的无偏移重试行为不能被新服务端无歧义地识别，建议使用随补丁更新的工具。

## 未验证及限制

- 屏幕/三键实体电气、真实UART、真实v9旧档、芯片NVS掉电、深睡/RTC、长时间运行、堆栈峰值/PSRAM和看门狗：**BLOCKED，未执行**。离线外设替代不算硬件通过。
- 目标编译器夹具不是设备旧档；未重新生成既有夹具，只验证来源哈希和当前目标ABI及生产读取。
- 模拟中断依赖单键原子替换契约；未声称普通v9多键轮转保存具备事务。
- 本轮没有新做所有RTC/睡前分支的故障矩阵，深睡硬件路径仍待专用设备验收；没有把日期/离线全部原有语义改成新玩法。
- 原有未使用变量/函数警告仍在，没有进行无关格式化或清理。ASan主机适配禁用目标32位printf与主机格式不匹配告警；存档ABI单独检查。
