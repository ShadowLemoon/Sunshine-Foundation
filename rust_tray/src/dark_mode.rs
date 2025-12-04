//! Dark mode support stub
//!
//! This module previously handled dark mode for Windows context menus,
//! but that functionality has been moved to C++ (src/platform/windows/win_dark_mode.cpp)
//! to consolidate process-wide dark mode handling.
//!
//! Dark mode is now controlled by C++ calling win_dark_mode::enable_process_dark_mode()
//! before creating the tray icon. The Rust tray library simply follows the process's
//! dark mode setting without needing to manage it.
//!
//! These functions are kept as stubs for API compatibility with the C FFI layer.

/// Enable dark mode (stub - now handled by C++)
///
/// Dark mode is controlled at the process level by C++ code.
/// This function does nothing and exists only for API compatibility.
pub fn enable_dark_mode() {
    // No-op: Dark mode is now handled by C++ before tray creation
}

/// Force dark mode (stub - now handled by C++)
///
/// Dark mode is controlled at the process level by C++ code.
/// This function does nothing and exists only for API compatibility.
pub fn force_dark_mode() {
    // No-op: Dark mode is now handled by C++ before tray creation
}

/// Force light mode (stub - now handled by C++)
///
/// Dark mode is controlled at the process level by C++ code.
/// This function does nothing and exists only for API compatibility.
pub fn force_light_mode() {
    // No-op: Dark mode is now handled by C++ before tray creation
}
