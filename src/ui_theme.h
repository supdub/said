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
    Gdiplus::Color on_accent;
    Gdiplus::Color success;
    Gdiplus::Color error;
    bool dark = false;
    bool high_contrast = false;
};

inline Gdiplus::Color system_color(int index) {
    const COLORREF color = GetSysColor(index);
    return Gdiplus::Color(GetRValue(color), GetGValue(color), GetBValue(color));
}

inline bool theme_override_is(const wchar_t * expected) {
    constexpr const wchar_t * kEnvironmentNames[]{L"SAID_THEME", L"VOICEKEY_THEME"};
    for (const wchar_t * name : kEnvironmentNames) {
        wchar_t override_value[16]{};
        const DWORD override_length = GetEnvironmentVariableW(
            name, override_value, static_cast<DWORD>(_countof(override_value)));
        if (override_length > 0 && override_length < _countof(override_value)) {
            return lstrcmpiW(override_value, expected) == 0;
        }
    }
    return false;
}

inline bool system_uses_dark_apps() {
    if (theme_override_is(L"dark")) {
        return true;
    }
    if (theme_override_is(L"light")) {
        return false;
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

inline bool system_reduces_motion() {
    wchar_t override_value[8]{};
    const DWORD override_length = GetEnvironmentVariableW(
        L"SAID_REDUCED_MOTION", override_value, static_cast<DWORD>(_countof(override_value)));
    if (override_length > 0 && override_length < _countof(override_value) &&
        lstrcmpW(override_value, L"0") != 0) {
        return true;
    }
    BOOL client_animation = TRUE;
    if (!SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &client_animation, 0)) {
        return false;
    }
    return client_animation == FALSE;
}

inline UiPalette high_contrast_palette() {
    return {
        system_color(COLOR_WINDOW),
        system_color(COLOR_WINDOW),
        system_color(COLOR_BTNFACE),
        system_color(COLOR_WINDOWTEXT),
        system_color(COLOR_WINDOWTEXT),
        system_color(COLOR_WINDOWTEXT),
        system_color(COLOR_HIGHLIGHT),
        system_color(COLOR_HIGHLIGHT),
        system_color(COLOR_HIGHLIGHTTEXT),
        system_color(COLOR_WINDOWTEXT),
        system_color(COLOR_WINDOWTEXT),
        false,
        true,
    };
}

inline UiPalette branded_dark_palette() {
    return {
        Gdiplus::Color(21, 22, 19),     // Ink
        Gdiplus::Color(36, 37, 34),     // Raised ink
        Gdiplus::Color(44, 45, 41),
        Gdiplus::Color(245, 242, 233),  // Bone
        Gdiplus::Color(197, 195, 186),  // Muted bone
        Gdiplus::Color(69, 70, 65),     // Dark hairline
        Gdiplus::Color(245, 242, 233),
        Gdiplus::Color(197, 195, 186),
        Gdiplus::Color(21, 22, 19),
        Gdiplus::Color(245, 242, 233),
        Gdiplus::Color(245, 242, 233),
        true,
        false,
    };
}

inline UiPalette current_palette() {
    HIGHCONTRASTW high_contrast{};
    high_contrast.cbSize = sizeof(high_contrast);
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast), &high_contrast, 0);
    if ((high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0 ||
        theme_override_is(L"high-contrast")) {
        return high_contrast_palette();
    }

    if (system_uses_dark_apps()) {
        return branded_dark_palette();
    }

    return {
        Gdiplus::Color(245, 242, 233),  // Bone
        Gdiplus::Color(239, 236, 227),
        Gdiplus::Color(250, 247, 238),
        Gdiplus::Color(21, 22, 19),     // Ink
        Gdiplus::Color(80, 81, 75),
        Gdiplus::Color(209, 206, 197),  // Light hairline
        Gdiplus::Color(21, 22, 19),
        Gdiplus::Color(36, 37, 34),
        Gdiplus::Color(245, 242, 233),
        Gdiplus::Color(21, 22, 19),
        Gdiplus::Color(21, 22, 19),
        false,
        false,
    };
}

inline UiPalette overlay_palette() {
    HIGHCONTRASTW high_contrast{};
    high_contrast.cbSize = sizeof(high_contrast);
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast), &high_contrast, 0);
    return (high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0 ||
            theme_override_is(L"high-contrast")
        ? high_contrast_palette()
        : branded_dark_palette();
}

inline void draw_said_mark(Gdiplus::Graphics & graphics, const Gdiplus::RectF & bounds,
                           const Gdiplus::Color & tile, const Gdiplus::Color & glyph) {
    using namespace Gdiplus;
    const REAL radius = bounds.Width * 0.19F;
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
    SolidBrush glyph_brush(glyph);
    constexpr REAL kDotCenters[]{126.0F, 209.0F, 282.0F};
    for (REAL center : kDotCenters) {
        graphics.FillEllipse(&glyph_brush,
                             bounds.X + (center - 28.0F) * unit,
                             bounds.Y + 228.0F * unit,
                             56.0F * unit,
                             56.0F * unit);
    }
    graphics.FillRectangle(&glyph_brush,
                           bounds.X + 327.0F * unit,
                           bounds.Y + 140.0F * unit,
                           34.0F * unit,
                           232.0F * unit);
    graphics.FillEllipse(&glyph_brush,
                         bounds.X + 327.0F * unit,
                         bounds.Y + 123.0F * unit,
                         34.0F * unit,
                         34.0F * unit);
    graphics.FillEllipse(&glyph_brush,
                         bounds.X + 327.0F * unit,
                         bounds.Y + 355.0F * unit,
                         34.0F * unit,
                         34.0F * unit);
}

inline int dpi_scale(HWND window, int value) {
    return MulDiv(value, static_cast<int>(GetDpiForWindow(window)), 96);
}
