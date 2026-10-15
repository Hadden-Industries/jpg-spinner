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
    $makeAppx = & $toolchain.msBuildExecutablePath (Join-Path $repositoryRoot 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') `
        -nologo -noAutoResponse -p:Platform=x64 -p:GenerateAppxPackageOnBuild=true '-getProperty:MakeAppxExeFullPath'
    if ($LASTEXITCODE -ne 0 -or -not [IO.File]::Exists($makeAppx)) { throw 'The owning MSIX targets did not select MakeAppx.' }
    # Both tools ship in the same selected SDK architecture directory. Do not
    # use the unrelated legacy MSBuild SignToolPath or a globally newest SDK.
    # https://learn.microsoft.com/windows/msix/package/sign-app-package-using-signtool
    $signTool = Join-Path ([IO.Path]::GetDirectoryName($makeAppx)) 'signtool.exe'
    if (-not [IO.File]::Exists($signTool)) { throw 'The owning MSIX SDK lacks its companion SignTool.' }
    [pscustomobject]@{MakeAppxExecutablePath=[IO.Path]::GetFullPath($makeAppx); SignToolExecutablePath=$signTool}
}
finally { Pop-Location }
