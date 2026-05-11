# Assets Directory - 美术资源放置指南

## 核心原则

**文件名 = 变量名 = 自动加载**

只需将 PNG 文件放入对应子目录，文件名遵循命名规则，构建时自动转换并注册到游戏中。
无需手动修改任何代码文件。

---

## 目录结构

```
assets/
├── idle/           ← 待机动画 (形态×状态)
├── eating/         ← 进食动画 (形态×食物 或 形态通用)
├── reaction/       ← 反应表情 (形态×反应类型)
├── food_icons/     ← 食物被吃动画 (独立于宠物)
├── combo/          ← Combo 给予者动画 (梅林、Archer 等)
├── poke/           ← 戳一戳动画
├── evolution/      ← 进化演出
├── random_idle/    ← 随机待机动画
└── special/        ← 特殊演出 (麻婆豆腐等)
```

---

## 文件命名规则

### 静态图 (单帧)
```
{描述名}.png
```

### 序列动画 (多帧)
```
{描述名}_f0.png    ← 第 0 帧
{描述名}_f1.png    ← 第 1 帧
{描述名}_f2.png    ← 第 2 帧
...
```

帧序号从 0 开始，必须连续。构建脚本按序号排列组成动画。

---

## 各目录详细说明

### `idle/` - 待机动画

每个形态在不同状态下的待机动画。

**命名格式:** `{form}_{condition}_f{N}.png`

| 文件名示例 | 含义 |
|-----------|------|
| `lily_normal_f0.png` ~ `_f7.png` | Lily 普通待机 (8帧循环) |
| `lily_hp_low_f0.png` ~ `_f3.png` | Lily 低血量待机 |
| `lily_hp_critical_f0.png` | Lily 濒危待机 |
| `white_saber_normal_f0.png` | 白 Saber 普通待机 |
| `white_saber_sr_high_f0.png` | 白 Saber 高 SR 待机 |
| `white_lancer_lion_king_sr_max_f0.png` | 白 Lancer SR=100 专属 |
| `white_lancer_lion_king_sr_critical_f0.png` | 白 Lancer 狮子王计时中 |
| `black_saber_normal_f0.png` | 黑 Saber 普通待机 |
| `rhongomyniad_f0.png` | 狮子王终态待机 |

**condition 可选值:**
- `normal` - 普通状态
- `sr_high` - SR >= 80
- `hp_low` - HP < 35
- `hp_critical` - HP < 15
- `lion_king_sr_max` - (仅白 Lancer) SR == 100
- `lion_king_sr_critical` - (仅白 Lancer) 计时中且 SR >= 80

---

### `eating/` - 进食动画

**方式 A: 形态×食物 专属动画 (最高优先级)**
```
{form}_{food}_f{N}.png
```
例: `black_saber_cake_f0.png` ~ `_f9.png` = 黑 Saber 吃蛋糕的完整独立动画

**方式 B: 形态通用进食动作 (fallback)**
```
{form}_generic_f{N}.png
```
例: `black_saber_generic_f0.png` ~ `_f5.png` = 黑 Saber 通用进食动作

**优先级:** 系统先查找 `{form}_{food}` 专属动画，不存在则使用 `{form}_generic`。

**form 可选值:**
```
lily, white_saber, black_saber, white_lancer, black_lancer,
white_archer, black_rider, white_ruler, rhongomyniad,
black_rhongomyniad, nobu, oda_nobunaga
```

**food 可选值:**
```
rice_ball, salad, grilled_fish, bread, cake, fried_chicken, soda, candy
```

---

### `reaction/` - 反应表情动画

**命名格式:** `{form}_{reaction}_f{N}.png`

| 文件名示例 | 含义 |
|-----------|------|
| `black_saber_like_f0.png` | 黑 Saber 喜欢 (吃到 junk) |
| `black_saber_dislike_f0.png` | 黑 Saber 讨厌 (吃到 healthy) |
| `black_saber_umu_f0.png` | 黑 Saber 整轮满足 (垃圾投喂) |
| `black_saber_eww_f0.png` | 黑 Saber 整轮不满 (健康投喂) |
| `black_saber_satisfy_f0.png` | 黑 Saber all junk combo 满足 |
| `black_saber_abhor_f0.png` | 黑 Saber all healthy combo 厌恶 |
| `black_saber_perfect_f0.png` | 黑 Saber 每日完美投喂 |
| `white_saber_like_f0.png` | 白 Saber 喜欢 (吃到 healthy) |

**reaction 可选值:**
- `like` - 单食物喜欢
- `dislike` - 单食物讨厌
- `umu` - 整轮总结满足 (非 combo)
- `eww` - 整轮总结不满 (非 combo)
- `satisfy` - combo 阵营匹配满足
- `abhor` - combo 阵营不匹配厌恶
- `perfect` - 每日完美投喂

---

### `food_icons/` - 食物被吃动画

独立于宠物的食物层动画 (食物从出现到被吃掉消失)。
与宠物进食动画同时播放，叠加在宠物嘴部坐标。

**命名格式:** `{food}_f{N}.png`

例:
```
rice_ball_f0.png ~ _f5.png     ← 饭团从完整到被吃掉
cake_f0.png ~ _f4.png          ← 蛋糕从完整到消失
```

建议尺寸: 32×32 像素

---

### `combo/` - Combo 给予者动画

特殊食物选择后，给予者角色的演出动画。

**命名格式:** `{special_food}_f{N}.png`

例:
```
golden_apple_f0.png ~ _f7.png      ← 梅林递出金苹果
emiya_cooking_f0.png ~ _f9.png     ← Archer 做饭
holy_grail_mug_f0.png ~ _f5.png    ← 圣杯杯递出
jaguar_snack_f0.png ~ _f6.png      ← 大河递零食
```

**special_food 可选值:**
```
golden_apple, holy_grail_mug, emiya_cooking, jaguar_snack
```

---

### `poke/` - 戳一戳动画

**命名格式:** `{form}_f{N}.png`

例: `black_saber_f0.png` ~ `_f4.png`

---

### `evolution/` - 进化演出

**命名格式:** `{form_before}_to_{form_after}_f{N}.png`

例: `lily_to_white_saber_f0.png` ~ `_f11.png`

---

### `random_idle/` - 随机待机动画

待机一段时间后偶尔触发的特殊小动作。

**命名格式:** `{form}_{action}_f{N}.png`

例:
```
lily_yawn_f0.png ~ _f5.png         ← Lily 打哈欠
black_saber_smirk_f0.png ~ _f3.png ← 黑 Saber 冷笑
white_saber_sigh_f0.png ~ _f4.png  ← 白 Saber 叹气
```

---

### `special/` - 特殊演出

**麻婆豆腐:**
```
mapo_tofu_{form}_f{N}.png
```
例: `mapo_tofu_black_saber_f0.png` ~ `_f9.png`

---

## 图片规格要求

| 项目 | 要求 |
|------|------|
| 格式 | PNG (支持透明通道) |
| 色彩模式 | RGBA 或 RGB |
| 宠物精灵尺寸 | 建议 64×64 或 48×48 |
| 食物图标尺寸 | 建议 32×32 |
| 全屏演出尺寸 | 128×128 (屏幕全尺寸) |
| 颜色数 | 建议 ≤ 16 色 (4-bit 调色板最优) |
| 背景 | 透明 (alpha=0 的区域不绘制) |

**关于颜色数:**
- 默认使用 4-bit 调色板 (16 色)，适合像素画风格
- 如果某个精灵需要更多颜色，在文件名末尾加 `_hq` 后缀
  例: `lily_normal_hq_f0.png` → 使用 RGB565 全彩存储
- 大部分情况下 16 色足够表现像素画

---

## 构建流程

```bash
# 安装依赖 (首次)
pip install Pillow numpy

# 转换资源
python tools/convert_assets.py

# 编译固件 (PlatformIO 会自动调用转换脚本)
pio run
```

构建脚本会:
1. 扫描 `assets/` 下所有 PNG
2. 按命名规则解析类型
3. 转换为二进制格式 → `src/display/generated_assets/*.bin`
4. 生成索引头文件 → `src/display/generated_assets/generated_asset_index.h`
5. 编译时自动链接

---

## 添加新形态/新食物的步骤

1. 在 `src/core/game_state.h` 的 `Form` 枚举中添加新形态
2. 在 `assets/` 对应目录中放入 PNG 文件 (遵循命名规则)
3. 运行 `python tools/convert_assets.py`
4. 编译

无需修改渲染层或演出层代码。系统通过文件名自动识别并注册资源。

---

## 示例: 为 Black Saber 添加完整资源

```
assets/
├── idle/
│   ├── black_saber_normal_f0.png
│   ├── black_saber_normal_f1.png
│   ├── black_saber_normal_f2.png
│   ├── black_saber_normal_f3.png
│   ├── black_saber_sr_high_f0.png
│   ├── black_saber_sr_high_f1.png
│   ├── black_saber_hp_low_f0.png
│   └── black_saber_hp_critical_f0.png
├── eating/
│   ├── black_saber_generic_f0.png    ← 通用进食 (fallback)
│   ├── black_saber_generic_f1.png
│   ├── black_saber_generic_f2.png
│   ├── black_saber_cake_f0.png       ← 吃蛋糕专属 (可选)
│   ├── black_saber_cake_f1.png
│   └── black_saber_cake_f2.png
├── reaction/
│   ├── black_saber_like_f0.png
│   ├── black_saber_like_f1.png
│   ├── black_saber_dislike_f0.png
│   ├── black_saber_dislike_f1.png
│   ├── black_saber_umu_f0.png
│   ├── black_saber_eww_f0.png
│   ├── black_saber_satisfy_f0.png
│   ├── black_saber_abhor_f0.png
│   └── black_saber_perfect_f0.png
├── poke/
│   ├── black_saber_f0.png
│   └── black_saber_f1.png
└── random_idle/
    ├── black_saber_smirk_f0.png
    └── black_saber_cross_arms_f0.png
```
