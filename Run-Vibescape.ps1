# Launch the packaged Vibescape build with its own application identity and preferences.
param(
    [Parameter(Position = 0)]
    [string]$File
)
$ErrorActionPreference = 'Stop'
$binaryDirectory = Join-Path $PSScriptRoot 'build/windows/install/bin'
$executable = Join-Path $binaryDirectory 'inkscape.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw 'The packaged build is missing. Run Build-Windows.ps1 first.'
}
$profileDirectory = Join-Path ([Environment]::GetFolderPath('ApplicationData')) 'Vibescape'
[IO.Directory]::CreateDirectory($profileDirectory) | Out-Null

$launchArguments = '--app-id-tag=Vibescape'
if ($File) {
    $document = (Resolve-Path -LiteralPath $File).ProviderPath
    $launchArguments += ' "' + $document + '"'
}

$start = New-Object Diagnostics.ProcessStartInfo
$start.FileName = $executable
$start.WorkingDirectory = $binaryDirectory
$start.Arguments = $launchArguments
$start.UseShellExecute = $false
$start.EnvironmentVariables['INKSCAPE_PROFILE_DIR'] = $profileDirectory
$start.EnvironmentVariables['PATH'] = $binaryDirectory + ';' + $env:PATH
$process = [Diagnostics.Process]::Start($start)
if ($process.WaitForExit(2500) -and $process.ExitCode -ne 0) {
    throw "Vibescape exited during startup with code $($process.ExitCode)."
}
Write-Host "Vibescape launched. Preferences: $profileDirectory"
