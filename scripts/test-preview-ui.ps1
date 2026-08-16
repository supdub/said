param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]$Executable
)

$ErrorActionPreference = "Stop"
$Dash = [char]0x2014

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class SAIDPreviewTestNative {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr parent, int identifier);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr window, StringBuilder text, int maximumCount);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
}
"@

function Get-RegistrySnapshot {
    $Queries = @(
        @("query", "HKCU\Software\SAID", "/s"),
        @("query", "HKCU\Software\VoiceKey", "/s"),
        @("query", "HKCU\Software\Microsoft\Windows\CurrentVersion\Run", "/v", "SAID"),
        @("query", "HKCU\Software\Microsoft\Windows\CurrentVersion\Run", "/v", "VoiceKey")
    )
    $Sections = foreach ($Query in $Queries) {
        try {
            $Output = & reg.exe @Query 2>$null
        }
        catch {
            # A missing optional key/value is part of the snapshot, not a test failure.
            $Output = @()
        }
        "[$($Query -join ' ')]`n$($Output -join "`n")"
    }
    return $Sections -join "`n"
}

function Wait-ForSetupWindow([System.Diagnostics.Process]$Process) {
    $Deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "SAID preview exited with code $($Process.ExitCode)."
        }
        $Window = $Process.MainWindowHandle
        if ($Window -eq [IntPtr]::Zero) {
            $Window = [SAIDPreviewTestNative]::FindWindow("SAIDSetupWindow", $null)
        }
        if ($Window -ne [IntPtr]::Zero) {
            return $Window
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $Deadline)
    throw "SAID preview did not create its setup window."
}

function Get-ControlText([IntPtr]$Window, [int]$Identifier) {
    $Control = [SAIDPreviewTestNative]::GetDlgItem($Window, $Identifier)
    if ($Control -eq [IntPtr]::Zero) {
        throw "SAID preview control $Identifier was not found."
    }
    $Text = New-Object System.Text.StringBuilder 512
    [SAIDPreviewTestNative]::GetWindowText($Control, $Text, $Text.Capacity) | Out-Null
    return $Text.ToString()
}

function Invoke-Control([IntPtr]$Window, [int]$Identifier) {
    $Control = [SAIDPreviewTestNative]::GetDlgItem($Window, $Identifier)
    if ($Control -eq [IntPtr]::Zero) {
        throw "SAID preview control $Identifier was not found."
    }
    # BM_CLICK exercises the same WM_COMMAND path as a keyboard or pointer activation.
    [SAIDPreviewTestNative]::SendMessage($Control, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 75
}

$Before = Get-RegistrySnapshot
$Process = Start-Process -FilePath $Executable -ArgumentList @("--preview-ui", "--preview-page-3") -PassThru
try {
    $Window = Wait-ForSetupWindow $Process
    $ExpectedDefaults = @{
        205 = "Launch SAID when I sign in ${Dash} off"
        207 = "Type while I speak ${Dash} off. Text appears after dictation finishes"
        208 = "Exact. Recognizer text stays unchanged"
        209 = "Clean ${Dash} selected. Fixes speech mistakes; bundled"
        210 = "Adapt. Optional local model; 16 GB RAM recommended; about 1.7 GB while warm"
        216 = "Chinese ${Dash} always selected"
        217 = "English ${Dash} always selected"
        218 = "Japanese ${Dash} not selected"
        219 = "Korean ${Dash} not selected"
    }
    foreach ($Entry in $ExpectedDefaults.GetEnumerator()) {
        $Actual = Get-ControlText $Window $Entry.Key
        if ($Actual -cne $Entry.Value) {
            throw "Control $($Entry.Key) accessible text was '$Actual'; expected '$($Entry.Value)'."
        }
    }

    # Exercise every preview setting that does not start an optional model download.
    foreach ($Identifier in @(207, 208, 209, 218, 219, 205)) {
        Invoke-Control $Window $Identifier
    }

    $ExpectedChanges = @{
        205 = "Launch SAID when I sign in ${Dash} on"
        207 = "Type while I speak ${Dash} on. Short phrases appear after a natural pause"
        209 = "Clean ${Dash} selected. Fixes speech mistakes; bundled"
        218 = "Japanese ${Dash} selected"
        219 = "Korean ${Dash} selected"
    }
    foreach ($Entry in $ExpectedChanges.GetEnumerator()) {
        $Actual = Get-ControlText $Window $Entry.Key
        if ($Actual -cne $Entry.Value) {
            throw "Control $($Entry.Key) accessible text was '$Actual'; expected '$($Entry.Value)'."
        }
    }

    # Finish first-run setup too; its completion flag must remain preview-only.
    Invoke-Control $Window 202
}
finally {
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

$After = Get-RegistrySnapshot
if ($After -cne $Before) {
    throw "Preview interactions changed SAID, VoiceKey, or startup registry settings."
}

Write-Output "Preview defaults, accessible control states, and registry isolation passed."
