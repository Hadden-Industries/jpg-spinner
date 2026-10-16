<# Verify native owning-target selection without compiling or packaging. This
   covers every supported configuration, not a second XML property evaluator. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$toolchain = & (Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1') -RepositoryRoot $repositoryRoot
Import-Module (Join-Path $repositoryRoot 'scripts/JsonObjectMemberValidation.psm1')
Push-Location -LiteralPath $repositoryRoot
try {
    foreach ($configuration in @('Debug','Release')) {
        foreach ($platform in @('Win32','x64','ARM64')) {
            $evaluation = & $toolchain.msBuildExecutablePath 'src/JpgSpinner.App/JpgSpinner.App.vcxproj' `
                -nologo -noAutoResponse "-p:Configuration=$configuration" "-p:Platform=$platform" `
                -p:GenerateAppxPackageOnBuild=true '-getProperty:AppxBundleManifestVersion,MakeAppxExeFullPath'
            if ($LASTEXITCODE -ne 0) { throw "Native bundle evaluation failed for $configuration/$platform." }
            $properties = (ConvertFrom-JsonWithUniqueObjectMembers -JsonText ($evaluation -join "`n") `
                -SourceDescription "Native bundle selection $configuration/$platform" -RequiredRootValueKind Object).Properties
            # Independent expected value, not a value copied from the producer.
            if ($properties.AppxBundleManifestVersion -cne '2026.2.0.0' -or
                -not [IO.File]::Exists($properties.MakeAppxExeFullPath)) {
                throw "Owning targets did not retain the fixed bundle version/tool for $configuration/$platform."
            }
            Write-Output "PASS: owning targets select bundle 2026.2.0.0 for $configuration/$platform."
        }
    }
}
finally { Pop-Location }
