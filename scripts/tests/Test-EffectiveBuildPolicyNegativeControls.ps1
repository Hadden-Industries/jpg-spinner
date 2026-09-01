[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$temporaryParent = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$temporaryRepositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $temporaryParent "jpg-spinner-effective-policy-$([guid]::NewGuid().ToString('N'))")
)

if (-not $temporaryRepositoryRoot.StartsWith($temporaryParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create an isolated repository outside the operating-system temporary directory: $temporaryRepositoryRoot"
}

function Copy-RepositoryFile {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $sourcePath = Join-Path $repositoryRoot $RelativePath
    $destinationPath = Join-Path $temporaryRepositoryRoot $RelativePath
    [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $destinationPath))
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

function Invoke-IsolatedEffectiveBuildPolicy {
    $powerShellExecutablePath = (Get-Process -Id $PID).Path
    $effectiveBuildPolicyPath = Join-Path $temporaryRepositoryRoot 'scripts/Test-EffectiveBuildPolicy.ps1'
    $output = & $powerShellExecutablePath `
        -NoProfile `
        -File $effectiveBuildPolicyPath `
        -EvidenceScope ToolchainProbe `
        2>&1

    return [pscustomobject]@{
        exitCode = $LASTEXITCODE
        output = $output -join "`n"
    }
}

try {
    [void][System.IO.Directory]::CreateDirectory($temporaryRepositoryRoot)
    foreach ($relativePath in @(
        'Directory.Build.props',
        'Directory.Build.targets',
        'eng/toolchain-lock.json',
        'eng/BuildPolicyProbe/BuildPolicyProbe.cpp',
        'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj',
        'scripts/DumpbinImageMitigationMetadata.psm1',
        'scripts/JsonObjectMemberValidation.psm1',
        'scripts/Resolve-MSBuildToolchain.ps1',
        'scripts/Test-EffectiveBuildPolicy.ps1'
    )) {
        Copy-RepositoryFile -RelativePath $relativePath
    }

    $probeProjectPath = Join-Path $temporaryRepositoryRoot 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject = [System.Xml.XmlDocument]::new()
    $probeProject.PreserveWhitespace = $true
    $msBuildNamespace = 'http://schemas.microsoft.com/developer/msbuild/2003'
    $namespaceManager = [System.Xml.XmlNamespaceManager]::new($probeProject.NameTable)
    $namespaceManager.AddNamespace('msb', $msBuildNamespace)
    $isolatedToolchain = & (Join-Path $temporaryRepositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1') `
        -RepositoryRoot $temporaryRepositoryRoot

    $buildPropertiesPath = Join-Path $temporaryRepositoryRoot 'Directory.Build.props'
    $buildProperties = [System.Xml.XmlDocument]::new()
    $buildProperties.PreserveWhitespace = $true
    $buildProperties.Load($buildPropertiesPath)
    $spectreMitigationNode = $buildProperties.SelectSingleNode('/Project/PropertyGroup/SpectreMitigation')
    if ($null -eq $spectreMitigationNode) {
        throw 'The valid fixture must select Spectre-mitigated libraries before the negative mutation is applied.'
    }
    [void]$spectreMitigationNode.ParentNode.RemoveChild($spectreMitigationNode)
    $buildProperties.Save($buildPropertiesPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    $requiredDiagnosticFragments = @(
        'does not select an architecture-specific',
        'Spectre-mitigated MSVC library path'
    )
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted raw /Qspectre without selecting mitigated MSVC libraries.`n$($result.output)"
    }
    foreach ($requiredDiagnosticFragment in $requiredDiagnosticFragments) {
        if ($result.output.IndexOf($requiredDiagnosticFragment, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "Effective build policy failed without the required Spectre-library diagnostic.`n$($result.output)"
        }
    }

    # Values supplied with MSBuild /p: are global and cannot be changed during
    # project evaluation. The verifier must therefore let the repository select
    # the toolset it is testing; otherwise this invalid value is masked by the
    # verifier itself and the probe reports a false green result.
    Copy-RepositoryFile -RelativePath 'Directory.Build.props'
    $buildProperties.Load($buildPropertiesPath)
    $platformToolsetNode = $buildProperties.SelectSingleNode('/Project/PropertyGroup/PlatformToolset')
    if ($null -eq $platformToolsetNode) {
        throw 'The valid fixture must declare PlatformToolset before the negative mutation is applied.'
    }
    $platformToolsetNode.InnerText = 'JpgSpinnerInvalidToolset'
    $buildProperties.Save($buildPropertiesPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy masked an invalid repository PlatformToolset with a global property.`n$($result.output)"
    }
    if ($result.output.IndexOf(
            'JpgSpinnerInvalidToolset',
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw "Effective build policy failed without exposing the invalid evaluated toolset.`n$($result.output)"
    }

    # MSBuild automatically discovers Directory.Build.rsp above the first
    # project. A reviewed-looking global supplied there must not make an
    # invalid repository declaration appear effective to this policy probe.
    $automaticResponseFilePath = Join-Path $temporaryRepositoryRoot 'Directory.Build.rsp'
    [System.IO.File]::WriteAllText(
        $automaticResponseFilePath,
        "-property:PlatformToolset=v145`n",
        [System.Text.UTF8Encoding]::new($false)
    )

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw (
            'Effective build policy accepted an invalid repository PlatformToolset after ' +
            "an automatic response file masked it with a global property.`n$($result.output)"
        )
    }
    [System.IO.File]::Delete($automaticResponseFilePath)

    # LINK searches /LIBPATH directories before the LIB environment path and
    # searches each collection in declaration order. Prepend the ordinary MSVC
    # runtime directory while retaining the reviewed Spectre directory later;
    # a membership-only verifier would incorrectly accept this effective order.
    Copy-RepositoryFile -RelativePath 'Directory.Build.props'
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $targetLibraryArchitectureByPlatform = [ordered]@{
        Win32 = 'x86'
        x64 = 'x64'
        ARM64 = 'arm64'
    }
    foreach ($targetPlatform in $targetLibraryArchitectureByPlatform.Keys) {
        $unmitigatedRuntimeLibraryDirectoryPath = Join-Path `
            $isolatedToolchain.vcToolsDirectoryPath `
            "lib/$($targetLibraryArchitectureByPlatform[$targetPlatform])"
        $librarySearchOverrideGroup = $probeProject.CreateElement('PropertyGroup', $msBuildNamespace)
        $librarySearchOverrideGroup.SetAttribute('Condition', "'`$(Platform)' == '$targetPlatform'")
        $libraryPathElement = $probeProject.CreateElement('LibraryPath', $msBuildNamespace)
        $libraryPathElement.InnerText = "$unmitigatedRuntimeLibraryDirectoryPath;`$(LibraryPath)"
        [void]$librarySearchOverrideGroup.AppendChild($libraryPathElement)
        [void]$probeProject.DocumentElement.AppendChild($librarySearchOverrideGroup)
    }
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw (
            'Effective build policy accepted an ordinary MSVC runtime directory before the ' +
            "Spectre-mitigated runtime directory.`n$($result.output)"
        )
    }
    foreach ($requiredLibraryPrecedenceDiagnostic in @(
        'effective library search order',
        'unmitigated MSVC runtime directory',
        'before required Spectre-mitigated'
    )) {
        if ($result.output.IndexOf(
                $requiredLibraryPrecedenceDiagnostic,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
            throw (
                "Effective build policy failed without library-precedence diagnostic " +
                "'$requiredLibraryPrecedenceDiagnostic'.`n$($result.output)"
            )
        }
    }

    # Directory.Build.props is an early default, not an immutable global. A
    # project can override the selected SDK before Microsoft.Cpp.props consumes
    # it, so the effective gate must read MSBuild's evaluated property rather
    # than infer SDK selection from the root file.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $cppPropertiesImport = $probeProject.SelectSingleNode(
        '/msb:Project/msb:Import[contains(@Project, "\Microsoft.Cpp.props")]',
        $namespaceManager
    )
    if ($null -eq $cppPropertiesImport) {
        throw 'The build-policy probe fixture lacks its Microsoft.Cpp.props import.'
    }
    $alternateWindowsSdkVersion = '10.0.26100.0'
    $sdkOverrideGroup = $probeProject.CreateElement('PropertyGroup', $msBuildNamespace)
    $sdkVersionElement = $probeProject.CreateElement('WindowsTargetPlatformVersion', $msBuildNamespace)
    $sdkVersionElement.InnerText = $alternateWindowsSdkVersion
    [void]$sdkOverrideGroup.AppendChild($sdkVersionElement)
    [void]$probeProject.DocumentElement.InsertBefore($sdkOverrideGroup, $cppPropertiesImport)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw (
            "Effective build policy accepted alternate installed Windows SDK $alternateWindowsSdkVersion.`n" +
            $result.output
        )
    }
    foreach ($requiredSdkDiagnostic in @(
        'effective Windows SDK version',
        $alternateWindowsSdkVersion,
        '10.0.28000.0'
    )) {
        if ($result.output.IndexOf(
                $requiredSdkDiagnostic,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
            throw (
                "Effective build policy failed without SDK-selection diagnostic " +
                "'$requiredSdkDiagnostic'.`n$($result.output)"
            )
        }
    }
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'

    # Ambient CL/LINK variables and UseEnv-controlled VC directory variables are
    # unreviewed build inputs. Exercise both documented surfaces against the
    # real compiler and linker: the child verifier must remove them before
    # MSBuild starts and reconstructs platform-specific directories.
    Copy-RepositoryFile -RelativePath 'Directory.Build.props'
    # Remove the repository-owned UseEnv=false declaration from this isolated
    # fixture when it exists. The control must prove that the verifier's child
    # process boundary independently excludes ambient VC-directory state; a
    # valid props file must not be able to mask a regression in that boundary.
    $ambientBuildPropertiesPath = Join-Path $temporaryRepositoryRoot 'Directory.Build.props'
    $ambientBuildPropertiesDocument = [System.Xml.XmlDocument]::new()
    $ambientBuildPropertiesDocument.PreserveWhitespace = $true
    $ambientBuildPropertiesDocument.Load($ambientBuildPropertiesPath)
    $useEnvironmentNode = $ambientBuildPropertiesDocument.SelectSingleNode(
        '/Project/PropertyGroup/UseEnv'
    )
    if ($null -ne $useEnvironmentNode) {
        [void]$useEnvironmentNode.ParentNode.RemoveChild($useEnvironmentNode)
        $ambientBuildPropertiesDocument.Save($ambientBuildPropertiesPath)
    }

    $probeSourcePath = Join-Path $temporaryRepositoryRoot 'eng/BuildPolicyProbe/BuildPolicyProbe.cpp'
    $probeSourceContent = [System.IO.File]::ReadAllText($probeSourcePath)
    [System.IO.File]::WriteAllText(
        $probeSourcePath,
        @'
#ifdef JPG_SPINNER_AMBIENT_COMPILER_OPTION
#error Ambient CL options reached the policy probe.
#endif

'@ + $probeSourceContent,
        [System.Text.UTF8Encoding]::new($false)
    )

    $ambientOptionVariableNames = @(
        'CL',
        '_CL_',
        'LINK',
        '_LINK_',
        'UseEnv',
        'LIB',
        'LIBPATH',
        'INCLUDE',
        'EXTERNAL_INCLUDE'
    )
    $originalAmbientOptionValues = @{}
    foreach ($ambientOptionVariableName in $ambientOptionVariableNames) {
        $originalAmbientOptionValues[$ambientOptionVariableName] =
            [Environment]::GetEnvironmentVariable(
                $ambientOptionVariableName,
                [EnvironmentVariableTarget]::Process
            )
    }

    try {
        [Environment]::SetEnvironmentVariable(
            'CL',
            '/DJPG_SPINNER_AMBIENT_COMPILER_OPTION=1',
            [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            '_CL_',
            '/WX-',
            [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            'LINK',
            '/NODEFAULTLIB',
            [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            '_LINK_',
            '/GUARD:NO /CETCOMPAT:NO',
            [EnvironmentVariableTarget]::Process
        )
        [Environment]::SetEnvironmentVariable(
            'UseEnv',
            'true',
            [EnvironmentVariableTarget]::Process
        )
        foreach ($ambientVcDirectoryVariableName in @('LIB', 'LIBPATH', 'INCLUDE', 'EXTERNAL_INCLUDE')) {
            [Environment]::SetEnvironmentVariable(
                $ambientVcDirectoryVariableName,
                'C:\JPG_SPINNER_AMBIENT_VC_DIRECTORY',
                [EnvironmentVariableTarget]::Process
            )
        }

        $result = Invoke-IsolatedEffectiveBuildPolicy
    }
    finally {
        foreach ($ambientOptionVariableName in $ambientOptionVariableNames) {
            [Environment]::SetEnvironmentVariable(
                $ambientOptionVariableName,
                $originalAmbientOptionValues[$ambientOptionVariableName],
                [EnvironmentVariableTarget]::Process
            )
        }
    }

    if ($result.exitCode -ne 0) {
        throw "Effective build policy allowed ambient MSVC option variables to alter the probe build.`n$($result.output)"
    }

    $ambientOptionEvidence = @(
        Get-ChildItem -LiteralPath (Join-Path $temporaryRepositoryRoot 'artifacts/build-policy-probe') -Filter '*.log' |
            Select-String -Pattern (
                'JPG_SPINNER_AMBIENT_COMPILER_OPTION|JPG_SPINNER_AMBIENT_VC_DIRECTORY|D9025|LNK4075'
            )
    )
    if ($ambientOptionEvidence.Count -gt 0) {
        throw (
            "Effective build policy retained ambient MSVC option evidence:`n" +
            ($ambientOptionEvidence -join "`n")
        )
    }

    # Restore the production probe, then append conflicting compiler and linker
    # tokens after the inherited options. MSVC resolves these option families by
    # command-line order, so the verifier must reject the rightmost effective
    # member even though every required positive token still appears earlier.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.cpp'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    if ($null -eq $itemDefinitionGroup) {
        throw 'The build-policy probe fixture lacks its expected ItemDefinitionGroup.'
    }

    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = (
        '%(AdditionalOptions) /std:c++17 /permissive /W0 /WX- /sdl- ' +
        '/guard:cf- /Qspectre- /Zc:__cplusplus- /Od /GL-'
    )
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)

    $linkerDefinition = $itemDefinitionGroup.SelectSingleNode('msb:Link', $namespaceManager)
    if ($null -eq $linkerDefinition) {
        throw 'The build-policy probe fixture lacks its expected Link definition.'
    }
    $linkerAdditionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $linkerAdditionalOptions.InnerText = '%(AdditionalOptions) /GUARD:NO /CETCOMPAT:NO'
    [void]$linkerDefinition.AppendChild($linkerAdditionalOptions)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    $requiredOptionDiagnostics = @(
        'effective language-standard', "'/std:c++17'", "required '/std:c++20'",
        'effective conformance', "'/permissive'", "required '/permissive-'",
        'effective warning-level', "'/W0'", "required '/W4'",
        'effective warnings-as-errors', "'/WX-'", "required '/WX'",
        'effective SDL-check', "'/sdl-'", "required '/sdl'",
        'effective compiler Control Flow Guard', "'/guard:cf-'",
        'effective Spectre-v1-mitigation', "'/Qspectre-'", "required '/Qspectre'",
        'effective updated-__cplusplus-macro', "'/Zc:__cplusplus-'", "required '/Zc:__cplusplus'",
        'effective optimization', "'/Od'", "required '/O2'",
        'effective whole-program-optimization', "'/GL-'", "required '/GL'",
        'effective linker Control Flow Guard', "'/GUARD:NO'",
        'effective CET-compatibility', "'/CETCOMPAT:NO'", "required '/CETCOMPAT'"
    )
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted disabling tokens after their required positive counterparts.`n$($result.output)"
    }
    foreach ($requiredOptionDiagnostic in $requiredOptionDiagnostics) {
        if ($result.output.IndexOf($requiredOptionDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "Effective build policy failed without the required '$requiredOptionDiagnostic' diagnostic.`n$($result.output)"
        }
    }

    # CL accepts either '/' or '-' as the option specifier. A verifier that
    # recognizes only the conventional slash spelling will miss this later
    # language-standard override and falsely report that C++20 is effective.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = '%(AdditionalOptions) -std:c++17'
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy ignored a rightmost dash-prefixed CL option.`n$($result.output)"
    }
    foreach ($requiredDashOptionDiagnostic in @(
        'effective language-standard', "'-std:c++17'", "required '/std:c++20'"
    )) {
        if ($result.output.IndexOf($requiredDashOptionDiagnostic, [System.StringComparison]::Ordinal) -lt 0) {
            throw (
                "Effective build policy failed without the required dash-option diagnostic " +
                "'$requiredDashOptionDiagnostic'.`n$($result.output)"
            )
        }
    }

    # LINK likewise accepts '-' as an option specifier, and processes repeated
    # options from left to right. Exercise both protected linker families so a
    # slash-only matcher cannot mistake the earlier required values as effective.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $linkerDefinition = $probeProject.SelectSingleNode(
        '/msb:Project/msb:ItemDefinitionGroup/msb:Link',
        $namespaceManager
    )
    $linkerAdditionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $linkerAdditionalOptions.InnerText = '%(AdditionalOptions) -GUARD:NO -CETCOMPAT:NO'
    [void]$linkerDefinition.AppendChild($linkerAdditionalOptions)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy ignored rightmost dash-prefixed LINK options.`n$($result.output)"
    }
    foreach ($requiredLinkerDashOptionDiagnostic in @(
        'effective linker Control Flow Guard', "'-GUARD:NO'",
        'effective CET-compatibility', "'-CETCOMPAT:NO'"
    )) {
        if ($result.output.IndexOf(
                $requiredLinkerDashOptionDiagnostic,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
            throw (
                "Effective build policy failed without LINK dash-option diagnostic " +
                "'$requiredLinkerDashOptionDiagnostic'.`n$($result.output)"
            )
        }
    }

    # Unlike LINK, CL documents option names as case-sensitive (except HELP).
    # This invalidly cased lookalike must not override the valid /WX that the
    # shared build policy supplied earlier in the compiler option stream.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = '%(AdditionalOptions) /wx-'
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -ne 0) {
        throw (
            'Effective build policy treated an invalidly cased CL lookalike as a real option. ' +
            "CL option names are case-sensitive.`n$($result.output)"
        )
    }

    # The compiler option name `guard` remains case-sensitive, but the locked
    # v145 compiler accepts and canonicalizes case variants of the `cf`
    # argument. Prove that a consumed, rightmost /guard:CF- cannot disappear
    # behind an earlier policy-owned /guard:cf token.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = '%(AdditionalOptions) /guard:CF-'
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy ignored the rightmost compiler-consumed /guard:CF-.`n$($result.output)"
    }
    $normalizedMixedCaseGuardOutput =
        $result.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
    foreach ($requiredMixedCaseGuardDiagnostic in @(
        'effective compiler Control Flow Guard', "'/guard:CF-'", "required '/guard:cf'"
    )) {
        if ($normalizedMixedCaseGuardOutput.IndexOf(
                $requiredMixedCaseGuardDiagnostic,
                [System.StringComparison]::Ordinal
            ) -lt 0) {
            throw (
                'Effective build policy failed without mixed-case CFG diagnostic ' +
                "'$requiredMixedCaseGuardDiagnostic'.`n$($result.output)"
            )
        }
    }

    # Microsoft documents STATUS and NOSTATUS as display-only LTCG arguments,
    # while INCREMENTAL still performs link-time code generation for affected
    # inputs. Each is therefore a compliant rightmost effective state rather
    # than a spelling alias for OFF.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $buildTargetsPath = Join-Path $temporaryRepositoryRoot 'Directory.Build.targets'
    $releaseLinkerDefinitionXPath =
        '/Project/ItemDefinitionGroup[' +
        'contains(@Condition, "$(Configuration)") and ' +
        'contains(@Condition, "Release")]/Link'
    foreach ($documentedEnablingLtcgArgument in @('STATUS', 'NOSTATUS', 'INCREMENTAL')) {
        Copy-RepositoryFile -RelativePath 'Directory.Build.targets'
        $buildTargets = [System.Xml.XmlDocument]::new()
        $buildTargets.PreserveWhitespace = $true
        $buildTargets.Load($buildTargetsPath)
        $releaseLinkerDefinition = $buildTargets.SelectSingleNode($releaseLinkerDefinitionXPath)
        if ($null -eq $releaseLinkerDefinition) {
            throw 'The valid build-policy fixture lacks its expected Release Link definition.'
        }
        $releaseLinkerAdditionalOptions = $buildTargets.CreateElement('AdditionalOptions')
        $releaseLinkerAdditionalOptions.InnerText =
            "%(AdditionalOptions) /LTCG:$documentedEnablingLtcgArgument"
        [void]$releaseLinkerDefinition.AppendChild($releaseLinkerAdditionalOptions)
        $buildTargets.Save($buildTargetsPath)

        $result = Invoke-IsolatedEffectiveBuildPolicy
        if ($result.exitCode -ne 0) {
            throw (
                "Effective build policy rejected enabling /LTCG:$documentedEnablingLtcgArgument.`n" +
                $result.output
            )
        }
    }

    # OFF is the documented disabling state. Keeping this negative control
    # beside the enabling-state controls prevents an implementation from
    # accepting every token that merely begins with /LTCG.
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'
    $buildTargets.Load($buildTargetsPath)
    $releaseLinkerDefinition = $buildTargets.SelectSingleNode($releaseLinkerDefinitionXPath)
    $releaseLinkerAdditionalOptions = $buildTargets.CreateElement('AdditionalOptions')
    $releaseLinkerAdditionalOptions.InnerText = '%(AdditionalOptions) /LTCG:OFF'
    [void]$releaseLinkerDefinition.AppendChild($releaseLinkerAdditionalOptions)
    $buildTargets.Save($buildTargetsPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted the disabling /LTCG:OFF state.`n$($result.output)"
    }
    $normalizedDisabledLtcgOutput =
        $result.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
    foreach ($requiredDisabledLtcgDiagnostic in @(
        'effective link-time-code-generation', "'/LTCG:OFF'", "required '/LTCG'"
    )) {
        if ($normalizedDisabledLtcgOutput.IndexOf(
                $requiredDisabledLtcgDiagnostic,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
            throw (
                'Effective build policy failed without disabled-LTCG diagnostic ' +
                "'$requiredDisabledLtcgDiagnostic'.`n$($result.output)"
            )
        }
    }
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'

    # CL expands a command file at the position of its @ argument. Keep the
    # path quoted and include a space so this also exercises the documented
    # Microsoft C/C++ argv quoting rules rather than whitespace splitting.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $compilerCommandFilePath =
        Join-Path $temporaryRepositoryRoot 'eng/BuildPolicyProbe/compiler policy.rsp'
    [System.IO.File]::WriteAllText(
        $compilerCommandFilePath,
        "-std:c++17`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = "%(AdditionalOptions) @`"$compilerCommandFilePath`""
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted an uninspected CL command file.`n$($result.output)"
    }
    foreach ($requiredCommandFileDiagnostic in @(
        'compiler command line contains command-file argument',
        'compiler policy.rsp'
    )) {
        if ($result.output.IndexOf($requiredCommandFileDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw (
                "Effective build policy failed without command-file diagnostic " +
                "'$requiredCommandFileDiagnostic'.`n$($result.output)"
            )
        }
    }

    # The alternate option specifier applies to exact required options too,
    # not only to conflict families. Replace the sole compiler /utf-8 spelling
    # with its documented dash-prefixed equivalent and require a green result.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $buildTargetsPath = Join-Path $temporaryRepositoryRoot 'Directory.Build.targets'
    $buildTargets = [System.Xml.XmlDocument]::new()
    $buildTargets.PreserveWhitespace = $true
    $buildTargets.Load($buildTargetsPath)
    $compilerAdditionalOptions =
        $buildTargets.SelectSingleNode('/Project/ItemDefinitionGroup/ClCompile/AdditionalOptions')
    if ($null -eq $compilerAdditionalOptions) {
        throw 'The valid build-policy fixture lacks compiler AdditionalOptions.'
    }
    $compilerAdditionalOptions.InnerText =
        $compilerAdditionalOptions.InnerText.Replace('/utf-8', '-utf-8')
    $buildTargets.Save($buildTargetsPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -ne 0) {
        throw (
            'Effective build policy rejected the documented dash-prefixed spelling of ' +
            "the required CL /utf-8 option.`n$($result.output)"
        )
    }
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'

    # CL option names are case-sensitive. An invalidly cased exact-option
    # lookalike must not satisfy the required /utf-8 policy assertion.
    $buildTargets.Load($buildTargetsPath)
    $compilerAdditionalOptions =
        $buildTargets.SelectSingleNode('/Project/ItemDefinitionGroup/ClCompile/AdditionalOptions')
    $compilerAdditionalOptions.InnerText =
        $compilerAdditionalOptions.InnerText.Replace('/utf-8', '/UTF-8')
    $buildTargets.Save($buildTargetsPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted an invalidly cased CL /UTF-8 lookalike.`n$($result.output)"
    }
    if ($result.output.IndexOf(
            "compiler lacks effective option '/utf-8'",
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw "Effective build policy failed without the exact-option case diagnostic.`n$($result.output)"
    }
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'

    # The locked compiler diagnoses /utf-8 combined with either explicit
    # charset form as D8016. Exercise options after /utf-8 to prove the gate
    # models mutual incompatibility rather than rightmost-family precedence.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = (
        '%(AdditionalOptions) /source-charset:.1252 /execution-charset:.1252'
    )
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted character-set overrides after /utf-8.`n$($result.output)"
    }
    foreach ($requiredCharacterSetDiagnostic in @(
        'incompatible source-character-set', "'/source-charset:.1252'",
        'incompatible execution-character-set', "'/execution-charset:.1252'"
    )) {
        if ($result.output.IndexOf(
                $requiredCharacterSetDiagnostic,
                [System.StringComparison]::Ordinal
            ) -lt 0) {
            throw (
                "Effective build policy failed without character-set diagnostic " +
                "'$requiredCharacterSetDiagnostic'.`n$($result.output)"
            )
        }
    }

    # The locked compiler reports separate charset forms as incompatible with
    # /utf-8 regardless of ordering. Put dash-prefixed forms before /utf-8 so
    # the gate cannot accidentally implement this as a rightmost-family rule.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'
    $buildTargets.Load($buildTargetsPath)
    $compilerAdditionalOptions =
        $buildTargets.SelectSingleNode('/Project/ItemDefinitionGroup/ClCompile/AdditionalOptions')
    $compilerAdditionalOptions.InnerText = $compilerAdditionalOptions.InnerText.Replace(
        '/utf-8',
        '-source-charset:.1252 -execution-charset:.1252 /utf-8'
    )
    $buildTargets.Save($buildTargetsPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted separate charset options before /utf-8.`n$($result.output)"
    }
    foreach ($requiredCharacterSetDiagnostic in @(
        'incompatible source-character-set', "'-source-charset:.1252'",
        'incompatible execution-character-set', "'-execution-charset:.1252'"
    )) {
        if ($result.output.IndexOf(
                $requiredCharacterSetDiagnostic,
                [System.StringComparison]::Ordinal
            ) -lt 0) {
            throw (
                "Effective build policy failed without inverse-order charset diagnostic " +
                "'$requiredCharacterSetDiagnostic'.`n$($result.output)"
            )
        }
    }
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'

    # Exact /link terminates CL option parsing; every following argument is a
    # linker argument even when it resembles a valid compiler option. Remove
    # the real compiler spellings and place lookalikes only beyond that
    # boundary so neither may satisfy compiler policy.
    $buildTargets.Load($buildTargetsPath)
    $compilerAdditionalOptions =
        $buildTargets.SelectSingleNode('/Project/ItemDefinitionGroup/ClCompile/AdditionalOptions')
    $compilerAdditionalOptions.InnerText =
        $compilerAdditionalOptions.InnerText.Replace('/guard:cf ', '').Replace('/Brepro ', '')
    $buildTargets.Save($buildTargetsPath)

    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = '%(AdditionalOptions) /link /guard:cf /Brepro'
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted compiler lookalikes after the CL /link boundary.`n$($result.output)"
    }
    foreach ($requiredLinkBoundaryDiagnostic in @(
        "compiler lacks effective option '/Brepro'",
        'compiler Control Flow Guard option'
    )) {
        if ($result.output.IndexOf($requiredLinkBoundaryDiagnostic, [System.StringComparison]::Ordinal) -lt 0) {
            throw (
                "Effective build policy failed without /link-boundary diagnostic " +
                "'$requiredLinkBoundaryDiagnostic'.`n$($result.output)"
            )
        }
    }
    Copy-RepositoryFile -RelativePath 'Directory.Build.targets'
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'

    # LINK also accepts command files in place and applies the last processed
    # conflicting option. Final-image inspection may notice the consequence,
    # but the verifier must independently identify the uninspected input that
    # made its option-family conclusion incomplete.
    $linkerCommandFilePath =
        Join-Path $temporaryRepositoryRoot 'eng/BuildPolicyProbe/linker-policy.rsp'
    [System.IO.File]::WriteAllText(
        $linkerCommandFilePath,
        "-GUARD:NO -CETCOMPAT:NO`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    $probeProject.Load($probeProjectPath)
    $linkerDefinition = $probeProject.SelectSingleNode(
        '/msb:Project/msb:ItemDefinitionGroup/msb:Link',
        $namespaceManager
    )
    $linkerAdditionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $linkerAdditionalOptions.InnerText =
        '%(AdditionalOptions) @eng\BuildPolicyProbe\linker-policy.rsp'
    [void]$linkerDefinition.AppendChild($linkerAdditionalOptions)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -eq 0) {
        throw "Effective build policy accepted an uninspected LINK command file.`n$($result.output)"
    }
    foreach ($requiredLinkerCommandFileDiagnostic in @(
        'linker command line contains command-file argument',
        'linker-policy.rsp'
    )) {
        if ($result.output.IndexOf(
                $requiredLinkerCommandFileDiagnostic,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
            throw (
                "Effective build policy failed without LINK command-file diagnostic " +
                "'$requiredLinkerCommandFileDiagnostic'.`n$($result.output)"
            )
        }
    }
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'

    # An earlier disabling token is harmless when the required family member is
    # rightmost. This inverse control prevents the verifier from merely banning
    # negative spellings without modelling MSVC's documented precedence rule.
    Copy-RepositoryFile -RelativePath 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $probeProject.Load($probeProjectPath)
    $itemDefinitionGroup = $probeProject.SelectSingleNode('/msb:Project/msb:ItemDefinitionGroup', $namespaceManager)
    $compilerDefinition = $probeProject.CreateElement('ClCompile', $msBuildNamespace)
    $additionalOptions = $probeProject.CreateElement('AdditionalOptions', $msBuildNamespace)
    $additionalOptions.InnerText = '%(AdditionalOptions) /sdl- /sdl'
    [void]$compilerDefinition.AppendChild($additionalOptions)
    [void]$itemDefinitionGroup.AppendChild($compilerDefinition)
    $probeProject.Save($probeProjectPath)

    $result = Invoke-IsolatedEffectiveBuildPolicy
    if ($result.exitCode -ne 0) {
        throw "Effective build policy rejected a required option that is rightmost in its family.`n$($result.output)"
    }

    Write-Output (
        'PASS: effective build policy consumes repository-selected toolchain values, rejects ' +
        'alternate SDK selection, ambient MSVC inputs, compiler-only or shadowed Spectre runtime ' +
        'selection, and incompatible charset forms; it validates ordered CL/LINK option streams, ' +
        'option-specific case rules, semantic LTCG states, both option specifiers, /link boundaries, ' +
        'and fail-closed command files.'
    )
}
finally {
    if (Test-Path -LiteralPath $temporaryRepositoryRoot) {
        Remove-Item -LiteralPath $temporaryRepositoryRoot -Recurse -Force
    }
}
