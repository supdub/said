param(
    [Parameter(Mandatory = $true)] [string]$Executable,
    [Parameter(Mandatory = $true)] [string]$Output
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System.Runtime.InteropServices;
public static class VoiceKeyOverlayCapture {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[VoiceKeyOverlayCapture]::SetProcessDPIAware() | Out-Null

$Process = Start-Process -FilePath $Executable -ArgumentList "--preview-overlay" -PassThru
try {
    Start-Sleep -Milliseconds 900
    if ($Process.HasExited) {
        throw "VoiceKey overlay preview exited with code $($Process.ExitCode)."
    }
    $Work = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    $Width = 610
    $Height = 150
    $Left = $Work.Left + [math]::Floor(($Work.Width - $Width) / 2)
    $Top = $Work.Bottom - $Height - 30
    $Bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
    $Graphics = [System.Drawing.Graphics]::FromImage($Bitmap)
    try {
        $Graphics.CopyFromScreen($Left, $Top, 0, 0, $Bitmap.Size)
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Output) | Out-Null
        $Bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $Graphics.Dispose()
        $Bitmap.Dispose()
    }
    Write-Output "Captured $Output."
}
finally {
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
    }
}
