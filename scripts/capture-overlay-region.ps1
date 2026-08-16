param(
    [Parameter(Mandatory = $true)] [string]$Output,
    [int]$Width = 380,
    [int]$Height = 72,
    [int]$BottomOffset = 40
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System.Runtime.InteropServices;
public static class SAIDRegionCapture {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[SAIDRegionCapture]::SetProcessDPIAware() | Out-Null
$Work = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
$Left = $Work.Left + [math]::Floor(($Work.Width - $Width) / 2)
$Top = $Work.Bottom - $Height - $BottomOffset
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
