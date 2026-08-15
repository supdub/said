#pragma once

#include "app_settings.h"
#include "audio_capture.h"

#include <windows.h>

#include <string>

constexpr UINT kMessageShortcutChanged = WM_APP + 20;

class SetupWindow {
public:
    SetupWindow() = default;
    ~SetupWindow();

    SetupWindow(const SetupWindow &) = delete;
    SetupWindow & operator=(const SetupWindow &) = delete;

    bool create(HINSTANCE instance, HWND notify_window, HICON icon, AppSettings * settings);
    void show(bool first_run, int initial_page = 0);
    bool visible() const;
    HWND window() const { return window_; }
    bool handle_shortcut_pressed(ShortcutKey shortcut);

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    void paint();
    void draw_button(const DRAWITEMSTRUCT & item);
    void layout();
    void go_to_page(int page);
    void update_controls();
    void update_theme();
    void close_window();

    HWND window_ = nullptr;
    HWND notify_window_ = nullptr;
    HWND back_button_ = nullptr;
    HWND next_button_ = nullptr;
    HWND right_alt_button_ = nullptr;
    HWND f8_button_ = nullptr;
    HWND login_button_ = nullptr;
    HICON icon_ = nullptr;
    AppSettings * settings_ = nullptr;
    AudioCapture microphone_test_;
    std::wstring microphone_error_;
    int page_ = 0;
    bool first_run_ = false;
    bool shortcut_confirmed_ = false;
    bool dark_ = false;
};
