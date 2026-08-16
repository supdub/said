param(
    [Parameter(Mandatory = $true)] [string]$Executable,
    [Parameter(Mandatory = $true)] [string]$Output,
    [ValidateSet("listening", "streaming", "streaming-clean", "streaming-adapt", "streaming-shell",
                 "streaming-paused", "transcribing", "finalizing", "correcting", "fallback",
                 "success", "silence", "copied", "error")]
    [string]$State = "transcribing",
    [ValidateSet("system", "light", "dark", "high-contrast")]
    [string]$Theme = "system",
    [ValidateSet(0, 100, 150, 200)]
    [int]$ScalePercent = 0,
    [ValidateRange(0, 15000)]
    [int]$DelayMilliseconds = 350,
    [switch]$ReducedMotion
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SAIDOverlayCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindow(string className, string windowName);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[SAIDOverlayCapture]::SetProcessDPIAware() | Out-Null

if ($Theme -eq "system") {
    Remove-Item Env:SAID_THEME -ErrorAction SilentlyContinue
}
else {
    $env:SAID_THEME = $Theme
}
if ($ReducedMotion) {
    $env:SAID_REDUCED_MOTION = "1"
}
else {
    Remove-Item Env:SAID_REDUCED_MOTION -ErrorAction SilentlyContinue
}
if ($ScalePercent -gt 0) {
    $env:SAID_PREVIEW_DPI = [string][math]::Round(96 * $ScalePercent / 100)
}
else {
    Remove-Item Env:SAID_PREVIEW_DPI -ErrorAction SilentlyContinue
}

$Process = Start-Process -FilePath $Executable -ArgumentList "--preview-overlay-$State" -PassThru
try {
    $Deadline = [DateTime]::UtcNow.AddSeconds(8)
    $WindowHandle = [IntPtr]::Zero
    do {
        Start-Sleep -Milliseconds 150
        $Process.Refresh()
        $WindowHandle = $Process.MainWindowHandle
        if ($WindowHandle -eq [IntPtr]::Zero) {
            $WindowHandle = [SAIDOverlayCapture]::FindWindow("SAIDOverlayWindow", $null)
        }
    } while ($WindowHandle -eq [IntPtr]::Zero -and
             [DateTime]::UtcNow -lt $Deadline)
    if ($Process.HasExited) {
        throw "SAID overlay preview exited with code $($Process.ExitCode)."
    }
    if ($WindowHandle -eq [IntPtr]::Zero) {
        throw "SAID overlay preview did not create a visible window."
    }
    Start-Sleep -Milliseconds $DelayMilliseconds
    $Rectangle = New-Object SAIDOverlayCapture+RECT
    if (-not [SAIDOverlayCapture]::GetWindowRect($WindowHandle, [ref]$Rectangle)) {
        throw "Could not read the SAID overlay bounds."
    }
    $Width = $Rectangle.Right - $Rectangle.Left
    $Height = $Rectangle.Bottom - $Rectangle.Top
    $Bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    try {
        $Graphics.CopyFromScreen($Rectangle.Left, $Rectangle.Top, 0, 0, $Bitmap.Size)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Output) | Out-Null
        $Bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
    Write-Output "Captured $Output (${Width}x${Height})."
}
finally {
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
    }
}
