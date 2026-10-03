[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$repositoryPolicyPath = Join-Path $repositoryRoot 'scripts/Test-RepositoryPolicy.ps1'
$temporaryRoot = Join-Path (
    [System.IO.Path]::GetTempPath()
) "jpg-spinner-repository-policy-$([guid]::NewGuid().ToString('N'))"

function Invoke-IsolatedRepositoryPolicy {
    $powerShellExecutablePath = (Get-Process -Id $PID).Path
    $output = & $powerShellExecutablePath `
        -NoProfile `
        -File $repositoryPolicyPath `
        -RepositoryRoot $temporaryRoot 2>&1

    return [pscustomobject]@{
        exitCode = $LASTEXITCODE
        output = $output -join "`n"
    }
}

function Assert-RejectedMutation {
    param(
        [Parameter(Mandatory)]
        [string[]]$ExpectedDiagnostics
    )

    $result = Invoke-IsolatedRepositoryPolicy
    if ($result.exitCode -eq 0) {
        throw "Repository policy accepted a forbidden mutation requiring: $($ExpectedDiagnostics -join ', ')."
    }
    # PowerShell's native error renderer can insert ` | ` while wrapping a
    # child process's stderr. Remove that presentation delimiter before
    # matching semantic diagnostics, then collapse ordinary whitespace.
    $normalizedOutput =
        $result.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
    foreach ($expectedDiagnostic in $ExpectedDiagnostics) {
        if ($normalizedOutput.IndexOf($expectedDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "Repository policy rejected the mutation without the required '$expectedDiagnostic' diagnostic.`n$($result.output)"
        }
    }
}

try {
    [void][System.IO.Directory]::CreateDirectory($temporaryRoot)
    $gitInitializationOutput = & git -C $temporaryRoot init --quiet 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Could not initialize the isolated policy repository.`n$($gitInitializationOutput -join "`n")"
    }

    foreach ($relativeFilePath in @(
        '.vsconfig',
        '.editorconfig',
        '.clang-format',
        '.clang-tidy',
        '.gitignore',
        'Directory.Build.props',
        'Directory.Build.targets',
        'NuGet.config',
        'vcpkg.json',
        'JpgSpinner.sln',
        'JPG Spinner/JPG Spinner.vcxproj'
    )) {
        $destinationPath = Join-Path $temporaryRoot $relativeFilePath
        [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $destinationPath))
        Copy-Item -LiteralPath (Join-Path $repositoryRoot $relativeFilePath) -Destination $destinationPath
    }
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'eng') `
        -Destination (Join-Path $temporaryRoot 'eng') `
        -Recurse
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'vcpkg-triplets') `
        -Destination (Join-Path $temporaryRoot 'vcpkg-triplets') `
        -Recurse
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'src') `
        -Destination (Join-Path $temporaryRoot 'src') `
        -Recurse
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'tests') `
        -Destination (Join-Path $temporaryRoot 'tests') `
        -Recurse
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'fuzz') `
        -Destination (Join-Path $temporaryRoot 'fuzz') `
        -Recurse

    $baselineResult = Invoke-IsolatedRepositoryPolicy
    if ($baselineResult.exitCode -ne 0) {
        throw "The isolated valid configuration must pass before negative controls run.`n$($baselineResult.output)"
    }

    $directoryBuildPropertiesRelativePath = 'Directory.Build.props'
    # A separate sanitizer triplet must actually instrument its dependencies;
    # the name alone is not evidence of annotation-ABI consistency.
    $sanitizerTripletPath = Join-Path $temporaryRoot 'vcpkg-triplets/x64-windows-static-md-asan.cmake'
    $sanitizerTripletText = [System.IO.File]::ReadAllText($sanitizerTripletPath)
    [System.IO.File]::WriteAllText($sanitizerTripletPath, $sanitizerTripletText.Replace(' /fsanitize=address', ''))
    Assert-RejectedMutation -ExpectedDiagnostics 'VCPKG_CXX_FLAGS'
    [System.IO.File]::WriteAllText($sanitizerTripletPath, $sanitizerTripletText)
    $directoryBuildPropertiesPath =
        Join-Path $temporaryRoot $directoryBuildPropertiesRelativePath
    $directoryBuildPropertiesSourcePath =
        Join-Path $repositoryRoot $directoryBuildPropertiesRelativePath
    $directoryBuildPropertiesDocument = [System.Xml.XmlDocument]::new()
    $directoryBuildPropertiesDocument.PreserveWhitespace = $true

    # The repository-owned triplet directory is the sole approved extension to
    # vcpkg's builtin triplet search. A second path could replace the reviewed
    # CMake program while preserving the same triplet name.
    $directoryBuildPropertiesDocument.Load($directoryBuildPropertiesPath)
    $vcpkgInstallOptionsNode = $directoryBuildPropertiesDocument.SelectSingleNode(
        '/Project/PropertyGroup/VcpkgAdditionalInstallOptions'
    )
    if ($null -eq $vcpkgInstallOptionsNode) {
        throw 'The valid Directory.Build.props fixture lacks VcpkgAdditionalInstallOptions.'
    }
    $vcpkgInstallOptionsNode.InnerText =
        '--overlay-triplets="C:\unapproved-triplets"'
    $directoryBuildPropertiesDocument.Save($directoryBuildPropertiesPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        'VcpkgAdditionalInstallOptions must be'
    )
    Copy-Item `
        -LiteralPath $directoryBuildPropertiesSourcePath `
        -Destination $directoryBuildPropertiesPath `
        -Force

    # Platform selection is a closed mapping. Falling back to a dynamically
    # linked builtin triplet would change both linkage and dependency binaries.
    $directoryBuildPropertiesDocument.Load($directoryBuildPropertiesPath)
    $expectedX64TripletCondition = "'`$(Platform)' == 'x64' and '`$(EnableASAN)' != 'true'"
    $x64TripletNodes = @(
        $directoryBuildPropertiesDocument.SelectNodes('/Project/PropertyGroup/VcpkgTriplet') |
            Where-Object {
                $_.GetAttribute('Condition') -ceq $expectedX64TripletCondition
            }
    )
    if ($x64TripletNodes.Count -ne 1) {
        throw 'The valid Directory.Build.props fixture lacks its x64 VcpkgTriplet mapping.'
    }
    $x64TripletNode = $x64TripletNodes[0]
    $x64TripletNode.InnerText = 'x64-windows'
    $directoryBuildPropertiesDocument.Save($directoryBuildPropertiesPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "must map $expectedX64TripletCondition to 'x64-windows-static-md'"
    )
    Copy-Item `
        -LiteralPath $directoryBuildPropertiesSourcePath `
        -Destination $directoryBuildPropertiesPath `
        -Force

    # The effective-policy probe intentionally has no dependency graph. If it
    # inherits manifest installation, every policy check would perform a large
    # unrelated package build and stop being a focused compiler/linker proof.
    $buildPolicyProbeRelativePath = 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $buildPolicyProbePath = Join-Path $temporaryRoot $buildPolicyProbeRelativePath
    $buildPolicyProbeSourcePath = Join-Path $repositoryRoot $buildPolicyProbeRelativePath
    $buildPolicyProbeDocument = [System.Xml.XmlDocument]::new()
    $buildPolicyProbeDocument.PreserveWhitespace = $true
    $buildPolicyProbeDocument.Load($buildPolicyProbePath)
    $probeVcpkgEnabledNode = $buildPolicyProbeDocument.SelectSingleNode(
        '/*[local-name()="Project"]/*[local-name()="PropertyGroup" and @Label="Globals"]/*[local-name()="VcpkgEnabled"]'
    )
    if ($null -eq $probeVcpkgEnabledNode) {
        throw 'The valid build-policy probe fixture lacks VcpkgEnabled.'
    }
    [void]$probeVcpkgEnabledNode.ParentNode.RemoveChild($probeVcpkgEnabledNode)
    $buildPolicyProbeDocument.Save($buildPolicyProbePath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "$buildPolicyProbeRelativePath must declare VcpkgEnabled exactly once"
    )
    Copy-Item `
        -LiteralPath $buildPolicyProbeSourcePath `
        -Destination $buildPolicyProbePath `
        -Force

    $domainProjectRelativePath = 'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj'
    $domainProjectPath = Join-Path $temporaryRoot $domainProjectRelativePath
    $domainProjectSourcePath = Join-Path $repositoryRoot $domainProjectRelativePath
    $domainProjectDocument = [System.Xml.XmlDocument]::new()
    $domainProjectDocument.PreserveWhitespace = $true
    $domainProjectDocument.Load($domainProjectPath)
    $msBuildNamespace = $domainProjectDocument.DocumentElement.NamespaceURI

    # The domain project is the dependency graph's innermost layer. A
    # syntactically valid reference back to orchestration would create a cycle
    # in the architectural direction and must be rejected as an unapproved edge.
    $unapprovedReferenceItemGroup = $domainProjectDocument.CreateElement(
        'ItemGroup',
        $msBuildNamespace
    )
    $unapprovedReference = $domainProjectDocument.CreateElement(
        'ProjectReference',
        $msBuildNamespace
    )
    $unapprovedReference.SetAttribute(
        'Include',
        '..\JpgSpinner.BatchProcessing\JpgSpinner.BatchProcessing.vcxproj'
    )
    [void]$unapprovedReferenceItemGroup.AppendChild($unapprovedReference)
    [void]$domainProjectDocument.DocumentElement.AppendChild($unapprovedReferenceItemGroup)
    $domainProjectDocument.Save($domainProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "$domainProjectRelativePath contains unapproved project reference " +
        "'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj'"
    )
    Copy-Item -LiteralPath $domainProjectSourcePath -Destination $domainProjectPath -Force

    # MSBuild property and item identities are case-insensitive even though XML
    # element names are not. Vary both spellings so the policy must follow the
    # build engine's semantic identity instead of the source text's casing.
    $domainProjectDocument.Load($domainProjectPath)
    $caseVariedPropertyGroup = $domainProjectDocument.CreateElement(
        'PropertyGroup',
        $msBuildNamespace
    )
    $caseVariedUseEnvironmentProperty = $domainProjectDocument.CreateElement(
        'useenv',
        $msBuildNamespace
    )
    $caseVariedUseEnvironmentProperty.InnerText = 'true'
    [void]$caseVariedPropertyGroup.AppendChild($caseVariedUseEnvironmentProperty)
    [void]$domainProjectDocument.DocumentElement.AppendChild($caseVariedPropertyGroup)

    $caseVariedReferenceItemGroup = $domainProjectDocument.CreateElement(
        'ItemGroup',
        $msBuildNamespace
    )
    $caseVariedUnapprovedReference = $domainProjectDocument.CreateElement(
        'projectreference',
        $msBuildNamespace
    )
    $caseVariedUnapprovedReference.SetAttribute(
        'Include',
        '..\JpgSpinner.BatchProcessing\JpgSpinner.BatchProcessing.vcxproj'
    )
    [void]$caseVariedReferenceItemGroup.AppendChild($caseVariedUnapprovedReference)
    [void]$domainProjectDocument.DocumentElement.AppendChild($caseVariedReferenceItemGroup)
    $domainProjectDocument.Save($domainProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "$domainProjectRelativePath must not redeclare root-owned property 'UseEnv'"
        "$domainProjectRelativePath contains unapproved project reference " +
            "'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj'"
    )
    Copy-Item -LiteralPath $domainProjectSourcePath -Destination $domainProjectPath -Force

    # Win32 is MSBuild's canonical x86 platform name for these native projects.
    # Adding the distinct legacy ARM platform must not be mistaken for ARM64.
    $domainProjectDocument.Load($domainProjectPath)
    $projectConfigurationItemGroup = $domainProjectDocument.SelectSingleNode(
        '/*[local-name()="Project"]/*[local-name()="ItemGroup" and @Label="ProjectConfigurations"]'
    )
    if ($null -eq $projectConfigurationItemGroup) {
        throw 'The valid domain project fixture lacks its ProjectConfigurations item group.'
    }
    $arm32ProjectConfiguration = $domainProjectDocument.CreateElement(
        'ProjectConfiguration',
        $msBuildNamespace
    )
    $arm32ProjectConfiguration.SetAttribute('Include', 'Debug|ARM')
    $arm32Configuration = $domainProjectDocument.CreateElement('Configuration', $msBuildNamespace)
    $arm32Configuration.InnerText = 'Debug'
    $arm32Platform = $domainProjectDocument.CreateElement('Platform', $msBuildNamespace)
    $arm32Platform.InnerText = 'ARM'
    [void]$arm32ProjectConfiguration.AppendChild($arm32Configuration)
    [void]$arm32ProjectConfiguration.AppendChild($arm32Platform)
    [void]$projectConfigurationItemGroup.AppendChild($arm32ProjectConfiguration)
    $domainProjectDocument.Save($domainProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "$domainProjectRelativePath contains unsupported project configuration 'Debug|ARM'"
    )
    Copy-Item -LiteralPath $domainProjectSourcePath -Destination $domainProjectPath -Force

    # Solution-level configuration names do not prove the per-project mapping.
    # Redirect one ActiveCfg and remove its Build.0 entry to require both exact
    # platform selection and participation for every project/configuration pair.
    $solutionRelativePath = 'JpgSpinner.sln'
    $solutionPath = Join-Path $temporaryRoot $solutionRelativePath
    $solutionSourcePath = Join-Path $repositoryRoot $solutionRelativePath
    $solutionText = [System.IO.File]::ReadAllText($solutionPath)
    $domainProjectGuid = '{004936AC-71F4-4208-A8A2-B0AE7B18291E}'
    $expectedActiveConfigurationLine =
        "`t$domainProjectGuid.Release|ARM64.ActiveCfg = Release|ARM64"
    $expectedBuildParticipationLine =
        "`t$domainProjectGuid.Release|ARM64.Build.0 = Release|ARM64"
    $solutionLineEnding = if ($solutionText.Contains("`r`n")) { "`r`n" } else { "`n" }
    $mutatedSolutionText = $solutionText.Replace(
        $expectedActiveConfigurationLine,
        "`t$domainProjectGuid.Release|ARM64.ActiveCfg = Release|x64"
    ).Replace(
        $expectedBuildParticipationLine + $solutionLineEnding,
        ''
    )
    if (
        $mutatedSolutionText -ceq $solutionText -or
        $mutatedSolutionText.Contains($expectedActiveConfigurationLine) -or
        $mutatedSolutionText.Contains($expectedBuildParticipationLine)
    ) {
        throw 'The valid solution fixture lacks the expected Domain Release|ARM64 mappings.'
    }
    [System.IO.File]::WriteAllText(
        $solutionPath,
        $mutatedSolutionText,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "$domainProjectRelativePath maps solution configuration 'Release|ARM64' to 'Release|x64'"
        "$domainProjectRelativePath is excluded from solution configuration 'Release|ARM64'"
    )
    Copy-Item -LiteralPath $solutionSourcePath -Destination $solutionPath -Force

    # Toolchain selection is repository-owned and imported before every modern
    # project. A project-local value after that import can mask the lock even if
    # its current text happens to match, so the declaration itself is forbidden.
    $domainProjectDocument.Load($domainProjectPath)
    $maskedToolchainPropertyGroup = $domainProjectDocument.CreateElement(
        'PropertyGroup',
        $msBuildNamespace
    )
    $maskedToolchainProperty = $domainProjectDocument.CreateElement(
        'VCToolsVersion',
        $msBuildNamespace
    )
    $maskedToolchainProperty.InnerText = '14.51.36231'
    [void]$maskedToolchainPropertyGroup.AppendChild($maskedToolchainProperty)
    [void]$domainProjectDocument.DocumentElement.AppendChild($maskedToolchainPropertyGroup)
    $domainProjectDocument.Save($domainProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "$domainProjectRelativePath must not redeclare root-owned property 'VCToolsVersion'"
    )
    Copy-Item -LiteralPath $domainProjectSourcePath -Destination $domainProjectPath -Force

    # Only the presentation shell owns package generation. A second AppxPackage
    # flag would create an independently distributable artifact and violate the
    # single-project MSIX boundary.
    $domainProjectDocument.Load($domainProjectPath)
    $secondMsixPropertyGroup = $domainProjectDocument.CreateElement(
        'PropertyGroup',
        $msBuildNamespace
    )
    $secondMsixProperty = $domainProjectDocument.CreateElement(
        'AppxPackage',
        $msBuildNamespace
    )
    $secondMsixProperty.InnerText = 'true'
    [void]$secondMsixPropertyGroup.AppendChild($secondMsixProperty)
    [void]$domainProjectDocument.DocumentElement.AppendChild($secondMsixPropertyGroup)
    $domainProjectDocument.Save($domainProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        'JpgSpinner.sln must contain exactly one single-project MSIX packaging project; found 2'
    )
    Copy-Item -LiteralPath $domainProjectSourcePath -Destination $domainProjectPath -Force

    $appProjectRelativePath = 'src/JpgSpinner.App/JpgSpinner.App.vcxproj'
    $appProjectPath = Join-Path $temporaryRoot $appProjectRelativePath
    $appProjectDocument = [System.Xml.XmlDocument]::new()
    $appProjectDocument.PreserveWhitespace = $true
    $appProjectDocument.Load($appProjectPath)
    $appMsBuildNamespace = $appProjectDocument.DocumentElement.NamespaceURI

    # WindowsPackageType=None activates the Windows App SDK's official
    # unpackaged bootstrapper auto-initializer. Production is packaged and must
    # obtain its runtime through the package graph instead.
    $unpackagedProductionPropertyGroup = $appProjectDocument.CreateElement(
        'PropertyGroup',
        $appMsBuildNamespace
    )
    $unpackagedProductionProperty = $appProjectDocument.CreateElement(
        'WindowsPackageType',
        $appMsBuildNamespace
    )
    $unpackagedProductionProperty.InnerText = 'None'
    [void]$unpackagedProductionPropertyGroup.AppendChild($unpackagedProductionProperty)
    [void]$appProjectDocument.DocumentElement.AppendChild($unpackagedProductionPropertyGroup)
    $appProjectDocument.Save($appProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "$appProjectRelativePath is production code and must not enable the Windows App SDK unpackaged bootstrapper"
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot $appProjectRelativePath) `
        -Destination $appProjectPath `
        -Force

    $bootstrapperSourcePath = Join-Path $temporaryRoot 'src/JpgSpinner.App/BootstrapperNegativeControl.cpp'
    [System.IO.File]::WriteAllText(
        $bootstrapperSourcePath,
        "void InvokeForbiddenBootstrapper() { MddBootstrapInitialize(); }`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics (
        'src/JpgSpinner.App/BootstrapperNegativeControl.cpp must not call the Windows App SDK bootstrapper'
    )
    [System.IO.File]::Delete($bootstrapperSourcePath)

    # The current Windows App SDK exposes the options-bearing Initialize2 API
    # alongside the original initializer. Its numeric suffix must remain within
    # the packaged-production bootstrap prohibition.
    [System.IO.File]::WriteAllText(
        $bootstrapperSourcePath,
        "void InvokeForbiddenBootstrapper() { MddBootstrapInitialize2(); }`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics (
        'src/JpgSpinner.App/BootstrapperNegativeControl.cpp must not call the Windows App SDK bootstrapper'
    )
    [System.IO.File]::Delete($bootstrapperSourcePath)

    # The legacy project must establish its exception before the root policy is
    # imported. Flipping the project-owned switch recreates the reviewed
    # /ZW-plus-/std:c++20 conflict even though the shared files remain intact.
    $legacyCppCxProjectPath =
        Join-Path $temporaryRoot 'JPG Spinner/JPG Spinner.vcxproj'
    $legacyCppCxProjectDocument = [System.Xml.XmlDocument]::new()
    $legacyCppCxProjectDocument.PreserveWhitespace = $true
    $legacyCppCxProjectDocument.Load($legacyCppCxProjectPath)
    $legacyPolicyParticipationNode = $legacyCppCxProjectDocument.SelectSingleNode(
        '/*[local-name()="Project"]/*[local-name()="PropertyGroup"]/' +
        '*[local-name()="JpgSpinnerModernCppBuildPolicyEnabled"]'
    )
    if ($null -eq $legacyPolicyParticipationNode) {
        throw 'The valid legacy project fixture lacks its modern C++ policy participation property.'
    }
    $legacyPolicyParticipationNode.InnerText = 'true'
    $legacyCppCxProjectDocument.Save($legacyCppCxProjectPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        "legacy C++/CX modern build-policy participation must be 'false'"
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'JPG Spinner/JPG Spinner.vcxproj') `
        -Destination $legacyCppCxProjectPath `
        -Force

    # The default-on switch and the guarded selection group form one scope
    # contract. Removing either condition would make the legacy opt-out inert
    # for some or all of Directory.Build.props.
    $buildPropertiesPath = Join-Path $temporaryRoot 'Directory.Build.props'
    $buildPropertiesDocument = [System.Xml.XmlDocument]::new()
    $buildPropertiesDocument.PreserveWhitespace = $true
    $buildPropertiesDocument.Load($buildPropertiesPath)
    $modernCppBuildPolicyDefaultNode = $buildPropertiesDocument.SelectSingleNode(
        '/Project/PropertyGroup/JpgSpinnerModernCppBuildPolicyEnabled'
    )
    if ($null -eq $modernCppBuildPolicyDefaultNode) {
        throw 'The valid build-properties fixture lacks its modern C++ policy default.'
    }
    $modernCppBuildPolicyDefaultNode.RemoveAttribute('Condition')
    $buildPropertiesDocument.Save($buildPropertiesPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        'JpgSpinnerModernCppBuildPolicyEnabled must use property condition'
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') `
        -Destination $buildPropertiesPath `
        -Force

    $buildPropertiesDocument.Load($buildPropertiesPath)
    $modernSelectionPropertyGroup = $buildPropertiesDocument.SelectSingleNode(
        '/Project/PropertyGroup[PlatformToolset]'
    )
    if ($null -eq $modernSelectionPropertyGroup) {
        throw 'The valid build-properties fixture lacks its modern selection-property group.'
    }
    $modernSelectionPropertyGroup.RemoveAttribute('Condition')
    $buildPropertiesDocument.Save($buildPropertiesPath)
    Assert-RejectedMutation -ExpectedDiagnostics (
        'PlatformToolset must use PropertyGroup condition'
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') `
        -Destination $buildPropertiesPath `
        -Force

    # UseEnv=true delegates VC directory selection to INCLUDE, LIB, LIBPATH,
    # and PATH from the calling shell. The modern policy must instead retain
    # MSBuild's project/toolset-selected directories as a unique, guarded
    # repository contract.
    $buildPropertiesDocument.Load($buildPropertiesPath)
    $useEnvironmentNode = $buildPropertiesDocument.SelectSingleNode(
        '/Project/PropertyGroup/UseEnv'
    )
    if ($null -eq $useEnvironmentNode) {
        throw 'The valid build-properties fixture lacks its UseEnv policy declaration.'
    }
    $useEnvironmentNode.InnerText = 'true'
    $buildPropertiesDocument.Save($buildPropertiesPath)
    Assert-RejectedMutation -ExpectedDiagnostics "UseEnv must be 'false'; found 'true'."
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') `
        -Destination $buildPropertiesPath `
        -Force

    # Compiler/linker policy is late item metadata. Exercise every shared group
    # independently so a configuration- or architecture-specific group cannot
    # leak into the legacy project behind an otherwise correct base guard.
    $targetsPath = Join-Path $temporaryRoot 'Directory.Build.targets'
    $targetsDocument = [System.Xml.XmlDocument]::new()
    $targetsDocument.PreserveWhitespace = $true
    $modernCppPolicyGroupControls = @(
        [pscustomobject]@{
            policyDescription = 'base'
            groupXPath = '/Project/ItemDefinitionGroup[ClCompile/LanguageStandard]'
        },
        [pscustomobject]@{
            policyDescription = 'Debug'
            groupXPath = '/Project/ItemDefinitionGroup[ClCompile/DebugInformationFormat]'
        },
        [pscustomobject]@{
            policyDescription = 'Release'
            groupXPath = '/Project/ItemDefinitionGroup[ClCompile/Optimization]'
        },
        [pscustomobject]@{
            policyDescription = 'x64'
            groupXPath = '/Project/ItemDefinitionGroup[Link/AdditionalOptions[contains(translate(., "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "/CETCOMPAT")]]'
        }
    )
    foreach ($modernCppPolicyGroupControl in $modernCppPolicyGroupControls) {
        Copy-Item `
            -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.targets') `
            -Destination $targetsPath `
            -Force
        $targetsDocument.Load($targetsPath)
        $modernCppPolicyGroup = $targetsDocument.SelectSingleNode(
            $modernCppPolicyGroupControl.groupXPath
        )
        if ($null -eq $modernCppPolicyGroup) {
            throw (
                'The valid build-targets fixture lacks its modern C++ ' +
                "$($modernCppPolicyGroupControl.policyDescription) policy group."
            )
        }
        $modernCppPolicyGroup.RemoveAttribute('Condition')
        $targetsDocument.Save($targetsPath)
        Assert-RejectedMutation -ExpectedDiagnostics (
            'modern C++ ' +
            $modernCppPolicyGroupControl.policyDescription +
            ' policy ItemDefinitionGroup'
        )
    }
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.targets') `
        -Destination $targetsPath `
        -Force

    # The structured SpectreMitigation property selects both compiler
    # instrumentation and the architecture-specific mitigated runtime library
    # path. A raw /Qspectre token alone must not satisfy repository policy.
    $buildPropertiesContent = [System.IO.File]::ReadAllText($buildPropertiesPath)
    $mutatedBuildPropertiesContent = $buildPropertiesContent.Replace(
        '    <SpectreMitigation>Spectre</SpectreMitigation>' + "`n",
        ''
    )
    [System.IO.File]::WriteAllText(
        $buildPropertiesPath,
        $mutatedBuildPropertiesContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics 'SpectreMitigation'
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') -Destination $buildPropertiesPath -Force

    # Visual Studio 2026 keeps native vcxproj PackageReference support behind
    # an explicit project-system opt-in. Removing it makes restore and build
    # consume different dependency graphs, so repository policy must treat it
    # as a unique toolchain-selection property rather than a template detail.
    $buildPropertiesContent = [System.IO.File]::ReadAllText($buildPropertiesPath)
    $mutatedBuildPropertiesContent = $buildPropertiesContent.Replace(
        '    <EnableNativePackageReferenceSupport>true</EnableNativePackageReferenceSupport>' + "`n",
        ''
    )
    if ($mutatedBuildPropertiesContent -ceq $buildPropertiesContent) {
        throw 'The valid Directory.Build.props fixture lacks EnableNativePackageReferenceSupport.'
    }
    [System.IO.File]::WriteAllText(
        $buildPropertiesPath,
        $mutatedBuildPropertiesContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics 'EnableNativePackageReferenceSupport'
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') -Destination $buildPropertiesPath -Force

    # Early toolchain-selection properties are single-valued repository
    # contracts. Selecting only the first XML node would let a later declaration
    # change normal MSBuild evaluation while the structural gate reports the
    # reviewed-looking first value.
    $buildPropertiesDocument.PreserveWhitespace = $true
    $buildPropertiesDocument.Load($buildPropertiesPath)
    $duplicateSelectionPropertyGroup = $buildPropertiesDocument.CreateElement('PropertyGroup')
    foreach ($duplicateSelectionProperty in @(
        [pscustomobject]@{ name = 'PlatformToolset'; value = 'JpgSpinnerInvalidToolset' },
        [pscustomobject]@{ name = 'VCToolsVersion'; value = '0.0.0' },
        [pscustomobject]@{ name = 'WindowsTargetPlatformVersion'; value = '0.0.0.0' },
        [pscustomobject]@{ name = 'UseEnv'; value = 'true' },
        [pscustomobject]@{ name = 'EnableNativePackageReferenceSupport'; value = 'false' }
    )) {
        $duplicateSelectionPropertyNode =
            $buildPropertiesDocument.CreateElement($duplicateSelectionProperty.name)
        $duplicateSelectionPropertyNode.InnerText = $duplicateSelectionProperty.value
        [void]$duplicateSelectionPropertyGroup.AppendChild($duplicateSelectionPropertyNode)
    }
    [void]$buildPropertiesDocument.DocumentElement.AppendChild($duplicateSelectionPropertyGroup)
    $buildPropertiesDocument.Save($buildPropertiesPath)

    Assert-RejectedMutation -ExpectedDiagnostics @(
        'Directory.Build.props must declare PlatformToolset exactly once'
        'Directory.Build.props must declare VCToolsVersion exactly once'
        'Directory.Build.props must declare WindowsTargetPlatformVersion exactly once'
        'Directory.Build.props must declare UseEnv exactly once'
        'Directory.Build.props must declare EnableNativePackageReferenceSupport exactly once'
    )
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') -Destination $buildPropertiesPath -Force

    # MSBuild properties can also be declared below Choose/When/Otherwise. A
    # root-only XPath overlooks those declarations even though MSBuild evaluates
    # them, allowing a conditional selection value to mask the reviewed root
    # value for projects that satisfy the branch condition.
    $buildPropertiesDocument = [System.Xml.XmlDocument]::new()
    $buildPropertiesDocument.PreserveWhitespace = $true
    $buildPropertiesDocument.Load($buildPropertiesPath)
    $conditionalPropertySelection = $buildPropertiesDocument.CreateElement('Choose')
    $conditionalPropertyBranch = $buildPropertiesDocument.CreateElement('When')
    $conditionalPropertyBranch.SetAttribute(
        'Condition',
        "'`$(MSBuildProjectName)' != 'BuildPolicyProbe'"
    )
    $conditionalPropertyGroup = $buildPropertiesDocument.CreateElement('PropertyGroup')
    foreach ($conditionalSelectionProperty in @(
        [pscustomobject]@{ name = 'PlatformToolset'; value = 'JpgSpinnerConditionallyMaskedToolset' },
        # MSBuild property names are case-insensitive. Vary the spelling here so
        # the control also proves that policy follows MSBuild semantics rather
        # than XML element-name casing.
        [pscustomobject]@{ name = 'vctoolsversion'; value = '0.0.0' },
        [pscustomobject]@{ name = 'WindowsTargetPlatformVersion'; value = '0.0.0.0' },
        [pscustomobject]@{ name = 'useenv'; value = 'true' },
        [pscustomobject]@{ name = 'enablenativepackagereferencesupport'; value = 'false' }
    )) {
        $conditionalSelectionPropertyNode =
            $buildPropertiesDocument.CreateElement($conditionalSelectionProperty.name)
        $conditionalSelectionPropertyNode.InnerText = $conditionalSelectionProperty.value
        [void]$conditionalPropertyGroup.AppendChild($conditionalSelectionPropertyNode)
    }
    [void]$conditionalPropertyBranch.AppendChild($conditionalPropertyGroup)
    [void]$conditionalPropertySelection.AppendChild($conditionalPropertyBranch)
    [void]$buildPropertiesDocument.DocumentElement.AppendChild($conditionalPropertySelection)
    $buildPropertiesDocument.Save($buildPropertiesPath)

    Assert-RejectedMutation -ExpectedDiagnostics @(
        'Directory.Build.props must declare PlatformToolset exactly once'
        'Directory.Build.props must declare VCToolsVersion exactly once'
        'Directory.Build.props must declare WindowsTargetPlatformVersion exactly once'
        'Directory.Build.props must declare UseEnv exactly once'
        'Directory.Build.props must declare EnableNativePackageReferenceSupport exactly once'
    )
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.props') -Destination $buildPropertiesPath -Force

    $targetsContent = [System.IO.File]::ReadAllText($targetsPath)
    $mutatedTargetsContent = $targetsContent.Replace(
        '/utf-8 /Zc:__cplusplus',
        '/std:c++latest /utf-8 /Zc:__cplusplus'
    )
    [System.IO.File]::WriteAllText($targetsPath, $mutatedTargetsContent, [System.Text.UTF8Encoding]::new($false))
    Assert-RejectedMutation -ExpectedDiagnostics '/std:c++latest'
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.targets') -Destination $targetsPath -Force

    # Release optimization is a repository-owned build contract. It must remain
    # explicit rather than depending on a project-template or toolset default.
    $targetsDocument.PreserveWhitespace = $true
    $targetsDocument.Load($targetsPath)
    $releaseCompilerDefinition = $targetsDocument.SelectSingleNode(
        '/Project/ItemDefinitionGroup[contains(@Condition, "$(Configuration)") and contains(@Condition, "Release")]/ClCompile'
    )
    if ($null -eq $releaseCompilerDefinition) {
        throw 'The valid fixture lacks its expected Release ClCompile definition.'
    }
    $releaseOptimization = $releaseCompilerDefinition.SelectSingleNode('Optimization')
    if ($null -eq $releaseOptimization) {
        $releaseOptimization = $targetsDocument.CreateElement('Optimization')
        [void]$releaseCompilerDefinition.PrependChild($releaseOptimization)
    }
    $releaseOptimization.InnerText = 'Disabled'
    $targetsDocument.Save($targetsPath)
    Assert-RejectedMutation -ExpectedDiagnostics 'Release ClCompile Optimization'
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'Directory.Build.targets') -Destination $targetsPath -Force

    $visualStudioConfigurationPath = Join-Path $temporaryRoot '.vsconfig'
    $visualStudioConfiguration =
        Get-Content -LiteralPath $visualStudioConfigurationPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $visualStudioConfiguration.components += 'Microsoft.VisualStudio.Component.VC.Tools.ARM'
    $mutatedVisualStudioConfiguration = $visualStudioConfiguration | ConvertTo-Json -Depth 20
    [System.IO.File]::WriteAllText(
        $visualStudioConfigurationPath,
        $mutatedVisualStudioConfiguration + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics 'ARM32'
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot '.vsconfig') `
        -Destination $visualStudioConfigurationPath `
        -Force

    # A valid JSON literal can still have the wrong document-root type. In
    # particular, ConvertFrom-Json represents `null` as PowerShell $null, which
    # must not be confused with the separate "optional file is absent" state.
    # Exercise every repository JSON policy surface that expects an object.
    $jsonObjectRootControls = @(
        [pscustomobject]@{
            relativePath = '.vsconfig'
            restoreFromRepository = $true
        },
        [pscustomobject]@{
            relativePath = 'eng/toolchain-lock.json'
            restoreFromRepository = $true
        },
        [pscustomobject]@{
            relativePath = 'vcpkg.json'
            restoreFromRepository = $true
        },
        [pscustomobject]@{
            relativePath = 'vcpkg-configuration.json'
            restoreFromRepository = $false
        }
    )
    foreach ($jsonObjectRootControl in $jsonObjectRootControls) {
        $jsonObjectPath = Join-Path $temporaryRoot $jsonObjectRootControl.relativePath
        [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $jsonObjectPath))
        [System.IO.File]::WriteAllText(
            $jsonObjectPath,
            "null`n",
            [System.Text.UTF8Encoding]::new($false)
        )

        Assert-RejectedMutation -ExpectedDiagnostics (
            "$($jsonObjectRootControl.relativePath) root must be a JSON object"
        )

        if ($jsonObjectRootControl.restoreFromRepository) {
            Copy-Item `
                -LiteralPath (Join-Path $repositoryRoot $jsonObjectRootControl.relativePath) `
                -Destination $jsonObjectPath `
                -Force
        }
        else {
            [System.IO.File]::Delete($jsonObjectPath)
        }
    }

    # A triplet is executable CMake, not passive data. Exercise both ways a
    # later option can defeat an approved token: directly in a set() value and
    # through an otherwise ignored command that mutates a previously set value.
    $tripletRelativePath = 'vcpkg-triplets/x64-windows-static-md.cmake'
    $tripletPath = Join-Path $temporaryRoot $tripletRelativePath
    $tripletContent = [System.IO.File]::ReadAllText($tripletPath)
    $mutatedTripletContent = $tripletContent.Replace(
        'set(VCPKG_C_FLAGS "/guard:cf /Qspectre")',
        'set(VCPKG_C_FLAGS "/guard:cf /Qspectre /guard:cf- /Qspectre-")'
    ).Replace(
        'set(VCPKG_LINKER_FLAGS "/guard:cf")',
        'set(VCPKG_LINKER_FLAGS "/guard:cf /GUARD:NO")'
    )
    $mutatedTripletContent += "`nlist(APPEND VCPKG_CXX_FLAGS `" /Qspectre-`")`n"
    [System.IO.File]::WriteAllText(
        $tripletPath,
        $mutatedTripletContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "$tripletRelativePath must set VCPKG_C_FLAGS to exact value '/guard:cf /Qspectre'"
        "$tripletRelativePath must set VCPKG_LINKER_FLAGS to exact value '/guard:cf'"
        "$tripletRelativePath executes unsupported CMake command 'list'"
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot $tripletRelativePath) `
        -Destination $tripletPath `
        -Force

    # CMake bracket comments can span lines. A line-oriented parser sees the
    # apparently valid set() calls below even though CMake never executes them.
    # The policy must inspect the commands consumed by the selected CMake
    # interpreter, not infer CMake grammar from individual source lines.
    $tripletContent = [System.IO.File]::ReadAllText($tripletPath)
    $mutatedTripletContent = $tripletContent.Replace(
        'set(VCPKG_C_FLAGS "/guard:cf /Qspectre")',
        'set(VCPKG_C_FLAGS "/guard:cf /Qspectre") #[['
    ).Replace(
        'set(VCPKG_LINKER_FLAGS "/guard:cf")',
        "set(VCPKG_LINKER_FLAGS `"/guard:cf`")`n# ]]"
    )
    [System.IO.File]::WriteAllText(
        $tripletPath,
        $mutatedTripletContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "$tripletRelativePath must execute exactly one set command for VCPKG_CXX_FLAGS"
        "$tripletRelativePath must execute exactly one set command for VCPKG_LINKER_FLAGS"
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot $tripletRelativePath) `
        -Destination $tripletPath `
        -Force

    # A native interpreter error is still a policy result, and its redirected
    # JSON trace is temporary evidence. Prove the failure path removes that
    # artifact instead of accumulating one file per rejected policy run.
    $operatingSystemTemporaryRoot = [System.IO.Path]::GetFullPath(
        [System.IO.Path]::GetTempPath()
    )
    $traceFileSearchPattern = 'jpg-spinner-cmake-trace-*.jsonl'
    $tracePathsBeforeFailure = @(
        Get-ChildItem `
            -LiteralPath $operatingSystemTemporaryRoot `
            -Filter $traceFileSearchPattern `
            -File |
            ForEach-Object FullName
    )
    $tripletContent = [System.IO.File]::ReadAllText($tripletPath)
    [System.IO.File]::WriteAllText(
        $tripletPath,
        $tripletContent + "`nunknown_policy_test_command()`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics (
        "$tripletRelativePath did not execute successfully under the selected CMake interpreter"
    )
    $tracePathsAfterFailure = @(
        Get-ChildItem `
            -LiteralPath $operatingSystemTemporaryRoot `
            -Filter $traceFileSearchPattern `
            -File |
            ForEach-Object FullName
    )
    $leakedTracePaths = @(
        $tracePathsAfterFailure | Where-Object { $_ -cnotin $tracePathsBeforeFailure }
    )
    foreach ($leakedTracePath in $leakedTracePaths) {
        # The search is rooted in the resolved operating-system temporary
        # directory and the prefix is owned by this policy implementation.
        Remove-Item -LiteralPath $leakedTracePath -Force
    }
    if ($leakedTracePaths.Count -ne 0) {
        throw "Repository policy leaked CMake trace evidence after interpreter failure: $($leakedTracePaths -join ', ')"
    }
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot $tripletRelativePath) `
        -Destination $tripletPath `
        -Force

    # vcpkg's configuration file and the manifest's embedded `configuration`
    # field are equivalent package-authority surfaces. Only the two approved
    # native metadata overlay ports may supersede the immutable baseline.
    $vcpkgManifestPath = Join-Path $temporaryRoot 'vcpkg.json'
    $validOverlayManifestText = [System.IO.File]::ReadAllText($vcpkgManifestPath)
    foreach ($invalidOverlayPaths in @(
        @{ value = $null },
        @{ value = 'vcpkg-ports/exiv2' },
        @{ value = @{} },
        @{ value = @() },
        @{ value = @('vcpkg-ports/exiv2', 'vcpkg-ports/adobe-xmp-core') },
        @{ value = @('vcpkg-ports/exiv2', 'vcpkg-ports/exiv2') },
        @{ value = @('vcpkg-ports/adobe-xmp-core', 'vcpkg-ports/exiv2', 'unapproved') },
        @{ value = @('VCPKG-PORTS/adobe-xmp-core', 'vcpkg-ports/exiv2') },
        @{ value = @('vcpkg-ports/adobe-xmp-core', 42) }
    )) {
        $manifestMutation = $validOverlayManifestText | ConvertFrom-Json -AsHashtable
        $manifestMutation.configuration['overlay-ports'] = $invalidOverlayPaths.value
        [System.IO.File]::WriteAllText(
            $vcpkgManifestPath, ($manifestMutation | ConvertTo-Json -Depth 20),
            [System.Text.UTF8Encoding]::new($false)
        )
        Assert-RejectedMutation -ExpectedDiagnostics "must not declare 'overlay-ports' outside the exact approved"
    }
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath, $validOverlayManifestText, [System.Text.UTF8Encoding]::new($false)
    )

    $unapprovedVcpkgConfiguration = [ordered]@{
        'default-registry' = [ordered]@{
            kind = 'git'
            repository = 'https://example.invalid/unapproved-default-registry'
            baseline = '1111111111111111111111111111111111111111'
        }
        registries = @(
            [ordered]@{
                kind = 'git'
                repository = 'https://example.invalid/unapproved-additional-registry'
                baseline = '2222222222222222222222222222222222222222'
                packages = @('*')
            }
        )
        'overlay-ports' = @('unapproved-ports')
        'overlay-triplets' = @('unapproved-triplets')
    }
    $unapprovedAuthorityDiagnostics = @(
        "must not declare 'default-registry'",
        "must not declare additional 'registries'",
        "must not declare 'overlay-ports'",
        "must not declare 'overlay-triplets'"
    )

    $vcpkgConfigurationPath = Join-Path $temporaryRoot 'vcpkg-configuration.json'
    [System.IO.File]::WriteAllText(
        $vcpkgConfigurationPath,
        ($unapprovedVcpkgConfiguration | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics $unapprovedAuthorityDiagnostics
    [System.IO.File]::Delete($vcpkgConfigurationPath)

    $vcpkgManifestPath = Join-Path $temporaryRoot 'vcpkg.json'
    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $vcpkgManifest['configuration'] = [ordered]@{}
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        ($vcpkgManifest | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    [System.IO.File]::WriteAllText(
        $vcpkgConfigurationPath,
        "{}`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics (
        "vcpkg-configuration.json cannot coexist with vcpkg.json embedded 'configuration'"
    )
    [System.IO.File]::Delete($vcpkgConfigurationPath)
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Destination $vcpkgManifestPath -Force

    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $vcpkgManifest['configuration'] = $unapprovedVcpkgConfiguration
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        ($vcpkgManifest | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics $unapprovedAuthorityDiagnostics
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Destination $vcpkgManifestPath -Force

    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $vcpkgManifest['vcpkg-configuration'] = $unapprovedVcpkgConfiguration
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        ($vcpkgManifest | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "legacy 'vcpkg-configuration' spelling is prohibited"
        $unapprovedAuthorityDiagnostics
    )
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Destination $vcpkgManifestPath -Force

    # Hashtable-backed JSON objects cannot retain repeated member names. Build
    # these controls as raw text so they prove the policy rejects the source
    # document before any last-member-wins conversion can erase evidence.
    $duplicateJsonMemberControlFailures = [System.Collections.Generic.List[string]]::new()

    $validVcpkgManifestText = [System.IO.File]::ReadAllText($vcpkgManifestPath)
    $libjpegDependencyText = @"
      "name": "libjpeg-turbo",
      "default-features": false
"@
    $duplicateDefaultFeaturesText = @"
      "name": "libjpeg-turbo",
      "default-features": true,
      "default-features": false
"@
    $duplicateDependencyMemberManifestText = $validVcpkgManifestText.Replace(
        $libjpegDependencyText,
        $duplicateDefaultFeaturesText
    )
    if ($duplicateDependencyMemberManifestText -ceq $validVcpkgManifestText) {
        throw 'The valid vcpkg manifest fixture lacks the expected libjpeg-turbo dependency text.'
    }
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        $duplicateDependencyMemberManifestText,
        [System.Text.UTF8Encoding]::new($false)
    )
    try {
        Assert-RejectedMutation -ExpectedDiagnostics (
            'vcpkg.json is not valid strict JSON: Duplicate property'
        )
    }
    catch {
        [void]$duplicateJsonMemberControlFailures.Add($_.Exception.Message)
    }
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Destination $vcpkgManifestPath -Force

    $duplicateRegistryConfigurationText = @'
{
  "registries": [
    {
      "kind": "git",
      "repository": "https://example.invalid/erased-registry",
      "baseline": "3333333333333333333333333333333333333333",
      "packages": ["*"]
    }
  ],
  "registries": []
}
'@
    [System.IO.File]::WriteAllText(
        $vcpkgConfigurationPath,
        $duplicateRegistryConfigurationText + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    try {
        Assert-RejectedMutation -ExpectedDiagnostics (
            'vcpkg-configuration.json is not valid strict JSON: Duplicate property'
        )
    }
    catch {
        [void]$duplicateJsonMemberControlFailures.Add($_.Exception.Message)
    }
    [System.IO.File]::Delete($vcpkgConfigurationPath)

    if ($duplicateJsonMemberControlFailures.Count -gt 0) {
        throw (
            "Repository policy duplicate-JSON-member controls failed:`n - " +
            ($duplicateJsonMemberControlFailures -join "`n - ")
        )
    }

    # A dependency's object fields participate in resolution just as directly
    # as its name. Close each approved entry shape so conditional, host-only,
    # feature, and alternate-version semantics cannot bypass the reviewed graph.
    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $libjpegDependency = @(
        $vcpkgManifest.dependencies |
            Where-Object { $_.name -ceq 'libjpeg-turbo' }
    )[0]
    $libjpegDependency.platform = '!arm64'
    $libjpegDependency.host = $true
    $libjpegDependency.features = @('tools')
    $libjpegOverride = @(
        $vcpkgManifest.overrides |
            Where-Object { $_.name -ceq 'libjpeg-turbo' }
    )[0]
    $libjpegOverride['port-version'] = 1
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        ($vcpkgManifest | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "vcpkg dependency 'libjpeg-turbo' contains unapproved property 'platform'"
        "vcpkg dependency 'libjpeg-turbo' contains unapproved property 'host'"
        "vcpkg dependency 'libjpeg-turbo' contains unapproved property 'features'"
        "vcpkg override 'libjpeg-turbo' contains unapproved property 'port-version'"
    )
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Destination $vcpkgManifestPath -Force

    # ConvertFrom-Json values retain their JSON data types, but PowerShell's
    # comparison and array operators can coerce strings into apparently valid
    # booleans or one-item collections. The policy must validate types first.
    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $libjpegDependency = @(
        $vcpkgManifest.dependencies |
            Where-Object { $_.name -ceq 'libjpeg-turbo' }
    )[0]
    $libjpegDependency['default-features'] = 'false'
    $exiv2Dependency = @(
        $vcpkgManifest.dependencies |
            Where-Object { $_.name -ceq 'exiv2' }
    )[0]
    $exiv2Dependency.features = 'xmp'
    $catch2Dependency = @(
        $vcpkgManifest.dependencies |
            Where-Object { $_.name -ceq 'catch2' }
    )[0]
    $catch2Dependency.name = 42
    $exiv2Override = @(
        $vcpkgManifest.overrides |
            Where-Object { $_.name -ceq 'exiv2' }
    )[0]
    $exiv2Override.version = @('0.28.8')
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        ($vcpkgManifest | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "vcpkg dependency at index 2 property 'name' must be a JSON string"
        "vcpkg dependency 'libjpeg-turbo' property 'default-features' must be the JSON boolean false"
        "vcpkg dependency 'exiv2' property 'features' must be a JSON array"
        "vcpkg override 'exiv2' property 'version' must be a JSON string"
    )
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Destination $vcpkgManifestPath -Force

    # The approved manifest is a closed direct-dependency set. A package cannot
    # become part of the product merely by appearing alongside the three pinned
    # packages, even when it disables default features.
    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $vcpkgManifest.dependencies += [ordered]@{
        name = 'zlib'
        'default-features' = $false
    }
    $mutatedVcpkgManifest = $vcpkgManifest | ConvertTo-Json -Depth 20
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        $mutatedVcpkgManifest + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics "unapproved direct dependency 'zlib'"

    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') `
        -Destination $vcpkgManifestPath `
        -Force

    # Root manifest features are another dependency-bearing path: vcpkg can
    # activate a feature from `default-features` and add every dependency that
    # feature declares without changing the top-level `dependencies` array.
    # The application manifest is intentionally non-extensible at this layer.
    $vcpkgManifest =
        Get-Content -LiteralPath $vcpkgManifestPath -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $vcpkgManifest['default-features'] = @('unapproved-dependency')
    $vcpkgManifest.features = [ordered]@{
        'unapproved-dependency' = [ordered]@{
            description = 'Negative control for a hidden dependency path.'
            dependencies = @('zlib')
        }
    }
    [System.IO.File]::WriteAllText(
        $vcpkgManifestPath,
        ($vcpkgManifest | ConvertTo-Json -Depth 20) + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-RejectedMutation -ExpectedDiagnostics @(
        "vcpkg.json manifest contains unapproved property 'default-features'"
        "vcpkg.json manifest contains unapproved property 'features'"
    )
    Copy-Item `
        -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') `
        -Destination $vcpkgManifestPath `
        -Force

    # NuGet combines each collection independently across machine, user, and
    # repository files. A reviewed packageSources collection therefore does not
    # prevent an inherited or local disabledPackageSources entry from turning
    # off the repository's sole approved feed.
    $nuGetConfigurationPath = Join-Path $temporaryRoot 'NuGet.config'
    $nuGetConfiguration = [System.Xml.XmlDocument]::new()
    $nuGetConfiguration.PreserveWhitespace = $true
    $nuGetConfiguration.Load($nuGetConfigurationPath)
    $disabledPackageSources =
        $nuGetConfiguration.SelectSingleNode('/configuration/disabledPackageSources')
    if ($null -eq $disabledPackageSources) {
        $disabledPackageSources = $nuGetConfiguration.CreateElement('disabledPackageSources')
        [void]$nuGetConfiguration.DocumentElement.AppendChild($disabledPackageSources)
    }
    if ($null -eq $disabledPackageSources.SelectSingleNode('clear')) {
        [void]$disabledPackageSources.AppendChild($nuGetConfiguration.CreateElement('clear'))
    }
    $disabledNuGetOrgSource = $nuGetConfiguration.CreateElement('add')
    $disabledNuGetOrgSource.SetAttribute('key', 'nuget.org')
    $disabledNuGetOrgSource.SetAttribute('value', 'true')
    [void]$disabledPackageSources.AppendChild($disabledNuGetOrgSource)
    $nuGetConfiguration.Save($nuGetConfigurationPath)

    Assert-RejectedMutation -ExpectedDiagnostics (
        'NuGet.config must clear inherited disabled package sources and declare no disabled source entries.'
    )

    Write-Output (
        'PASS: repository policy rejects weakened or duplicate build settings, unscoped modern C++ ' +
        'policy, legacy C++/CX re-enablement, ambient VC-directory selection, non-object JSON roots, ' +
        'executable triplet mutations, ' +
        'alternate vcpkg authorities, duplicate JSON members, root dependency-activation surfaces, ' +
        'disabled NuGet sources, and unreviewed dependency semantics.'
    )
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
        $resolvedOperatingSystemTemporaryRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
        if (-not $resolvedTemporaryRoot.StartsWith(
            $resolvedOperatingSystemTemporaryRoot,
            [System.StringComparison]::OrdinalIgnoreCase
        )) {
            throw "Refusing to remove test directory outside the operating-system temporary root: $resolvedTemporaryRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
