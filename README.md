# XDriverTool

基于 Qt Widgets 的 Windows 桌面上位机，通过串口控制运行 RT-Thread 的 XDriver 步进电机驱动，并周期读取 `xdrv status` 显示状态与诊断信息。

## 功能

- 扫描并连接串口；可选择扫描到的端口，也可手动输入端口号（例如 `COM3`）。
- 串口参数可配置：波特率、数据位、校验位、停止位和流控；默认 **115200、8N1、无流控**，选择会保存在应用设置中。
- 每 200 ms（约 5 Hz）轮询驱动状态：运行状态、功率级、控制模式、位置、速度、电流、校准、堵转/过载标志。
- 连续保留最近 5 分钟的位置、速度和电流数据；每条曲线下方的滚动条可回看历史，鼠标滚轮可围绕指针所在时间缩放。串口日志显示在曲线下方，并隐藏周期状态轮询及其回显。
- 界面按参考图采用左侧串口/控制/状态/诊断栏、右侧纵向三曲线与曲线下方串口日志布局；实时诊断按“通信与系统负载”和“控制器与驱动参数”分组折叠展示。
- 显示 ISR、SPI、UART、CAN 诊断信息及控制器内部量。
- 提供位置、速度、电流目标控制，以及使能、失能/停止、刹车、编码器校准、设零点和保存配置操作。
- 收发日志保留最近 1200 行；手动命令框仅接受 `xdrv` 命令。

固件当前控制台使用 USART1（PB6 TX、PB7 RX），参数为 115200 8N1。上位机默认按此参数连接；若更换设备或固件配置，可在连接区更改串口参数。端口下拉框支持手动输入未扫描到的端口号。连接前需使用与 MCU 电平匹配的 USB 转串口设备，并共地、TX/RX 交叉连接。

## 构建

需要 C++17 编译器、CMake 3.16+ 和 Qt 5.15+ 或 Qt 6（`Widgets`、`SerialPort`；运行测试时还需要 `Test`）。本机已安装 Qt 6.8.3 MinGW 64-bit Kit 与匹配的 MinGW 13.1 到 `E:\06_Program\Qt`，可直接在 PowerShell 中执行：

```powershell
$qt = 'E:\06_Program\Qt\6.8.3\mingw_64'
$mingw = 'E:\06_Program\Qt\Tools\mingw1310_64\bin'
$env:PATH = "$mingw;$qt\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$qt"
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

如安装在其他位置，将 `$qt` 和 `$mingw` 替换为对应 Kit 路径。也可以在 Qt Creator 中打开本目录的 `CMakeLists.txt`，选择已安装的 Kit 后构建。

Windows 构建会自动将 Qt DLL、平台插件和 MinGW 运行库部署到 `build` 目录，因此可直接运行 `build\XDriverTool.exe`，无需额外设置 Qt 的运行时 `PATH`。

## 使用与安全

1. 启动 `XDriverTool`，刷新端口，选择连接驱动的串口并连接。
2. 确认状态区能持续刷新。位置命令单位为圈，速度为圈/秒，电流为 mA；位置范围按固件内部有符号位置表示限制，速度和电流范围会跟随状态中报告的配置上限。
3. 校准前确保电机空载、轴可以自由转动；校准会主动驱动电机转动。校准、设零点和保存配置操作会先发送 `xdrv off`。
4. `xdrv off` 是固件定义的失能/释放轴命令；`xdrv brake` 会短接两相线圈。两者效果不同，请按机构安全要求操作。

本程序不替代机械限位、急停或驱动器硬件保护。串口断开、状态刷新超时或驱动报错时，应停止下发运动命令并检查设备。
