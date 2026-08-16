#include "overlay.h"

#include "audio_capture.h"
#include "ui_theme.h"

#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr wchar_t kOverlayClassName[] = L"SAIDOverlayWindow";
constexpr int kOverlayWidth = 380;
constexpr int kOverlayHeight = 72;
constexpr UINT_PTR kAnimationTimer = 1;

using namespace Gdiplus;

void make_rounded_path(GraphicsPath & path, const RectF & rect, REAL radius) {
    const REAL diameter = radius * 2.0F;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0F, 90.0F);
    path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0F, 90.0F);
    path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0F, 90.0F);
    path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0F, 90.0F);
    path.CloseFigure();
}

void draw_line_text(Graphics & graphics, const std::wstring & text, const RectF & bounds,
                    REAL size, INT style, const Color & color) {
    FontFamily family(L"Segoe UI");
    Font font(&family, size, style, UnitPixel);
    SolidBrush brush(color);
    StringFormat format;
    format.SetAlignment(StringAlignmentNear);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);
    graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, bounds, &format, &brush);
}

}

Overlay::~Overlay() {
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
}

bool Overlay::create(HINSTANCE instance) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = Overlay::window_proc;
    window_class.lpszClassName = kOverlayClassName;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&window_class);

    window_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
        kOverlayClassName,
        L"SAID status",
        WS_POPUP,
        0,
        0,
        kOverlayWidth,
        kOverlayHeight,
        nullptr,
        nullptr,
        instance,
        this);
    return window_ != nullptr;
}

void Overlay::show_listening(HWND target, const AudioCapture * audio, std::wstring shortcut) {
    audio_ = audio;
    display(target, Mode::Listening, L"Listening", L"Tap " + std::move(shortcut) + L" to finish", 0);
}

void Overlay::show_transcribing(HWND target) {
    audio_ = nullptr;
    display(target, Mode::Transcribing, L"Turning speech into text", L"Usually about a second", 0);
}

void Overlay::show_notice(HWND target, std::wstring text, unsigned int milliseconds) {
    audio_ = nullptr;
    std::wstring subtitle;
    Mode mode = Mode::Notice;
    if (text == L"Inserted") {
        subtitle = L"Ready for the next thought";
        mode = Mode::Success;
    } else if (text.find(L"copied") != std::wstring::npos) {
        subtitle = L"Focus changed while transcribing";
        mode = Mode::Success;
    } else if (text == L"No speech detected") {
        subtitle = L"Try again a little closer to the microphone";
    }
    display(target, mode, std::move(text), std::move(subtitle), milliseconds);
}

void Overlay::show_error(HWND target, std::wstring text, unsigned int milliseconds) {
    audio_ = nullptr;
    std::wstring subtitle = L"Open SAID from the tray for details";
    if (text.find(L"Speech model not found") != std::wstring::npos) {
        text = L"Speech model not found";
        subtitle = L"Reinstall SAID or open its model folder";
    } else if (text.find(L"Ten-minute limit reached") != std::wstring::npos) {
        text = L"Ten-minute limit reached";
        subtitle = L"Transcribing the audio captured so far";
    }
    display(target, Mode::Error, std::move(text), std::move(subtitle), milliseconds);
}

void Overlay::hide() {
    hide_at_ = 0;
    if (window_ != nullptr) {
        KillTimer(window_, kAnimationTimer);
        ShowWindow(window_, SW_HIDE);
    }
}

void Overlay::display(HWND target, Mode mode, std::wstring title, std::wstring subtitle,
                      unsigned int hide_after_ms) {
    if (window_ == nullptr) {
        return;
    }
    mode_ = mode;
    title_ = std::move(title);
    subtitle_ = std::move(subtitle);
    animation_frame_ = 0;
    waveform_levels_.fill(0.04F);
    hide_at_ = hide_after_ms == 0 ? 0 : GetTickCount64() + hide_after_ms;
    position_near(target);
    SetTimer(window_, kAnimationTimer, 65, nullptr);
    render();
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void Overlay::position_near(HWND target) {
    HMONITOR monitor = MonitorFromWindow(target, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &monitor_info.rcWork, 0);
    }
    const RECT & area = monitor_info.rcWork;
    const int width = dpi_scale(window_, kOverlayWidth);
    const int height = dpi_scale(window_, kOverlayHeight);
    const int x = area.left + ((area.right - area.left) - width) / 2;
    const int y = area.bottom - height - dpi_scale(window_, 40);
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

LRESULT CALLBACK Overlay::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    Overlay * self = reinterpret_cast<Overlay *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto * create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
        self = static_cast<Overlay *>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
    }
    return self != nullptr
        ? self->handle_message(message, wparam, lparam)
        : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Overlay::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_TIMER:
        if (wparam == kAnimationTimer) {
            ++animation_frame_;
            if (hide_at_ != 0 && GetTickCount64() >= hide_at_) {
                hide();
            } else {
                render();
            }
        }
        return 0;
    case WM_DPICHANGED:
        position_near(GetForegroundWindow());
        render();
        return 0;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        render();
        return 0;
    default:
        return DefWindowProcW(window_, message, wparam, lparam);
    }
}

void Overlay::render() {
    using namespace Gdiplus;
    if (window_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int width = std::max(1L, client.right - client.left);
    const int height = std::max(1L, client.bottom - client.top);
    const REAL scale = static_cast<REAL>(GetDpiForWindow(window_)) / 96.0F;
    const UiPalette palette = overlay_palette();
    const Color signal = palette.text;
    const bool reduced_motion = system_reduces_motion();

    Bitmap bitmap(width, height, PixelFormat32bppPARGB);
    Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    graphics.Clear(Color(0, 0, 0, 0));

    RectF body(6.0F * scale, 5.0F * scale,
               static_cast<REAL>(width) - 12.0F * scale,
               static_cast<REAL>(height) - 13.0F * scale);
    GraphicsPath body_path;
    make_rounded_path(body_path, body, 12.0F * scale);
    SolidBrush surface(palette.surface);
    Pen border(palette.border, 1.0F * scale);
    graphics.FillPath(&surface, &body_path);
    graphics.DrawPath(&border, &body_path);

    const REAL center_y = body.Y + body.Height / 2.0F;
    const REAL visual_left = body.X + 19.0F * scale;
    if (mode_ == Mode::Listening) {
        const float level = audio_ != nullptr ? std::clamp(audio_->level(), 0.04F, 1.0F) : 0.32F;
        for (size_t index = 0; index + 1 < waveform_levels_.size(); ++index) {
            waveform_levels_[index] = waveform_levels_[index + 1];
        }
        waveform_levels_.back() = level;
        SolidBrush bar(signal);
        for (size_t index = 0; index < waveform_levels_.size(); ++index) {
            const REAL bar_height = (5.0F + 24.0F * waveform_levels_[index]) * scale;
            const REAL x = visual_left + static_cast<REAL>(index) * 5.0F * scale;
            RectF bar_rect(x, center_y - bar_height / 2.0F, 3.0F * scale, bar_height);
            GraphicsPath bar_path;
            make_rounded_path(bar_path, bar_rect, 1.5F * scale);
            graphics.FillPath(&bar, &bar_path);
        }
    } else if (mode_ == Mode::Transcribing) {
        const float linear = reduced_motion
            ? 0.0F
            : static_cast<float>(animation_frame_ % 18U) / 17.0F;
        const float travel = 1.0F - std::pow(1.0F - linear, 4.0F);
        for (int index = 0; index < 3; ++index) {
            const BYTE alpha = reduced_motion
                ? 230
                : static_cast<BYTE>(230.0F - 100.0F * travel);
            SolidBrush dot(Color(alpha, signal.GetR(), signal.GetG(), signal.GetB()));
            const REAL diameter = 6.0F * scale;
            const REAL x = visual_left + index * 8.0F * scale
                + (reduced_motion ? 0.0F : (index + 1) * 2.2F * travel * scale);
            graphics.FillEllipse(&dot, x,
                                 center_y - diameter / 2.0F, diameter, diameter);
        }
        const BYTE caret_alpha = reduced_motion
            ? 255
            : static_cast<BYTE>(130.0F + 125.0F * travel);
        Pen caret(Color(caret_alpha, signal.GetR(), signal.GetG(), signal.GetB()), 3.0F * scale);
        caret.SetStartCap(LineCapRound);
        caret.SetEndCap(LineCapRound);
        graphics.DrawLine(&caret, visual_left + 32.0F * scale, center_y - 13.0F * scale,
                          visual_left + 32.0F * scale, center_y + 13.0F * scale);
    } else if (mode_ == Mode::Error) {
        Pen mark(signal, 3.0F * scale);
        mark.SetStartCap(LineCapRound);
        mark.SetEndCap(LineCapRound);
        graphics.DrawLine(&mark, visual_left + 17.0F * scale, center_y - 13.0F * scale,
                          visual_left + 17.0F * scale, center_y + 4.0F * scale);
        SolidBrush dot(signal);
        graphics.FillEllipse(&dot, visual_left + 14.5F * scale, center_y + 10.0F * scale,
                             5.0F * scale, 5.0F * scale);
    } else if (mode_ == Mode::Success) {
        Pen check(signal, 3.0F * scale);
        check.SetStartCap(LineCapRound);
        check.SetEndCap(LineCapRound);
        graphics.DrawLine(&check, visual_left + 5.0F * scale, center_y,
                          visual_left + 14.0F * scale, center_y + 9.0F * scale);
        graphics.DrawLine(&check, visual_left + 14.0F * scale, center_y + 9.0F * scale,
                          visual_left + 31.0F * scale, center_y - 10.0F * scale);
    } else {
        Pen dots(signal, 2.0F * scale);
        for (int index = 0; index < 3; ++index) {
            graphics.DrawEllipse(&dots, visual_left + (5.0F + index * 10.0F) * scale,
                                 center_y - 3.0F * scale, 6.0F * scale, 6.0F * scale);
        }
    }

    Pen divider(palette.border, 1.0F * scale);
    graphics.DrawLine(&divider, body.X + 62.0F * scale, body.Y + 13.0F * scale,
                      body.X + 62.0F * scale, body.GetBottom() - 13.0F * scale);
    const REAL text_x = body.X + 79.0F * scale;
    if (subtitle_.empty()) {
        draw_line_text(graphics, title_, RectF(text_x, body.Y, body.GetRight() - text_x - 16.0F * scale,
                       body.Height), 14.0F * scale, FontStyleBold, palette.text);
    } else {
        draw_line_text(graphics, title_, RectF(text_x, body.Y + 7.0F * scale,
                       body.GetRight() - text_x - 16.0F * scale, 24.0F * scale),
                       14.0F * scale, FontStyleBold, palette.text);
        draw_line_text(graphics, subtitle_, RectF(text_x, body.Y + 29.0F * scale,
                       body.GetRight() - text_x - 16.0F * scale, 20.0F * scale),
                       11.5F * scale, FontStyleRegular, palette.muted);
    }

    HBITMAP hbitmap = nullptr;
    bitmap.GetHBITMAP(Color(0, 0, 0, 0), &hbitmap);
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    HGDIOBJ old_bitmap = SelectObject(memory, hbitmap);
    POINT source{0, 0};
    SIZE size{width, height};
    RECT window_rect{};
    GetWindowRect(window_, &window_rect);
    POINT destination{window_rect.left, window_rect.top};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(window_, screen, &destination, &size, memory, &source, 0, &blend, ULW_ALPHA);
    SelectObject(memory, old_bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    DeleteObject(hbitmap);
}
