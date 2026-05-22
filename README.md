# MIDI Mapper

MIDI 信号读取与键盘鼠标映射工具。读取计算机上的 MIDI 输入信号，映射为键盘按键和鼠标操作。

## 功能

- **多设备支持**：枚举所有 MIDI 输入设备，支持同时打开多个设备
- **实时监控**：查看所有 MIDI 消息（Note On/Off、CC、Pitch Bend、Aftertouch 等），方便手动配置映射
- **三种触发模式**：
  - `hold` — 按住直到释放（适合键盘类设备）
  - `toggle` — 按一次开，再按一次关（适合打击垫切换状态）
  - `tap` — 按下后自动释放（适合电子鼓等瞬间触发设备）
- **键盘映射**：MIDI 音符 → 键盘按键（支持单键和组合键）
- **鼠标映射**：MIDI 音符 → 鼠标点击，CC 控制器 → 鼠标移动/滚轮
- **自定义 CC 名称**：在配置文件中为你的设备定义 CC 编号的含义
- **可配置**：通过 `midi_map.ini` 文件自定义所有映射规则

## 快速开始

1. 双击 `midi_mapper.exe` 运行
2. 输入 `list` 查看 MIDI 设备
3. 输入 `open 0` 打开第一个设备
4. 输入 `monitor on` 查看实时 MIDI 信号
5. 根据看到的音符号/CC号编辑 `midi_map.ini`
6. 输入 `reload` 重新加载配置

## 命令列表

| 命令 | 说明 |
|------|------|
| `list` | 列出所有 MIDI 输入设备 |
| `open <id>` | 打开指定设备 |
| `close <id>` | 关闭指定设备 |
| `openall` | 打开所有设备 |
| `monitor [on/off]` | 开关实时 MIDI 监控 |
| `mapping [on/off]` | 开关键鼠映射 |
| `mode <hold/toggle/tap>` | 切换触发模式（运行时） |
| `reload` | 重新加载配置文件 |
| `status` | 显示当前状态 |
| `quit` | 退出程序 |

## 配置文件 (midi_map.ini)

### 音符映射 [NoteMap]
```ini
60 = KEY_A          # 音符60(C4) -> 按键A
36 = MOUSE_LEFT     # 音符36 -> 鼠标左键
48 = KEY_CTRL+KEY_C # 音符48 -> Ctrl+C
```

### CC映射 [CCMap]
```ini
1 = MOUSE_MOVE_Y    # CC1 -> 鼠标Y轴
7 = MOUSE_MOVE_X    # CC7 -> 鼠标X轴
10 = MOUSE_SCROLL   # CC10 -> 鼠标滚轮
64 = KEY_SPACE       # CC64 -> 空格键 (>=64按下, <64释放)
```

### 自定义CC名称 [CCNames]
```ini
# 为你的设备定义CC编号含义，monitor显示时更清晰
4 = Hi-Hat Pedal     # 电子鼓踩镲
16 = EQ High         # DJ控制器
```

### 设置 [Settings]
```ini
mouse_sensitivity = 30     # 鼠标灵敏度 (1-100)
cc_deadzone = 2            # CC死区
note_mode = hold           # 触发模式: hold / toggle / tap
tap_duration = 50          # tap模式按键持续时间(ms)
```

### 支持的动作
- 键盘: `KEY_A`~`KEY_Z`, `KEY_0`~`KEY_9`, `KEY_F1`~`KEY_F12`
- 特殊键: `KEY_SPACE`, `KEY_ENTER`, `KEY_ESC`, `KEY_TAB`, `KEY_BACKSPACE`
- 方向键: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`
- 修饰键: `KEY_SHIFT`, `KEY_CTRL`, `KEY_ALT`
- 鼠标: `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`
- 鼠标移动: `MOUSE_MOVE_X`, `MOUSE_MOVE_Y`, `MOUSE_SCROLL`

## 编译

需要 MinGW g++：
```
g++ -O2 -std=c++17 -o midi_mapper.exe midi_mapper.cpp -lwinmm -static
```
或直接运行 `build.bat`。
