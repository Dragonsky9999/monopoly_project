# Installs Qt 6 (MSVC 64-bit) and Qt Installer Framework into C:\Qt using aqtinstall.
# aqtinstall is installed into a private venv: %LOCALAPPDATA%\monopoly_aqt
# Tries the newest Qt 6 version first and falls back to older ones if aqt cannot fetch it.
$ErrorActionPreference = 'Stop'
$log = Join-Path $PSScriptRoot 'setup_qt.log'
Start-Transcript -Path $log -Force | Out-Null
try {
    function Run($exe, [string[]]$argv) {
        Write-Host ">> $exe $($argv -join ' ')"
        $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        & $exe @argv 2>&1 | ForEach-Object { "$_" } | Out-Host
        $code = $LASTEXITCODE
        $ErrorActionPreference = $old
        if ($code -ne 0) { throw "command failed ($code): $exe $($argv -join ' ')" }
    }
    function Try-Native($exe, [string[]]$argv) {
        $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        $out = (& $exe @argv 2>&1 | ForEach-Object { "$_" }) -join "`n"
        $code = $LASTEXITCODE
        $ErrorActionPreference = $old
        Write-Host ">> $exe $($argv -join ' ')  (exit $code)"
        Write-Host $out
        return @{ Code = $code; Out = $out }
    }

    $venv = Join-Path $env:LOCALAPPDATA 'monopoly_aqt'
    if (!(Test-Path "$venv\Scripts\python.exe")) { Run 'py' @('-3', '-m', 'venv', $venv) }
    $py = "$venv\Scripts\python.exe"
    Run $py @('-m', 'pip', 'install', '--quiet', '--upgrade', 'pip', 'aqtinstall')
    $aqt = "$venv\Scripts\aqt.exe"

    # already installed?
    $existing = Get-ChildItem C:\Qt -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
                Where-Object { Get-ChildItem $_.FullName -Directory | Where-Object { $_.Name -match '^msvc20\d\d_64$' } }
    if ($existing) {
        Write-Host "Qt already installed: $($existing.FullName -join ', ')"
    } else {
        $r = Try-Native $aqt @('list-qt', 'windows', 'desktop')
        $versions = @(($r.Out -split '\s+') | Where-Object { $_ -match '^6\.\d+\.\d+$' } |
                      Where-Object { [version]$_ -ge [version]'6.5.0' } | Sort-Object { [version]$_ } -Descending)
        Write-Host "versions: $($versions -join ', ')"
        $installed = $false
        foreach ($ver in ($versions | Select-Object -First 8)) {
            $r = Try-Native $aqt @('list-qt', 'windows', 'desktop', '--arch', $ver)
            if ($r.Code -ne 0) { continue }
            $arch = @(($r.Out -split '\s+') | Where-Object { $_ -match '^win64_msvc20\d\d_64$' } | Sort-Object -Descending) | Select-Object -First 1
            if (-not $arch) { continue }
            $r = Try-Native $aqt @('install-qt', 'windows', 'desktop', $ver, $arch, '-O', 'C:\Qt')
            if ($r.Code -eq 0) { Write-Host "INSTALLED Qt $ver $arch"; $installed = $true; break }
        }
        if (-not $installed) { throw 'Qt install failed for all candidate versions' }
    }

    if (Test-Path C:\Qt\Tools\QtInstallerFramework) {
        Write-Host 'IFW already installed'
    } else {
        $r = Try-Native $aqt @('list-tool', 'windows', 'desktop', 'tools_ifw')
        Run $aqt @('install-tool', 'windows', 'desktop', 'tools_ifw', '-O', 'C:\Qt')
    }

    Get-ChildItem C:\Qt -Directory | ForEach-Object { $_.FullName }
    Get-ChildItem C:\Qt\Tools -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
    Get-ChildItem C:\Qt\Tools\QtInstallerFramework -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }
    Write-Host 'SETUP_OK'
}
catch {
    Write-Host "SETUP_FAILED: $_"
}
finally {
    Stop-Transcript | Out-Null
}
