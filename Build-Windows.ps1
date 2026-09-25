# Build and stage Vibescape with MSYS2 UCRT64, reusing unchanged configuration and runtime files.
# See doc/building/vibescape-windows.md for dependency setup and available actions.
param(
    [ValidateSet('Build', 'Install', 'Configure')][string]$Action = 'Build',
    [ValidateRange(1, 64)][int]$Jobs = 12,
    [string]$MsysRoot = 'C:\msys64',
    [switch]$Fresh
)
$ErrorActionPreference = 'Stop'
$bash = Join-Path $MsysRoot 'usr\bin\bash.exe'
if (-not (Test-Path -LiteralPath $bash)) { throw "MSYS2 was not found at $MsysRoot." }
$names = @('MSYSTEM', 'CHERE_INVOKING', 'VIBESCAPE_JOBS', 'VIBESCAPE_FRESH', 'VIBESCAPE_ACTION')
$previous = @{}
foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$timer = [Diagnostics.Stopwatch]::StartNew()
Push-Location $PSScriptRoot
try {
    $env:MSYSTEM = 'UCRT64'
    $env:CHERE_INVOKING = '1'
    $env:VIBESCAPE_JOBS = [string]$Jobs
    $env:VIBESCAPE_FRESH = [string][int]$Fresh.IsPresent
    $env:VIBESCAPE_ACTION = $Action
    $commands = @'
set -euo pipefail
mkdir -p build/windows/logs
export PKG_CONFIG_PATH="$PWD/build/windows/deps/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export PATH="$PWD/build/windows/deps/bin:$PATH"
configure_args=(
    -S . -B build/windows -G Ninja
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=OFF
    -DWITH_INTERNAL_2GEOM=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    "-DCMAKE_PREFIX_PATH=$PWD/build/windows/deps"
    "-DCMAKE_INSTALL_PREFIX=$PWD/build/windows/install"
    "-DCMAKE_C_COMPILER=$(cygpath -m "$MINGW_PREFIX/bin/cc.exe")"
    "-DCMAKE_CXX_COMPILER=$(cygpath -m "$MINGW_PREFIX/bin/c++.exe")"
)
config_state=build/windows/vibescape-configure.args
expected_config=$(printf '%s\n' "${configure_args[@]}" "MSYS2=$(cygpath -am /)")
previous_config=$(cat "$config_state" 2>/dev/null || true)

if [[ "$VIBESCAPE_FRESH" == 1 || "$VIBESCAPE_ACTION" != Build ||
      ! -f build/windows/CMakeCache.txt || ! -f build/windows/build.ninja ||
      "$expected_config" != "$previous_config" ]]; then
    fresh=()
    if [[ "$VIBESCAPE_FRESH" == 1 ]]; then fresh=(--fresh); fi
    cmake "${fresh[@]}" "${configure_args[@]}" 2>&1 | tee build/windows/logs/configure.log
    printf '%s\n' "$expected_config" > "$config_state"
    pacman -Q > build/windows/logs/msys2-packages.txt
else
    echo "Reusing configuration; Ninja will reconfigure if CMake inputs changed."
fi

if [[ "$VIBESCAPE_ACTION" == Configure ]]; then exit 0; fi

{
    cmake --build build/windows --parallel "$VIBESCAPE_JOBS"
    if [[ "$VIBESCAPE_ACTION" == Install || ! -f build/windows/install/bin/inkscape.exe ]]; then
        # Keep Ninja's completion record in sync, including forced repairs.
        cmake -E rm -f build/windows/vibescape-installed.stamp
    fi
    cmake --build build/windows --target vibescape-stage --parallel "$VIBESCAPE_JOBS"
} 2>&1 | tee build/windows/logs/build.log
'@
    # A script file preserves Bash quoting in both Windows PowerShell 5.1 and PowerShell 7.
    $buildDirectory = Join-Path $PSScriptRoot 'build/windows'
    [IO.Directory]::CreateDirectory($buildDirectory) | Out-Null
    $driver = Join-Path $buildDirectory 'run-build.sh'
    [IO.File]::WriteAllText($driver, $commands.Replace("`r`n", "`n"), [Text.UTF8Encoding]::new($false))
    & $bash --login $driver.Replace('\', '/')
    if ($LASTEXITCODE -ne 0) {
        throw "Vibescape $Action failed with exit code $LASTEXITCODE. See build/windows/logs. If a running app locks a file, close it and retry."
    }
    $timer.Stop()
    Write-Host ("Vibescape {0} completed in {1:N2} seconds." -f $Action, $timer.Elapsed.TotalSeconds)
    if ($Action -ne 'Configure') {
        Write-Host "Run: $PSScriptRoot\Run-Vibescape.cmd"
    }
}
finally {
    Pop-Location
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
}
