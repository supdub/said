#pragma once

#include <windows.h>

#include <array>
#include <string>

class AudioCapture;

class Overlay {
public:
    enum class Mode {
        Listening,
        Transcribing,
        Correcting,
        Success,
        Notice,
        Error,
    };

    Overlay() = default;
    ~Overlay();

    Overlay(const Overlay &) = delete;
    Overlay & operator=(const Overlay &) = delete;

    bool create(HINSTANCE instance);
    void show_listening(HWND target, const AudioCapture * audio,
                        std::wstring shortcut = L"Right Alt", bool streaming = false,
                        std::wstring refinement = {});
    void show_streaming_paused(HWND target, const AudioCapture * audio,
                               std::wstring shortcut = L"Right Alt");
    void show_transcribing(HWND target);
    void show_finalizing(HWND target);
    void show_correcting(HWND target, std::wstring shortcut = L"Right Alt",
                         bool streaming = false);
    void show_success(HWND target, std::wstring title, std::wstring subtitle,
                      unsigned int milliseconds = 1200);
    void show_notice(HWND target, std::wstring text, unsigned int milliseconds = 1200);
    void show_error(HWND target, std::wstring text, unsigned int milliseconds = 4500);
    void hide();

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    void display(HWND target, Mode mode, std::wstring title, std::wstring subtitle,
                 unsigned int hide_after_ms);
    void render();
    void position_near(HWND target);

    HWND window_ = nullptr;
    Mode mode_ = Mode::Notice;
    std::wstring title_;
    std::wstring subtitle_;
    const AudioCapture * audio_ = nullptr;
    ULONGLONG hide_at_ = 0;
    ULONGLONG display_started_at_ = 0;
    unsigned int animation_frame_ = 0;
    std::wstring correcting_shortcut_ = L"Right Alt";
    bool correcting_streaming_ = false;
    std::array<float, 7> waveform_levels_{};
};
