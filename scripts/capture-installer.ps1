param(
    [Parameter(Mandatory = $true)] [string]$Installer,
    [Parameter(Mandatory = $true)] [string]$Output
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class VoiceKeyInstallerCapture {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[VoiceKeyInstallerCapture]::SetProcessDPIAware() | Out-Null

Start-Process -FilePath $Installer | Out-Null
try {
    $Deadline = [DateTime]::UtcNow.AddSeconds(12)
    do {
        Start-Sleep -Milliseconds 250
        $Process = Get-Process | Where-Object { $_.MainWindowTitle -eq "VoiceKey Setup" } |
            Select-Object -First 1
    } while (-not $Process -and [DateTime]::UtcNow -lt $Deadline)
    if (-not $Process) {
        throw "The VoiceKey installer window did not appear."
    }
    [VoiceKeyInstallerCapture]::SetForegroundWindow($Process.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 900
    $Rectangle = New-Object VoiceKeyInstallerCapture+RECT
    [VoiceKeyInstallerCapture]::GetWindowRect($Process.MainWindowHandle, [ref]$Rectangle) | Out-Null
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
    Get-Process | Where-Object {
        $_.MainWindowTitle -eq "VoiceKey Setup" -or $_.ProcessName -like "VoiceKey-Setup*"
    } | Stop-Process -Force -ErrorAction SilentlyContinue
}
