param(
    [Parameter(Mandatory = $true)] [string]$Executable,
    [Parameter(Mandatory = $true)] [string]$Output,
    [ValidateSet("listening", "transcribing", "success", "error")]
    [string]$State = "transcribing",
    [ValidateSet("system", "light", "dark", "high-contrast")]
    [string]$Theme = "system",
    [switch]$ReducedMotion
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System.Runtime.InteropServices;
public static class SAIDOverlayCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
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

$Process = Start-Process -FilePath $Executable -ArgumentList "--preview-overlay-$State" -PassThru
try {
    $Deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 150
        $Process.Refresh()
    } while ($Process.MainWindowHandle -eq [IntPtr]::Zero -and
             [DateTime]::UtcNow -lt $Deadline)
    if ($Process.HasExited) {
        throw "SAID overlay preview exited with code $($Process.ExitCode)."
    }
    if ($Process.MainWindowHandle -eq [IntPtr]::Zero) {
        throw "SAID overlay preview did not create a visible window."
    }
    Start-Sleep -Milliseconds 350
    $Rectangle = New-Object SAIDOverlayCapture+RECT
    if (-not [SAIDOverlayCapture]::GetWindowRect($Process.MainWindowHandle, [ref]$Rectangle)) {
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
