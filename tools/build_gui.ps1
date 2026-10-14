# Builds GUI + CLI (Release), runs tests, makes a portable folder (dist\app) and the installer (dist\*.exe).
# Log: tools\build_gui.log   Result line: BUILD_OK / BUILD_FAILED
param([switch]$NoInstaller)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$log = Join-Path $PSScriptRoot 'build_gui.log'
Start-Transcript -Path $log -Force | Out-Null
try {
    function Run($exe, [string[]]$argv) {
        Write-Host ">> $exe $($argv -join ' ')"
        $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        & $exe @argv 2>&1 | Out-Host
        $code = $LASTEXITCODE
        $ErrorActionPreference = $old
        if ($code -ne 0) { throw "command failed ($code): $exe $($argv -join ' ')" }
    }

    $qtVer = Get-ChildItem C:\Qt -Directory | Where-Object { $_.Name -match '^6\.\d+\.\d+$' } |
             Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
    if (-not $qtVer) { throw 'Qt 6 not found under C:\Qt (run setup_qt.bat first)' }
    $qtDir = Get-ChildItem $qtVer.FullName -Directory | Where-Object { $_.Name -match '^msvc20\d\d_64$' } |
             Select-Object -First 1
    if (-not $qtDir) { throw "MSVC 64-bit Qt not found in $($qtVer.FullName)" }
    Write-Host "Qt: $($qtDir.FullName)"

    $ifwArg = @()
    $ifw = Get-ChildItem C:\Qt\Tools\QtInstallerFramework -Directory -ErrorAction SilentlyContinue |
           Sort-Object Name -Descending | Select-Object -First 1
    if ($ifw) { Write-Host "IFW: $($ifw.FullName)"; $ifwArg = @("-DCPACK_IFW_ROOT=$($ifw.FullName -replace '\\','/')") }

    $build = Join-Path $root 'build-gui'
    $dist = Join-Path $root 'dist'
    Run 'cmake' (@('-S', $root, '-B', $build, "-DCMAKE_PREFIX_PATH=$($qtDir.FullName -replace '\\','/')") + $ifwArg)
    Run 'cmake' @('--build', $build, '--config', 'Release', '--parallel')
    Run 'ctest' @('--test-dir', $build, '-C', 'Release', '--output-on-failure')

    $app = Join-Path $dist 'app'
    if (Test-Path $app) { Remove-Item -Recurse -Force $app }
    Run 'cmake' @('--install', $build, '--config', 'Release', '--prefix', $app, '--component', 'app')
    Write-Host "portable app: $app\bin\monopoly_gui.exe"

    if (-not $NoInstaller) {
        if (-not $ifw) { throw 'Qt Installer Framework not found under C:\Qt\Tools' }
        Push-Location $build
        try { Run 'cpack' @('-C', 'Release', '-B', $dist) } finally { Pop-Location }
        Get-ChildItem $dist -Filter *.exe | ForEach-Object { Write-Host "installer: $($_.FullName) ($([math]::Round($_.Length/1MB,1)) MB)" }
    }
    Write-Host 'BUILD_OK'
}
catch {
    Write-Host "BUILD_FAILED: $_"
}
finally {
    Stop-Transcript | Out-Null
}
