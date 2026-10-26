Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Invoke-NativeDependencyProbe {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ProjectDirectory,
        [Parameter(Mandatory)][ValidatePattern('^[A-Za-z][A-Za-z0-9_-]*$')][string]$ExecutableName,
        [Parameter(Mandatory)][string]$InstalledPrefix,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [hashtable]$SourceDefinitions = @{}
    )
    $repositoryRoot = Split-Path -Parent $PSScriptRoot
    if (Test-Path -LiteralPath $OutputDirectory) { throw 'Native probes require fresh output.' }
    [void][IO.Directory]::CreateDirectory($OutputDirectory)
    $output = (Get-Item -LiteralPath $OutputDirectory).FullName
    $project = (Get-Item -LiteralPath $ProjectDirectory).FullName
    $prefix = (Get-Item -LiteralPath $InstalledPrefix).FullName
    $toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1')
    $vcpkgToolchain = Join-Path $toolchain.visualStudioInstallationPath 'VC/vcpkg/scripts/buildsystems/vcpkg.cmake'
    Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1')
    # One native-probe build contract keeps the SDK/toolset/architecture and
    # process handling identical across allocator, Unicode and codec controls.
    # ArtifactToolProcess owns the shared qualification time budgets.
    $configureArguments = @(
        '-S', $project, '-B', (Join-Path $output 'build'), '-G', 'Visual Studio 18 2026', '-A', 'x64',
        '-T', "$($toolchain.platformToolset),host=x64,version=$($toolchain.vcToolsVersion)",
        '-DCMAKE_SYSTEM_VERSION=10.0.28000.0', "-DCMAKE_PREFIX_PATH=$prefix",
        "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain", '-DVCPKG_TARGET_TRIPLET=x64-windows-static-md',
        "-DVCPKG_INSTALLED_DIR=$(Split-Path -Parent $prefix)", '-DVCPKG_MANIFEST_MODE=OFF',
        '-DVCPKG_MANIFEST_INSTALL=OFF', "-DVCPKG_OVERLAY_TRIPLETS=$(Join-Path $repositoryRoot 'vcpkg-triplets')"
    )
    # vcpkg owns package-name/library-name discovery, including its supplied
    # zlib CMake wrapper. Probe runs consume installed packages and never restore.
    foreach ($entry in $SourceDefinitions.GetEnumerator()) {
        $source = (Get-Item -LiteralPath $entry.Value).FullName
        $configureArguments += "-D$($entry.Key)=$source"
    }
    [void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $repositoryRoot `
            -LogDirectory $output -LogName configure -Arguments $configureArguments)
    [void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $repositoryRoot `
            -LogDirectory $output -LogName build -Arguments @('--build', (Join-Path $output 'build'), '--config', 'Release', '--parallel', '2'))
    [void](Invoke-ArtifactToolProcess -Executable (Join-Path $output "build/Release/$ExecutableName.exe") `
            -WorkingDirectory $output -LogDirectory $output -LogName $ExecutableName -Arguments @())
}

Export-ModuleMember -Function Invoke-NativeDependencyProbe
