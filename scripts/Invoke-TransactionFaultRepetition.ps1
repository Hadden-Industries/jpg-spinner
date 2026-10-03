#Requires -Version 7.4
[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [ValidateRange(1, 100)][int]$Repetitions = 100,
    [Parameter(Mandatory)][string]$EvidenceDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$platform = if ($Architecture -eq 'x86') { 'Win32' } else { 'x64' }
$executable = Join-Path $repositoryRoot "artifacts/bin/$platform/$Configuration/JpgSpinner.WindowsStorage.Tests/JpgSpinner.WindowsStorage.Tests.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Build the selected test executable first: $executable" }
$executableSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
$evidenceRoot = [IO.Path]::GetFullPath($EvidenceDirectory)
[void][IO.Directory]::CreateDirectory($evidenceRoot)
$runIdentifier = [guid]::NewGuid().ToString('D')
$observations = [Collections.Generic.List[object]]::new()
# These are the real copy/replacement effects, cancellation, containment and
# uncertain-completion controls. No provider override or hidden child helper is
# selected. Each process retains a fresh independently produced JUnit report.
$specification = '[copy],[replace],[journal-publication-matrix],[recovery],[journal][fault]'
try {
    for ($iteration = 1; $iteration -le $Repetitions; ++$iteration) {
        if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -ne $executableSha256) {
            throw 'The selected executable changed during qualification; do not combine candidate results.'
        }
        $report = Join-Path $evidenceRoot "$runIdentifier-$iteration.junit.xml"
        $start = [Diagnostics.ProcessStartInfo]::new()
        $start.FileName = $executable
        $start.WorkingDirectory = $repositoryRoot
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        $start.RedirectStandardOutput = $true
        $start.RedirectStandardError = $true
        foreach ($argument in @($specification, '--reporter', "JUnit::out=$report")) { [void]$start.ArgumentList.Add($argument) }
        $child = [Diagnostics.Process]::new()
        $child.StartInfo = $start
        try {
            [void]$child.Start()
            [void]$child.Handle
            $output = $child.StandardOutput.ReadToEndAsync()
            $errorOutput = $child.StandardError.ReadToEndAsync()
            if (-not $child.WaitForExit(60000)) { throw "Iteration $iteration exceeded its bounded deadline." }
            $observations.Add([ordered]@{ iteration = $iteration; exitCode = $child.ExitCode; report = $report })
            if ($child.ExitCode -ne 0) { throw "Iteration $iteration failed. Inspect $report. $($errorOutput.GetAwaiter().GetResult())" }
            Write-Output "Completed $iteration/$Repetitions"
        }
        finally {
            if (-not $child.HasExited) {
                $child.Kill()
                if (-not $child.WaitForExit(10000)) { throw 'Owned test process termination did not complete.' }
            }
            $child.Dispose()
        }
    }
}
finally {
    $receipt = [ordered]@{ schemaVersion = 1; runIdentifier = $runIdentifier; architecture = $Architecture;
        requestedRepetitions = $Repetitions; completedRepetitions = $observations.Count; specification = $specification;
        executableSha256 = $executableSha256;
        cases = $observations.ToArray() }
    $path = Join-Path $evidenceRoot "$runIdentifier-repetition.json"
    [IO.File]::WriteAllText($path, ($receipt | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    Write-Output $path
}
