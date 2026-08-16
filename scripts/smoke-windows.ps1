param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,
    [int]$Seconds = 3
)

$ErrorActionPreference = "Stop"
$Process = Start-Process -FilePath $Executable -PassThru
try {
    Start-Sleep -Seconds $Seconds
    $Process.Refresh()
    if ($Process.HasExited) {
        throw "SAID exited during startup with code $($Process.ExitCode)."
    }
    $WorkingSetMb = [math]::Round($Process.WorkingSet64 / 1MB, 1)
    Write-Output "SAID stayed running (PID $($Process.Id), working set ${WorkingSetMb} MB)."
}
finally {
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}
