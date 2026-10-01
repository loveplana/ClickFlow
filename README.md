# ClickFlow

[English](README.en.md) | **简体中文**

ClickFlow 是一个面向 Windows 10/11 x64 的本地鼠标自动化工具。它用 C17、Win32、Direct3D 11 和 Nuklear 编写，无账号、无会员、无遥测，V1 运行时不会访问网络。

![ClickFlow 英文界面](docs/images/clickflow-en.png)

## 功能

- **鼠标连点**：左键、中键、右键；单击或双击；按次数、时长或手动停止；跟随光标或固定坐标。
- **鼠标录制**：记录移动、按下、抬起、点击和滚轮，支持暂停、继续、保存、载入、改名、删除及变速回放。
- **可视化宏**：增删、复制、排序和编辑动作，设置速度、重复次数及宏专属全局热键。
- **设置**：深浅主题、默认点击间隔、启动/停止热键和紧急停止热键。
- **双语界面**：在设置中即时切换简体中文和 English，选择会写入本地配置。

默认按 `F8` 启动或停止连点。默认按 `Esc` 紧急停止任何正在运行的任务。程序启动后不会自动发送输入；同一时间只允许连点、录制或宏回放中的一项运行。

## 使用

1. 打开“连点”，设置按键、间隔、结束条件和位置，按“开始连点”或 `F8`。
2. 打开“录制”，输入名称后开始操作；完成后可直接回放或保存为 JSON。
3. 打开“宏”，新建或载入宏，添加并编辑动作，确认后运行。宏运行前会重新校验当前多显示器布局和坐标。
4. 遇到意外时按 `Esc`。停止请求可中断长时间等待，程序会释放自己保持的鼠标按键。

### 验证连点次数

运行 `clickflow_click_counter.exe`，把鼠标放在窗口中的深色测试区域，再从 ClickFlow 启动连点。测试器会分别显示左键、中键、右键和总点击数；点击“重置计数”、按 `R` 或按 `Delete` 可以清零。这样可以在不影响其他应用数据的情况下核对点击按键和次数。

录制文件的 `kind` 为 `recording`，建议扩展名 `.cfr.json`；宏文件的 `kind` 为 `macro`，建议扩展名 `.cfm.json`。两者都是 UTF-8、`schema_version: 1` 的 JSON 文件。文件选择位置由用户决定。

配置保存在 `%LOCALAPPDATA%\ClickFlow\config.json`。损坏的配置会移动到 `%LOCALAPPDATA%\ClickFlow\backups`，随后恢复默认值。删除 ClickFlow 程序目录和这个数据目录即可完成卸载；程序不写注册表、不安装服务，也不创建开机启动项。

## 构建

需要 CMake 3.24+、Ninja 和 MinGW-w64 GCC。项目已经固定并内置 Nuklear 与 cJSON 源码。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

生成的 `build\clickflow.exe` 是 Windows GUI 程序，`build\clickflow_click_counter.exe` 是点击计数验证工具。发布时请把 `README.md`、`README.en.md`、`LICENSES.md` 和 `third_party` 中的许可证一起提供。

## 限制

- V1 只录制和发送鼠标输入，不录制键盘，不识别图片，也不向后台窗口定向发送输入。
- 回放依赖录制时的虚拟桌面布局；显示器布局变化后会拒绝执行，以避免点击错误位置。
- 某些以管理员身份运行的程序可能不接受来自普通权限 ClickFlow 的输入。
- 全局热键若被其他程序占用，需要在“设置”中更换。
- V1 没有云同步、脚本执行、更新器或 AI 接口。AI 辅助生成宏的方向记录在 `docs/ROADMAP.md`。

ClickFlow 源代码使用 [MIT License](LICENSE)。第三方组件及其许可证见 [LICENSES.md](LICENSES.md)。
