#include "setup_window.h"

#include "ui_theme.h"

#include <windowsx.h>
#include <dwmapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {
constexpr wchar_t kSetupClassName[] = L"SAIDSetupWindow";
constexpr UINT_PTR kMeterTimer = 1;
constexpr int kButtonBack = 201;
constexpr int kButtonNext = 202;
constexpr int kButtonRightAlt = 203;
constexpr int kButtonF8 = 204;
constexpr int kButtonLogin = 205;

using namespace Gdiplus;

void rounded_rectangle(Graphics & graphics, const RectF & rect, REAL radius,
                       const Color & fill, const Color & border, REAL border_width = 1.0F) {
    GraphicsPath path;
    const REAL diameter = radius * 2.0F;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0F, 90.0F);
    path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0F, 90.0F);
    path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0F, 90.0F);
    path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0F, 90.0F);
    path.CloseFigure();
    SolidBrush brush(fill);
    graphics.FillPath(&brush, &path);
    if (border.GetA() != 0 && border_width > 0.0F) {
        Pen pen(border, border_width);
        graphics.DrawPath(&pen, &path);
    }
}

void draw_text(Graphics & graphics, const std::wstring & text, const RectF & rect,
               REAL size, INT style, const Color & color,
               StringAlignment horizontal = StringAlignmentNear,
               StringAlignment vertical = StringAlignmentNear) {
    FontFamily family(L"Segoe UI");
    Font font(&family, size, style, UnitPixel);
    SolidBrush brush(color);
    StringFormat format;
    format.SetAlignment(horizontal);
    format.SetLineAlignment(vertical);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, rect, &format, &brush);
}

void set_dark_titlebar(HWND window, bool dark) {
    BOOL enabled = dark ? TRUE : FALSE;
    constexpr DWORD kImmersiveDarkMode = 20;
    DwmSetWindowAttribute(window, kImmersiveDarkMode, &enabled, sizeof(enabled));
}
}

SetupWindow::~SetupWindow() {
    microphone_test_.stop();
    if (window_ != nullptr) {
        DestroyWindow(window_);
    }
}

bool SetupWindow::create(HINSTANCE instance, HWND notify_window, HICON icon, AppSettings * settings) {
    notify_window_ = notify_window;
    settings_ = settings;
    icon_ = icon;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = SetupWindow::window_proc;
    window_class.lpszClassName = kSetupClassName;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = icon_;
    window_class.hIconSm = icon_;
    RegisterClassExW(&window_class);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                        WS_CLIPCHILDREN;
    RECT desired{0, 0, 780, 560};
    AdjustWindowRectExForDpi(&desired, style, FALSE, 0, 96);
    window_ = CreateWindowExW(0, kSetupClassName, L"SAID — Setup & settings", style,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              desired.right - desired.left, desired.bottom - desired.top,
                              nullptr, nullptr, instance, this);
    if (window_ == nullptr) {
        return false;
    }

    const DWORD button_style = WS_CHILD | WS_TABSTOP | BS_OWNERDRAW;
    back_button_ = CreateWindowW(L"BUTTON", L"Back", button_style, 0, 0, 0, 0,
                                 window_, reinterpret_cast<HMENU>(kButtonBack), instance, nullptr);
    next_button_ = CreateWindowW(L"BUTTON", L"Continue", button_style | WS_VISIBLE, 0, 0, 0, 0,
                                 window_, reinterpret_cast<HMENU>(kButtonNext), instance, nullptr);
    right_alt_button_ = CreateWindowW(L"BUTTON", L"Right Alt", button_style, 0, 0, 0, 0,
                                      window_, reinterpret_cast<HMENU>(kButtonRightAlt), instance, nullptr);
    f8_button_ = CreateWindowW(L"BUTTON", L"F8", button_style, 0, 0, 0, 0,
                               window_, reinterpret_cast<HMENU>(kButtonF8), instance, nullptr);
    login_button_ = CreateWindowW(L"BUTTON", L"Launch SAID when I sign in", button_style, 0, 0, 0, 0,
                                  window_, reinterpret_cast<HMENU>(kButtonLogin), instance, nullptr);
    update_theme();
    layout();
    return true;
}

void SetupWindow::show(bool first_run, int initial_page) {
    if (window_ == nullptr) {
        return;
    }
    first_run_ = first_run;
    shortcut_confirmed_ = false;
    go_to_page(initial_page);

    const UINT dpi = GetDpiForWindow(window_);
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE));
    const DWORD extended_style = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_EXSTYLE));
    RECT desired{0, 0, MulDiv(780, static_cast<int>(dpi), 96), MulDiv(560, static_cast<int>(dpi), 96)};
    AdjustWindowRectExForDpi(&desired, style, FALSE, extended_style, dpi);
    const int width = desired.right - desired.left;
    const int height = desired.bottom - desired.top;
    HMONITOR monitor = MonitorFromWindow(notify_window_ != nullptr ? notify_window_ : window_,
                                         MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoW(monitor, &monitor_info);
    const RECT & area = monitor_info.rcWork;
    const int x = static_cast<int>(area.left) +
        std::max(0, (static_cast<int>(area.right - area.left) - width) / 2);
    const int y = static_cast<int>(area.top) +
        std::max(0, (static_cast<int>(area.bottom - area.top) - height) / 2);
    SetWindowPos(window_, HWND_TOP, x, y, width, height, SWP_NOACTIVATE);
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
    SetFocus(next_button_);
    // The shortcut page introduces two owner-drawn child windows before the
    // parent is shown. Force one complete parent paint so the brand header is
    // not left outside a partial update region on some Windows compositors.
    InvalidateRect(window_, nullptr, FALSE);
    UpdateWindow(window_);
}

bool SetupWindow::visible() const {
    return window_ != nullptr && IsWindowVisible(window_);
}

bool SetupWindow::handle_shortcut_pressed(ShortcutKey shortcut) {
    if (!visible() || page_ != 2 || settings_ == nullptr || shortcut != settings_->shortcut()) {
        return false;
    }
    shortcut_confirmed_ = true;
    InvalidateRect(window_, nullptr, FALSE);
    return true;
}

LRESULT CALLBACK SetupWindow::window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    SetupWindow * self = reinterpret_cast<SetupWindow *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto * create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
        self = static_cast<SetupWindow *>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
    }
    return self != nullptr
        ? self->handle_message(message, wparam, lparam)
        : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT SetupWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_COMMAND: {
        const int command = LOWORD(wparam);
        if (command == kButtonBack && page_ > 0) {
            go_to_page(page_ - 1);
        } else if (command == kButtonNext) {
            if (page_ < 3) {
                go_to_page(page_ + 1);
            } else {
                if (settings_ != nullptr) {
                    settings_->set_onboarding_complete(true);
                }
                first_run_ = false;
                close_window();
            }
        } else if (command == kButtonRightAlt || command == kButtonF8) {
            if (settings_ != nullptr) {
                settings_->set_shortcut(command == kButtonF8 ? ShortcutKey::F8 : ShortcutKey::RightAlt);
                shortcut_confirmed_ = false;
                PostMessageW(notify_window_, kMessageShortcutChanged,
                             static_cast<WPARAM>(settings_->shortcut()), 0);
                InvalidateRect(window_, nullptr, FALSE);
                InvalidateRect(right_alt_button_, nullptr, FALSE);
                InvalidateRect(f8_button_, nullptr, FALSE);
            }
        } else if (command == kButtonLogin && settings_ != nullptr) {
            settings_->set_launch_at_login(!settings_->launch_at_login());
            InvalidateRect(login_button_, nullptr, FALSE);
        }
        return 0;
    }
    case WM_DRAWITEM:
        draw_button(*reinterpret_cast<const DRAWITEMSTRUCT *>(lparam));
        return TRUE;
    case WM_TIMER:
        if (wparam == kMeterTimer && page_ == 1) {
            invalidate_microphone_meter();
        }
        return 0;
    case WM_DPICHANGED: {
        const RECT * suggested = reinterpret_cast<const RECT *>(lparam);
        SetWindowPos(window_, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        layout();
        return 0;
    }
    case WM_SIZE:
        layout();
        return 0;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        update_theme();
        InvalidateRect(window_, nullptr, TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint();
        return 0;
    case WM_CLOSE:
        close_window();
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE && !first_run_) {
            close_window();
            return 0;
        }
        return DefWindowProcW(window_, message, wparam, lparam);
    default:
        return DefWindowProcW(window_, message, wparam, lparam);
    }
}

void SetupWindow::paint() {
    using namespace Gdiplus;
    PAINTSTRUCT paint_info{};
    HDC target = BeginPaint(window_, &paint_info);
    RECT client{};
    GetClientRect(window_, &client);
    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    HGDIOBJ old_bitmap = SelectObject(buffer, bitmap);

    Graphics graphics(buffer);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    const UiPalette palette = current_palette();
    graphics.Clear(palette.canvas);

    const REAL scale = static_cast<REAL>(GetDpiForWindow(window_)) / 96.0F;
    const REAL left = 64.0F * scale;
    const REAL top = 42.0F * scale;
    const REAL width = static_cast<REAL>(client.right) - left * 2.0F;

    draw_said_mark(graphics, RectF(left, top, 54.0F * scale, 54.0F * scale),
                   palette.accent, palette.on_accent);
    draw_text(graphics, L"SAID", RectF(left + 70.0F * scale, top + 2.0F * scale,
              180.0F * scale, 26.0F * scale), 15.0F * scale, FontStyleBold, palette.text);
    draw_text(graphics, L"LOCAL VOICE. EXACT TEXT.", RectF(left + 70.0F * scale, top + 29.0F * scale,
              220.0F * scale, 20.0F * scale), 10.0F * scale, FontStyleBold, palette.muted);

    const REAL dot_y = top + 22.0F * scale;
    for (int index = 0; index < 4; ++index) {
        const REAL x = left + width - (80.0F - index * 22.0F) * scale;
        if (index == page_) {
            SolidBrush dot(palette.accent);
            graphics.FillEllipse(&dot, x, dot_y, 8.0F * scale, 8.0F * scale);
        } else {
            Pen dot(palette.border, 1.5F * scale);
            graphics.DrawEllipse(&dot, x, dot_y, 8.0F * scale, 8.0F * scale);
        }
    }

    const REAL heading_y = 144.0F * scale;
    std::wstring heading;
    std::wstring body;
    if (page_ == 0) {
        heading = L"Say it once.";
        body = L"Your words land at the caret. On this PC.\nOne shortcut starts listening. The same shortcut finishes.";
    } else if (page_ == 1) {
        heading = L"Let’s hear your microphone.";
        body = L"Speak normally. The meter should move with your voice.\nAudio stays on this computer and is discarded after transcription.";
    } else if (page_ == 2) {
        heading = L"Choose your shortcut.";
        body = L"Tap once to listen, then tap again to type. Choose a key that\ndoesn’t interfere with your keyboard layout.";
    } else {
        heading = L"You’re ready.";
        body = L"Put the caret in any text field, tap your shortcut, and speak.\nSAID will stay quietly in the system tray.";
    }
    draw_text(graphics, heading, RectF(left, heading_y, width, 58.0F * scale),
              38.0F * scale, FontStyleBold, palette.text);
    draw_text(graphics, body, RectF(left, heading_y + 70.0F * scale, width, 66.0F * scale),
              15.0F * scale, FontStyleRegular, palette.muted);

    if (page_ == 0) {
        const REAL y = heading_y + 174.0F * scale;
        Pen line(palette.border, 1.0F * scale);
        graphics.DrawLine(&line, left, y, left + width, y);
        const std::array<std::pair<const wchar_t *, const wchar_t *>, 3> facts{{
            {L"LOCAL", L"Speech recognition runs on your PC"},
            {L"VERBATIM", L"No rewriting, summaries, or tone changes"},
            {L"FOCUS-SAFE", L"If focus moves, the transcript is copied"},
        }};
        for (size_t index = 0; index < facts.size(); ++index) {
            const REAL row_y = y + (22.0F + static_cast<REAL>(index) * 46.0F) * scale;
            draw_text(graphics, facts[index].first, RectF(left, row_y, 110.0F * scale, 22.0F * scale),
                      10.0F * scale, FontStyleBold, palette.accent);
            draw_text(graphics, facts[index].second, RectF(left + 124.0F * scale, row_y - 2.0F * scale,
                      width - 124.0F * scale, 24.0F * scale), 14.0F * scale, FontStyleRegular, palette.text);
        }
    } else if (page_ == 1) {
        const REAL meter_y = heading_y + 176.0F * scale;
        rounded_rectangle(graphics, RectF(left, meter_y, width, 102.0F * scale), 10.0F * scale,
                          palette.surface, palette.border, 1.0F * scale);
        const float level = microphone_test_.running()
            ? std::clamp(microphone_test_.level(), 0.0F, 1.0F)
            : 0.0F;
        for (int index = 0; index < 18; ++index) {
            const float threshold = static_cast<float>(index + 1) / 18.0F;
            const REAL bar_x = left + (24.0F + index * 24.0F) * scale;
            const REAL bar_height = (14.0F + (index % 4) * 7.0F) * scale;
            const Color color = level >= threshold ? palette.accent : palette.border;
            SolidBrush brush(color);
            graphics.FillRectangle(&brush, bar_x, meter_y + 51.0F * scale - bar_height / 2.0F,
                                   12.0F * scale, bar_height);
        }
        const std::wstring status = !microphone_error_.empty()
            ? L"Microphone unavailable — check Windows microphone privacy settings"
            : (level > 0.08F ? L"Signal detected" : L"Listening for your voice…");
        draw_text(graphics, status, RectF(left, meter_y + 118.0F * scale, width, 24.0F * scale),
                  13.0F * scale, FontStyleBold,
                  !microphone_error_.empty() ? palette.error : (level > 0.08F ? palette.success : palette.muted));
    } else if (page_ == 2) {
        const REAL y = heading_y + 264.0F * scale;
        const std::wstring prompt = shortcut_confirmed_
            ? std::wstring(L"✓  Shortcut confirmed — ") + shortcut_name(settings_->shortcut()) + L" is ready"
            : std::wstring(L"Press ") + shortcut_name(settings_->shortcut()) + L" now to rehearse it";
        draw_text(graphics, prompt, RectF(left, y, width, 32.0F * scale), 14.0F * scale,
                  FontStyleBold, shortcut_confirmed_ ? palette.success : palette.text);
    } else {
        const REAL y = heading_y + 150.0F * scale;
        Pen check(palette.text, 4.0F * scale);
        check.SetStartCap(LineCapRound);
        check.SetEndCap(LineCapRound);
        graphics.DrawLine(&check, left + 6.0F * scale, y + 34.0F * scale,
                          left + 20.0F * scale, y + 48.0F * scale);
        graphics.DrawLine(&check, left + 20.0F * scale, y + 48.0F * scale,
                          left + 48.0F * scale, y + 16.0F * scale);
        Pen path_pen(palette.border, 2.0F * scale);
        graphics.DrawLine(&path_pen, left + 72.0F * scale, y + 32.0F * scale,
                          left + 210.0F * scale, y + 32.0F * scale);
        SolidBrush caret(palette.text);
        graphics.FillRectangle(&caret, left + 232.0F * scale, y + 9.0F * scale,
                               4.0F * scale, 46.0F * scale);
        draw_text(graphics, shortcut_name(settings_->shortcut()),
                  RectF(left + 76.0F * scale, y + 4.0F * scale, 132.0F * scale, 54.0F * scale),
                  16.0F * scale, FontStyleBold, palette.text,
                  StringAlignmentCenter, StringAlignmentCenter);
    }

    const RECT & dirty = paint_info.rcPaint;
    BitBlt(target, dirty.left, dirty.top, dirty.right - dirty.left, dirty.bottom - dirty.top,
           buffer, dirty.left, dirty.top, SRCCOPY);
    SelectObject(buffer, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    EndPaint(window_, &paint_info);
}

void SetupWindow::draw_button(const DRAWITEMSTRUCT & item) {
    using namespace Gdiplus;
    Graphics graphics(item.hDC);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const UiPalette palette = current_palette();
    graphics.Clear(palette.canvas);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool primary = item.CtlID == kButtonNext;
    const bool option = item.CtlID == kButtonRightAlt || item.CtlID == kButtonF8;
    const bool login = item.CtlID == kButtonLogin;
    const bool selected = settings_ != nullptr &&
        ((item.CtlID == kButtonRightAlt && settings_->shortcut() == ShortcutKey::RightAlt) ||
         (item.CtlID == kButtonF8 && settings_->shortcut() == ShortcutKey::F8));

    RectF rect(static_cast<REAL>(item.rcItem.left + 2), static_cast<REAL>(item.rcItem.top + 2),
               static_cast<REAL>(item.rcItem.right - item.rcItem.left - 4),
               static_cast<REAL>(item.rcItem.bottom - item.rcItem.top - 4));
    Color fill = primary ? (pressed ? palette.accent_pressed : palette.accent) : palette.surface;
    Color border = primary ? palette.accent : ((option && selected) ? palette.accent : palette.border);
    if (login) {
        fill = palette.canvas;
        border = Color(0, 0, 0, 0);
    }
    rounded_rectangle(graphics, rect, 8.0F, fill, border, (option && selected) ? 2.0F : 1.0F);

    wchar_t label[128]{};
    GetWindowTextW(item.hwndItem, label, static_cast<int>(std::size(label)));
    RectF text_rect = rect;
    if (login) {
        const bool checked = settings_ != nullptr && settings_->launch_at_login();
        const RectF box(rect.X + 2.0F, rect.Y + (rect.Height - 20.0F) / 2.0F, 20.0F, 20.0F);
        rounded_rectangle(graphics, box, 5.0F, checked ? palette.accent : palette.surface,
                          checked ? palette.accent : palette.border, 1.0F);
        if (checked) {
            Pen check(palette.on_accent, 2.2F);
            check.SetStartCap(LineCapRound);
            check.SetEndCap(LineCapRound);
            graphics.DrawLine(&check, box.X + 5.0F, box.Y + 10.0F, box.X + 8.5F, box.Y + 14.0F);
            graphics.DrawLine(&check, box.X + 8.5F, box.Y + 14.0F, box.X + 16.0F, box.Y + 6.0F);
        }
        text_rect.X += 34.0F;
        text_rect.Width -= 34.0F;
    }
    Color text_color = primary ? palette.on_accent : palette.text;
    if (disabled) {
        text_color = palette.muted;
    }
    draw_text(graphics, label, text_rect, 14.0F, FontStyleBold, text_color,
              login ? StringAlignmentNear : StringAlignmentCenter, StringAlignmentCenter);
    if (focused) {
        RectF focus(rect.X - 1.0F, rect.Y - 1.0F, rect.Width + 2.0F, rect.Height + 2.0F);
        rounded_rectangle(graphics, focus, 10.0F, Color(0, 0, 0, 0), palette.accent, 2.0F);
    }
}

void SetupWindow::layout() {
    if (window_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int scale = static_cast<int>(GetDpiForWindow(window_));
    const auto px = [scale](int value) { return MulDiv(value, scale, 96); };
    const int bottom = client.bottom - px(32);
    MoveWindow(next_button_, client.right - px(64) - px(154), bottom - px(48), px(154), px(48), TRUE);
    MoveWindow(back_button_, px(64), bottom - px(48), px(104), px(48), TRUE);
    MoveWindow(right_alt_button_, px(64), px(326), px(188), px(56), TRUE);
    MoveWindow(f8_button_, px(268), px(326), px(112), px(56), TRUE);
    MoveWindow(login_button_, px(62), px(420), px(310), px(48), TRUE);
}

void SetupWindow::invalidate_microphone_meter() {
    if (window_ == nullptr || page_ != 1) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int dpi = static_cast<int>(GetDpiForWindow(window_));
    const auto px = [dpi](int value) { return MulDiv(value, dpi, 96); };
    const RECT meter{
        px(60),
        px(316),
        client.right - px(60),
        px(466),
    };
    InvalidateRect(window_, &meter, FALSE);
}

void SetupWindow::go_to_page(int page) {
    page = std::clamp(page, 0, 3);
    if (page_ == 1 && page != 1) {
        KillTimer(window_, kMeterTimer);
        microphone_test_.stop();
    }
    page_ = page;
    if (page_ == 1) {
        microphone_error_.clear();
        std::string error;
        if (!microphone_test_.start(error)) {
            microphone_error_.assign(error.begin(), error.end());
        }
        SetTimer(window_, kMeterTimer, 70, nullptr);
    }
    update_controls();
    InvalidateRect(window_, nullptr, FALSE);
}

void SetupWindow::update_controls() {
    ShowWindow(back_button_, page_ > 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(right_alt_button_, page_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(f8_button_, page_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(login_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    SetWindowTextW(next_button_, page_ == 3 ? L"Start SAID" : L"Continue");
    SetWindowTextW(back_button_, L"Back");
    InvalidateRect(next_button_, nullptr, FALSE);
    layout();
}

void SetupWindow::update_theme() {
    const UiPalette palette = current_palette();
    dark_ = palette.dark;
    if (window_ != nullptr) {
        set_dark_titlebar(window_, dark_);
    }
}

void SetupWindow::close_window() {
    if (page_ == 1) {
        KillTimer(window_, kMeterTimer);
        microphone_test_.stop();
    }
    ShowWindow(window_, SW_HIDE);
}
