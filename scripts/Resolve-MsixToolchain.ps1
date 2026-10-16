<# Resolve SDK packaging/verification tools from the locked owning app targets.
   This evaluates MSBuild only: no build, PATH fallback, install or retargeting. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $repositoryRoot
Push-Location -LiteralPath $repositoryRoot
try {
    # A multi-property query uses MSBuild's native JSON output. Reuse strict
    # platform JSON validation before PowerShell's lossy object conversion.
    $evaluation = & $toolchain.msBuildExecutablePath (Join-Path $repositoryRoot 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') `
        -nologo -noAutoResponse -p:Platform=x64 -p:GenerateAppxPackageOnBuild=true '-getProperty:MakeAppxExeFullPath,AppxBundleManifestVersion'
    if ($LASTEXITCODE -ne 0) { throw 'The owning MSIX targets could not be evaluated.' }
    Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1')
    $properties = (ConvertFrom-JsonWithUniqueObjectMembers -JsonText ($evaluation -join "`n") `
        -SourceDescription 'Native MSIX property evaluation' -RequiredRootValueKind Object).Properties
    $makeAppx = $properties.MakeAppxExeFullPath
    if (-not [IO.File]::Exists($makeAppx)) { throw 'The owning MSIX targets did not select MakeAppx.' }
    $bundleVersion = $properties.AppxBundleManifestVersion
    $parsedVersion = $null
    # Refuse missing/zero/incomplete versions before MakeAppx can substitute a
    # wall-clock default. The native SDK still owns the complete MSIX schema.
    if (-not [Version]::TryParse($bundleVersion, [ref]$parsedVersion) -or
        $parsedVersion.Revision -lt 0 -or $parsedVersion -eq [Version]'0.0.0.0') {
        throw 'The owning MSIX targets must select a fixed nonzero four-part bundle version.'
    }
    # Both tools ship in the same selected SDK architecture directory. Do not
    # use the unrelated legacy MSBuild SignToolPath or a globally newest SDK.
    # https://learn.microsoft.com/windows/msix/package/sign-app-package-using-signtool
    $signTool = Join-Path ([IO.Path]::GetDirectoryName($makeAppx)) 'signtool.exe'
    if (-not [IO.File]::Exists($signTool)) { throw 'The owning MSIX SDK lacks its companion SignTool.' }
    [pscustomobject]@{MakeAppxExecutablePath=[IO.Path]::GetFullPath($makeAppx); SignToolExecutablePath=$signTool; AppxBundleManifestVersion=$bundleVersion}
}
finally { Pop-Location }
