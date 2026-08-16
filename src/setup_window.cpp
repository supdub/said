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
constexpr int kButtonStreaming = 207;
constexpr int kButtonGrammarOff = 208;
constexpr int kButtonGrammarStandard = 209;
constexpr int kButtonGrammarAdvanced = 210;
constexpr int kButtonPageWelcome = 211;
constexpr int kButtonPageDictation = 214;
constexpr int kButtonCustomShortcut = 215;
constexpr int kButtonLanguageChinese = 216;
constexpr int kButtonLanguageEnglish = 217;
constexpr int kButtonLanguageJapanese = 218;
constexpr int kButtonLanguageKorean = 219;
constexpr int kPageButtonFirst = kButtonPageWelcome;
constexpr int kPageButtonLast = kButtonPageDictation;

using namespace Gdiplus;

void rounded_rectangle(Graphics & graphics, const RectF & rect, REAL radius,
                       const Color & fill, const Color & border, REAL border_width = 1.0F) {
    GraphicsPath path;
    const REAL safe_radius = std::max(
        0.0F, std::min(radius, std::min(rect.Width, rect.Height) / 2.0F));
    const REAL diameter = safe_radius * 2.0F;
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
    FontFamily variable_family(L"Segoe UI Variable");
    FontFamily fallback_family(L"Segoe UI");
    FontFamily * family = variable_family.IsAvailable()
        ? &variable_family
        : &fallback_family;
    Font font(family, size, style, UnitPixel);
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
    RECT desired{0, 0, 940, 660};
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
    constexpr const wchar_t * page_labels[]{
        L"01   Welcome", L"02   Microphone", L"03   Shortcut", L"04   Dictation",
    };
    for (size_t index = 0; index < page_buttons_.size(); ++index) {
        page_buttons_[index] = CreateWindowW(
            L"BUTTON", page_labels[index], button_style | WS_VISIBLE, 0, 0, 0, 0,
            window_, reinterpret_cast<HMENU>(kPageButtonFirst + index), instance, nullptr);
    }
    right_alt_button_ = CreateWindowW(L"BUTTON", L"Right Alt", button_style, 0, 0, 0, 0,
                                      window_, reinterpret_cast<HMENU>(kButtonRightAlt), instance, nullptr);
    f8_button_ = CreateWindowW(L"BUTTON", L"F8", button_style, 0, 0, 0, 0,
                               window_, reinterpret_cast<HMENU>(kButtonF8), instance, nullptr);
    custom_shortcut_button_ = CreateWindowW(
        L"BUTTON", L"Custom…", button_style, 0, 0, 0, 0,
        window_, reinterpret_cast<HMENU>(kButtonCustomShortcut), instance, nullptr);
    streaming_button_ = CreateWindowW(L"BUTTON", L"Type while I speak", button_style,
                                      0, 0, 0, 0, window_,
                                      reinterpret_cast<HMENU>(kButtonStreaming), instance, nullptr);
    grammar_off_button_ = CreateWindowW(L"BUTTON", L"Exact", button_style,
                                        0, 0, 0, 0, window_,
                                        reinterpret_cast<HMENU>(kButtonGrammarOff), instance, nullptr);
    grammar_standard_button_ = CreateWindowW(L"BUTTON", L"Clean", button_style,
                                             0, 0, 0, 0, window_,
                                             reinterpret_cast<HMENU>(kButtonGrammarStandard), instance, nullptr);
    grammar_advanced_button_ = CreateWindowW(
        L"BUTTON",
        L"Adapt — optional local model; 16 GB RAM recommended; about 1.7 GB while warm",
        button_style, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(kButtonGrammarAdvanced), instance, nullptr);
    const DWORD fixed_tag_style = WS_CHILD | BS_OWNERDRAW;
    chinese_language_button_ = CreateWindowW(
        L"BUTTON", L"✓  Chinese", fixed_tag_style, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(kButtonLanguageChinese), instance, nullptr);
    english_language_button_ = CreateWindowW(
        L"BUTTON", L"✓  English", fixed_tag_style, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(kButtonLanguageEnglish), instance, nullptr);
    japanese_language_button_ = CreateWindowW(
        L"BUTTON", L"Japanese", button_style, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(kButtonLanguageJapanese), instance, nullptr);
    korean_language_button_ = CreateWindowW(
        L"BUTTON", L"Korean", button_style, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(kButtonLanguageKorean), instance, nullptr);
    EnableWindow(chinese_language_button_, FALSE);
    EnableWindow(english_language_button_, FALSE);
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
    shortcut_capture_status_.clear();
    const int target_page = initial_page >= 0
        ? initial_page
        : (first_run_ ? 0 : (page_ == 0 ? 3 : page_));
    go_to_page(target_page);

    const UINT dpi = ui_dpi(window_);
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE));
    const DWORD extended_style = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_EXSTYLE));
    RECT desired{0, 0, MulDiv(940, static_cast<int>(dpi), 96), MulDiv(660, static_cast<int>(dpi), 96)};
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
    SetFocus(first_run_ ? next_button_ : page_buttons_[page_]);
    // The shortcut page introduces several owner-drawn child windows before the
    // parent is shown. Force one complete parent paint so the brand header is
    // not left outside a partial update region on some Windows compositors.
    InvalidateRect(window_, nullptr, FALSE);
    UpdateWindow(window_);
}

bool SetupWindow::visible() const {
    return window_ != nullptr && IsWindowVisible(window_);
}

bool SetupWindow::handle_shortcut_pressed(ShortcutKey shortcut) {
    if (!visible() || page_ != 2 || capturing_custom_shortcut_ ||
        settings_ == nullptr || shortcut != settings_->shortcut()) {
        return false;
    }
    shortcut_confirmed_ = true;
    InvalidateRect(window_, nullptr, FALSE);
    return true;
}

bool SetupWindow::handle_custom_shortcut_key(DWORD virtual_key) {
    if (!capturing_custom_shortcut_) {
        return false;
    }
    capture_custom_shortcut(virtual_key);
    return true;
}

void SetupWindow::set_advanced_model_state(AdvancedModelState state, int progress_percent) {
    advanced_model_state_ = state;
    advanced_model_progress_percent_ = std::clamp(progress_percent, 0, 100);
    refresh_grammar_controls();
}

void SetupWindow::refresh_grammar_controls() {
    if (window_ == nullptr) return;
    update_controls();
    InvalidateRect(grammar_off_button_, nullptr, FALSE);
    InvalidateRect(grammar_standard_button_, nullptr, FALSE);
    InvalidateRect(grammar_advanced_button_, nullptr, FALSE);
    InvalidateRect(window_, nullptr, FALSE);
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
        if (command >= kPageButtonFirst && command <= kPageButtonLast) {
            go_to_page(command - kPageButtonFirst);
        } else if (command == kButtonBack && first_run_ && page_ > 0) {
            go_to_page(page_ - 1);
        } else if (command == kButtonNext) {
            if (!first_run_) {
                close_window();
            } else if (page_ < 3) {
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
                cancel_custom_shortcut_capture();
                settings_->set_shortcut(command == kButtonF8 ? ShortcutKey::F8 : ShortcutKey::RightAlt);
                shortcut_confirmed_ = false;
                PostMessageW(notify_window_, kMessageShortcutChanged,
                             static_cast<WPARAM>(settings_->shortcut()),
                             static_cast<LPARAM>(settings_->shortcut_modifiers()));
                update_controls();
                InvalidateRect(window_, nullptr, FALSE);
                InvalidateRect(right_alt_button_, nullptr, FALSE);
                InvalidateRect(f8_button_, nullptr, FALSE);
                InvalidateRect(custom_shortcut_button_, nullptr, FALSE);
            }
        } else if (command == kButtonCustomShortcut) {
            begin_custom_shortcut_capture();
        } else if (command == kButtonLogin && settings_ != nullptr) {
            settings_->set_launch_at_login(!settings_->launch_at_login());
            update_controls();
            InvalidateRect(login_button_, nullptr, FALSE);
        } else if ((command == kButtonGrammarOff ||
                    command == kButtonGrammarStandard ||
                    command == kButtonGrammarAdvanced) && settings_ != nullptr) {
            const OutputMode mode = command == kButtonGrammarOff
                ? OutputMode::Exact
                : (command == kButtonGrammarStandard
                    ? OutputMode::Clean
                    : OutputMode::Adapt);
            PostMessageW(notify_window_, kMessageOutputModeRequested,
                         static_cast<WPARAM>(mode), reinterpret_cast<LPARAM>(window_));
        } else if (command == kButtonStreaming && settings_ != nullptr) {
            settings_->set_streaming_mode_enabled(!settings_->streaming_mode_enabled());
            update_controls();
            InvalidateRect(streaming_button_, nullptr, FALSE);
        } else if ((command == kButtonLanguageJapanese ||
                    command == kButtonLanguageKorean) && settings_ != nullptr) {
            const SpeechLanguage language = command == kButtonLanguageJapanese
                ? SpeechLanguage::Japanese
                : SpeechLanguage::Korean;
            settings_->set_speech_language_enabled(
                language,
                !speech_language_enabled(settings_->speech_languages(), language));
            update_controls();
        }
        return 0;
    }
    case WM_DRAWITEM:
        draw_button(*reinterpret_cast<const DRAWITEMSTRUCT *>(lparam));
        return TRUE;
    case WM_GETDLGCODE:
        if (capturing_custom_shortcut_) {
            return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTTAB;
        }
        return DefWindowProcW(window_, message, wparam, lparam);
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
    case WM_SYSKEYDOWN:
        if (capturing_custom_shortcut_) {
            if (wparam == VK_ESCAPE) {
                cancel_custom_shortcut_capture();
            } else {
                capture_custom_shortcut(static_cast<DWORD>(wparam));
            }
            return 0;
        }
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

    const REAL scale = static_cast<REAL>(ui_dpi(window_)) / 96.0F;
    const REAL sidebar_width = 210.0F * scale;
    const REAL left = 254.0F * scale;
    const REAL top = 32.0F * scale;
    const REAL width = static_cast<REAL>(client.right) - left - 54.0F * scale;

    SolidBrush sidebar_brush(palette.surface);
    graphics.FillRectangle(&sidebar_brush, 0.0F, 0.0F, sidebar_width,
                           static_cast<REAL>(client.bottom));
    Pen sidebar_edge(palette.border, 1.0F * scale);
    graphics.DrawLine(&sidebar_edge, sidebar_width, 0.0F, sidebar_width,
                      static_cast<REAL>(client.bottom));

    draw_said_mark(graphics, RectF(28.0F * scale, top, 48.0F * scale, 48.0F * scale),
                   palette.accent, palette.on_accent);
    draw_text(graphics, L"SAID", RectF(88.0F * scale, top + 1.0F * scale,
              92.0F * scale, 24.0F * scale), 15.0F * scale, FontStyleBold, palette.text);
    draw_text(graphics, L"SETTINGS", RectF(88.0F * scale, top + 27.0F * scale,
              96.0F * scale, 18.0F * scale), 9.5F * scale, FontStyleBold, palette.muted);
    draw_text(graphics, first_run_ ? L"FIRST-RUN SETUP" : L"SAVES AUTOMATICALLY",
              RectF(left, 48.0F * scale, width, 18.0F * scale),
              9.5F * scale, FontStyleBold, palette.muted);

    const REAL heading_y = 92.0F * scale;
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
        heading = L"Choose how words land.";
        body = L"Exact preserves recognition. Clean fixes speech mistakes.\nAdapt organizes the result for the app you are using.";
    }
    draw_text(graphics, heading, RectF(left, heading_y, width, 52.0F * scale),
              34.0F * scale, FontStyleBold, palette.text);
    draw_text(graphics, body, RectF(left, heading_y + 64.0F * scale, width, 66.0F * scale),
              15.0F * scale, FontStyleRegular, palette.muted);

    if (page_ == 0) {
        const REAL y = heading_y + 180.0F * scale;
        Pen line(palette.border, 1.0F * scale);
        graphics.DrawLine(&line, left, y, left + width, y);
        const std::array<std::pair<const wchar_t *, const wchar_t *>, 3> facts{{
            {L"LOCAL", L"Speech recognition runs on your PC"},
            {L"CONTROLLED", L"Writing changes are visible and optional"},
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
        const REAL meter_y = heading_y + 184.0F * scale;
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
        const REAL y = heading_y + 298.0F * scale;
        std::wstring prompt;
        if (capturing_custom_shortcut_) {
            prompt = shortcut_capture_status_.empty()
                ? L"Press a shortcut now · Esc cancels"
                : shortcut_capture_status_;
        } else if (shortcut_confirmed_) {
            prompt = L"✓  Shortcut confirmed — " +
                shortcut_name(settings_->shortcut(), settings_->shortcut_modifiers()) + L" is ready";
        } else {
            prompt = L"Press " +
                shortcut_name(settings_->shortcut(), settings_->shortcut_modifiers()) +
                L" now to rehearse it";
        }
        draw_text(graphics, prompt, RectF(left, y, width, 32.0F * scale), 14.0F * scale,
                  FontStyleBold, shortcut_confirmed_ ? palette.success : palette.text);
        draw_text(graphics,
                  L"For typing keys, use Ctrl or Alt; Shift can be added.",
                  RectF(left, y + 34.0F * scale, width, 24.0F * scale),
                  11.5F * scale, FontStyleRegular, palette.muted);
    } else {
        const bool streaming =
            settings_ != nullptr && settings_->streaming_mode_enabled();
        draw_text(graphics,
                  streaming
                      ? L"Short phrases appear after a natural pause"
                      : L"Text appears after dictation finishes",
                  RectF(left + 34.0F * scale, heading_y + 220.0F * scale,
                        width - 34.0F * scale, 20.0F * scale),
                  11.5F * scale, FontStyleRegular, palette.muted);
        draw_text(graphics, L"OUTPUT",
                  RectF(left, heading_y + 252.0F * scale, width, 20.0F * scale),
                  10.0F * scale, FontStyleBold, palette.muted);
        draw_text(graphics, L"LANGUAGE WHITELIST",
                  RectF(left, heading_y + 360.0F * scale, width, 18.0F * scale),
                  10.0F * scale, FontStyleBold, palette.muted);
        draw_text(graphics,
                  L"Chinese and English stay on · add languages you speak",
                  RectF(left, heading_y + 378.0F * scale, width, 20.0F * scale),
                  11.0F * scale, FontStyleRegular, palette.muted);
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
    const bool navigation = item.CtlID >= kPageButtonFirst && item.CtlID <= kPageButtonLast;
    graphics.Clear(navigation ? palette.surface : palette.canvas);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool focused = (item.itemState & ODS_FOCUS) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool primary = item.CtlID == kButtonNext;
    const bool grammar_option = item.CtlID == kButtonGrammarOff ||
                                item.CtlID == kButtonGrammarStandard ||
                                item.CtlID == kButtonGrammarAdvanced;
    const bool language_tag = item.CtlID >= kButtonLanguageChinese &&
                              item.CtlID <= kButtonLanguageKorean;
    const bool fixed_language_tag = item.CtlID == kButtonLanguageChinese ||
                                    item.CtlID == kButtonLanguageEnglish;
    const bool option = item.CtlID == kButtonRightAlt || item.CtlID == kButtonF8 ||
                        item.CtlID == kButtonCustomShortcut ||
                        grammar_option || language_tag;
    const bool check_option = item.CtlID == kButtonLogin || item.CtlID == kButtonStreaming;
    const bool selected = settings_ != nullptr &&
        ((item.CtlID == kButtonRightAlt && settings_->shortcut() == ShortcutKey::RightAlt &&
          settings_->shortcut_modifiers() == 0) ||
         (item.CtlID == kButtonF8 && settings_->shortcut() == ShortcutKey::F8 &&
          settings_->shortcut_modifiers() == 0) ||
         (item.CtlID == kButtonCustomShortcut &&
          (capturing_custom_shortcut_ ||
           settings_->shortcut_modifiers() != 0 ||
           (settings_->shortcut() != ShortcutKey::RightAlt &&
            settings_->shortcut() != ShortcutKey::F8))) ||
         (item.CtlID == kButtonGrammarOff &&
          settings_->output_mode() == OutputMode::Exact) ||
         (item.CtlID == kButtonGrammarStandard &&
          settings_->output_mode() == OutputMode::Clean) ||
         (item.CtlID == kButtonGrammarAdvanced &&
          settings_->output_mode() == OutputMode::Adapt) ||
         item.CtlID == kButtonLanguageChinese ||
         item.CtlID == kButtonLanguageEnglish ||
         (item.CtlID == kButtonLanguageJapanese &&
          speech_language_enabled(
              settings_->speech_languages(), SpeechLanguage::Japanese)) ||
         (item.CtlID == kButtonLanguageKorean &&
          speech_language_enabled(
              settings_->speech_languages(), SpeechLanguage::Korean)));
    const REAL scale = static_cast<REAL>(ui_dpi(item.hwndItem)) / 96.0F;
    const Color control_border = palette.high_contrast
        ? palette.border
        : Color(190, palette.muted.GetR(), palette.muted.GetG(), palette.muted.GetB());

    RectF rect(static_cast<REAL>(item.rcItem.left) + 2.0F * scale,
               static_cast<REAL>(item.rcItem.top) + 2.0F * scale,
               static_cast<REAL>(item.rcItem.right - item.rcItem.left) - 4.0F * scale,
               static_cast<REAL>(item.rcItem.bottom - item.rcItem.top) - 4.0F * scale);
    const bool navigation_selected = navigation &&
        static_cast<int>(item.CtlID) - kPageButtonFirst == page_;
    Color fill = primary ? (pressed ? palette.accent_pressed : palette.accent) : palette.surface;
    Color border = primary ? palette.accent : ((option && selected) ? palette.accent : palette.border);
    if (navigation) {
        fill = navigation_selected
            ? (pressed ? palette.accent_pressed : palette.accent)
            : palette.surface;
        border = Color(0, 0, 0, 0);
    }
    if (language_tag) {
        fill = selected
            ? (pressed ? palette.accent_pressed : palette.accent)
            : palette.surface;
        border = selected ? palette.accent : palette.border;
    }
    if (check_option) {
        fill = palette.canvas;
        border = Color(0, 0, 0, 0);
    }
    rounded_rectangle(graphics, rect, (language_tag ? 18.0F : 8.0F) * scale, fill, border,
                      (option && selected) ? 2.0F * scale : 1.0F * scale);

    wchar_t label[256]{};
    GetWindowTextW(item.hwndItem, label, static_cast<int>(std::size(label)));
    std::wstring visual_label = label;
    if (item.CtlID == kButtonRightAlt) {
        visual_label = L"Right Alt";
    } else if (item.CtlID == kButtonF8) {
        visual_label = L"F8";
    } else if (item.CtlID == kButtonCustomShortcut && settings_ != nullptr) {
        const bool custom = settings_->shortcut_modifiers() != 0 ||
            (settings_->shortcut() != ShortcutKey::RightAlt &&
             settings_->shortcut() != ShortcutKey::F8);
        visual_label = capturing_custom_shortcut_
            ? L"Press shortcut…"
            : (custom
                ? shortcut_name(settings_->shortcut(), settings_->shortcut_modifiers())
                : L"Custom…");
    } else if (item.CtlID == kButtonStreaming) {
        visual_label = L"Type while I speak";
    } else if (item.CtlID == kButtonLogin) {
        visual_label = L"Launch SAID when I sign in";
    } else if (item.CtlID == kButtonGrammarOff) {
        visual_label = L"Exact";
    } else if (item.CtlID == kButtonGrammarStandard) {
        visual_label = L"Clean";
    } else if (item.CtlID == kButtonLanguageChinese) {
        visual_label = L"✓  Chinese";
    } else if (item.CtlID == kButtonLanguageEnglish) {
        visual_label = L"✓  English";
    } else if (item.CtlID == kButtonLanguageJapanese && settings_ != nullptr) {
        visual_label = speech_language_enabled(
            settings_->speech_languages(), SpeechLanguage::Japanese)
            ? L"✓  Japanese"
            : L"Japanese";
    } else if (item.CtlID == kButtonLanguageKorean && settings_ != nullptr) {
        visual_label = speech_language_enabled(
            settings_->speech_languages(), SpeechLanguage::Korean)
            ? L"✓  Korean"
            : L"Korean";
    }
    RectF text_rect = rect;
    if (navigation) {
        text_rect.X += 14.0F * scale;
        text_rect.Width -= 28.0F * scale;
    } else if (grammar_option) {
        std::wstring description = item.CtlID == kButtonGrammarOff
            ? L"Recognizer text, unchanged"
            : (item.CtlID == kButtonGrammarStandard
                ? L"Fix speech mistakes · bundled"
                : L"About 1.7 GB warm · optional model");
        if (item.CtlID == kButtonGrammarAdvanced) {
            if (advanced_model_state_ == AdvancedModelState::Installed) {
                description = L"Ready · about 1.7 GB while warm";
            } else if (advanced_model_state_ == AdvancedModelState::Downloading) {
                description = L"Downloading · " +
                    std::to_wstring(advanced_model_progress_percent_) + L"% · click to cancel";
            } else if (advanced_model_state_ == AdvancedModelState::Verifying) {
                description = L"Verifying download…";
            } else if (advanced_model_state_ == AdvancedModelState::Failed) {
                description = L"Download failed · retry";
            }
        }
        const RectF dot(rect.X + 16.0F * scale, rect.Y + 17.0F * scale,
                        16.0F * scale, 16.0F * scale);
        SolidBrush dot_fill(selected ? palette.accent : palette.surface);
        Pen dot_border(selected ? palette.accent : control_border, 1.5F * scale);
        graphics.FillEllipse(&dot_fill, dot);
        graphics.DrawEllipse(&dot_border, dot);
        const bool adapt_option = item.CtlID == kButtonGrammarAdvanced;
        draw_text(graphics, adapt_option ? L"Adapt" : visual_label,
                  RectF(rect.X + 42.0F * scale, rect.Y + 10.0F * scale,
                        adapt_option ? 48.0F * scale : rect.Width - 54.0F * scale,
                        22.0F * scale),
                  13.0F * scale, FontStyleBold, palette.text);
        if (adapt_option) {
            draw_text(graphics, L"16 GB RAM",
                      RectF(rect.X + 90.0F * scale, rect.Y + 12.0F * scale,
                            rect.Width - 106.0F * scale, 18.0F * scale),
                      9.0F * scale, FontStyleBold, palette.muted,
                      StringAlignmentFar, StringAlignmentNear);
        }
        draw_text(graphics, description,
                  RectF(rect.X + 16.0F * scale, rect.Y + 39.0F * scale,
                        rect.Width - 28.0F * scale, 20.0F * scale),
                  (adapt_option ? 10.0F : 10.5F) * scale,
                  FontStyleRegular, palette.muted);
        if (item.CtlID == kButtonGrammarAdvanced &&
            advanced_model_state_ == AdvancedModelState::Downloading) {
            const REAL progress_width = (rect.Width - 32.0F * scale) *
                static_cast<REAL>(advanced_model_progress_percent_) / 100.0F;
            SolidBrush track(palette.border);
            SolidBrush progress(palette.accent);
            graphics.FillRectangle(&track, rect.X + 16.0F * scale,
                                   rect.GetBottom() - 8.0F * scale,
                                   rect.Width - 32.0F * scale, 2.0F * scale);
            graphics.FillRectangle(&progress, rect.X + 16.0F * scale,
                                   rect.GetBottom() - 8.0F * scale,
                                   progress_width, 2.0F * scale);
        }
    } else if (check_option) {
        const bool checked = settings_ != nullptr &&
            (item.CtlID == kButtonLogin
                ? settings_->launch_at_login()
                : settings_->streaming_mode_enabled());
        const RectF box(rect.X + 2.0F * scale,
                        rect.Y + (rect.Height - 20.0F * scale) / 2.0F,
                        20.0F * scale, 20.0F * scale);
        rounded_rectangle(graphics, box, 5.0F * scale,
                          checked ? palette.accent : palette.surface,
                          checked ? palette.accent : control_border, 1.0F * scale);
        if (checked) {
            Pen check(palette.on_accent, 2.2F * scale);
            check.SetStartCap(LineCapRound);
            check.SetEndCap(LineCapRound);
            graphics.DrawLine(&check, box.X + 5.0F * scale, box.Y + 10.0F * scale,
                              box.X + 8.5F * scale, box.Y + 14.0F * scale);
            graphics.DrawLine(&check, box.X + 8.5F * scale, box.Y + 14.0F * scale,
                              box.X + 16.0F * scale, box.Y + 6.0F * scale);
        }
        text_rect.X += 34.0F * scale;
        text_rect.Width -= 34.0F * scale;
    }
    Color text_color = (primary || navigation_selected || (language_tag && selected))
        ? palette.on_accent
        : palette.text;
    if (disabled && !fixed_language_tag) {
        text_color = palette.muted;
    }
    if (!grammar_option) {
        draw_text(graphics, visual_label, text_rect, 14.0F * scale, FontStyleBold, text_color,
                  (check_option || navigation) ? StringAlignmentNear : StringAlignmentCenter,
                  StringAlignmentCenter);
    }
    if (focused) {
        RectF focus(rect.X - 1.0F * scale, rect.Y - 1.0F * scale,
                    rect.Width + 2.0F * scale, rect.Height + 2.0F * scale);
        const Color focus_color = (primary || navigation_selected)
            ? palette.on_accent
            : palette.accent;
        rounded_rectangle(graphics, focus, 10.0F * scale,
                          Color(0, 0, 0, 0), focus_color, 2.0F * scale);
    }
}

void SetupWindow::layout() {
    if (window_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int scale = static_cast<int>(ui_dpi(window_));
    const auto px = [scale](int value) { return MulDiv(value, scale, 96); };
    const int bottom = client.bottom - px(32);
    MoveWindow(next_button_, client.right - px(54) - px(154), bottom - px(48), px(154), px(48), TRUE);
    MoveWindow(back_button_, px(254), bottom - px(48), px(104), px(48), TRUE);
    for (size_t index = 0; index < page_buttons_.size(); ++index) {
        MoveWindow(page_buttons_[index], px(22), px(126 + static_cast<int>(index) * 54),
                   px(166), px(42), TRUE);
    }
    MoveWindow(right_alt_button_, px(254), px(286), px(168), px(62), TRUE);
    MoveWindow(f8_button_, px(434), px(286), px(104), px(62), TRUE);
    MoveWindow(custom_shortcut_button_, px(550), px(286), px(336), px(62), TRUE);
    MoveWindow(streaming_button_, px(252), px(264), px(330), px(40), TRUE);
    MoveWindow(grammar_off_button_, px(254), px(370), px(184), px(76), TRUE);
    MoveWindow(grammar_standard_button_, px(450), px(370), px(200), px(76), TRUE);
    MoveWindow(grammar_advanced_button_, px(662), px(370), px(224), px(76), TRUE);
    MoveWindow(chinese_language_button_, px(254), px(486), px(138), px(38), TRUE);
    MoveWindow(english_language_button_, px(404), px(486), px(138), px(38), TRUE);
    MoveWindow(japanese_language_button_, px(554), px(486), px(154), px(38), TRUE);
    MoveWindow(korean_language_button_, px(720), px(486), px(142), px(38), TRUE);
    MoveWindow(login_button_, px(252), px(536), px(310), px(32), TRUE);
}

void SetupWindow::invalidate_microphone_meter() {
    if (window_ == nullptr || page_ != 1) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int dpi = static_cast<int>(ui_dpi(window_));
    const auto px = [dpi](int value) { return MulDiv(value, dpi, 96); };
    const RECT meter{
        px(250),
        px(268),
        client.right - px(50),
        px(428),
    };
    InvalidateRect(window_, &meter, FALSE);
}

void SetupWindow::go_to_page(int page) {
    page = std::clamp(page, 0, 3);
    if (page != 2) {
        cancel_custom_shortcut_capture();
    }
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
    ShowWindow(back_button_, first_run_ && page_ > 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(right_alt_button_, page_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(f8_button_, page_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(custom_shortcut_button_, page_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(streaming_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(grammar_off_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(grammar_standard_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(grammar_advanced_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(chinese_language_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(english_language_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(japanese_language_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(korean_language_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    ShowWindow(login_button_, page_ == 3 ? SW_SHOW : SW_HIDE);
    SetWindowTextW(next_button_, !first_run_ ? L"Done" : (page_ == 3 ? L"Start SAID" : L"Continue"));
    SetWindowTextW(back_button_, L"Back");
    if (settings_ != nullptr) {
        const bool custom = settings_->shortcut_modifiers() != 0 ||
            (settings_->shortcut() != ShortcutKey::RightAlt &&
             settings_->shortcut() != ShortcutKey::F8);
        const std::wstring custom_label = capturing_custom_shortcut_
            ? L"Press shortcut…"
            : (custom
                ? shortcut_name(settings_->shortcut(), settings_->shortcut_modifiers())
                : L"Custom…");
        const bool right_alt_selected = settings_->shortcut() == ShortcutKey::RightAlt &&
            settings_->shortcut_modifiers() == 0;
        const bool f8_selected = settings_->shortcut() == ShortcutKey::F8 &&
            settings_->shortcut_modifiers() == 0;
        const std::wstring custom_accessible_label = L"Custom shortcut — " + custom_label +
            (custom ? L" — selected" : L"");
        SetWindowTextW(
            right_alt_button_,
            right_alt_selected ? L"Right Alt — selected" : L"Right Alt");
        SetWindowTextW(
            f8_button_,
            f8_selected ? L"F8 — selected" : L"F8");
        SetWindowTextW(custom_shortcut_button_, custom_accessible_label.c_str());

        const bool streaming = settings_->streaming_mode_enabled();
        SetWindowTextW(
            streaming_button_,
            streaming
                ? L"Type while I speak — on. Short phrases appear after a natural pause"
                : L"Type while I speak — off. Text appears after dictation finishes");

        const OutputMode output_mode = settings_->output_mode();
        SetWindowTextW(
            grammar_off_button_,
            output_mode == OutputMode::Exact
                ? L"Exact — selected. Recognizer text stays unchanged"
                : L"Exact. Recognizer text stays unchanged");
        SetWindowTextW(
            grammar_standard_button_,
            output_mode == OutputMode::Clean
                ? L"Clean — selected. Fixes speech mistakes; bundled"
                : L"Clean. Fixes speech mistakes; bundled");
        SetWindowTextW(
            grammar_advanced_button_,
            output_mode == OutputMode::Adapt
                ? L"Adapt — selected. Optional local model; 16 GB RAM recommended; about 1.7 GB while warm"
                : L"Adapt. Optional local model; 16 GB RAM recommended; about 1.7 GB while warm");

        const bool japanese = speech_language_enabled(
            settings_->speech_languages(), SpeechLanguage::Japanese);
        const bool korean = speech_language_enabled(
            settings_->speech_languages(), SpeechLanguage::Korean);
        SetWindowTextW(chinese_language_button_, L"Chinese — always selected");
        SetWindowTextW(english_language_button_, L"English — always selected");
        SetWindowTextW(
            japanese_language_button_,
            japanese ? L"Japanese — selected" : L"Japanese — not selected");
        SetWindowTextW(
            korean_language_button_,
            korean ? L"Korean — selected" : L"Korean — not selected");
        SetWindowTextW(
            login_button_,
            settings_->launch_at_login()
                ? L"Launch SAID when I sign in — on"
                : L"Launch SAID when I sign in — off");
    }
    InvalidateRect(next_button_, nullptr, FALSE);
    for (HWND button : page_buttons_) {
        InvalidateRect(button, nullptr, FALSE);
    }
    InvalidateRect(chinese_language_button_, nullptr, FALSE);
    InvalidateRect(english_language_button_, nullptr, FALSE);
    InvalidateRect(japanese_language_button_, nullptr, FALSE);
    InvalidateRect(korean_language_button_, nullptr, FALSE);
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
    cancel_custom_shortcut_capture();
    if (page_ == 1) {
        KillTimer(window_, kMeterTimer);
        microphone_test_.stop();
    }
    ShowWindow(window_, SW_HIDE);
}

void SetupWindow::begin_custom_shortcut_capture() {
    if (window_ == nullptr || page_ != 2) {
        return;
    }
    capturing_custom_shortcut_ = true;
    shortcut_confirmed_ = false;
    shortcut_capture_status_ = L"Press a shortcut now · Esc cancels";
    update_controls();
    SetFocus(window_);
    InvalidateRect(custom_shortcut_button_, nullptr, FALSE);
    InvalidateRect(window_, nullptr, FALSE);
}

void SetupWindow::cancel_custom_shortcut_capture() {
    if (!capturing_custom_shortcut_) {
        return;
    }
    capturing_custom_shortcut_ = false;
    shortcut_capture_status_.clear();
    update_controls();
    if (visible() && page_ == 2) {
        SetFocus(custom_shortcut_button_);
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void SetupWindow::capture_custom_shortcut(DWORD virtual_key) {
    if (!capturing_custom_shortcut_ || settings_ == nullptr) {
        return;
    }
    if (is_shortcut_modifier_key(virtual_key)) {
        shortcut_capture_status_ = L"Keep the modifier held, then press another key";
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }
    if (!is_valid_shortcut_key(virtual_key)) {
        shortcut_capture_status_ = L"That key cannot be used · try another shortcut";
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    DWORD modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= kShortcutModifierControl;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= kShortcutModifierAlt;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= kShortcutModifierShift;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetKeyState(VK_RWIN) & 0x8000) != 0) {
        modifiers |= kShortcutModifierWindows;
    }

    if ((modifiers & kShortcutModifierWindows) != 0) {
        shortcut_capture_status_ = L"Windows-key shortcuts stay reserved for Windows · try Ctrl or Alt";
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    const bool typing_key = (virtual_key >= L'0' && virtual_key <= L'9') ||
        (virtual_key >= L'A' && virtual_key <= L'Z') ||
        (virtual_key >= VK_NUMPAD0 && virtual_key <= VK_DIVIDE) ||
        (virtual_key >= VK_OEM_1 && virtual_key <= VK_OEM_102) ||
        virtual_key == VK_SPACE || virtual_key == VK_RETURN || virtual_key == VK_BACK;
    const DWORD command_modifiers = modifiers &
        (kShortcutModifierControl | kShortcutModifierAlt);
    if (typing_key && command_modifiers == 0) {
        shortcut_capture_status_ = L"Add Ctrl or Alt to avoid blocking normal typing";
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }
    if (virtual_key == VK_DELETE &&
        (modifiers & (kShortcutModifierControl | kShortcutModifierAlt)) ==
            (kShortcutModifierControl | kShortcutModifierAlt)) {
        shortcut_capture_status_ = L"Ctrl + Alt + Delete is reserved by Windows · try another shortcut";
        InvalidateRect(window_, nullptr, FALSE);
        return;
    }

    settings_->set_shortcut(static_cast<ShortcutKey>(virtual_key), modifiers);
    capturing_custom_shortcut_ = false;
    shortcut_confirmed_ = false;
    shortcut_capture_status_.clear();
    PostMessageW(notify_window_, kMessageShortcutChanged,
                 static_cast<WPARAM>(settings_->shortcut()),
                 static_cast<LPARAM>(settings_->shortcut_modifiers()));
    update_controls();
    SetFocus(custom_shortcut_button_);
    InvalidateRect(right_alt_button_, nullptr, FALSE);
    InvalidateRect(f8_button_, nullptr, FALSE);
    InvalidateRect(custom_shortcut_button_, nullptr, FALSE);
    InvalidateRect(window_, nullptr, FALSE);
}
