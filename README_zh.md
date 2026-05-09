[English](README.md) | [简体中文](README_zh.md) | [日本語](README_ja.md)
# 👑 阿尔托莉雅 拓麻歌子 (Fate/Grand Order 虚拟宠物)

一个基于 **ESP32-S3 (N16R8)** 驱动的类拓麻歌子（电子宠物）项目。

培育你自己的阿尔托莉雅吧！从 Saber Lily 开始你的旅程并引导她成长。根据你的照顾方式和互动，她可以进化成不同的职阶，包括 Archer、Lancer、Rider 等等。

## ✨ 特性
* **进化系统：** 基于宠物状态（健康度、严肃度等）的多条成长路线。
* **时间与日期管理：** 内置时钟用于处理每日重置，以及未按时喂食的惩罚。
* **持久化存储：** 三槽存档系统，支持校验和验证、按槽串口导入导出，以及导入后时间对齐保护流程。
* **互动操作：** 喂食、戳一戳（互动）以及状态监控。
* **显示：** 通过 TFT_eSPI 驱动的 SSD1351 128x128 65K 彩色 OLED，并提供用于无头测试的串口占位符后端。

## 🚧 当前项目状态

* 显示：TFT_eSPI 后端 (SSD1351 128x128) 和串口占位符后端均可编译并运行。
* 输入：3 物理按键输入 (L/M/R)，支持串口模拟 (`btn l|m|r`, `btnl l|m|r`, `btnr l|m|r`)。
* 资源：当真实图片不存在时，使用占位符矩形和文本代替。

## 🛠️ 硬件需求

* ESP32-S3 开发板（推荐 N16R8 版本）
* SSD1351 1.5" 128x128 65K 彩色 OLED (SPI)
* 3 个微动按键 (GPIO 4, 5, 6)

### 引脚映射

| 功能 | GPIO |
|----------|------|
| BTN_L    | 4    |
| BTN_M    | 5    |
| BTN_R    | 6    |
| TFT_DC   | 8    |
| TFT_RST  | 9    |
| TFT_CS   | 10   |
| TFT_MOSI | 11   |
| TFT_SCLK | 12   |

## 编译

```bash
pio run                    # 编译 (默认 TFT_eSPI 后端)
pio run -t upload          # 烧录到设备
pio device monitor         # 串口监视器 (波特率 115200)
```

如需切换到串口占位符后端（无屏幕），请编辑 `src/display/display_config.h`：
```c
#define DISPLAY_BACKEND_SERIAL_PLACEHOLDER  1
#define DISPLAY_BACKEND_TFT_ESPI            0
```

## 按键映射

### 待机界面 (Idle Screen)
| 按键 | 操作 |
|--------|--------|
| L      | 喂食 (开始抽卡) |
| M (短按) | 状态面板 |
| M (长按) | 打开图鉴 |
| R      | 戳一戳 |

### 状态面板 (Status Panel)
| 按键 | 操作 |
|--------|--------|
| M      | 关闭 |

### 喂食选择 (Feed Pick - 4选3)
| 按键 | 操作 |
|--------|--------|
| L      | 光标左移 |
| R      | 光标右移 |
| M      | 切换选择状态 (选满3个自动提交) |

### 特殊食物选择 (Special Food Selection)
| 按键 | 操作 |
|--------|--------|
| L      | 光标上移 |
| R      | 光标下移 |
| M      | 确认选择 |

### 图鉴 (Gallery)
| 按键 | 操作 |
|--------|--------|
| L      | 光标左移 (支持跨页循环) |
| R      | 光标右移 (支持跨页循环) |
| M      | 关闭图鉴 (返回待机) |

### 销毁确认 (Destroy Confirm)
| 按键 | 操作 |
|--------|--------|
| L      | 移至 YES |
| R      | 移至 NO |
| M      | 确认选择 |

### 销毁连按组合 (Destroy Combo)
长按所有三个按键 (L+M+R) 5秒钟以触发销毁确认（默认光标停留在 NO 上）。

## 冒烟测试流程 (串口 / 按键)

使用串口命令模拟按键。连接波特率为 115200。

### 1. Boot -> Idle (启动 -> 待机)
开机。启动画面显示约 1.5 秒，然后自动切换至待机界面。

### 2. Status Panel (状态面板)
```
btn m          # 打开状态面板
btn m          # 关闭状态面板 (返回待机)
```

### 3. Feed Flow (喂食流程)
```
btn l          # 开始喂食抽卡 (带动画显示4张食物卡片)
               # 等待约 800ms 抽卡动画完成
               # 自动进入喂食选择界面
btn l          # 光标左移
btn r          # 光标右移
btn m          # 选择光标处的食物 (重复以选择3个)
btn m          # 第二次选择
btn m          # 第三次选择 -> 自动提交，显示喂食结果
               # 喂食结果至少停留 1.5 秒
               # 然后自动返回待机
```

### 4. Non-combo Feed Result Hold (无连携喂食结果停留)
选择 3 个食物后（未触发连携）：
- PAGE_FEED_RESULT 显示至少 1500 毫秒
- 停留期间输入被阻止
- 停留期结束后页面自动返回待机

### 5. Combo Feed Result -> Special Food (连携喂食结果 -> 特殊食物)
如果触发了连携：
- PAGE_FEED_RESULT 显示 "COMBO" 文本
- 播放 ANIM_COMBO 动画（约 1000 毫秒）
- 连携动画结束后，进入特殊食物选择
```
btn l          # 在特殊食物列表中移动光标
btn r          # 在特殊食物列表中移动光标
btn m          # 选择特殊食物
               # 确认画面至少显示 1 秒
               # 然后返回待机
```

### 6. Mapo Tofu Trigger (麻婆豆腐触发)
如果在特殊食物选择期间触发了麻婆豆腐：
- 播放 ANIM_MAPO_TOFU 动画（约 1500 毫秒）
- 动画期间输入被阻止
- 动画结束后返回待机

### 7. Poke (戳一戳)
```
btn r          # 播放戳一戳动画 (约 500 毫秒)
               # 动画完成后返回待机
```

### 8. Destroy (3-key combo) (销毁 - 3键组合)
```
btnl l         # 模拟长按 L (实际操作中需长按3键5秒)
               # 在串口测试中，请使用 'reset' 命令
               # 或测试销毁确认 UI:
ctx            # 检查当前上下文
```
销毁确认：默认光标在 NO（安全）。L/R 移动，M 确认。

### 9. Auto-save (自动保存)
自动保存会定期触发。它会在屏幕底部显示一个简短的 "Autosaved" 提示，且不会打断当前页面或动画。

### 10. Debug Commands (调试命令)
```
s              # 打印完整状态
h              # 帮助
fl             # 食物列表
t <min>        # 时间推移 N 分钟
d              # 推移 1 天 (触发日结)
save           # 手动保存
load           # 读取存档
erase          # 擦除存档
reset          # 销毁并重置宠物
stime Y M D H m  # 设置模拟时间
SET_TIME <epoch> # 设置系统时间 (unix 时间戳, 触发离线补偿)
hp <val>       # 调试: 设置健康度 (0-100)
sr <val>       # 调试: 设置严肃度 (0-100)
age <val>      # 调试: 设置年龄天数
grad           # 调试: 强制毕业
mapo           # 调试: 麻婆豆腐计数 +1
FORCE_NOBU     # 调试: 强制进入 nobu 路线
UNLOCK_ALL     # 调试: 解锁所有图鉴形态
RESET_GALLERY  # 调试: 重置图鉴 (锁定所有)
IMPORT_TIME_SETUP  # 强制进入“导入后设时”界面 (不做离线补偿)
SAVE_SLOT_STATUS   # 打印 slot0/1/2 状态 (seq/time/ver/size/crc)
SAVE_EXPORT <slot> # 通过串口导出单槽快照 (hex 流)
SAVE_IMPORT_BEGIN <slot> # 开始单槽导入会话
SAVE_IMPORT_DATA <hex>   # 追加一段 hex 负载
SAVE_IMPORT_COMMIT       # 提交导入负载
SAVE_IMPORT_ABORT        # 取消导入会话
s0 / s1 / s2   # 打印单槽快照状态
bright <0-15>  # 设置屏幕亮度
dim <0-15>     # 设置息屏亮度
dim_t <sec>    # 设置息屏超时 (秒)
off_t <sec>    # 设置关屏超时 (秒)
pwrsave        # 保存电源配置到 NVS
pwrinfo        # 打印电源配置
btn l|m|r      # 模拟短按
btnl l|m|r     # 模拟长按
btnr l|m|r     # 模拟连按
ctx            # 显示当前 UI 上下文
```

## 串口存档导入/导出

项目已支持按槽位的串口导入导出，用于备份和迁移流程。

### 固件侧命令

- `SAVE_SLOT_STATUS`：查询全部槽位状态（`slot0`、`slot1`、`slot2`）。
- `SAVE_EXPORT <slot>`：导出指定槽位为 hex 数据块。
- `SAVE_IMPORT_BEGIN <slot>` -> `SAVE_IMPORT_DATA <hex>` -> `SAVE_IMPORT_COMMIT`：导入指定槽位负载。
- `SAVE_IMPORT_ABORT`：中止当前导入会话。
- `s0` / `s1` / `s2`：按状态格式打印单槽快照。

### 主机侧辅助脚本（Python）

在 `SaveManager` 目录运行：

```bash
py save_manager.py
```

或使用命令行模式：

```bash
py save_manager.py --port COM5 status
py save_manager.py --port COM5 export --slot 0 --out slot0.bin
py save_manager.py --port COM5 import --slot 1 --in slot0.bin
```

### 导入行为（重要）

- 导入槽一定会加入活动存档槽对。
- 其余两个槽中会选“更旧”的一个作为另一活动槽。
- 剩余槽会被冻结，不再参与后续自动/手动覆盖轮转。
- 导入完成后设备会强制进入设时流程，并跳过离线补偿。
- 完成设时后会对齐并持久化导入存档时间；若校验失败，会在后台持续重试直到成功。

## 页面停留行为 (Page Hold Behavior)

显示系统使用“页面停留 (page hold)”机制来确保结果/确认页面可见：

- **喂食结果 (无连携)：** 停留 1500 毫秒，输入被阻止，然后自动返回待机。
- **特殊食物确认 (非麻婆)：** 停留 1000 毫秒，输入被阻止，然后自动返回待机。
- **麻婆豆腐：** 播放动画 1500 毫秒（由动画系统管理，而非停留）。
- **启动：** 1500 毫秒后自动切换至待机。
- **日结：** 播放动画 1500 毫秒，输入被阻止。
- **进化：** 播放动画 2000 毫秒，输入被阻止。
- **销毁动画：** 播放动画 2000 毫秒，输入被阻止。

在任何停留或动画期间，`isPageBlockingInput()` 返回 true，并且 MenuController 会跳过输入处理。