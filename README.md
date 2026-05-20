# MIDI Mapper

MIDI 信号读取与键盘鼠标映射工具。读取计算机上的 MIDI 输入信号，映射为键盘按键和鼠标操作。

## 功能

- **多设备支持**：枚举所有 MIDI 输入设备，支持同时打开多个设备
- **实时监控**：查看所有 MIDI 消息（音符、CC、弯音等），方便手动配置映射
- **键盘映射**：MIDI 音符 → 键盘按键（支持单键和组合键）
- **鼠标映射**：MIDI 音符 → 鼠标点击，CC 控制器 → 鼠标移动/滚轮
- **可配置**：通过 `midi_map.ini` 文件自定义映射规则

## 快速开始

1. 双击 `midi_mapper.exe` 运行
2. 输入 `list` 查看 MIDI 设备
3. 输入 `open 0` 打开第一个设备
4. 输入 `monitor on` 查看实时 MIDI 信号
5. 根据看到的音符号编辑 `midi_map.ini`
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
1 = MOUSE_MOVE_Y    # 调制轮 -> 鼠标Y轴
7 = MOUSE_MOVE_X    # 音量 -> 鼠标X轴
10 = MOUSE_SCROLL   # 声像 -> 鼠标滚轮
64 = KEY_SPACE       # 延音踏板 -> 空格键
```

### 支持的动作
- 键盘: `KEY_A`~`KEY_Z`, `KEY_0`~`KEY_9`, `KEY_F1`~`KEY_F12`
- 特殊键: `KEY_SPACE`, `KEY_ENTER`, `KEY_ESC`, `KEY_TAB`, `KEY_BACKSPACE`
- 方向键: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`
- 修饰键: `KEY_SHIFT`, `KEY_CTRL`, `KEY_ALT`
- 鼠标: `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`
- 鼠标移动: `MOUSE_MOVE_X`, `MOUSE_MOVE_Y`, `MOUSE_SCROLL`

## 编译（如需修改源码）

需要 MinGW g++，编译命令：
```
g++ -O2 -std=c++17 -o midi_mapper.exe midi_mapper.cpp -lwinmm -static
```
或直接运行 `build.bat`。
