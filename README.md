# ShakeCare 老人跌倒与呼救原型

ShakeCare 是一个基于 Waveshare ESP32-S3-Matrix 的老人照护 Hackathon 原型：把一块板子做成老人手腕端，用 IMU 判断异常冲击、摇手呼救和疑似昏倒；另一块板子做成护工端，通过 ESP-NOW 接收状态，同步显示颜色，并开启本地 Wi-Fi 数据面板供电脑查看。

当前 Demo 已完成从“手腕端检测”到“护工端显示 + 网页面板”的闭环。

## 已完成功能

- 老人手腕端读取 QMI8658 IMU 三轴加速度。
- 8x8 RGB Matrix 显示三种状态：绿色正常、黄色观察、红色报警。
- 检测逻辑支持普通冲击、严重冲击、黄色状态后的摇手呼救、黄色状态后 10 秒几乎静止报警。
- 老人端通过 ESP-NOW MAC 单播把状态发送到护工端。
- 护工端不做 IMU 识别，只接收状态、显示同色 LED，并提供电脑网页数据面板。
- 护工端网页包含实时状态、8x8 预览、基础指标、bash 风格 log、timeline hover 提示、状态变化动画。
- 已完成路演 PPT 和预览图。

## 项目结构

```text
shakecare/
  README.md
  assets/
    shakecare-product-hero.png
  firmware/
    elder_wristband/
      elder_wristband.ino
      shakecare_shared.h
    caregiver_hub/
      caregiver_hub.ino
      dashboard_page.h
      shakecare_shared.h
    tools/
      mac_address_reader/
        mac_address_reader.ino
  presentations/
    roadshow/
      ShakeCare_Roadshow_Pitch.pptx
      preview/
        slide-1.png ... slide-9.png
      qa/
        ShakeCare_Roadshow_Pitch.inspect.ndjson
```

### Firmware 角色

| 路径 | 烧录对象 | 作用 |
| --- | --- | --- |
| `firmware/elder_wristband/elder_wristband.ino` | 老人手腕端 | 读取 IMU、判断状态、显示 LED、通过 ESP-NOW 发送状态 |
| `firmware/caregiver_hub/caregiver_hub.ino` | 护工端 | 接收 ESP-NOW、显示 LED、开启 Wi-Fi AP 和网页面板 |
| `firmware/tools/mac_address_reader/mac_address_reader.ino` | 任意板子 | 打印 STA MAC，用于配置 ESP-NOW 单播 |
| `firmware/*/shakecare_shared.h` | 老人端和护工端本地头文件 | 状态枚举、消息结构、LED 参数、矩阵方向映射 |
| `firmware/caregiver_hub/dashboard_page.h` | 护工端网页 | 内嵌 HTML/CSS/JS 数据面板 |

说明：`shakecare_shared.h` 在老人端和护工端各放一份，是为了让 Arduino IDE 直接打开单个 sketch 文件夹时也能编译。两份内容需要保持一致。

## 硬件设定

- 开发板：Waveshare ESP32-S3-Matrix
- LED Matrix：板载 8x8 RGB，64 pixels
- LED 数据引脚：GPIO 14
- LED 格式：`NEO_RGB + NEO_KHZ800`
- IMU：QMI8658
- IMU I2C：SDA GPIO 11，SCL GPIO 12
- IMU I2C 地址：`0x6B`
- Arduino FQBN：`esp32:esp32:esp32s3`

当前矩阵方向修正在两个 `shakecare_shared.h` 中保持一致：

```cpp
inline uint16_t shakeCarePixelIndex(uint8_t row, uint8_t col) {
  return (SHAKECARE_WIDTH - 1 - col) * SHAKECARE_WIDTH + row;
}
```

## 状态逻辑

### 颜色含义

| 状态 | 颜色 | 含义 |
| --- | --- | --- |
| `STATE_NORMAL` | 绿色 | 正常活动 |
| `STATE_WARNING` | 黄色 | 疑似跌倒或异常，需要观察 |
| `STATE_ALERT` | 红色 | 严重冲击、主动摇手呼救，或疑似昏倒 |

### 老人端判断流程

1. 每 `40ms` 读取一次加速度。
2. 计算 `mag`：当前加速度模长，用来判断瞬时冲击强度。
3. 计算 `delta`：相邻两次加速度变化量，用来判断运动突变。
4. 计算 `accelActivity`：近期加速度活动累积量。它不是物理距离，而是“最近一段时间动作强度”的简化指标。
5. 正常状态下，`accelActivity` 超过门槛后才判断普通冲击或严重冲击。
6. 严重冲击直接红灯。
7. 普通冲击进入黄灯。
8. 黄灯后先等待 `SHAKE_ARM_MS`，避免摔倒余震立刻被误判成主动摇手。
9. 黄灯状态下连续检测到摇手，进入红灯。
10. 黄灯状态下连续 `10s` 几乎无运动，进入红灯。
11. 黄灯超过 `15s` 未升级报警则自动回到绿色。

## 当前关键参数

这些参数在 `firmware/elder_wristband/elder_wristband.ino` 顶部。

| 参数 | 当前值 | 作用 |
| --- | ---: | --- |
| `SAMPLE_MS` | `40` | IMU 采样间隔 |
| `IMPACT_DELTA_G` | `1.35` | 普通冲击的加速度变化阈值 |
| `IMPACT_MAG_G` | `2.5` | 普通冲击的加速度模长阈值 |
| `SEVERE_IMPACT_DELTA_G` | `3.5` | 严重冲击的加速度变化阈值 |
| `SEVERE_IMPACT_MAG_G` | `4.5` | 严重冲击的加速度模长阈值 |
| `ACCEL_ACTIVITY_G` | `3.5` | 近期动作强度门槛 |
| `ACCEL_ACTIVITY_DECAY` | `0.86` | 动作强度衰减系数 |
| `SHAKE_DELTA_G` | `0.65` | 摇手呼救单次动作阈值 |
| `SHAKE_HITS_REQUIRED` | `4` | 触发呼救需要的摇手次数 |
| `SHAKE_ARM_MS` | `1200` | 黄灯后摇手检测保护时间 |
| `STILL_DELTA_G` | `0.045` | 静止判断阈值 |
| `STILL_ALERT_MS` | `10000` | 黄灯后静止报警时间 |
| `WARNING_CLEAR_MS` | `15000` | 黄灯自动恢复时间 |
| `ESPNOW_SEND_MS` | `150` | ESP-NOW 状态发送间隔 |

## ESP-NOW 通信

当前采用 MAC 单播，不是广播。

消息结构在老人端和护工端的 `shakecare_shared.h` 中保持一致：

```cpp
struct ShakeCareMessage {
  uint8_t type;
  uint8_t state;
  uint32_t seq;
};
```

老人端会把 `state` 发送给 `CAREGIVER_MAC`。护工端收到后更新 LED、网页 `/status` JSON 和页面 log。

如果护工端超过 `1000ms` 没收到 ESP-NOW 消息，会显示黄色，表示连接或同步异常。

## 烧录流程

下面的命令用于编译验证。实际上传可以在 Arduino IDE 中打开对应 sketch 文件夹后选择端口点击 Upload；如果使用 Arduino CLI，把命令中的 `compile` 换成 `upload -p COMx`，其中 `COMx` 是你的板子串口。

1. 先给护工端烧录 MAC 工具：

   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "firmware/tools/mac_address_reader"
   ```

   上传后打开 Serial Monitor，波特率 `115200`，复制输出的 `STA MAC`。

2. 把 MAC 从 `AA:BB:CC:DD:EE:FF` 改成下面格式，填入老人端 `CAREGIVER_MAC`：

   ```cpp
   const uint8_t CAREGIVER_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
   ```

   当前代码里已经填过一次：

   ```cpp
   const uint8_t CAREGIVER_MAC[] = {0xA0, 0xF2, 0x62, 0xEB, 0x27, 0x88};
   ```

   如果换了护工端板子，需要重新读取并替换。

3. 烧录老人手腕端：

   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "firmware/elder_wristband"
   ```

4. 烧录护工端：

   ```powershell
   & "$env:LOCALAPPDATA\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 "firmware/caregiver_hub"
   ```

5. 电脑连接护工端 Wi-Fi：

   - SSID：`ShakeCare-Caregiver`
   - 密码：`12345678`
   - 页面：`http://192.168.4.1`

Arduino IDE 中也可以直接打开对应 sketch 文件夹：

- 老人端：`firmware/elder_wristband`
- 护工端：`firmware/caregiver_hub`
- MAC 工具：`firmware/tools/mac_address_reader`

## 护工端网页接口

护工端启动 AP 后提供两个 HTTP 入口：

| 路径 | 作用 |
| --- | --- |
| `/` | 数据面板网页 |
| `/status` | 实时状态 JSON |

`/status` 返回示例：

```json
{
  "online": true,
  "state": 0,
  "stateKey": "normal",
  "stateText": "NORMAL",
  "ageMs": 120,
  "seq": 42,
  "uptimeMs": 60000,
  "clients": 1
}
```

网页端行为：

- 状态变化时立即追加 bash 风格 log。
- 状态不变时每 5 秒追加一次 heartbeat log。
- timeline 每条记录可 hover 查看当时状态、连接情况和 seq。
- `Freeze log` 暂停新增 log。
- `Clear log` 清空 log 和 timeline。

## 技术迭代记录

### v0.1 单板检测

- 完成 QMI8658 初始化与三轴加速度读取。
- 完成绿色、黄色、红色三态 LED 显示。
- 完成普通冲击、严重冲击、摇手呼救、黄灯后静止报警的状态机。
- 将原先“距离”概念改为 `accelActivity`，避免误导成物理位移。

### v0.2 双板互联

- 增加 ESP-NOW MAC 单播。
- 老人端和护工端拆成不同 sketch。
- 护工端不读取 IMU，只接收状态并显示。
- 增加 MAC 读取工具 sketch。

### v0.3 数据面板与项目整理

- 护工端增加 Wi-Fi AP 和本地网页面板。
- 网页从 EventStream 改成轮询 `/status`，并用 bash log + timeline 展示状态历史。
- 页面改为浅色数据面板风格，加入状态动画和矩阵预览。
- 生成路演 PPT：`presentations/roadshow/ShakeCare_Roadshow_Pitch.pptx`。
- 整理项目结构，把 firmware、网页模板、路演材料拆分到清晰目录；公共协议头文件放进各自 sketch，保证 Arduino IDE 直接编译。

## 路演材料

- PPT：`presentations/roadshow/ShakeCare_Roadshow_Pitch.pptx`
- PPT 预览图：`presentations/roadshow/preview/`
- 产品概念图：`assets/shakecare-product-hero.png`

PPT 重点是产品创意、用户痛点、Demo 闭环、社会价值和商业落点，不展开过多底层技术细节。

## 痛点与价值

独居老人或行动不便者在跌倒、眩晕、低血糖等情况下，可能无法立刻拿起手机或按下呼叫按钮。传统摄像头方案又有明显隐私压力。

ShakeCare 用低成本、低侵入的可穿戴原型捕捉两类关键信号：

- 被动异常：突然冲击、跌倒后静止。
- 主动求救：黄色观察状态后的连续摇手。

它不试图替代医疗设备，而是为家庭照护、养老院夜间巡护、社区护理提供更早、更直观的风险提示。

## 后续方向

- 增加红灯后的手动复位逻辑。
- 串口输出 `mag`、`delta`、`accelActivity` 和触发原因，方便现场调参。
- 增加佩戴方向校准，减少手环绑法差异带来的误判。
- 把护工端网页事件上传到电脑端或云端，形成事件记录。
- 增加电池、电量提示、震动反馈和外壳绑带。
- 收集走路、坐下、拍桌、摔落、摇手等样本，降低误报和漏报。
