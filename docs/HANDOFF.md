# ClickFlow 开发交接

更新时间：2026-09-30

## 当前状态

- V1 功能、界面、文档和 Release 构建已经完成。
- 当前分支：`feature/clickflow-v1`
- 工作树：`D:\chatgpt\.worktrees\clickflow-v1`
- 基础分支：`main`
- 主工作区中的可运行发布包：`D:\chatgpt\build-release`
- 当前尚未合并到 `main`，工作树和分支需要保留。

## 已完成的功能

- 鼠标连点：三键、单击/双击、间隔、次数/时长/手动停止、当前/固定坐标。
- 鼠标录制与回放：暂停、继续、实时动作数和时长、变速、重复、保存、载入、改名、删除。
- 可视化宏：动作增删、复制、排序、参数编辑、速度、重复次数、专属全局热键。
- 设置：深浅主题、默认参数、启动/停止热键、紧急停止热键。
- 本地 JSON 原子存储、损坏配置备份、单实例、DPI 感知和安全停止。
- V2 AI 学习录制操作的边界和安全流程已写入 `docs/ROADMAP.md`，V1 未加入网络代码。

## 最近验证

- Debug 构建与 CTest：通过。
- Release 构建与 CTest：通过。
- Release GUI：窗口可创建、响应并干净退出。
- 依赖：仅 Windows 系统 DLL，无 MinGW 运行库 DLL。
- 30 秒 Release 资源采样：约 74.6 MiB，248–249 个句柄，无持续增长。
- Release EXE SHA-256：`400980B8D4AAF0A58669D00CA1229636C0033DD84FE13714F535FEA24C35F56B`。

## 明天继续

1. 先运行 `D:\chatgpt\build-release\clickflow.exe` 做人工体验。
2. 决定分支处理方式：本地合并到 `main`、推送并创建 PR，或继续保留分支。
3. 若继续开发 V2，先为 AI 服务商、隐私脱敏、凭据存储、响应 schema、成本、重试和提示注入另写设计，不直接在 V1 上添加 API 调用。
4. 如有专用测试环境，可补做一小时真实连点耐久测试；当前只完成了 30 秒资源稳定性检查。

恢复工作时进入：

```powershell
Set-Location D:\chatgpt\.worktrees\clickflow-v1
git status --short --branch
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```
