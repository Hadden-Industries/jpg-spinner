[CmdletBinding()]
param(
    [ValidateRange(1, 86400)]
    [int]$DurationSeconds = 60,
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $repositoryRoot
$fuzzDirectory = Join-Path $repositoryRoot 'artifacts/fuzz/scanner'
$corpusDirectory = Join-Path $fuzzDirectory 'corpus'
[void][System.IO.Directory]::CreateDirectory($corpusDirectory)

# Authored hex seeds are reviewable source, never user photos. Materialize them
# as binary only in the generated corpus; libFuzzer can extend that corpus.
$seedDirectory = Join-Path $repositoryRoot 'fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/corpus'
foreach ($seed in Get-ChildItem -LiteralPath $seedDirectory -Filter '*.hex') {
    $bytes = [Convert]::FromHexString(([System.IO.File]::ReadAllText($seed.FullName)).Trim())
    [System.IO.File]::WriteAllBytes((Join-Path $corpusDirectory $seed.BaseName), $bytes)
}

foreach ($projectName in @('JpgSpinner.FuzzToolchain.Smoke', 'JpgSpinner.JpegSegmentScanner.Fuzz')) {
    $projectPath = Join-Path $repositoryRoot "fuzz/$projectName/$projectName.vcxproj"
    & $toolchain.msBuildExecutablePath $projectPath -nologo -noAutoResponse `
        -p:Configuration=Fuzz -p:Platform=x64 -verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "Fuzz build failed: $projectName ($LASTEXITCODE)" }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = Join-Path $repositoryRoot "artifacts/bin/x64/Fuzz/$projectName/$projectName.exe"
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    # Match the native ASan debugger setup: the selected toolset supplies its
    # own runtime DLL and symbolizer. This affects only this child process.
    $startInfo.Environment['PATH'] = (Split-Path -Parent $toolchain.targetTools.x64.compilerExecutablePath) +
        [System.IO.Path]::PathSeparator + $startInfo.Environment['PATH']
    $arguments = if ($projectName -eq 'JpgSpinner.FuzzToolchain.Smoke') {
        @('-runs=100')
    } else {
        @($corpusDirectory, "-max_total_time=$DurationSeconds", '-timeout=10', '-rss_limit_mb=2048',
          '-max_len=1048576', '-seed=20260930', "-artifact_prefix=$fuzzDirectory/")
    }
    foreach ($argument in $arguments) { [void]$startInfo.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::Start($startInfo)
    try {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "Fuzz execution failed: $projectName ($($process.ExitCode))" }
    } finally {
        $process.Dispose()
    }
}
