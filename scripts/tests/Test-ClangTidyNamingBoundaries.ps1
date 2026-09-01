[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$rootConfigurationPath = Join-Path $repositoryRoot '.clang-tidy'
$appConfigurationPath = Join-Path $repositoryRoot 'src/JpgSpinner.App/.clang-tidy'
$resolverPath = Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1'
$temporaryParent = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$temporaryFixtureRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $temporaryParent "jpg-spinner-clang-tidy-$([guid]::NewGuid().ToString('N'))")
)

if (-not $temporaryFixtureRoot.StartsWith($temporaryParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create clang-tidy fixtures outside the operating-system temporary directory: $temporaryFixtureRoot"
}

foreach ($requiredPath in @($rootConfigurationPath, $appConfigurationPath, $resolverPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required clang-tidy naming-boundary input is absent: $requiredPath"
    }
}

function Write-Utf8Fixture {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Content
    )

    [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Invoke-ClangTidyFixture {
    param(
        [Parameter(Mandatory)]
        [string]$ClangTidyExecutablePath,

        [Parameter(Mandatory)]
        [string]$SourcePath,

        [Parameter(Mandatory)]
        [string]$IncludeDirectoryPath,

        [Parameter(Mandatory)]
        [string]$FixtureDescription
    )

    $output = & $ClangTidyExecutablePath `
        $SourcePath `
        '--quiet' `
        '--' `
        '-std=c++20' `
        "-I$IncludeDirectoryPath" 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "$FixtureDescription violates its effective clang-tidy naming policy.`n$($output -join "`n")"
    }
}

try {
    $resolvedToolchain = & $resolverPath -RepositoryRoot $repositoryRoot
    $clangTidyExecutablePath = Join-Path `
        $resolvedToolchain.visualStudioInstallationPath `
        'VC/Tools/Llvm/x64/bin/clang-tidy.exe'
    if (-not (Test-Path -LiteralPath $clangTidyExecutablePath -PathType Leaf)) {
        throw "The selected Visual Studio instance lacks clang-tidy: $clangTidyExecutablePath"
    }

    # Reproduce the repository hierarchy so clang-tidy exercises nearest-file
    # discovery and InheritParentConfig exactly as production analysis will.
    $temporaryRootConfigurationPath = Join-Path $temporaryFixtureRoot '.clang-tidy'
    $temporaryAppConfigurationPath = Join-Path $temporaryFixtureRoot 'src/JpgSpinner.App/.clang-tidy'
    [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $temporaryAppConfigurationPath))
    Copy-Item -LiteralPath $rootConfigurationPath -Destination $temporaryRootConfigurationPath
    Copy-Item -LiteralPath $appConfigurationPath -Destination $temporaryAppConfigurationPath

    $nativeFixturePath = Join-Path $temporaryFixtureRoot 'src/JpgSpinner.Core/NativeNamingFixture.cpp'
    Write-Utf8Fixture -Path $nativeFixturePath -Content @'
namespace jpg_spinner::domain {
class RotationOperation final {
public:
    void rotateClockwise() {}

private:
    void updatePreview() {}
    int rotationDegrees_{0};
};
} // namespace jpg_spinner::domain

int main() { return 0; }
'@

    $generatedHeaderPath = Join-Path $temporaryFixtureRoot 'src/JpgSpinner.App/Generated Files/Projected.g.h'
    Write-Utf8Fixture -Path $generatedHeaderPath -Content @'
#pragma once

// Generated projections follow their generator's contract, not authored-code
// naming policy. These deliberately nonconforming names prove the exclusion.
namespace GENERATED_PROJECTION {
class generated_projection_type {
public:
    void generated_projection_method() {}
};
} // namespace GENERATED_PROJECTION
'@

    $appFixturePath = Join-Path $temporaryFixtureRoot 'src/JpgSpinner.App/AppNamingFixture.cpp'
    Write-Utf8Fixture -Path $appFixturePath -Content @'
#include "Generated Files/Projected.g.h"

namespace winrt::JpgSpinner::implementation {
class RotationViewModel final {
public:
    void RotateClockwise() {}

protected:
    void OnNavigation() {}

private:
    void updatePreview() {}
    int rotationDegrees_{0};
};
} // namespace winrt::JpgSpinner::implementation

int main() { return 0; }
'@

    Invoke-ClangTidyFixture `
        -ClangTidyExecutablePath $clangTidyExecutablePath `
        -SourcePath $nativeFixturePath `
        -IncludeDirectoryPath (Split-Path -Parent $nativeFixturePath) `
        -FixtureDescription 'Native jpg_spinner fixture'
    Invoke-ClangTidyFixture `
        -ClangTidyExecutablePath $clangTidyExecutablePath `
        -SourcePath $appFixturePath `
        -IncludeDirectoryPath (Split-Path -Parent $appFixturePath) `
        -FixtureDescription 'C++/WinRT app fixture'

    Write-Output 'PASS: clang-tidy accepts native and C++/WinRT naming boundaries while excluding generated projections.'
}
finally {
    if (Test-Path -LiteralPath $temporaryFixtureRoot) {
        Remove-Item -LiteralPath $temporaryFixtureRoot -Recurse -Force
    }
}
