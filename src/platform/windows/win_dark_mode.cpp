/**
 * @file src/platform/windows/win_dark_mode.cpp
 * @brief Implementation of Windows dark mode support
 */

#ifdef _WIN32

#include "win_dark_mode.h"

#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace win_dark_mode {

  // Undocumented PreferredAppMode enum from uxtheme.dll
  enum class PreferredAppMode {
    Default = 0,     // Use system default (usually light)
    AllowDark = 1,   // Allow dark mode (follow system setting)
    ForceDark = 2,   // Force dark mode regardless of system setting
    ForceLight = 3,  // Force light mode regardless of system setting
    Max = 4,
  };

  // Function pointer types for undocumented uxtheme.dll APIs
  using SetPreferredAppModeFn = PreferredAppMode(WINAPI *)(PreferredAppMode);
  using AllowDarkModeForAppFn = BOOL(WINAPI *)(BOOL);

  // Global function pointers (initialized once)
  static SetPreferredAppModeFn g_SetPreferredAppMode = nullptr;
  static AllowDarkModeForAppFn g_AllowDarkModeForApp = nullptr;

  /**
   * @brief Initialize the dark mode API function pointers
   *
   * This function loads uxtheme.dll and retrieves the undocumented function pointers.
   * It only runs once and caches the results.
   */
  static void
  init_dark_mode_apis() {
    static bool initialized = false;
    if (initialized) {
      return;
    }
    initialized = true;

    // Load uxtheme.dll
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (!hUxTheme) {
      return;
    }

    // Try to get SetPreferredAppMode (Windows 10 1903+)
    // This is ordinal 135 but we'll try by name first (undocumented but more stable)
    // If that fails, we fall back to the older AllowDarkModeForApp
    g_SetPreferredAppMode =
      reinterpret_cast<SetPreferredAppModeFn>(
        GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));

    // If SetPreferredAppMode is not available, try AllowDarkModeForApp (Windows 10 1809-1903)
    // This is ordinal 135 on older versions, but the signature is different
    if (!g_SetPreferredAppMode) {
      g_AllowDarkModeForApp =
        reinterpret_cast<AllowDarkModeForAppFn>(
          GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
    }

    // Note: We intentionally don't call FreeLibrary(hUxTheme) because we need
    // the function pointers to remain valid for the lifetime of the process
  }

  void
  enable_process_dark_mode() {
    // Initialize the API function pointers
    init_dark_mode_apis();

    // Call the appropriate function based on what's available
    if (g_SetPreferredAppMode) {
      // Windows 10 1903+ supports SetPreferredAppMode
      // Use AllowDark to follow the system's dark/light mode preference
      g_SetPreferredAppMode(PreferredAppMode::AllowDark);
    }
    else if (g_AllowDarkModeForApp) {
      // Windows 10 1809-1903 supports AllowDarkModeForApp
      // TRUE means allow dark mode (follows system setting)
      g_AllowDarkModeForApp(TRUE);
    }
    // If neither API is available, dark mode is not supported on this Windows version
    // (Windows 10 < 1809 or earlier). We silently do nothing in this case.
  }

  void
  apply_window_dark_title_bar(HWND hwnd, bool enable) {
    if (!hwnd) {
      return;
    }

    // DWMWA_USE_IMMERSIVE_DARK_MODE is documented for Windows 11
    // but also works on Windows 10 20H1+
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE = 20;

    BOOL useDark = enable ? TRUE : FALSE;
    DwmSetWindowAttribute(
      hwnd,
      DWMWA_USE_IMMERSIVE_DARK_MODE,
      &useDark,
      sizeof(useDark));
  }

}  // namespace win_dark_mode

#endif  // _WIN32
