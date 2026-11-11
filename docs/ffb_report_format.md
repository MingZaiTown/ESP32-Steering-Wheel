# ESP32 方向盘 FFB 报告格式速查

> 依据：`main/usb_hid.c` 里编译后的 `hid_1ffb_desc[]`（1078 字节）反解析得到。
> 分析工具：解析 `build/strwhl.elf` 中 `hid_1ffb_desc` 符号的原始字节。
> 改动描述符后请重新跑一遍解析，本文档的偏移量会失效。

---

## 0. 先搞清楚：`tud_hid_set_report_cb` 有两条入站路径

主机发给设备的报告分两种传输方式，**规则不一样**，这是最容易踩坑的地方。

| 路径 | 触发 | `report_id` 参数 | `buffer` 内容 | 你日志里的样子 |
|---|---|---|---|---|
| **Feature 报告**（控制传输 SET_REPORT） | 主机写 Feature，如 `0x11` Create New Effect | **真实报告 ID**（如 `0x11`） | **不含 ID**，`buffer[0]` 是第一个字段 | `id=0x11 type=3 len=3: 01 00 00` |
| **Output 报告**（中断 OUT 端点） | 主机把 Output 报告发到 OUT 端点 | **恒为 0** | **含 ID**，`buffer[0]` 就是报告 ID | `id=0x00 type=2 len=18: 01 01 04 ...` |

- `report_type`：`1=INPUT`，`2=OUTPUT`，`3=FEATURE`。
- 所以判断一个包是什么报告，规则是:
  - 如果 `report_id != 0` → 用 `report_id` 判断（Feature 控制路径，buffer 从字段 0 开始）。
  - 如果 `report_id == 0 && type == OUTPUT` → **用 `buffer[0]` 判断**（OUT 端点路径，字段从 `buffer[1]` 开始）。

对应 TinyUSB 源码 `hid_device.c`：

```c
// 控制传输 SET_REPORT：先剥掉 ID，再交给回调（:337-342）
if ((report_id != HID_REPORT_TYPE_INVALID) && (report_len > 1) && (report_id == report_buf[0])) {
    report_buf++;
    report_len--;
}
tud_hid_set_report_cb(hid_itf, report_id, ..., report_buf, report_len);

// 中断 OUT 端点：整包原样交给回调，report_id 固定 0（:410）
tud_hid_set_report_cb(instance, 0, HID_REPORT_TYPE_OUTPUT, p_epbuf->epout, xferred_bytes);
```

> GET 方向同理：`tud_hid_get_report_cb` 的 `buffer` **不含 ID**，你返回的是**载荷长度**，TinyUSB 自己会补上 ID。

---

## 1. 报告总表

| ID | 方向 | 类型 | 名称 | 载荷长度（不含 ID） | 传输 |
|----|------|------|------|------|------|
| `0x01` | OUT | Output | Set Effect（设置效果） | **17** | OUT 端点 |
| `0x02` | OUT | Output | Set Envelope（包络） | 13 | OUT 端点 |
| `0x03` | OUT | Output | Set Condition（条件：弹簧/阻尼/摩擦/惯性） | 14 | OUT 端点 |
| `0x04` | OUT | Output | Set Periodic（周期：正弦/方波/三角…） | 11 | OUT 端点 |
| `0x05` | OUT | Output | Set Constant Force（恒定力） | 3 | OUT 端点 |
| `0x06` | OUT | Output | Set Ramp Force（斜坡） | 5 | OUT 端点 |
| `0x0A` | OUT | Output | Effect Operation（播放/停止） | 3 | OUT 端点 |
| `0x0B` | OUT | Output | Block Free（释放效果槽） | 1 | OUT 端点 |
| `0x0C` | OUT | Output | Device Control（使能/复位/全停…） | 1（位域） | OUT 端点 |
| `0x0D` | OUT | Output | Device Gain（总增益） | 1 | OUT 端点 |
| `0x11` | 主机→设备 | Feature | Create New Effect（申请槽位） | 3 | 控制 SET_REPORT |
| `0x12` | 设备→主机 | Feature | Block Load（分配结果） | 4 | 控制 GET_REPORT |
| `0x13` | 设备→主机 | Feature | PID Pool（能力查询） | 4 | 控制 GET_REPORT |
| `0x02` | 设备→主机 | Input | PID State（设备状态，可选上报） | 1 | IN 端点 |
| `0x01` | 设备→主机 | Input | Gamepad（方向盘轴+按钮上报） | 24 | IN 端点 |

> **关于"方向"的说明**：HID 里 `Input / Output / Feature` 是**报告类型**，不是数据方向。
> - `Input` 报告：设备→主机（IN 端点）。
> - `Output` 报告：主机→设备（OUT 端点）。
> - `Feature` 报告：**双向**，靠控制传输，方向取决于主机做的是 GET 还是 SET。
>   - `0x11` Create New Effect 是 Feature，主机用 **SET_REPORT 写给它** → 数据是主机→设备。
>   - `0x12` / `0x13` 是 Feature，主机用 **GET_REPORT 读** → 数据是设备→主机。
>
> 所以 `0x11` 虽然数字上像 Output，但它本质是 Feature SET，在 `tud_hid_set_report_cb` 里收到时 `report_id=0x11`、`buffer` 不含 ID。

> 注意 `ID` 只在**同一方向**内唯一。`0x01` 既是 OUT 的 Set Effect，也是 IN 的 Gamepad；`0x02` 既是 OUT 的 Envelope，也是 IN 的 PID State。

---

## 2. 各报告字段布局

以下偏移量均从 **载荷第 0 字节**算起（不含报告 ID）。OUT 端点包的真实 `buffer` 要整体 +1。

### 0x01 OUT — Set Effect（17 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | 效果槽 1..8 |
| 1 | 8 | Effect Type | `0F:25`（数组） | 见 §3 效果类型表 1..11 |
| 2..3 | 16 | Duration | `0F:50` | 持续时间，ms |
| 4..5 | 16 | Trigger Repeat Interval | `0F:54` | 触发重复间隔，ms |
| 6..7 | 16 | Sample Period | `0F:51` | 采样周期，ms |
| 8..9 | 16 | Start Delay | `0F:A7` | 启动延迟，ms |
| 10 | 8 | Gain | `0F:52` | 0..255 |
| 11 | 8 | Trigger Button | `0F:53` | 触发按钮 1..8，0=无 |
| 12 bit0 | 1 | Axis Enable X | `01:30` | X 轴使能 |
| 12 bit1 | 1 | Direction Enable | `0F:56` | 方向使能 |
| 12 bit2..7 | 6 | — | — | 填充 |
| 13..14 | 16 | Direction | `0F:57` | 0..36000，单位 0.01°，即 0..360.00° |
| 15..16 | 16 | Type Specific Block Offset | `0F:58` | 0..32765 |

示例 `01 01 04 20 4E 00 00 00 00 E8 03 FF 00 02 28 23 00 00`（含 ID）：
- ID=`01`, index=`01`, type=`04`(Sine), duration=`0x4E20`=20000ms
- trigger repeat=`0`, sample period=`0`, start delay=`0x03E8`=1000ms
- gain=`0xFF`, trigger button=`0`
- byte12=`0x02` → Direction Enable=1；Direction=`0x2328`=9000 → 90.00°；block offset=0

### 0x02 OUT — Set Envelope（13 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | |
| 1..2 | 16 | Attack Level | `0F:5B` | 0..0x7FFF |
| 3..4 | 16 | Fade Level | `0F:5D` | 0..0x7FFF |
| 5..8 | 32 | Attack Time | `0F:5C` | ms |
| 9..12 | 32 | Fade Time | `0F:5E` | ms |

### 0x03 OUT — Set Condition（14 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | |
| 1 bit0..5 | 6 | Parameter Block Offset | `0F:23` | 0..3 |
| 1 bit6..7 | 2 | Type Specific Block Offset | `0A:01` | 序号/实例 |
| 2..3 | 16 | CP Offset | `0F:60` | 有符号 |
| 4..5 | 16 | Positive Coefficient | `0F:61` | 有符号 |
| 6..7 | 16 | Negative Coefficient | `0F:62` | 有符号 |
| 8..9 | 16 | Positive Saturation | `0F:63` | |
| 10..11 | 16 | Negative Saturation | `0F:64` | |
| 12..13 | 16 | Dead Band | `0F:65` | |

### 0x04 OUT — Set Periodic（11 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | |
| 1..2 | 16 | Magnitude | `0F:70` | 0..0x7FFF |
| 3..4 | 16 | Offset | `0F:6F` | 有符号 |
| 5..6 | 16 | Phase | `0F:71` | 0..35999，0.01° |
| 7..10 | 32 | Period | `0F:72` | ms |

### 0x05 OUT — Set Constant Force（3 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | |
| 1..2 | 16 | Magnitude | `0F:70` | 有符号 -32767..32767 |

示例 `05 01 00 20`（含 ID）：index=1, magnitude=`0x2000`=8192。

### 0x06 OUT — Set Ramp Force（5 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | |
| 1..2 | 16 | Ramp Start | `0F:75` | 有符号 |
| 3..4 | 16 | Ramp End | `0F:76` | 有符号 |

### 0x0A OUT — Effect Operation（3 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | |
| 1 | 8 | Effect Operation | `0F:78` | **1=Start，2=Start Solo，3=Stop** |
| 2 | 8 | Loop Count | `0F:7C` | 0..255 |

示例 `0A 01 03 00`（含 ID）：index=1, op=3 → **Stop**。

### 0x0B OUT — Block Free（1 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | 释放该槽 |

示例 `0B 01`：释放 1 号槽。

### 0x0C OUT — Device Control（1 字节，位域）

| 位 | 字段 | Usage |
|---|---|---|
| bit0 | Enable Actuators | `0F:97` |
| bit1 | Disable Actuators | `0F:98` |
| bit2 | Stop All Effects | `0F:99` |
| bit3 | Device Reset | `0F:9A` |
| bit4 | Device Pause | `0F:9B` |
| bit5 | Device Continue | `0F:9C` |
| bit6..7 | 填充 | — |

示例：`0C 01`=Enable Actuators，`0C 04`=Stop All，`0C 08`=Device Reset。

### 0x0D OUT — Device Gain（1 字节）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Device Gain | `0F:7E` | 0..255（物理 0..10000） |

### 0x11 Feature — Create New Effect（3 字节，主机写入）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Type | `0F:25` | 1..11，见 §3 |
| 1 + 2低2位 | 10 | Byte Count | `01:3B` | 保留，通常 0 |
| 2高6位 | 6 | 填充 | — | 0 |

示例 `01 00 00`：申请一个 Constant 效果的槽位。
**固件动作：找一个空闲槽（1..8）记下，供后面的 `0x12` GET 返回。**

### 0x12 Feature — Block Load（4 字节，主机读取）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0 | 8 | Effect Block Index | `0F:22` | 刚分配的槽号 |
| 1 | 8 | Block Load Status | `0F:8B` | **1=Success，2=Full，3=Error** |
| 2..3 | 16 | Pool Available | `0F:AC` | 剩余池空间 |

### 0x13 Feature — PID Pool（4 字节，主机读取）

| 偏移 | 位宽 | 字段 | Usage | 说明 |
|---|---|---|---|---|
| 0..1 | 16 | RAM Pool Size | `0F:80` | |
| 2 | 8 | Simultaneous Effects Max | `0F:83` | 本工程 = `MAX_EFFECTS` = 8 |
| 3 bit0 | 1 | Device Managed Pool | `0F:A9` | **必须=1**，否则内核 `-EPERM` |
| 3 bit1 | 1 | Shared Parameter Blocks | `0F:AA` | |
| 3 bit2..7 | 6 | 填充 | — | 0 |

### 0x02 IN — PID State（1 字节，可选上报）

| 位 | 字段 | Usage |
|---|---|---|
| bit0 | Device Paused | `0F:9F` |
| bit1 | Actuators Enabled | `0F:A0` |
| bit2 | Safety Switch | `0F:A4` |
| bit3 | Actuator Power | `0F:A6` |
| bit4 | Effect Playing | `0F:94` |
| bit5..7 | 填充 | — |

### 0x01 IN — Gamepad（24 字节，设备上报）

| 偏移 | 字节 | 字段 |
|---|---|---|
| 0..7 | 8 | 64 个按钮位 |
| 8..9 | 2 | X |
| 10..11 | 2 | Y |
| 12..13 | 2 | Z |
| 14..15 | 2 | RX |
| 16..17 | 2 | RY |
| 18..19 | 2 | RZ |
| 20..21 | 2 | Throttle |
| 22..23 | 2 | Rudder |

轴为 16 位有符号，范围 -32767..32767。整包（含 ID）共 25 字节，用 `tud_hid_report(0x01, buf, 24)` 发送。

---

## 3. 效果类型编号（Effect Type，1..11）

| 值 | 类型 | Usage | 配套参数报告 |
|---|---|---|---|
| 1 | Constant Force 恒定力 | `0x26` | `0x05` |
| 2 | Ramp 斜坡 | `0x27` | `0x06` |
| 3 | Square 方波 | `0x30` | `0x04` |
| 4 | Sine 正弦 | `0x31` | `0x04` |
| 5 | Triangle 三角 | `0x32` | `0x04` |
| 6 | Sawtooth Up 锯齿上 | `0x33` | `0x04` |
| 7 | Sawtooth Down 锯齿下 | `0x34` | `0x04` |
| 8 | Spring 弹簧 | `0x40` | `0x03` |
| 9 | Damper 阻尼 | `0x41` | `0x03` |
| 10 | Inertia 惯性 | `0x42` | `0x03` |
| 11 | Friction 摩擦 | `0x43` | `0x03` |

---

## 4. 典型流程（从日志里能直接对上）

```
# ① 内核初始化（每次插拔都会走一遍）
[S] 0x0D FF        Device Gain = 最大
[S] 0x0B 01        Block Free 1（擦掉 autocenter 探针效果）
[S] 0x0D BF        Gain = 0xBF
[S] 0x0C 08        Device Reset
[S] 0x0C 04        Stop All Effects
[S] 0x0C 01        Enable Actuators

# ② 上传一个效果（例如正弦）
[S] 0x11 04 00 00  申请槽位，类型=4(Sine)          → 固件分配槽号 N
[S] 0x12 (GET)     主机读 Block Load                → 固件回 {N, 1=Success, 剩余池}
[S] 0x01 ...       Set Effect（总参数，含 duration/方向）
[S] 0x04 ...       Set Periodic（magnitude/offset/phase/period）
[S] 0x02 ...       Set Envelope（attack/fade）

# ③ 播放
[S] 0x0A <idx> 01 00   Effect Operation = Start
# ④ 停止
[S] 0x0A <idx> 03 00   Effect Operation = Stop
# ⑤ 销毁
[S] 0x0B <idx>         Block Free
```

> 你那份日志里 `0A 01 03 00` 出现了很多次，但**没有 `0A 01 01 00`（Start）**，说明当时只发了 Stop。真正点“Play”时才会出现 `01`。

---

## 5. 固件需要做什么（映射到代码）

| 收到 | 该干什么 |
|---|---|
| `0x11` Create New Effect | 分配空闲槽，记下 `last_alloc_idx` 和效果类型 |
| `0x12` GET Block Load | 返回 `{last_alloc_idx, 1, 剩余池}` |
| `0x13` GET Pool | 返回 `{0xFFFF, 8, 0x01}`（注意 buffer 不含 ID，返回长度 4） |
| `0x01` Set Effect | 保存该槽的总参数（type/duration/direction/gain…） |
| `0x03/0x04/0x05/0x06` | 保存该槽的类型专有参数 |
| `0x0A` Effect Operation | op=1/2 → `active=true`；op=3 → `active=false` |
| `0x0B` Block Free | 清空该槽 |
| `0x0C` Device Control | bit2 停止全部、bit3 复位、bit0/1 使能/失能电机 |
| `0x0D` Device Gain | 保存总增益，乘到输出力上 |

然后在 `ffb_task` 里：遍历 `effects[1..8]`，对 `active` 的按类型算力，叠加后限幅，驱动电机（注意要支持正反方向）。

---

## 6. 快速自检命令

```bash
# 已注册 FF？
cat /sys/class/input/event5/device/capabilities/ev   # 应含 bit21 (EV_FF)
cat /sys/class/input/event5/device/capabilities/ff   # 非零

# 看协议
fftest /dev/input/event5
ffcfstress /dev/input/event5     # 自动左右恒定力，验证电机正反转
```

---

## 7. 真实游戏抓包（Euro Truck Simulator 2 / ATS）

> 采集方式：Windows 端串口抓 `printf` 日志。
> 原始数据：`ReceivedTofile-COM9-2026_11_7_20-31-47.DAT`（3096 行，过滤后 3038 条报文）。

### 7.1 整场生命周期

| 阶段 | 报文 | 次数 | 含义 |
|---|---|---|---|
| 上电 / 复位 | `[GET] 0x13` | 1 | 内核问能力，答 `{0xFFFF, 8, 0x01}` |
| | `0C 08` | 1 | Device Reset（bit3）|
| | `0C 04` | 1 | Stop All Effects（bit2）|
| | `0D FF` | 1 | Device Gain = 255 |
| | `0x11 01 00 00` | **1** | **申请槽位，Effect Type = 1（Constant）** |
| | `[GET] 0x12` | 1 | Block Load 回 index + Status |
| 进游戏 / 开车 | `05 01 <mag>` → `01 01 01 …` → `0A 01 01 01` | **700 轮** | 改力矩 → 改参数 → Start |
| 按 Esc 暂停 | `0A 01 03 00` | **930** | 每帧反复 Stop，属正常现象 |
| 退出游戏 | `0B 01` / `0C 08` | 1 / 1 | Block Free 槽 1 + Device Reset |

**结论：整场只申请了一次槽位，自始至终只用 1 号槽的 Constant 效果。**
=> 只实现恒定力即可获得完整手感，Periodic / Spring 一行都不用写。

### 7.2 Set Effect 实测解码

```
01 | 01 01 FF FF 00 00 00 00 00 00 FF FF 02 28 23 00 00
```

| 字段 | 原始 | 值 | 说明 |
|---|---|---|---|
| Effect Block Index | `01` | 1 | 槽 1 |
| Effect Type | `01` | 1 | Constant Force |
| Duration | `FF FF` | 65535 ms | ≈ 无限，实际靠 Stop 终止 |
| Trigger Repeat Interval | `00 00` | 0 | 无 |
| Sample Period | `00 00` | 0 | 无 |
| Start Delay | `00 00` | 0 | 立即 |
| Gain | `FF` | 255 | 满增益 |
| Trigger Button | `FF` | 无 | 不由按钮触发 |
| Axis Enable / Dir Enable | `02` | Direction Enable = 1 | |
| **Direction** | `28 23` | **9000 = 90.00°** | 全程恒定不变 |
| Type Specific Block Offset | `00 00` | 0 | |

**方向恒为 90°，因此方向信息完全由 `magnitude` 的符号承载** —— 单轴方向盘可以简化为：

```
力 = magnitude   // 直接带符号使用
```

### 7.3 力矩（`0x05`）实测特征

- 共 700 帧，其中 **316 帧非零**，其余为 `0x0000`（不施加力）
- 范围 **-3224 ~ +2637**，共 **238 个不同取值**
- 变化平滑连续，典型片段：

```
-3 → -20 → -134 → -439 → -865 → -1117 → -1340 → -1553 → … → -3224 → -2962 → … → +1687
```

- **远未到满量程 32767**，因此不能直接把 magnitude 当 PWM 占空比，
  必须做缩放映射，否则实际出力会小到几乎感觉不到。

### 7.4 实现时必须注意的三个坑

1. **`0x11` 是 Feature SET，`report_id = 0x11`（非 0）**
   不要写在 `else`（Output 分支）的 `switch (buffer[0])` 里，否则永远进不去。
   并且 Feature 路径的 `buffer` **不含 ID**，`buffer[0]` 就是 Effect Type。

2. **`0x01` 在 `0x05` 之后到达**（顺序：`05` → `01` → `0A`）
   因此 `0x01` 的处理**只能更新公共字段**（type / duration / direction / gain），
   **绝不能重置整个结构体**，否则会清掉刚收到的 magnitude。

3. **Start / Stop 是高频电平信号**（700 次 Start、930 次 Stop，暂停时每帧一次）
   `active` 只当布尔开关用，不要在 Start 里做一次性初始化。

### 7.5 最小可用实现清单

| 报文 | 动作 |
|---|---|
| `0x11` Feature SET | 找空闲槽（1..8），存 Type，记 `lst_blk_idx` |
| `0x12` GET | 回 `{lst_blk_idx, Status=1, 0xFF, 0xFF}` |
| `0x05` | `effects[idx].p.constant.magnitude = int16(buffer[1..2])` |
| `0x01` | 只更新 `type / duration / direction / gain` |
| `0x0A` | op=1/2 → `active = true`；op=3 → `active = false` |
| `0x0B` | `in_use = false`，槽位释放 |
| `0x0C` | bit2 清所有 `active`；bit3 复位所有槽 |
| `0x0D` | 保存全局 gain |

`ffb_task` 里：

```c
int32_t F = 0;
for (int i = 1; i <= MAX_EFFECTS; i++) {
    if (!effects[i].in_use || !effects[i].active) continue;
    if (effects[i].type == EFFECT_CONSTANT)
        F += effects[i].p.constant.magnitude;
}
F = F * global_gain / 255;      // 缩放，避免满量程直接打满电机
motor_set_force(clamp(F));      // 需支持正反方向
```
