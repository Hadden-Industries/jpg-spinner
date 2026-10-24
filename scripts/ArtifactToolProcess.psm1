<# Shared native process boundary for artifact generation and dependency
   preparation. Callers own explicit working/log directories and environment
   values. This module never acquires tools, retries, or changes the host. #>
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1')

function Invoke-ArtifactToolProcess {
    <# Bound one owned native child and preserve both diagnostic streams.
       A timeout, nonzero exit, unresolved termination or undrained stream
       throws; a successful call returns the complete standard output. #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$LogName,
        [Parameter(Mandatory)][string]$WorkingDirectory,
        [Parameter(Mandatory)][string]$LogDirectory,
        [hashtable]$EnvironmentValues = @{},
        [hashtable]$EnvironmentOverrides = @{},
        # Cold qualification tools may initialize MSBuild/Roslyn or verify a
        # large package graph. This is a process ceiling, not a product SLA.
        [ValidateRange(1, 3600)][int]$TimeoutSeconds = (Get-QualificationTimeout -Category nativeToolProcess)
    )
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.WorkingDirectory = $WorkingDirectory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    # Child-only values preserve absent versus empty host settings. Native
    # ArgumentList owns quoting; never reparse paths as shell source.
    foreach ($name in $EnvironmentValues.Keys) { $start.Environment[$name] = $EnvironmentValues[$name] }
    foreach ($name in $EnvironmentOverrides.Keys) { $start.Environment[$name] = $EnvironmentOverrides[$name] }
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw "Artifact tool did not start: $LogName" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) {
            $process.Kill($true)
            # A termination request is not proof of quiescence.
            if (-not $process.WaitForExit((Get-QualificationTimeout -Category nativeTeardown -Unit Milliseconds))) { throw "Artifact tool termination remains unresolved: $LogName" }
        }
        # Descendants may inherit redirected handles. Bound stream completion
        # independently, including after observing the direct child's exit.
        if (-not [Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout, $stderr), (Get-QualificationTimeout -Category nativeTeardown -Unit Milliseconds))) {
            throw "Artifact tool diagnostic stream completion remains unresolved: $LogName"
        }
        $standardOutput = $stdout.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $LogDirectory "$LogName.stdout.log"), $standardOutput)
        [IO.File]::WriteAllText((Join-Path $LogDirectory "$LogName.stderr.log"), $stderr.GetAwaiter().GetResult())
        # Keep completed diagnostics even when the operation was unsuccessful.
        if ($timedOut) { throw "Artifact tool exceeded its deadline: $LogName" }
        if ($process.ExitCode -ne 0) { throw "Native artifact tool failed: $LogName (exit $($process.ExitCode))" }
        return $standardOutput
    } finally { $process.Dispose() }
}

Export-ModuleMember -Function Invoke-ArtifactToolProcess
