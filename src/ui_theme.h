#pragma once

#include <windows.h>
#include <propidl.h>
#include <gdiplus.h>

struct UiPalette {
    Gdiplus::Color canvas;
    Gdiplus::Color surface;
    Gdiplus::Color surface_raised;
    Gdiplus::Color text;
    Gdiplus::Color muted;
    Gdiplus::Color border;
    Gdiplus::Color accent;
    Gdiplus::Color accent_pressed;
    Gdiplus::Color success;
    Gdiplus::Color error;
    bool dark = false;
    bool high_contrast = false;
};

inline bool system_uses_dark_apps() {
    wchar_t override_value[16]{};
    const DWORD override_length = GetEnvironmentVariableW(
        L"VOICEKEY_THEME", override_value, static_cast<DWORD>(_countof(override_value)));
    if (override_length > 0 && override_length < _countof(override_value)) {
        if (lstrcmpiW(override_value, L"dark") == 0) {
            return true;
        }
        if (lstrcmpiW(override_value, L"light") == 0) {
            return false;
        }
    }
    DWORD light = 1;
    DWORD size = sizeof(light);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) != ERROR_SUCCESS) {
        return false;
    }
    return light == 0;
}

inline UiPalette current_palette() {
    HIGHCONTRASTW high_contrast{};
    high_contrast.cbSize = sizeof(high_contrast);
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast), &high_contrast, 0);
    if ((high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0) {
        return {
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_WINDOW)), GetGValue(GetSysColor(COLOR_WINDOW)), GetBValue(GetSysColor(COLOR_WINDOW))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_WINDOW)), GetGValue(GetSysColor(COLOR_WINDOW)), GetBValue(GetSysColor(COLOR_WINDOW))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_BTNFACE)), GetGValue(GetSysColor(COLOR_BTNFACE)), GetBValue(GetSysColor(COLOR_BTNFACE))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_WINDOWTEXT)), GetGValue(GetSysColor(COLOR_WINDOWTEXT)), GetBValue(GetSysColor(COLOR_WINDOWTEXT))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_GRAYTEXT)), GetGValue(GetSysColor(COLOR_GRAYTEXT)), GetBValue(GetSysColor(COLOR_GRAYTEXT))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_WINDOWTEXT)), GetGValue(GetSysColor(COLOR_WINDOWTEXT)), GetBValue(GetSysColor(COLOR_WINDOWTEXT))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_HIGHLIGHT)), GetGValue(GetSysColor(COLOR_HIGHLIGHT)), GetBValue(GetSysColor(COLOR_HIGHLIGHT))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_HIGHLIGHT)), GetGValue(GetSysColor(COLOR_HIGHLIGHT)), GetBValue(GetSysColor(COLOR_HIGHLIGHT))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_HIGHLIGHT)), GetGValue(GetSysColor(COLOR_HIGHLIGHT)), GetBValue(GetSysColor(COLOR_HIGHLIGHT))),
            Gdiplus::Color(GetRValue(GetSysColor(COLOR_HOTLIGHT)), GetGValue(GetSysColor(COLOR_HOTLIGHT)), GetBValue(GetSysColor(COLOR_HOTLIGHT))),
            false,
            true,
        };
    }

    if (system_uses_dark_apps()) {
        return {
            Gdiplus::Color(22, 23, 22),
            Gdiplus::Color(34, 35, 33),
            Gdiplus::Color(42, 43, 40),
            Gdiplus::Color(242, 240, 233),
            Gdiplus::Color(167, 168, 159),
            Gdiplus::Color(58, 59, 55),
            Gdiplus::Color(240, 100, 73),
            Gdiplus::Color(211, 76, 52),
            Gdiplus::Color(84, 185, 138),
            Gdiplus::Color(240, 107, 103),
            true,
            false,
        };
    }

    return {
        Gdiplus::Color(244, 242, 237),
        Gdiplus::Color(252, 250, 245),
        Gdiplus::Color(255, 254, 250),
        Gdiplus::Color(32, 33, 30),
        Gdiplus::Color(104, 106, 99),
        Gdiplus::Color(216, 214, 207),
        Gdiplus::Color(226, 75, 50),
        Gdiplus::Color(191, 55, 36),
        Gdiplus::Color(38, 122, 85),
        Gdiplus::Color(185, 54, 50),
        false,
        false,
    };
}

inline void draw_voice_cursor(Gdiplus::Graphics & graphics, const Gdiplus::RectF & bounds,
                              const Gdiplus::Color & tile, const Gdiplus::Color & glyph) {
    using namespace Gdiplus;
    const REAL radius = bounds.Width * 0.23F;
    GraphicsPath path;
    const REAL diameter = radius * 2.0F;
    path.AddArc(bounds.X, bounds.Y, diameter, diameter, 180.0F, 90.0F);
    path.AddArc(bounds.GetRight() - diameter, bounds.Y, diameter, diameter, 270.0F, 90.0F);
    path.AddArc(bounds.GetRight() - diameter, bounds.GetBottom() - diameter, diameter, diameter, 0.0F, 90.0F);
    path.AddArc(bounds.X, bounds.GetBottom() - diameter, diameter, diameter, 90.0F, 90.0F);
    path.CloseFigure();
    SolidBrush tile_brush(tile);
    graphics.FillPath(&tile_brush, &path);

    const REAL unit = bounds.Width / 512.0F;
    Pen pen(glyph, 44.0F * unit);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    graphics.DrawLine(&pen, bounds.X + 132.0F * unit, bounds.Y + 218.0F * unit,
                      bounds.X + 204.0F * unit, bounds.Y + 218.0F * unit);
    graphics.DrawLine(&pen, bounds.X + 132.0F * unit, bounds.Y + 294.0F * unit,
                      bounds.X + 248.0F * unit, bounds.Y + 294.0F * unit);
    Pen caret(glyph, 48.0F * unit);
    caret.SetStartCap(LineCapRound);
    caret.SetEndCap(LineCapRound);
    graphics.DrawLine(&caret, bounds.X + 322.0F * unit, bounds.Y + 146.0F * unit,
                      bounds.X + 322.0F * unit, bounds.Y + 366.0F * unit);
}

inline int dpi_scale(HWND window, int value) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(window)), 96);
}
