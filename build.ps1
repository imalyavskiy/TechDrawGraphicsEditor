param([string]$QtRoot='F:\Qt\5.15.2\mingw81_64', [string]$CompilerRoot='F:\Qt\Tools\mingw810_64')
$ErrorActionPreference='Stop'
$previousPath=$env:PATH
try {
    $env:PATH="$QtRoot\bin;$CompilerRoot\bin;$previousPath"
    $buildPath=Join-Path $PSScriptRoot 'build'
    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
    Push-Location $buildPath
    try {
        & "$QtRoot\bin\qmake.exe" (Join-Path $PSScriptRoot 'Drawing.pro') 'CONFIG+=release'
        if ($LASTEXITCODE -ne 0) { throw 'qmake failed' }
        & "$CompilerRoot\bin\mingw32-make.exe" '-j4'
        if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    } finally { Pop-Location }
    $deployPath=Join-Path $PSScriptRoot 'dist\Drawing'
    New-Item -ItemType Directory -Force -Path $deployPath | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildPath 'release\Drawing.exe') -Destination $deployPath
    foreach ($library in @('Qt5Core.dll','Qt5Gui.dll','Qt5Widgets.dll')) {
        Copy-Item -LiteralPath (Join-Path "$QtRoot\bin" $library) -Destination $deployPath
    }
    foreach ($library in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path "$CompilerRoot\bin" $library) -Destination $deployPath
    }
    foreach ($plugin in @('platforms\qwindows.dll','platforms\qoffscreen.dll','styles\qwindowsvistastyle.dll')) {
        $destination=Join-Path $deployPath $plugin
        New-Item -ItemType Directory -Force -Path (Split-Path $destination) | Out-Null
        Copy-Item -LiteralPath (Join-Path "$QtRoot\plugins" $plugin) -Destination $destination
    }
    Set-Content -LiteralPath (Join-Path $deployPath 'qt.conf') -Value "[Paths]`nPrefix=.`nPlugins=." -Encoding ascii
    Write-Output "Ready: $deployPath\Drawing.exe"
} finally { $env:PATH=$previousPath }
