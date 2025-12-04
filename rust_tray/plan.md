# 系统托盘替换方案（已更新：大部分逻辑迁移至 Rust 库）

要点结论：
- 大部分托盘逻辑、i18n 与菜单处理已迁移到 Rust 库，主实现见 [`rust_tray/src/lib.rs`](rust_tray/src/lib.rs:1)。
- 对外 C API 以扩展接口为主：请使用 [`tray_init_ex`、`tray_loop`、`tray_exit` 等](rust_tray/include/rust_tray.h:61)；旧 `tray_init` 为遗留且不推荐使用。
- C++ 端使用薄包装器 [`src/system_tray_rust.cpp`](src/system_tray_rust.cpp:1) 与 Rust 库交互，CMake 已改为始终链接 Rust 实现。
- **Windows 深色模式现由 C++ 统一管理**：新增 [`src/platform/windows/win_dark_mode.cpp`](src/platform/windows/win_dark_mode.cpp:1) 在进程级别控制深色模式，Rust 托盘仅跟随系统设置。

关键文件（快速索引）：
- Rust 实现：[`rust_tray/src/lib.rs`](rust_tray/src/lib.rs:1)
- 国际化：[`rust_tray/src/i18n.rs`](rust_tray/src/i18n.rs:1)
- 菜单/动作：[`rust_tray/src/actions.rs`](rust_tray/src/actions.rs:1)
- **Windows 深色模式**：[`src/platform/windows/win_dark_mode.h/cpp`](src/platform/windows/win_dark_mode.h:1)
- C 头（导出 API）：[`rust_tray/include/rust_tray.h`](rust_tray/include/rust_tray.h:1)
- C++ 包装器：[`src/system_tray_rust.cpp`](src/system_tray_rust.cpp:1)
- CMake 目标：[`cmake/targets/rust_tray.cmake`](cmake/targets/rust_tray.cmake:1)

架构要点：
1. Rust 负责：菜单结构、i18n、事件循环、图标/通知、动作映射（MenuAction -> TrayAction）。
2. C++ 负责：应用内响应（打开 UI、重启、退出等）和平台特殊处理（如 Windows 特权/进程管理、**深色模式控制**）。
3. 边界：Rust 通过 C API 导出简单函数；C++ 通过回调接收用户操作事件。
4. **深色模式**：C++ 在托盘初始化前调用 `win_dark_mode::enable_process_dark_mode()`，影响整个进程的菜单、对话框和窗口。

构建与集成：
- CMake 现在包含并构建 `rust_tray`，使用 `cargo build` 生成静态库并链接到主程序（见 [`cmake/compile_definitions/*`](cmake/compile_definitions/common.cmake:1) 的改动）。
- 头文件为 [`rust_tray/include/rust_tray.h`](rust_tray/include/rust_tray.h:1)，C++ 仅需包含该头并注册回调。
- CI：需保证 Rust toolchain 可用；建议在 CI 中添加 Rust 安装步骤。

运行时与 API 变化：
- 初始化：推荐使用 `tray_init_ex(icon_normal, icon_playing, icon_pausing, icon_locked, tooltip, locale, callback)`。
- 事件循环：使用 `tray_loop(blocking)` 驱动；返回 -1 表示要求退出。可在单线程或分线程中调用（包装器提供线程化入口）。
- 运行时更新：`tray_set_icon`、`tray_set_tooltip`、`tray_set_vdd_checked`、`tray_set_vdd_enabled`、`tray_set_locale`、`tray_show_notification`。
- 兼容层：实现了 `tray_update`（部分支持）；但 `tray_init` 已被降级（返回错误并打印警告）。

i18n 与菜单：
- i18n 数据与逻辑在 Rust 层管理，参见 [`rust_tray/src/i18n.rs`](rust_tray/src/i18n.rs:1)。
- 语言切换由 Rust 处理并原子更新菜单文本，必要时会重设 TrayIcon 的菜单以确保生效（Windows 行为）。

图标与通知：
- 图标加载：Windows 优先使用 .ico（多分辨率），Linux 支持图标名称或文件路径，macOS 使用文件路径。
- 通知：当前为占位实现（日志输出），需要按平台补全真实通知接口（待办）。

测试清单（必验）：
- 编译通过并链接 Rust 静态库。
- 托盘图标在 Windows / Linux / macOS 显示正确。
- 菜单项触发后，C++ 回调收到匹配的 `TrayAction`（见头文件枚举）。
- 语言切换后菜单文本更新并在 UI 上可见。
- 图标切换与 tooltip 更新正常。
- 通知调用至少不会崩溃（后续完善行为）。

已知限制与后续工作：
- 完成平台通知实现（Rust 层需要具体实现）。
- 若需更细粒度的日志或错误上报，考虑在 Rust 层引入更丰富的日志接口并暴露给 C++。
- 可选：清理 `third-party/tray` 中多余源文件，仅保留头文件以减小仓库体积。
- 在 CI 中加入交叉编译与多平台验证。

## Windows 深色模式架构变更（2025-12）

### 变更背景
原先深色模式逻辑在 Rust 层实现（`dark_mode.rs`），通过调用未公开的 Windows API（`SetPreferredAppMode` / `AllowDarkModeForApp`）。
为了更好地分离关注点，将深色模式控制迁移到 C++ 层，使 Rust 托盘专注于业务逻辑。

### 主要变更
1. **新增 C++ 模块** [`src/platform/windows/win_dark_mode.{h,cpp}`](src/platform/windows/win_dark_mode.h:1)：
   - `enable_process_dark_mode()`：在进程级别启用深色模式（调用 `SetPreferredAppMode` 或 `AllowDarkModeForApp`）
   - `apply_window_dark_title_bar(HWND, bool)`：为特定窗口应用深色标题栏（通过 `DwmSetWindowAttribute`）
   - 使用 ordinal 135 查找未公开 API，兼容 Windows 10 1809+ 和 Windows 11

2. **简化 Rust 层** [`rust_tray/src/dark_mode.rs`](rust_tray/src/dark_mode.rs:1)：
   - 移除所有 Windows API 调用（`SetPreferredAppMode`、`FlushMenuThemes`）
   - 保留空函数以维持 FFI API 兼容性（标记为已弃用）
   - 减少对 `windows-sys` crate 的依赖（移除 `Win32_System_LibraryLoader` feature）

3. **调用时机**：
   - C++ 在 [`system_tray_rust.cpp::init_tray()`](src/system_tray_rust.cpp:180) 中**优先**调用 `win_dark_mode::enable_process_dark_mode()`
   - 确保在创建托盘图标和菜单前设置进程深色模式
   - Rust 层在 `tray_init_ex()` 中不再调用 `dark_mode::enable_dark_mode()`

4. **影响范围**：
   - 深色模式现在影响整个进程的所有菜单、对话框和窗口
   - 托盘菜单自动跟随系统深色/浅色设置，无需 Rust 层管理

### 优势
- **关注点分离**：Windows 平台特性由 C++ 统一管理，Rust 层更纯粹
- **代码集中**：深色模式相关的 Win32 hack 全部在 C++ 的单一模块中
- **易于维护**：后续需要调整深色模式行为（如支持窗口标题栏）时只改 C++ 层

### 兼容性说明
- Rust FFI 函数 `tray_enable_dark_mode()` 等保留但已变为空操作
- C++ 旧代码不受影响（深色模式由 `init_tray()` 自动启用）
- 不影响非 Windows 平台（Linux/macOS 上这些函数本就是空操作）

迁移结论：
本次提交把「菜单、i18n、事件循环、图标管理、部分文件对话（导入/导出）」这些横跨平台且逻辑密集的功能迁移到 Rust，提高了可维护性与一致性。**深色模式控制回归 C++**，作为进程级 Windows 平台特性统一管理。C++ 侧保留平台特性与应用逻辑，双方通过稳定的 C API 协作。

参考实现与调试入口：
- 查看实现：[`rust_tray/src/lib.rs`](rust_tray/src/lib.rs:1)
- 深色模式：[`src/platform/windows/win_dark_mode.cpp`](src/platform/windows/win_dark_mode.cpp:1)
- 头文件：[`rust_tray/include/rust_tray.h`](rust_tray/include/rust_tray.h:1)
- C++ 包装示例：[`src/system_tray_rust.cpp`](src/system_tray_rust.cpp:1)
- i18n 数据：[`rust_tray/src/i18n.rs`](rust_tray/src/i18n.rs:1)

完成。