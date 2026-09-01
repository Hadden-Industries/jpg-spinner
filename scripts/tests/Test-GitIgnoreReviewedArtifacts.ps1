[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

function Invoke-GitIgnoreQuery {
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRelativePath
    )

    & git `
        -C $repositoryRoot `
        check-ignore `
        --no-index `
        --quiet `
        -- `
        $RepositoryRelativePath
    $exitCode = $LASTEXITCODE

    $explanation = ''
    if ($exitCode -eq 0) {
        $explanation = & git `
            -C $repositoryRoot `
            check-ignore `
            --no-index `
            --verbose `
            -- `
            $RepositoryRelativePath 2>&1
        $explanation = $explanation -join "`n"
    }

    return [pscustomobject]@{
        exitCode = $exitCode
        output = $explanation
    }
}

# These files are review evidence or human-curated inputs. They must remain
# visible to Git even before they exist, so test the planned repository-relative
# paths with --no-index rather than relying on today's working tree.
foreach ($reviewedArtifactPath in @(
    'src/Example/packages.lock.json',
    'tests/FuzzCorpus/reviewed-input.jpg',
    'artifacts/release/2.0.0.0/JpgSpinner.spdx.json',
    'artifacts/release/2.0.0.0/_manifest/spdx_3.0/manifest.spdx.json'
)) {
    $query = Invoke-GitIgnoreQuery -RepositoryRelativePath $reviewedArtifactPath
    if ($query.exitCode -eq 0) {
        throw "Reviewed artifact '$reviewedArtifactPath' is ignored: $($query.output)"
    }
    if ($query.exitCode -ne 1) {
        throw "git check-ignore failed for '$reviewedArtifactPath' with exit code $($query.exitCode): $($query.output)"
    }
}

# The release exception must not expose ordinary compiler outputs and logs.
$generatedArtifactPath = 'artifacts/build-policy-probe/debug-x64.log'
$generatedArtifactQuery = Invoke-GitIgnoreQuery -RepositoryRelativePath $generatedArtifactPath
if ($generatedArtifactQuery.exitCode -ne 0) {
    throw "Generated artifact '$generatedArtifactPath' is no longer ignored: $($generatedArtifactQuery.output)"
}

Write-Output 'PASS: reviewed locks, fuzz inputs, and release SBOMs are visible while generated artifacts remain ignored.'
