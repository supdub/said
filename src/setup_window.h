#pragma once

#include "advanced_model_download.h"
#include "app_settings.h"
#include "audio_capture.h"

#include <windows.h>

#include <array>
#include <string>

constexpr UINT kMessageShortcutChanged = WM_APP + 20;
constexpr UINT kMessageOutputModeRequested = WM_APP + 21;

class SetupWindow {
public:
    SetupWindow() = default;
    ~SetupWindow();

    SetupWindow(const SetupWindow &) = delete;
    SetupWindow & operator=(const SetupWindow &) = delete;

    bool create(HINSTANCE instance, HWND notify_window, HICON icon, AppSettings * settings);
    void show(bool first_run, int initial_page = -1);
    bool visible() const;
    HWND window() const { return window_; }
    bool handle_shortcut_pressed(ShortcutKey shortcut);
    bool handle_custom_shortcut_key(DWORD virtual_key);
    void set_advanced_model_state(AdvancedModelState state, int progress_percent = 0);
    void refresh_grammar_controls();

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    void draw_button(const DRAWITEMSTRUCT & item);
    void layout();
    void invalidate_microphone_meter();
    void go_to_page(int page);
    void update_controls();
    void update_theme();
    void close_window();
    void begin_custom_shortcut_capture();
    void cancel_custom_shortcut_capture();
    void capture_custom_shortcut(DWORD virtual_key);

    HWND window_ = nullptr;
    HWND notify_window_ = nullptr;
    HWND back_button_ = nullptr;
    HWND next_button_ = nullptr;
    std::array<HWND, 4> page_buttons_{};
    HWND right_alt_button_ = nullptr;
    HWND f8_button_ = nullptr;
    HWND custom_shortcut_button_ = nullptr;
    HWND streaming_button_ = nullptr;
    HWND grammar_off_button_ = nullptr;
    HWND grammar_standard_button_ = nullptr;
    HWND grammar_advanced_button_ = nullptr;
    HWND chinese_language_button_ = nullptr;
    HWND english_language_button_ = nullptr;
    HWND japanese_language_button_ = nullptr;
    HWND korean_language_button_ = nullptr;
    HWND login_button_ = nullptr;
    HICON icon_ = nullptr;
    AppSettings * settings_ = nullptr;
    AudioCapture microphone_test_;
    std::wstring microphone_error_;
    int page_ = 0;
    bool first_run_ = false;
    bool shortcut_confirmed_ = false;
    bool capturing_custom_shortcut_ = false;
    std::wstring shortcut_capture_status_;
    bool dark_ = false;
    AdvancedModelState advanced_model_state_ = AdvancedModelState::NotInstalled;
    int advanced_model_progress_percent_ = 0;
};
