#Requires -Version 7.4
[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
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
$fixtureParent = Join-Path ([IO.Path]::GetTempPath()) 'jpg-spinner-termination-tests'
[void][IO.Directory]::CreateDirectory($fixtureParent)
$fixtureRoot = Join-Path $fixtureParent $runIdentifier
# New-Item without Force establishes exclusive ownership. Failed runs retain
# their exact fixture root; no age-based scan or broad temporary-root deletion.
[void](New-Item -ItemType Directory -Path $fixtureRoot)
$completed = $false

function New-QualificationProcess {
    param([string]$Specification, [string]$Fixture, [string]$Disposition,
          [int]$Generation, [string]$Boundary, [string]$EventName,
          [string]$Expectation, [string]$Report)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $executable
    $start.WorkingDirectory = $repositoryRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    # Delegate Windows escaping to the maintained .NET consumer. Never assemble
    # a shell command or implement another Microsoft argument parser here.
    foreach ($argument in @($Specification, '--reporter', "JUnit::out=$Report")) {
        [void]$start.ArgumentList.Add($argument)
    }
    $start.Environment['JPG_SPINNER_TERMINATION_FIXTURE'] = $Fixture
    $start.Environment['JPG_SPINNER_TERMINATION_DISPOSITION'] = $Disposition
    $start.Environment['JPG_SPINNER_TERMINATION_GENERATION'] = [string]$Generation
    $start.Environment['JPG_SPINNER_TERMINATION_BOUNDARY'] = $Boundary
    $start.Environment['JPG_SPINNER_TERMINATION_EVENT'] = $EventName
    $start.Environment['JPG_SPINNER_TERMINATION_EXPECTATION'] = $Expectation
    $child = [Diagnostics.Process]::new()
    $child.StartInfo = $start
    [void]$child.Start()
    # Retain the associated process handle before waiting. Termination never
    # targets a process name, a guessed PID, descendants, or unrelated apps.
    [void]$child.Handle
    return [pscustomobject]@{
        Process = $child
        Output = $child.StandardOutput.ReadToEndAsync()
        Error = $child.StandardError.ReadToEndAsync()
    }
}

function Stop-OwnedQualificationProcess {
    param($Child)
    if (-not $Child.Process.HasExited) { $Child.Process.Kill() }
    if (-not $Child.Process.WaitForExit(10000)) { throw 'Owned qualification process termination did not complete.' }
}

$observations = [Collections.Generic.List[object]]::new()
try {
    foreach ($disposition in @('copy', 'replace')) {
        $lastGeneration = if ($disposition -eq 'copy') { 5 } else { 6 }
        for ($generation = 1; $generation -le $lastGeneration; ++$generation) {
            foreach ($boundary in @('before-write', 'partial-write', 'after-write', 'after-flush',
                                    'after-close', 'before-publish', 'after-publish', 'before-next-effect')) {
                $caseName = "$disposition-$generation-$boundary"
                if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -ne $executableSha256) {
                    throw 'The selected executable changed during qualification; do not combine candidate results.'
                }
                $fixture = Join-Path $fixtureRoot $caseName
                [void](New-Item -ItemType Directory -Path $fixture)
                [void](New-Item -ItemType Directory -Path (Join-Path $fixture 'journal-store'))
                $eventName = "Local\JpgSpinnerTermination-$([guid]::NewGuid().ToString('D'))"
                $createdNew = $false
                $reached = [Threading.EventWaitHandle]::new($false, [Threading.EventResetMode]::ManualReset,
                                                          $eventName, [ref]$createdNew)
                if (-not $createdNew) { throw 'The qualification event was not exclusively created.' }
                $child = $null
                $recovery = $null
                try {
                    $child = New-QualificationProcess '[termination-child]' $fixture $disposition $generation $boundary $eventName '' (Join-Path $evidenceRoot "$runIdentifier-$caseName-child.junit.xml")
                    $deadline = [Diagnostics.Stopwatch]::StartNew()
                    $observedBoundary = $false
                    # The event supplies the ordering proof; this bounded poll
                    # only detects an early child failure without a 30-second wait.
                    while ($deadline.Elapsed.TotalSeconds -lt 30) {
                        if ($reached.WaitOne(100)) { $observedBoundary = $true; break }
                        if ($child.Process.HasExited) { break }
                    }
                    if (-not $observedBoundary) {
                        Stop-OwnedQualificationProcess $child
                        throw "Boundary not reached: $caseName. $($child.Output.GetAwaiter().GetResult()) $($child.Error.GetAwaiter().GetResult())"
                    }
                    Stop-OwnedQualificationProcess $child
                    $expectation = if ($generation -eq 1 -and $boundary -notin @('after-publish', 'before-next-effect')) {
                        'conflict'
                    } elseif (($disposition -eq 'copy' -and $generation -ge 4) -or
                              ($disposition -eq 'replace' -and $generation -ge 5)) {
                        'committed'
                    } else { 'preserved' }
                    $report = Join-Path $evidenceRoot "$runIdentifier-$caseName-recovery.junit.xml"
                    $recovery = New-QualificationProcess '[termination-recovery]' $fixture $disposition $generation $boundary '' $expectation $report
                    if (-not $recovery.Process.WaitForExit(30000)) { throw "Recovery timed out: $caseName" }
                    if ($recovery.Process.ExitCode -ne 0) {
                        throw "Recovery failed: $caseName. Inspect $report. $($recovery.Error.GetAwaiter().GetResult())"
                    }
                    $observations.Add([ordered]@{ case = $caseName; expectation = $expectation;
                        boundarySignaled = $observedBoundary; terminatedExitCode = $child.Process.ExitCode;
                        recoveryExitCode = $recovery.Process.ExitCode; recoveryReport = $report })
                }
                finally {
                    foreach ($ownedChild in @($child, $recovery)) {
                        if ($null -ne $ownedChild) {
                            try { Stop-OwnedQualificationProcess $ownedChild } finally { $ownedChild.Process.Dispose() }
                        }
                    }
                    $reached.Dispose()
                }
            }
        }
    }
    $completed = $true
}
finally {
    # This is producer-owned qualification evidence, not an assurance assertion.
    # Persist partial observations on failure and retain their generated fixtures.
    $receipt = [ordered]@{ schemaVersion = 1; runIdentifier = $runIdentifier; architecture = $Architecture;
        executableSha256 = $executableSha256; fixtureRoot = $fixtureRoot; completed = $completed; cases = $observations.ToArray() }
    $receiptPath = Join-Path $evidenceRoot "$runIdentifier-termination.json"
    [IO.File]::WriteAllText($receiptPath, ($receipt | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    Write-Output $receiptPath
    if ($completed) {
        # Remove only this producer's exclusive generated-data child, after all
        # child handles have exited and reports are retained elsewhere. Reject
        # link substitution and widened targets before recursive test-data cleanup.
        $resolved = [IO.Path]::GetFullPath($fixtureRoot)
        if ([IO.Path]::GetDirectoryName($resolved) -ne [IO.Path]::GetFullPath($fixtureParent) -or
            [IO.Path]::GetFileName($resolved) -ne $runIdentifier -or
            (Get-Item -LiteralPath $resolved).Attributes.HasFlag([IO.FileAttributes]::ReparsePoint) -or
            @(Get-ChildItem -LiteralPath $resolved -Recurse -Force | Where-Object {
                $_.Attributes.HasFlag([IO.FileAttributes]::ReparsePoint)
            }).Count -ne 0) { throw "Generated fixture cleanup ownership changed: $resolved" }
        Remove-Item -LiteralPath $resolved -Recurse
    }
}
