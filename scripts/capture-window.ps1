param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [Parameter(Mandatory = $true)]
    [string]$Output,
    [string[]]$Arguments = @("--preview-ui"),
    [int]$Advance = 0,
    [int]$PreviewPage = -1,
    [ValidateSet("system", "light", "dark", "high-contrast")]
    [string]$Theme = "system",
    [switch]$ReducedMotion,
    [switch]$NoArguments
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SAIDCaptureNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr window, out RECT rectangle);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")]
    public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr window, IntPtr deviceContext, uint flags);
}
"@

[SAIDCaptureNative]::SetProcessDPIAware() | Out-Null

if ($NoArguments) {
    $Arguments = @()
}
elseif ($PreviewPage -ge 0) {
    $Arguments = @("--preview-page-$PreviewPage")
}
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
if ($Arguments.Count -eq 0) {
    $Process = Start-Process -FilePath $Executable -PassThru
}
else {
    $Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru
}
try {
    $CaptureProcess = $Process
    $Deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        Start-Sleep -Milliseconds 150
        $CaptureProcess.Refresh()
        if ($CaptureProcess.MainWindowHandle -eq [IntPtr]::Zero) {
            $Candidate = Get-Process | Where-Object { $_.MainWindowTitle -like "*SAID*" } |
                Sort-Object StartTime -Descending | Select-Object -First 1
            if ($Candidate) {
                $CaptureProcess = $Candidate
            }
        }
    } while ($CaptureProcess.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $Deadline)

    if ($CaptureProcess.HasExited) {
        throw "SAID preview exited with code $($CaptureProcess.ExitCode)."
    }
    if ($CaptureProcess.MainWindowHandle -eq [IntPtr]::Zero) {
        throw "SAID preview did not create a visible window."
    }

    [SAIDCaptureNative]::SetForegroundWindow($CaptureProcess.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 350
    if ($Advance -gt 0) {
        Add-Type -AssemblyName System.Windows.Forms
        for ($Index = 0; $Index -lt $Advance; $Index++) {
            [System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
            Start-Sleep -Milliseconds 350
        }
    }
    $Rectangle = New-Object SAIDCaptureNative+RECT
    if (-not [SAIDCaptureNative]::GetWindowRect($CaptureProcess.MainWindowHandle, [ref]$Rectangle)) {
        throw "Could not read the SAID window bounds."
    }
    $Width = $Rectangle.Right - $Rectangle.Left
    $Height = $Rectangle.Bottom - $Rectangle.Top
    $Bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    try {
        $Graphics.CopyFromScreen($Rectangle.Left, $Rectangle.Top, 0, 0, $Bitmap.Size)
        $Directory = Split-Path -Parent $Output
        New-Item -ItemType Directory -Force -Path $Directory | Out-Null
        $Bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
    Write-Output "Captured $Output (${Width}x${Height})."
}
finally {
    if ($CaptureProcess -and $CaptureProcess.Id -ne $Process.Id -and -not $CaptureProcess.HasExited) {
        Stop-Process -Id $CaptureProcess.Id -Force
        $CaptureProcess.WaitForExit()
    }
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}
