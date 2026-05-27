# MH4G-Damage-Overlay
基于 C++ 和 ImGui 开发的《怪物猎人4G》(Citra模拟器) 外部独立显血与伤害飘字工具。采用 DirectComposition 图形管线解决 DWM 串行锁引起的帧步调抖动，并配合三级时钟异步内存扫描降低对模拟器的 CPU 开销。
## ⚙️ 配置文件参数说明 (config.ini)

程序运行时会读取同目录下的 `config.ini`。若文件不存在，则全量启用代码内的硬编码默认值。颜色字段支持 `#RRGGBB` 和 `#RRGGBBAA`，带 alpha 时最后两位表示透明度。

| 配置节 (Section) | 参数名 (Key) | 默认值 | 作用说明 |
| :--- | :--- | :--- | :--- |
| **[Renderer]** | FontPath | `C:\Windows\Fonts\bahnschrift.ttf` | 渲染使用的本地 TTF 字体绝对路径。 |
| | FontSize | `60` | 伤害飘字的基础字号大小（浮点数）。 |
| | ShowMonsterHP | `1` | 是否显示怪物 HP 文本，`0` 关闭。 |
| | ShowDamageNumbers | `1` | 是否显示伤害飘字，`0` 关闭。 |
| | DamageColor | `#FFB31A` | 伤害数字颜色，支持 `#RRGGBB` 或 `#RRGGBBAA`。 |
| | DamageShadowEnabled | `1` | 是否启用伤害数字阴影/描边，建议保持开启。 |
| | DamageShadowColor | `#000000D9` | 伤害阴影颜色，支持 alpha；末尾 `D9` 约等于 85% 不透明。 |
| | DamageShadowOffsetX | `2` | 投影相对文字的 X 偏移像素。 |
| | DamageShadowOffsetY | `2` | 投影相对文字的 Y 偏移像素。 |
| | DamageShadowThickness | `2` | 描边厚度，范围 `0-8`；越大越清晰但绘制次数更多。 |
| **[Logic]** | Lifetime | `90` | 单个伤害飘字在屏幕上的总生存帧数。 |
| | FadeTime | `30` | 伤害飘字在消失前开始执行淡出动画的帧数（需小于 Lifetime）。 |
| | XStaggerStep | `45` | 触发连续伤害时，X轴左右随机错开的像素步长。 |
| | OverlapMax | `10` | 同一时间段内允许向上堆叠的伤害数字最大数量，达到后重置堆叠。 |
| **[Scanner]** | HpMaxLimit | `40000` | 内存扫描时的血量上限过滤阈值，超出此数值的地址将被剔除。 |

<img width="1583" height="951" alt="Image" src="https://github.com/user-attachments/assets/41845dbc-4c86-4690-8fed-ec0fa0e32124" />
