<# Exercise the hosted entry point's refusal boundary. Every case is invalid
   before native GitHub selection or remote I/O; no valid hosted run is faked. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$entry = Join-Path (Split-Path -Parent $PSScriptRoot) Update-ReleaseCandidateArchive.ps1
$variables = @('GITHUB_ACTIONS', 'GITHUB_REPOSITORY', 'GITHUB_REF', 'GITHUB_SHA', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT')
$original = @{}
foreach ($name in $variables) { $original[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $baseline = @{
        GITHUB_ACTIONS = 'true'; GITHUB_REPOSITORY = 'Hadden-Industries/jpg-spinner'; GITHUB_REF = 'refs/heads/main'
        GITHUB_SHA = ('a' * 40); GITHUB_RUN_ID = '1234'; GITHUB_RUN_ATTEMPT = '1'
    }
    foreach ($case in @(
            @{Name = 'outside-actions'; Variable = 'GITHUB_ACTIONS'; Value = 'false'; Error = 'trusted-main Actions context' },
            @{Name = 'other-repository'; Variable = 'GITHUB_REPOSITORY'; Value = 'someone/else'; Error = 'trusted-main Actions context' },
            @{Name = 'other-ref'; Variable = 'GITHUB_REF'; Value = 'refs/heads/feature'; Error = 'trusted-main Actions context' },
            @{Name = 'invalid-source'; Variable = 'GITHUB_SHA'; Value = 'main'; Error = 'source commit identity' },
            @{Name = 'run-overflow'; Variable = 'GITHUB_RUN_ID'; Value = '9223372036854775808'; Error = 'producer run identity' },
            @{Name = 'run-zero'; Variable = 'GITHUB_RUN_ID'; Value = '0'; Error = 'producer run identity' },
            @{Name = 'attempt-text'; Variable = 'GITHUB_RUN_ATTEMPT'; Value = 'first'; Error = 'attempt identity' },
            @{Name = 'attempt-zero'; Variable = 'GITHUB_RUN_ATTEMPT'; Value = '0'; Error = 'attempt identity' }
        )) {
        foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $baseline[$name], 'Process') }
        [Environment]::SetEnvironmentVariable($case.Variable, $case.Value, 'Process')
        $destination = Join-Path $OwnedOutputRoot ('context-' + [Guid]::NewGuid().ToString('N'))
        $failure = $null
        try { & $entry -WorkingDirectory $destination | Out-Null } catch { $failure = $_.Exception.Message }
        if ($null -eq $failure -or -not $failure.Contains($case.Error, [StringComparison]::Ordinal)) {
            throw "Candidate context refusal failed at its intended boundary: $($case.Name): $failure"
        }
        if (Test-Path -LiteralPath $destination) { throw 'Refused hosted identity created archival working material.' }
        Write-Output "PASS: candidate context refusal $($case.Name)"
    }
}
finally {
    foreach ($name in $variables) { [Environment]::SetEnvironmentVariable($name, $original[$name], 'Process') }
}
