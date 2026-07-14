param(
    [switch]$Clean,
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Preset,
    [ValidateSet('Debug', 'Release')]
    [string[]]$Configuration
)

$ErrorActionPreference = 'Stop'

$depsDir = if ($env:DEPS_DIR) { $env:DEPS_DIR } else { 'Dependencies' }
$appRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

if ([System.IO.Path]::IsPathRooted($depsDir)) {
    $srcDir = $depsDir
} else {
    $srcDir = Join-Path $appRoot $depsDir
}

if (-not (Test-Path -LiteralPath $srcDir)) {
    throw "Dependencies directory not found: $srcDir"
}

$srcDir = (Resolve-Path -LiteralPath $srcDir).Path
$srcDirWithSeparator = $srcDir.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar

function Get-ConfigurePresets {
    $output = & cmake -S $srcDir --list-presets
    $inConfigureSection = $false

    foreach ($line in $output) {
        if ($line -match '^Available configure presets:') {
            $inConfigureSection = $true
            continue
        }

        if ($line -match '^Available ') {
            $inConfigureSection = $false
        }

        if ($inConfigureSection -and $line -match '"([^"]+)"') {
            $matches[1]
        }
    }
}

function Show-Usage {
    @"
Usage:
  ./scripts/build-dependencies.ps1                 # configure+build ALL configure presets
  ./scripts/build-dependencies.ps1 <preset> [...]  # configure+build only the given presets
  ./scripts/build-dependencies.ps1 -Clean <preset> # delete build/install for the preset, then configure+build
  ./scripts/build-dependencies.ps1 -Configuration Release <preset>
  ./scripts/build-dependencies.ps1 --list          # list available configure presets

Env:
  DEPS_DIR=Dependencies (default, relative to the application root)  # set if your folder name differs
"@
}

if ($Preset.Count -gt 0 -and ($Preset[0] -eq '--help' -or $Preset[0] -eq '-h')) {
    Show-Usage
    exit 0
}

if ($Preset.Count -gt 0 -and $Preset[0] -eq '--list') {
    Get-ConfigurePresets
    exit 0
}

$presetsToRun = if ($Preset.Count -gt 0) { $Preset } else { @(Get-ConfigurePresets) }

if ($presetsToRun.Count -eq 0) {
    throw "No configure presets found (looked in: $srcDir/CMakePresets.json and/or CMakeUserPresets.json)."
}

foreach ($presetName in $presetsToRun) {
    $presetConfiguration = if ($presetName -match '(?i)-debug$') {
        'Debug'
    }
    elseif ($presetName -match '(?i)-release$') {
        'Release'
    }

    if ($Configuration.Count -gt 0 -and $presetConfiguration -and
        ($Configuration.Count -ne 1 -or $Configuration[0] -ine $presetConfiguration)) {
        throw "Preset '$presetName' is a $presetConfiguration single-config preset; requested configuration(s): $($Configuration -join ', ')."
    }

    Write-Host "==> [$presetName] configure"

    $buildDir = Join-Path $srcDir "build/$presetName"
    $installDir = Join-Path $srcDir "install/$presetName"

    if ($Clean) {
        foreach ($dir in @($buildDir, $installDir)) {
            if (Test-Path $dir) {
                $resolved = (Resolve-Path $dir).Path
                if ($resolved -ne $srcDir -and -not $resolved.StartsWith($srcDirWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
                    throw "Refusing to remove path outside dependencies root: $resolved"
                }

                Write-Host "==> [$presetName] clean $resolved"
                Remove-Item -LiteralPath $resolved -Recurse -Force
            }
        }
    }

    New-Item -ItemType Directory -Path $installDir -Force | Out-Null

    & cmake --preset $presetName -S $srcDir
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $configurationsToBuild = if ($Configuration.Count -gt 0) {
        $Configuration
    }
    elseif ($presetConfiguration) {
        @($presetConfiguration)
    }
    else {
        # Multi-config presets must install both runtime variants because the
        # main project can be built in either configuration.
        @('Debug', 'Release')
    }

    foreach ($config in $configurationsToBuild) {
        Write-Host "==> [$presetName] build $config"
        & cmake --build $buildDir --config $config
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}
