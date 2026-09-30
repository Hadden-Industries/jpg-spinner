[CmdletBinding()]
param(
    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$jsonObjectMemberValidationModulePath =
    Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1'
Import-Module -Name $jsonObjectMemberValidationModulePath -Force

$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $repositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $repositoryRoot"
}
$policyFailures = [System.Collections.Generic.List[string]]::new()

function Add-PolicyFailure {
    param(
        [Parameter(Mandatory)]
        [string]$Message
    )

    [void]$policyFailures.Add($Message)
}

# Discover the build-language authorities once and retain the immutable result
# for every native policy reader below. Loading Microsoft.Build.dll from the
# selected Visual Studio instance prevents an unrelated SDK or NuGet cache from
# interpreting Directory.Build.props with different construction semantics.
$repositoryBuildToolchain = $null
try {
    $repositoryBuildToolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') `
        -RepositoryRoot $repositoryRoot
    [void][System.Reflection.Assembly]::LoadFrom(
        $repositoryBuildToolchain.msBuildAssemblyPath
    )
}
catch {
    Add-PolicyFailure -Message "Could not resolve the repository build toolchain: $($_.Exception.Message)"
}

function Get-RepositoryPath {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    return Join-Path -Path $repositoryRoot -ChildPath $RelativePath
}

function Read-JsonDocument {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }

    try {
        $jsonText = Get-Content -LiteralPath $path -Raw
        $jsonDocument = ConvertFrom-JsonWithUniqueObjectMembers `
            -JsonText $jsonText `
            -SourceDescription $RelativePath `
            -RequiredRootValueKind Object `
            -AsHashtable `
            -MaximumDepth 100

        return $jsonDocument
    }
    catch {
        Add-PolicyFailure -Message "$RelativePath is not valid JSON: $($_.Exception.Message)"
        return $null
    }
}

function Read-XmlDocument {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }

    try {
        $document = [System.Xml.XmlDocument]::new()
        $document.PreserveWhitespace = $true
        $document.Load($path)
        return $document
    }
    catch {
        Add-PolicyFailure -Message "$RelativePath is not valid XML: $($_.Exception.Message)"
        return $null
    }
}

function Read-EditorConfigDocument {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }

    # EditorConfig is an INI-like format whose preamble and sections have
    # different scopes. Parsing those scopes prevents a correct-looking value
    # in an irrelevant file glob from satisfying repository policy.
    $document = [ordered]@{
        preamble = [ordered]@{}
        sections = [ordered]@{}
    }
    $currentProperties = $document.preamble
    foreach ($line in Get-Content -LiteralPath $path) {
        $trimmedLine = $line.Trim()
        if ($trimmedLine.Length -eq 0 -or $trimmedLine.StartsWith('#') -or $trimmedLine.StartsWith(';')) {
            continue
        }

        if ($trimmedLine -match '^\[(?<section>.+)\]$') {
            $sectionName = $Matches.section
            if ($document.sections.Contains($sectionName)) {
                Add-PolicyFailure -Message "$RelativePath declares duplicate section '[$sectionName]'."
                $currentProperties = $document.sections[$sectionName]
                continue
            }

            $currentProperties = [ordered]@{}
            $document.sections[$sectionName] = $currentProperties
            continue
        }

        if ($trimmedLine -notmatch '^(?<name>[^=:]+?)\s*[=:]\s*(?<value>.*)$') {
            Add-PolicyFailure -Message "$RelativePath contains an invalid non-comment line: '$trimmedLine'."
            continue
        }

        $propertyName = $Matches.name.Trim()
        if ($currentProperties.Contains($propertyName)) {
            Add-PolicyFailure -Message "$RelativePath declares property '$propertyName' more than once in the same scope."
        }
        $currentProperties[$propertyName] = $Matches.value.Trim()
    }

    return $document
}

function Read-SimpleYamlMapping {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }

    # The repository-owned clang files intentionally use only nested YAML
    # mappings. This small parser rejects unsupported constructs instead of
    # pretending to validate YAML shapes it does not understand.
    $settings = [ordered]@{}
    $parentByIndent = @{}
    foreach ($line in Get-Content -LiteralPath $path) {
        if ($line -match '^\s*(?:#.*)?$' -or $line -match '^\s*(?:---|\.\.\.)\s*$') {
            continue
        }
        if ($line -notmatch '^(?<indent> *)(?<name>[A-Za-z0-9_.-]+):(?:\s*(?<value>.*))?$') {
            Add-PolicyFailure -Message "$RelativePath uses unsupported YAML syntax: '$($line.Trim())'."
            continue
        }

        $indent = $Matches.indent.Length
        $propertyName = $Matches.name
        $propertyValue = $Matches.value.Trim()
        foreach ($knownIndent in @($parentByIndent.Keys | Where-Object { [int]$_ -ge $indent })) {
            $parentByIndent.Remove($knownIndent)
        }

        $parentPath = @(
            $parentByIndent.GetEnumerator() |
                Sort-Object { [int]$_.Key } |
                ForEach-Object Value
        )
        $qualifiedPropertyName = (@($parentPath) + $propertyName) -join '.'
        if ($propertyValue.Length -eq 0) {
            $parentByIndent[$indent] = $propertyName
            continue
        }

        if ($settings.Contains($qualifiedPropertyName)) {
            Add-PolicyFailure -Message "$RelativePath declares YAML property '$qualifiedPropertyName' more than once."
        }
        $settings[$qualifiedPropertyName] = $propertyValue.Trim("'").Trim('"')
    }

    return $settings
}

function Assert-ExactValue {
    param(
        [AllowNull()]
        [object]$Actual,

        [Parameter(Mandatory)]
        [object]$Expected,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if ($null -eq $Actual -or [string]$Actual -cne [string]$Expected) {
        Add-PolicyFailure -Message "$Description must be '$Expected'; found '$Actual'."
    }
}

function Read-MSBuildProjectRootElement {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    if ($null -eq $repositoryBuildToolchain) {
        return $null
    }

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }

    try {
        # ProjectRootElement is MSBuild's lossless construction model. Unlike a
        # hand-authored XPath, Properties traverses PropertyGroup declarations
        # beneath Choose/When/Otherwise and therefore sees every declaration
        # that can participate in MSBuild evaluation.
        return [Microsoft.Build.Construction.ProjectRootElement]::Open($path)
    }
    catch {
        Add-PolicyFailure -Message "$RelativePath is not a valid MSBuild project: $($_.Exception.Message)"
        return $null
    }
}

function Get-RequiredSingleRootMSBuildPropertyElement {
    param(
        [Parameter(Mandatory)]
        [object]$ProjectRootElement,

        [Parameter(Mandatory)]
        [string]$PropertyName,

        [Parameter(Mandatory)]
        [string]$Description,

        [Parameter(Mandatory)]
        [string]$SourceDescription,

        [AllowEmptyString()]
        [string]$ExpectedPropertyCondition = '',

        [AllowEmptyString()]
        [string]$ExpectedPropertyGroupCondition = ''
    )

    # MSBuild property identity is case-insensitive, so the policy must reject
    # differently cased duplicates just as the evaluator would consume them.
    $matchingPropertyElements = @(
        $ProjectRootElement.Properties |
            Where-Object {
                [System.StringComparer]::OrdinalIgnoreCase.Equals(
                    $_.Name,
                    $PropertyName
                )
            }
    )
    if ($matchingPropertyElements.Count -ne 1) {
        Add-PolicyFailure -Message (
            "$SourceDescription must declare $Description exactly once; " +
            "found $($matchingPropertyElements.Count) declarations."
        )
        return $null
    }

    $propertyElement = $matchingPropertyElements[0]
    $propertyGroupElement = $propertyElement.Parent
    $isRootPropertyGroup =
        $null -ne $propertyGroupElement -and
        [object]::ReferenceEquals($propertyGroupElement.Parent, $ProjectRootElement)
    if (-not $isRootPropertyGroup) {
        Add-PolicyFailure -Message (
            "$SourceDescription must declare $Description in a root PropertyGroup."
        )
        return $null
    }

    $actualPropertyCondition = [string]$propertyElement.Condition
    if ($actualPropertyCondition -cne $ExpectedPropertyCondition) {
        Add-PolicyFailure -Message (
            "$SourceDescription $Description must use property condition " +
            "'$ExpectedPropertyCondition'; found '$actualPropertyCondition'."
        )
        return $null
    }

    $actualPropertyGroupCondition = [string]$propertyGroupElement.Condition
    if ($actualPropertyGroupCondition -cne $ExpectedPropertyGroupCondition) {
        Add-PolicyFailure -Message (
            "$SourceDescription $Description must use PropertyGroup condition " +
            "'$ExpectedPropertyGroupCondition'; found '$actualPropertyGroupCondition'."
        )
        return $null
    }

    return $propertyElement
}

function Get-RequiredSingleItemDefinitionGroup {
    param(
        [Parameter(Mandatory)]
        [System.Xml.XmlDocument]$Document,

        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$Condition,

        [Parameter(Mandatory)]
        [string]$Description
    )

    # These repository-owned groups intentionally use one canonical condition
    # representation. Exact matching avoids maintaining a second, incomplete
    # parser for MSBuild's condition language; evaluated behavior is covered by
    # Test-ModernCppBuildPolicyScope.ps1.
    $matchingGroups = @(
        $Document.SelectNodes('/Project/ItemDefinitionGroup') |
            Where-Object { $_.GetAttribute('Condition') -ceq $Condition }
    )
    if ($matchingGroups.Count -ne 1) {
        Add-PolicyFailure -Message (
            "Directory.Build.targets must declare exactly one $Description ItemDefinitionGroup " +
            "with condition '$Condition'; found $($matchingGroups.Count)."
        )
        return $null
    }

    return $matchingGroups[0]
}

function Invoke-CMakeScriptJsonTrace {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    if ($null -eq $repositoryBuildToolchain) {
        return @()
    }

    $scriptPath = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
        return @()
    }

    $tracePath = Join-Path (
        [System.IO.Path]::GetTempPath()
    ) "jpg-spinner-cmake-trace-$([guid]::NewGuid().ToString('N')).jsonl"
    try {
        $process = $null
        try {
            # ArgumentList conveys each option to CMake without a shell or a
            # second command-line parser. WorkingDirectory makes repository-
            # relative CMake behavior deterministic even when this policy is
            # launched elsewhere.
            $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
            $startInfo.FileName = $repositoryBuildToolchain.cmakeExecutablePath
            $startInfo.WorkingDirectory = $repositoryRoot
            $startInfo.UseShellExecute = $false
            $startInfo.RedirectStandardOutput = $true
            $startInfo.RedirectStandardError = $true
            foreach ($argument in @(
                '--trace',
                '--trace-format=json-v1',
                "--trace-source=$scriptPath",
                "--trace-redirect=$tracePath",
                '-P',
                $scriptPath
            )) {
                [void]$startInfo.ArgumentList.Add($argument)
            }

            $process = [System.Diagnostics.Process]::new()
            $process.StartInfo = $startInfo
            [void]$process.Start()
            $standardOutputReadTask = $process.StandardOutput.ReadToEndAsync()
            $standardErrorReadTask = $process.StandardError.ReadToEndAsync()
            $process.WaitForExit()
            $standardOutput = $standardOutputReadTask.GetAwaiter().GetResult()
            $standardError = $standardErrorReadTask.GetAwaiter().GetResult()
            if ($process.ExitCode -ne 0) {
                $processDiagnostic = @($standardError.Trim(), $standardOutput.Trim()) `
                    | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
                Add-PolicyFailure -Message (
                    "$RelativePath did not execute successfully under the selected CMake " +
                    "interpreter (exit code $($process.ExitCode)): " +
                    ($processDiagnostic -join ' ')
                )
                return @()
            }
        }
        catch {
            Add-PolicyFailure -Message (
                "Could not trace $RelativePath with the selected CMake interpreter: " +
                $_.Exception.Message
            )
            return @()
        }
        finally {
            if ($null -ne $process) {
                $process.Dispose()
            }
        }

        if (-not (Test-Path -LiteralPath $tracePath -PathType Leaf)) {
            Add-PolicyFailure -Message "$RelativePath did not produce the required CMake JSON trace."
            return @()
        }

        $traceLines = @(Get-Content -LiteralPath $tracePath)
        if ($traceLines.Count -eq 0) {
            Add-PolicyFailure -Message "$RelativePath produced an empty CMake JSON trace."
            return @()
        }

        $traceHeader = ConvertFrom-JsonWithUniqueObjectMembers `
            -JsonText $traceLines[0] `
            -SourceDescription "$RelativePath CMake trace header" `
            -RequiredRootValueKind Object `
            -AsHashtable `
            -MaximumDepth 20
        $traceVersion = $traceHeader['version']
        if (
            $traceVersion -isnot [System.Collections.IDictionary] -or
            $traceVersion['major'] -isnot [long] -or
            $traceVersion['minor'] -isnot [long] -or
            $traceVersion['major'] -ne 1 -or
            $traceVersion['minor'] -lt 2
        ) {
            Add-PolicyFailure -Message (
                "$RelativePath requires a compatible CMake JSON trace format " +
                'with major version 1 and minor version 2 or newer.'
            )
            return @()
        }

        $traceRecords = [System.Collections.Generic.List[object]]::new()
        for ($lineIndex = 1; $lineIndex -lt $traceLines.Count; $lineIndex++) {
            try {
                $traceRecord = ConvertFrom-JsonWithUniqueObjectMembers `
                    -JsonText $traceLines[$lineIndex] `
                    -SourceDescription "$RelativePath CMake trace line $($lineIndex + 1)" `
                    -RequiredRootValueKind Object `
                    -AsHashtable `
                    -MaximumDepth 20
                [void]$traceRecords.Add($traceRecord)
            }
            catch {
                Add-PolicyFailure -Message $_.Exception.Message
            }
        }

        return $traceRecords.ToArray()
    }
    finally {
        # This exact GUID-derived path is the only artifact created by this
        # function. The outer finally owns it across process-start errors,
        # nonzero exits, early returns, and malformed trace data.
        if (Test-Path -LiteralPath $tracePath -PathType Leaf) {
            Remove-Item -LiteralPath $tracePath -Force
        }
    }
}

function Assert-CMakeTraceContainsClosedTripletSettings {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath,

        [Parameter(Mandatory)]
        [ValidateSet('x86', 'x64', 'arm64')]
        [string]$Architecture,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [object[]]$TraceRecords
    )

    $expectedSettings = [ordered]@{
        VCPKG_TARGET_ARCHITECTURE = $Architecture
        VCPKG_CRT_LINKAGE = 'dynamic'
        VCPKG_LIBRARY_LINKAGE = 'static'
        VCPKG_C_FLAGS = '/guard:cf /Qspectre'
        VCPKG_CXX_FLAGS = '/guard:cf /Qspectre'
        VCPKG_LINKER_FLAGS = '/guard:cf'
    }
    $executedSetCommands = [System.Collections.Generic.List[object]]::new()
    foreach ($traceRecord in $TraceRecords) {
        $commandName = $traceRecord['cmd']
        $commandArguments = $traceRecord['args']
        if (
            $commandName -isnot [string] -or
            $commandArguments -isnot [System.Collections.IList] -or
            @($commandArguments | Where-Object { $_ -isnot [string] }).Count -ne 0
        ) {
            Add-PolicyFailure -Message "$RelativePath contains a malformed CMake JSON trace record."
            continue
        }

        try {
            $executedSourcePath = [System.IO.Path]::GetFullPath([string]$traceRecord['file'])
        }
        catch {
            Add-PolicyFailure -Message "$RelativePath contains a CMake trace record with an invalid source path."
            continue
        }
        $expectedSourcePath = [System.IO.Path]::GetFullPath(
            (Get-RepositoryPath -RelativePath $RelativePath)
        )
        if (-not $executedSourcePath.Equals(
            $expectedSourcePath,
            [System.StringComparison]::OrdinalIgnoreCase
        )) {
            Add-PolicyFailure -Message (
                "$RelativePath contains a CMake trace record attributed to " +
                "unexpected source '$executedSourcePath'."
            )
            continue
        }

        if ($commandName -cne 'set') {
            Add-PolicyFailure -Message "$RelativePath executes unsupported CMake command '$commandName'."
            continue
        }
        if ($commandArguments.Count -eq 0) {
            Add-PolicyFailure -Message "$RelativePath executes set() without a variable name."
            continue
        }

        $settingName = [string]$commandArguments[0]
        if ($settingName -cnotin @($expectedSettings.Keys)) {
            Add-PolicyFailure -Message "$RelativePath sets unapproved triplet variable '$settingName'."
            continue
        }
        [void]$executedSetCommands.Add($traceRecord)
    }

    foreach ($expectedSetting in $expectedSettings.GetEnumerator()) {
        $matchingSetCommands = @(
            $executedSetCommands |
                Where-Object {
                    $_['args'].Count -gt 0 -and
                    [string]$_['args'][0] -ceq $expectedSetting.Key
                }
        )
        if ($matchingSetCommands.Count -ne 1) {
            Add-PolicyFailure -Message (
                "$RelativePath must execute exactly one set command for " +
                "$($expectedSetting.Key); found $($matchingSetCommands.Count)."
            )
            continue
        }

        $settingArguments = $matchingSetCommands[0]['args']
        if ($settingArguments.Count -ne 2) {
            Add-PolicyFailure -Message (
                "$RelativePath must execute set($($expectedSetting.Key) <value>) " +
                "with exactly two arguments; found $($settingArguments.Count)."
            )
            continue
        }
        if ([string]$settingArguments[1] -cne [string]$expectedSetting.Value) {
            Add-PolicyFailure -Message (
                "$RelativePath must set $($expectedSetting.Key) to exact value " +
                "'$($expectedSetting.Value)'; found '$($settingArguments[1])'."
            )
        }
    }
}

function Assert-ExactJsonObjectPropertySet {
    param(
        [AllowNull()]
        [object]$JsonObject,

        [Parameter(Mandatory)]
        [string[]]$ExpectedPropertyNames,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if ($JsonObject -isnot [System.Collections.IDictionary]) {
        Add-PolicyFailure -Message "$Description must be a JSON object."
        return
    }

    # vcpkg manifest fields can alter graph selection and version resolution at
    # both the document root and nested entries. Treat every reviewed property
    # set as a closed schema instead of accepting new behavior-bearing fields.
    $actualPropertyNames = @($JsonObject.Keys | ForEach-Object { [string]$_ })
    foreach ($expectedPropertyName in $ExpectedPropertyNames) {
        if ($expectedPropertyName -cnotin $actualPropertyNames) {
            Add-PolicyFailure -Message "$Description lacks required property '$expectedPropertyName'."
        }
    }
    foreach ($actualPropertyName in $actualPropertyNames) {
        if ($actualPropertyName -cnotin $ExpectedPropertyNames) {
            Add-PolicyFailure -Message "$Description contains unapproved property '$actualPropertyName'."
        }
    }
}

function Assert-JsonStringProperty {
    param(
        [AllowNull()]
        [object]$JsonObject,

        [Parameter(Mandatory)]
        [string]$PropertyName,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if (
        $JsonObject -is [System.Collections.IDictionary] -and
        $JsonObject.Contains($PropertyName) -and
        $JsonObject[$PropertyName] -isnot [string]
    ) {
        Add-PolicyFailure -Message "$Description property '$PropertyName' must be a JSON string."
    }
}

function Assert-RequiredFile {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-PolicyFailure -Message "Required configuration file is absent: $RelativePath"
    }
}

function Get-VcpkgManifestEntryName {
    param(
        [AllowNull()]
        [object]$Entry
    )

    # vcpkg permits a dependency to be either a package-name string or an
    # object. Normalize both shapes so malformed and unapproved entries are
    # reported as policy violations instead of causing incidental script errors.
    if ($Entry -is [string]) {
        return [string]$Entry
    }
    if ($Entry -is [System.Collections.IDictionary] -and $Entry.Contains('name')) {
        return [string]$Entry['name']
    }

    return ''
}

function Test-VcpkgResolutionAuthorityConfiguration {
    param(
        [AllowNull()]
        [object]$Configuration,

        [Parameter(Mandatory)]
        [string]$SourceDescription
    )

    if ($Configuration -isnot [System.Collections.IDictionary]) {
        Add-PolicyFailure -Message "$SourceDescription must be a JSON object when present."
        return
    }

    # vcpkg accepts the same resolution configuration beside the manifest or
    # embedded in it. A declared default registry can replace the builtin one,
    # while registries and overlays can supersede selected ports or triplets.
    # This repository approves only the manifest's immutable builtin baseline
    # and its reviewed vcpkg-triplets directory, so every alternate authority
    # surface must remain absent (an explicitly empty array is harmless).
    if ($Configuration.Contains('default-registry')) {
        Add-PolicyFailure -Message (
            "$SourceDescription must not declare 'default-registry'; omit it so " +
            'vcpkg uses the immutable builtin-baseline from vcpkg.json.'
        )
    }

    foreach ($collectionFieldPolicy in @(
        @{ name = 'registries'; description = "additional 'registries'" },
        @{ name = 'overlay-ports'; description = "'overlay-ports'" },
        @{ name = 'overlay-triplets'; description = "'overlay-triplets'" }
    )) {
        if (
            $Configuration.Contains($collectionFieldPolicy.name) -and
            @($Configuration[$collectionFieldPolicy.name]).Count -gt 0
        ) {
            Add-PolicyFailure -Message (
                "$SourceDescription must not declare $($collectionFieldPolicy.description); " +
                'the repository policy permits no alternate vcpkg resolution authority.'
            )
        }
    }
}

function Assert-VcpkgConfigurationRepresentationExclusivity {
    param(
        [Parameter(Mandatory)]
        [System.Collections.IDictionary]$Manifest
    )

    $standaloneConfigurationPath = Get-RepositoryPath -RelativePath 'vcpkg-configuration.json'
    if (-not (Test-Path -LiteralPath $standaloneConfigurationPath -PathType Leaf)) {
        return
    }

    # vcpkg offers standalone and manifest-embedded representations of the
    # same configuration, but does not permit both in one manifest directory.
    # Presence—not non-emptiness—is therefore the relevant structural test.
    foreach ($embeddedConfigurationPropertyName in @('configuration', 'vcpkg-configuration')) {
        if ($Manifest.Contains($embeddedConfigurationPropertyName)) {
            Add-PolicyFailure -Message (
                'vcpkg-configuration.json cannot coexist with vcpkg.json embedded ' +
                "'$embeddedConfigurationPropertyName'."
            )
        }
    }
}

function Test-VisualStudioConfiguration {
    $configuration = Read-JsonDocument -RelativePath '.vsconfig'
    if ($null -eq $configuration) {
        return
    }

    Assert-ExactValue -Actual $configuration.version -Expected '1.0' -Description '.vsconfig version'

    $requiredComponents = @(
        'Microsoft.VisualStudio.Workload.NativeDesktop',
        'Microsoft.VisualStudio.Workload.Universal',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        'Microsoft.VisualStudio.Component.VC.Tools.ARM64',
        'Microsoft.VisualStudio.Component.VC.ASAN',
        'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
        'Microsoft.VisualStudio.Component.VC.CMake.Project',
        'Microsoft.VisualStudio.Component.Vcpkg',
        'Microsoft.VisualStudio.Component.NuGet',
        'Microsoft.VisualStudio.Component.NuGet.BuildTools',
        'Microsoft.VisualStudio.Component.WindowsAppSdkSupport.Cpp',
        'Microsoft.VisualStudio.Component.VC.14.51.x86.x64.Spectre',
        'Microsoft.VisualStudio.Component.VC.14.51.ARM64.Spectre',
        'Microsoft.VisualStudio.Component.Windows11SDK.28000'
    )

    $components = @($configuration.components)
    foreach ($requiredComponent in $requiredComponents) {
        if ($requiredComponent -cnotin $components) {
            Add-PolicyFailure -Message ".vsconfig does not include required stable component '$requiredComponent'."
        }
    }

    foreach ($component in $components) {
        if ([string]$component -match '(?i)(preview|experimental)') {
            Add-PolicyFailure -Message ".vsconfig includes prerelease component '$component'."
        }
        if ([string]$component -match '(?i)(?:^|\.)ARM(?:\.|$)') {
            Add-PolicyFailure -Message ".vsconfig includes unsupported ARM32 component '$component'; only ARM64 is allowed."
        }
    }
}

function Test-ToolchainLock {
    $toolchainLock = Read-JsonDocument -RelativePath 'eng/toolchain-lock.json'
    if ($null -eq $toolchainLock) {
        return
    }

    Assert-ExactValue -Actual $toolchainLock.schemaVersion -Expected 1 -Description 'toolchain lock schemaVersion'
    Assert-ExactValue -Actual $toolchainLock.visualStudioMajorVersion -Expected 18 -Description 'toolchain lock visualStudioMajorVersion'
    Assert-ExactValue -Actual $toolchainLock.platformToolset -Expected 'v145' -Description 'toolchain lock platformToolset'

    $vcToolsVersion = [string]$toolchainLock.vcToolsVersion
    if ($vcToolsVersion -notmatch '^14\.51\.\d+$') {
        Add-PolicyFailure -Message "toolchain lock vcToolsVersion must identify an exact 14.51 servicing directory; found '$vcToolsVersion'."
    }
}

function Test-DirectoryBuildProperties {
    $document = Read-XmlDocument -RelativePath 'Directory.Build.props'
    if ($null -eq $document) {
        return
    }

    $projectRootElement =
        Read-MSBuildProjectRootElement -RelativePath 'Directory.Build.props'

    $modernCppBuildPolicyPropertyName = 'JpgSpinnerModernCppBuildPolicyEnabled'
    $modernCppBuildPolicyDefaultCondition =
        "'`$(JpgSpinnerModernCppBuildPolicyEnabled)' == ''"
    $modernCppBuildPolicyEnabledCondition =
        "'`$(JpgSpinnerModernCppBuildPolicyEnabled)' == 'true'"

    $requiredSelectionProperties = [ordered]@{
        PlatformToolset = 'v145'
        WindowsTargetPlatformMinVersion = '10.0.19045.0'
        WindowsTargetPlatformVersion = '10.0.28000.0'
        WindowsAppSDKSelfContained = 'false'
        EnableNativePackageReferenceSupport = 'true'
        PreferredToolArchitecture = 'x64'
        UseEnv = 'false'
        SpectreMitigation = 'Spectre'
    }
    $vcToolsVersion = $null
    if ($null -ne $projectRootElement) {
        $modernCppBuildPolicyElement = Get-RequiredSingleRootMSBuildPropertyElement `
            -ProjectRootElement $projectRootElement `
            -PropertyName $modernCppBuildPolicyPropertyName `
            -Description $modernCppBuildPolicyPropertyName `
            -SourceDescription 'Directory.Build.props' `
            -ExpectedPropertyCondition $modernCppBuildPolicyDefaultCondition
        if ($null -ne $modernCppBuildPolicyElement) {
            Assert-ExactValue `
                -Actual $modernCppBuildPolicyElement.Value `
                -Expected 'true' `
                -Description $modernCppBuildPolicyPropertyName
        }

        foreach ($requiredSelectionProperty in $requiredSelectionProperties.GetEnumerator()) {
            $selectionPropertyElement = Get-RequiredSingleRootMSBuildPropertyElement `
                -ProjectRootElement $projectRootElement `
                -PropertyName $requiredSelectionProperty.Key `
                -Description $requiredSelectionProperty.Key `
                -SourceDescription 'Directory.Build.props' `
                -ExpectedPropertyGroupCondition $modernCppBuildPolicyEnabledCondition
            if ($null -ne $selectionPropertyElement) {
                Assert-ExactValue `
                    -Actual $selectionPropertyElement.Value `
                    -Expected $requiredSelectionProperty.Value `
                    -Description $requiredSelectionProperty.Key
            }
        }

        $vcToolsVersionElement = Get-RequiredSingleRootMSBuildPropertyElement `
            -ProjectRootElement $projectRootElement `
            -PropertyName 'VCToolsVersion' `
            -Description 'VCToolsVersion' `
            -SourceDescription 'Directory.Build.props' `
            -ExpectedPropertyGroupCondition $modernCppBuildPolicyEnabledCondition
        if ($null -ne $vcToolsVersionElement) {
            $vcToolsVersion = $vcToolsVersionElement.Value
            if ($vcToolsVersion -notmatch '^14\.51\.\d+$') {
                Add-PolicyFailure -Message "Directory.Build.props VCToolsVersion must identify an exact 14.51 servicing directory; found '$vcToolsVersion'."
            }
        }

        # Import vcpkg once at the repository boundary, using the instance
        # selected by MSBuild itself. This is the official project-local
        # integration path and does not depend on `vcpkg integrate install`.
        foreach ($requiredVcpkgProperty in ([ordered]@{
            VcpkgRoot = '$(VsInstallRoot)\VC\vcpkg\'
            VcpkgEnableClassic = 'false'
            VcpkgEnableManifest = 'true'
            VcpkgManifestInstall = 'true'
            VcpkgAutoBootstrap = 'false'
            VcpkgManifestRoot = '$(MSBuildThisFileDirectory)'
            VcpkgApplocalDeps = 'false'
            VcpkgAdditionalInstallOptions = '--overlay-triplets="$(MSBuildThisFileDirectory)vcpkg-triplets"'
        }).GetEnumerator()) {
            $vcpkgPropertyElement = Get-RequiredSingleRootMSBuildPropertyElement `
                -ProjectRootElement $projectRootElement `
                -PropertyName $requiredVcpkgProperty.Key `
                -Description $requiredVcpkgProperty.Key `
                -SourceDescription 'Directory.Build.props' `
                -ExpectedPropertyGroupCondition $modernCppBuildPolicyEnabledCondition
            if ($null -ne $vcpkgPropertyElement) {
                Assert-ExactValue `
                    -Actual $vcpkgPropertyElement.Value `
                    -Expected $requiredVcpkgProperty.Value `
                    -Description $requiredVcpkgProperty.Key
            }
        }

        $expectedTripletByPropertyCondition = [ordered]@{
            "'`$(Platform)' == 'Win32'" = 'x86-windows-static-md'
            "'`$(Platform)' == 'x64'" = 'x64-windows-static-md'
            "'`$(Platform)' == 'ARM64'" = 'arm64-windows-static-md'
        }
        $tripletElements = @(
            $projectRootElement.Properties |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.Name,
                        'VcpkgTriplet'
                    )
                }
        )
        foreach ($expectedTriplet in $expectedTripletByPropertyCondition.GetEnumerator()) {
            $matchingTripletElements = @(
                $tripletElements |
                    Where-Object {
                        $_.Condition -ceq $expectedTriplet.Key -and
                        $_.Parent.Condition -ceq $modernCppBuildPolicyEnabledCondition
                    }
            )
            if (
                $matchingTripletElements.Count -ne 1 -or
                $matchingTripletElements[0].Value -cne $expectedTriplet.Value
            ) {
                Add-PolicyFailure -Message (
                    "Directory.Build.props must map $($expectedTriplet.Key) to " +
                    "'$($expectedTriplet.Value)' exactly once in the modern policy group."
                )
            }
        }
        if ($tripletElements.Count -ne $expectedTripletByPropertyCondition.Count) {
            Add-PolicyFailure -Message (
                'Directory.Build.props must contain only the three reviewed ' +
                'platform-specific VcpkgTriplet mappings.'
            )
        }

        $vcpkgPropsImports = @(
            $projectRootElement.Imports |
                Where-Object {
                    $_.Project -match '(?i)vcpkg\.props$'
                }
        )
        $expectedVcpkgImportCondition =
            "$modernCppBuildPolicyEnabledCondition and '`$(VcpkgEnabled)' != 'false'"
        if (
            $vcpkgPropsImports.Count -ne 1 -or
            $vcpkgPropsImports[0].Project -cne
                '$(VcpkgRoot)scripts\buildsystems\msbuild\vcpkg.props' -or
            $vcpkgPropsImports[0].Condition -cne $expectedVcpkgImportCondition
        ) {
            Add-PolicyFailure -Message (
                'Directory.Build.props must import the selected Visual Studio ' +
                'vcpkg.props exactly once for enabled modern projects.'
            )
        }
    }

    $toolchainLock = Read-JsonDocument -RelativePath 'eng/toolchain-lock.json'
    if (
        $null -ne $toolchainLock -and
        $null -ne $projectRootElement -and
        [string]$toolchainLock.vcToolsVersion -cne [string]$vcToolsVersion
    ) {
        Add-PolicyFailure -Message 'Directory.Build.props VCToolsVersion does not match eng/toolchain-lock.json.'
    }

    $misplacedMetadataNames = @(
        'LanguageStandard',
        'ConformanceMode',
        'WarningLevel',
        'TreatWarningAsError',
        'SDLCheck',
        'ControlFlowGuard',
        'Optimization',
        'WholeProgramOptimization',
        'LinkTimeCodeGeneration'
    )
    foreach ($metadataName in $misplacedMetadataNames) {
        if ($null -ne $document.SelectSingleNode("/Project/PropertyGroup/$metadataName")) {
            Add-PolicyFailure -Message "$metadataName is compiler/linker metadata and must not be declared as a free property in Directory.Build.props."
        }
    }
}

function Test-DirectoryBuildTargets {
    $document = Read-XmlDocument -RelativePath 'Directory.Build.targets'
    if ($null -eq $document) {
        return
    }

    $modernCppBuildPolicyEnabledCondition =
        "'`$(JpgSpinnerModernCppBuildPolicyEnabled)' == 'true'"
    $basePolicyGroup = Get-RequiredSingleItemDefinitionGroup `
        -Document $document `
        -Condition $modernCppBuildPolicyEnabledCondition `
        -Description 'modern C++ base policy'
    $compilerDefinition = if ($null -eq $basePolicyGroup) {
        $null
    }
    else {
        $basePolicyGroup.SelectSingleNode('ClCompile')
    }
    $linkerDefinition = if ($null -eq $basePolicyGroup) {
        $null
    }
    else {
        $basePolicyGroup.SelectSingleNode('Link')
    }
    if ($null -eq $compilerDefinition) {
        Add-PolicyFailure -Message 'Directory.Build.targets lacks its scoped modern C++ ClCompile item definition.'
    }
    else {
        Assert-ExactValue -Actual $compilerDefinition.LanguageStandard -Expected 'stdcpp20' -Description 'ClCompile LanguageStandard'
        Assert-ExactValue -Actual $compilerDefinition.ConformanceMode -Expected 'true' -Description 'ClCompile ConformanceMode'
        Assert-ExactValue -Actual $compilerDefinition.WarningLevel -Expected 'Level4' -Description 'ClCompile WarningLevel'
        Assert-ExactValue -Actual $compilerDefinition.TreatWarningAsError -Expected 'true' -Description 'ClCompile TreatWarningAsError'
        Assert-ExactValue -Actual $compilerDefinition.SDLCheck -Expected 'true' -Description 'ClCompile SDLCheck'

        $compilerOptions = [string]$compilerDefinition.AdditionalOptions
        foreach ($requiredOption in @('/utf-8', '/Zc:__cplusplus', '/guard:cf', '/Brepro', '%(AdditionalOptions)')) {
            if ($compilerOptions -notmatch [regex]::Escape($requiredOption)) {
                Add-PolicyFailure -Message "ClCompile AdditionalOptions lacks '$requiredOption'."
            }
        }
    }

    if ($null -eq $linkerDefinition) {
        Add-PolicyFailure -Message 'Directory.Build.targets lacks its scoped modern C++ Link item definition.'
    }
    else {
        $linkerOptions = [string]$linkerDefinition.AdditionalOptions
        foreach ($requiredOption in @('/guard:cf', '/Brepro', '%(AdditionalOptions)')) {
            if ($linkerOptions -notmatch [regex]::Escape($requiredOption)) {
                Add-PolicyFailure -Message "Link AdditionalOptions lacks '$requiredOption'."
            }
        }

        if ($linkerOptions -match '(?i)/CETCOMPAT') {
            Add-PolicyFailure -Message '/CETCOMPAT must not appear in an unconditional Link item definition.'
        }
    }

    $x64PolicyCondition =
        "$modernCppBuildPolicyEnabledCondition and '`$(Platform)' == 'x64'"
    $x64PolicyGroup = Get-RequiredSingleItemDefinitionGroup `
        -Document $document `
        -Condition $x64PolicyCondition `
        -Description 'modern C++ x64 policy'
    $cetDefinitions = @(
        if ($null -ne $x64PolicyGroup) {
            $x64PolicyGroup.SelectNodes('Link/AdditionalOptions[contains(translate(., "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "/CETCOMPAT")]')
        }
    )
    if ($cetDefinitions.Count -ne 1) {
        Add-PolicyFailure -Message "Directory.Build.targets must contain exactly one conditional /CETCOMPAT link definition; found $($cetDefinitions.Count)."
    }
    elseif ([string]$cetDefinitions[0].InnerText -notmatch [regex]::Escape('%(AdditionalOptions)')) {
        Add-PolicyFailure -Message 'The x64 /CETCOMPAT definition must preserve inherited AdditionalOptions.'
    }

    $releasePolicyCondition =
        "$modernCppBuildPolicyEnabledCondition and '`$(Configuration)' == 'Release'"
    $releasePolicyGroup = Get-RequiredSingleItemDefinitionGroup `
        -Document $document `
        -Condition $releasePolicyCondition `
        -Description 'modern C++ Release policy'
    $releaseCompilerDefinition = if ($null -eq $releasePolicyGroup) {
        $null
    }
    else {
        $releasePolicyGroup.SelectSingleNode('ClCompile')
    }
    $releaseLinkerDefinition = if ($null -eq $releasePolicyGroup) {
        $null
    }
    else {
        $releasePolicyGroup.SelectSingleNode('Link')
    }
    $releaseOptimization = if ($null -eq $releaseCompilerDefinition) { $null } else { $releaseCompilerDefinition.Optimization }
    $releaseWholeProgramOptimization = if ($null -eq $releaseCompilerDefinition) { $null } else { $releaseCompilerDefinition.WholeProgramOptimization }
    $releaseLinkTimeCodeGeneration = if ($null -eq $releaseLinkerDefinition) { $null } else { $releaseLinkerDefinition.LinkTimeCodeGeneration }
    Assert-ExactValue -Actual $releaseOptimization -Expected 'MaxSpeed' -Description 'Release ClCompile Optimization'
    Assert-ExactValue -Actual $releaseWholeProgramOptimization -Expected 'true' -Description 'Release ClCompile WholeProgramOptimization'
    Assert-ExactValue -Actual $releaseLinkTimeCodeGeneration -Expected 'UseLinkTimeCodeGeneration' -Description 'Release Link LinkTimeCodeGeneration'

    $debugPolicyCondition =
        "$modernCppBuildPolicyEnabledCondition and '`$(Configuration)' == 'Debug'"
    $debugPolicyGroup = Get-RequiredSingleItemDefinitionGroup `
        -Document $document `
        -Condition $debugPolicyCondition `
        -Description 'modern C++ Debug policy'
    $debugCompilerDefinition = if ($null -eq $debugPolicyGroup) {
        $null
    }
    else {
        $debugPolicyGroup.SelectSingleNode('ClCompile')
    }
    $debugInformationFormat = if ($null -eq $debugCompilerDefinition) { $null } else { $debugCompilerDefinition.DebugInformationFormat }
    Assert-ExactValue -Actual $debugInformationFormat -Expected 'ProgramDatabase' -Description 'Debug ClCompile DebugInformationFormat'

    $misplacedMetadata = $document.SelectNodes('/Project/PropertyGroup/LanguageStandard | /Project/PropertyGroup/ConformanceMode | /Project/PropertyGroup/WarningLevel | /Project/PropertyGroup/TreatWarningAsError | /Project/PropertyGroup/SDLCheck | /Project/PropertyGroup/ControlFlowGuard | /Project/PropertyGroup/Optimization | /Project/PropertyGroup/WholeProgramOptimization | /Project/PropertyGroup/LinkTimeCodeGeneration')
    foreach ($metadata in $misplacedMetadata) {
        Add-PolicyFailure -Message "$($metadata.Name) is compiler/linker metadata and must be declared under ItemDefinitionGroup."
    }

    $compilerPolicyNodes = @($document.SelectNodes('/Project/ItemDefinitionGroup/ClCompile/*'))
    foreach ($compilerPolicyNode in $compilerPolicyNodes) {
        if ([string]$compilerPolicyNode.InnerText -match '(?i)/std:c\+\+latest|stdcpplatest') {
            Add-PolicyFailure -Message "Directory.Build.targets contains forbidden floating language mode '/std:c++latest'."
        }
    }

    $projectRootElement =
        Read-MSBuildProjectRootElement -RelativePath 'Directory.Build.targets'
    if ($null -ne $projectRootElement) {
        $vcpkgTargetsImports = @(
            $projectRootElement.Imports |
                Where-Object {
                    $_.Project -match '(?i)vcpkg\.targets$'
                }
        )
        $expectedVcpkgImportCondition =
            "$modernCppBuildPolicyEnabledCondition and '`$(VcpkgEnabled)' != 'false'"
        if (
            $vcpkgTargetsImports.Count -ne 1 -or
            $vcpkgTargetsImports[0].Project -cne
                '$(VcpkgRoot)scripts\buildsystems\msbuild\vcpkg.targets' -or
            $vcpkgTargetsImports[0].Condition -cne $expectedVcpkgImportCondition
        ) {
            Add-PolicyFailure -Message (
                'Directory.Build.targets must import the selected Visual Studio ' +
                'vcpkg.targets exactly once for enabled modern projects.'
            )
        }
    }
}

function Test-BuildPolicyProbeProject {
    $relativeProjectPath = 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
    $document = Read-XmlDocument -RelativePath $relativeProjectPath
    $projectRootElement =
        Read-MSBuildProjectRootElement -RelativePath $relativeProjectPath
    if ($null -eq $document -or $null -eq $projectRootElement) {
        return
    }

    $platformNodes = @(
        $document.SelectNodes(
            '/*[local-name()="Project"]/*[local-name()="ItemGroup"]/*[local-name()="ProjectConfiguration"]/*[local-name()="Platform"]'
        )
    )
    foreach ($platformNode in $platformNodes) {
        if ([string]$platformNode.InnerText -ceq 'ARM') {
            Add-PolicyFailure -Message 'BuildPolicyProbe.vcxproj includes unsupported ARM32 platform ARM; only ARM64 is allowed.'
        }
    }


    $vcpkgParticipationProperty = Get-RequiredSingleRootMSBuildPropertyElement `
        -ProjectRootElement $projectRootElement `
        -PropertyName 'VcpkgEnabled' `
        -Description 'VcpkgEnabled' `
        -SourceDescription $relativeProjectPath
    if ($null -ne $vcpkgParticipationProperty) {
        Assert-ExactValue `
            -Actual $vcpkgParticipationProperty.Value `
            -Expected 'false' `
            -Description 'dependency-free build-policy probe vcpkg participation'
        if ([string]$vcpkgParticipationProperty.Parent.Label -cne 'Globals') {
            Add-PolicyFailure -Message (
                "$relativeProjectPath must declare VcpkgEnabled in its Globals PropertyGroup."
            )
        }

        $firstImport = @($projectRootElement.Imports)[0]
        if (
            $null -eq $firstImport -or
            $vcpkgParticipationProperty.Location.Line -ge $firstImport.Location.Line
        ) {
            Add-PolicyFailure -Message (
                "$relativeProjectPath must disable vcpkg before its first MSBuild import."
            )
        }
    }
}

function Test-LegacyCppCxProjectBuildPolicyScope {
    $relativeProjectPath = 'JPG Spinner/JPG Spinner.vcxproj'
    $projectRootElement =
        Read-MSBuildProjectRootElement -RelativePath $relativeProjectPath
    if ($null -eq $projectRootElement) {
        return
    }

    $policyParticipationProperty = Get-RequiredSingleRootMSBuildPropertyElement `
        -ProjectRootElement $projectRootElement `
        -PropertyName 'JpgSpinnerModernCppBuildPolicyEnabled' `
        -Description 'JpgSpinnerModernCppBuildPolicyEnabled' `
        -SourceDescription $relativeProjectPath
    if ($null -eq $policyParticipationProperty) {
        return
    }

    Assert-ExactValue `
        -Actual $policyParticipationProperty.Value `
        -Expected 'false' `
        -Description 'legacy C++/CX modern build-policy participation'
    if ([string]$policyParticipationProperty.Parent.Label -cne 'Globals') {
        Add-PolicyFailure -Message (
            "$relativeProjectPath must declare JpgSpinnerModernCppBuildPolicyEnabled " +
            'in its Globals PropertyGroup.'
        )
    }

    $firstImport = @($projectRootElement.Imports)[0]
    if (
        $null -eq $firstImport -or
        $policyParticipationProperty.Location.Line -ge $firstImport.Location.Line
    ) {
        Add-PolicyFailure -Message (
            "$relativeProjectPath must opt out of the modern C++ build policy before " +
            'its first MSBuild import.'
        )
    }
}

function Get-RepositoryRelativePath {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    return [System.IO.Path]::GetRelativePath($repositoryRoot, $fullPath).Replace('\', '/')
}

function Test-OrdinalIgnoreCaseIdentityCollectionContains {
    param(
        [Parameter(Mandatory)]
        [System.Collections.IEnumerable]$Identities,

        [Parameter(Mandatory)]
        [string]$Candidate
    )

    foreach ($identity in $Identities) {
        if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
                [string]$identity,
                $Candidate
            )) {
            return $true
        }
    }
    return $false
}

function Test-ModernSolutionArchitecture {
    # These approved standalone targets instrument only x64 code. They are
    # visible in the solution but never built by an ordinary Debug/Release run.
    $fuzzProjectPaths = @(
        'fuzz/JpgSpinner.FuzzToolchain.Smoke/JpgSpinner.FuzzToolchain.Smoke.vcxproj',
        'fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/JpgSpinner.JpegSegmentScanner.Fuzz.vcxproj'
    )
    # The reference graph is deliberately closed. A project may depend on the
    # exact lower-level capabilities listed here and nothing else; keeping the
    # graph as data makes additions visible during review instead of silently
    # accepting every syntactically valid ProjectReference.
    $expectedProjects = [ordered]@{
        'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj' = [pscustomobject]@{
            configurationType = 'StaticLibrary'
            references = @()
        }
        'src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj' = [pscustomobject]@{
            configurationType = 'StaticLibrary'
            references = @('src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj')
        }
        'src/JpgSpinner.WindowsStorage/JpgSpinner.WindowsStorage.vcxproj' = [pscustomobject]@{
            configurationType = 'StaticLibrary'
            references = @('src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj')
        }
        'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj' = [pscustomobject]@{
            configurationType = 'StaticLibrary'
            references = @(
                'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj',
                'src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj'
            )
        }
        'src/JpgSpinner.App/JpgSpinner.App.vcxproj' = [pscustomobject]@{
            configurationType = 'Application'
            references = @(
                'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj',
                'src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj',
                'src/JpgSpinner.WindowsStorage/JpgSpinner.WindowsStorage.vcxproj',
                'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj'
            )
        }
        'tests/TestSupport/TestSupport.vcxproj' = [pscustomobject]@{
            configurationType = 'StaticLibrary'
            references = @()
        }
        'tests/JpgSpinner.Domain.Tests/JpgSpinner.Domain.Tests.vcxproj' = [pscustomobject]@{
            configurationType = 'Application'
            references = @(
                'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj',
                'tests/TestSupport/TestSupport.vcxproj'
            )
        }
        'tests/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.vcxproj' = [pscustomobject]@{
            configurationType = 'Application'
            references = @(
                'src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj',
                'tests/TestSupport/TestSupport.vcxproj'
            )
        }
        'tests/JpgSpinner.WindowsStorage.Tests/JpgSpinner.WindowsStorage.Tests.vcxproj' = [pscustomobject]@{
            configurationType = 'Application'
            references = @(
                'src/JpgSpinner.WindowsStorage/JpgSpinner.WindowsStorage.vcxproj',
                'tests/TestSupport/TestSupport.vcxproj'
            )
        }
        'tests/JpgSpinner.BatchProcessing.Tests/JpgSpinner.BatchProcessing.Tests.vcxproj' = [pscustomobject]@{
            configurationType = 'Application'
            references = @(
                'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj',
                'tests/TestSupport/TestSupport.vcxproj'
            )
        }
        'tests/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.vcxproj' = [pscustomobject]@{
            configurationType = 'Application'
            references = @(
                'src/JpgSpinner.App/JpgSpinner.App.vcxproj',
                'tests/TestSupport/TestSupport.vcxproj'
            )
        }
    }
    $expectedConfigurationNames = @(
        'Debug|Win32',
        'Debug|x64',
        'Debug|ARM64',
        'Release|Win32',
        'Release|x64',
        'Release|ARM64'
    )
    foreach ($fuzzProjectPath in $fuzzProjectPaths) {
        $expectedProjects[$fuzzProjectPath] = [pscustomobject]@{
            configurationType = 'Application'
            references = @()
        }
    }

    $solutionPath = Get-RepositoryPath -RelativePath 'JpgSpinner.sln'
    if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
        return
    }
    if ($null -eq $repositoryBuildToolchain) {
        return
    }

    try {
        # SolutionFile is MSBuild's parser for the Visual Studio solution
        # format. It preserves configuration mappings and project identities
        # without duplicating that format in repository-owned PowerShell.
        $solution = [Microsoft.Build.Construction.SolutionFile]::Parse($solutionPath)
    }
    catch {
        Add-PolicyFailure -Message "JpgSpinner.sln is not a valid Visual Studio solution: $($_.Exception.Message)"
        return
    }

    $solutionProjectEntries = @(
        $solution.ProjectsInOrder |
            Where-Object ProjectType -EQ ([Microsoft.Build.Construction.SolutionProjectType]::KnownToBeMSBuildFormat) |
            ForEach-Object {
                [pscustomobject]@{
                    Model = $_
                    RelativePath = Get-RepositoryRelativePath -Path (
                        Join-Path $repositoryRoot $_.RelativePath
                    )
                }
            }
    )
    $solutionProjectPaths = @($solutionProjectEntries | ForEach-Object RelativePath)
    foreach ($expectedProjectPath in $expectedProjects.Keys) {
        $matchingProjectEntries = @(
            $solutionProjectEntries |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.RelativePath,
                        $expectedProjectPath
                    )
                }
        )
        $matchingProjectCount = $matchingProjectEntries.Count
        if ($matchingProjectCount -ne 1) {
            Add-PolicyFailure -Message (
                "JpgSpinner.sln must contain project '$expectedProjectPath' exactly once; " +
                "found $matchingProjectCount entries."
            )
            continue
        }

        # ProjectConfigurations is MSBuild's authoritative solution mapping
        # model. Validate both ActiveCfg (FullName) and Build.0
        # (IncludeInBuild) instead of reparsing the .sln text.
        $isFuzzProject = $fuzzProjectPaths -ccontains $expectedProjectPath
        $requiredMappingNames = if ($isFuzzProject) { @('Debug|x64', 'Release|x64') } else { $expectedConfigurationNames }
        if ($isFuzzProject) {
            foreach ($mapping in $matchingProjectEntries[0].Model.ProjectConfigurations.Values) {
                if ($mapping.IncludeInBuild) {
                    Add-PolicyFailure -Message "$expectedProjectPath must not participate in ordinary solution builds."
                }
            }
        }
        foreach ($expectedConfigurationName in $requiredMappingNames) {
            $matchingConfigurationMappings = @(
                $matchingProjectEntries[0].Model.ProjectConfigurations.GetEnumerator() |
                    Where-Object {
                        [System.StringComparer]::OrdinalIgnoreCase.Equals(
                            $_.Key,
                            $expectedConfigurationName
                        )
                    }
            )
            if ($matchingConfigurationMappings.Count -ne 1) {
                Add-PolicyFailure -Message (
                    "$expectedProjectPath must map solution configuration " +
                    "'$expectedConfigurationName' exactly once."
                )
                continue
            }

            $projectConfigurationMapping = $matchingConfigurationMappings[0].Value
            $requiredMappedConfiguration = if ($isFuzzProject) { 'Fuzz|x64' } else { $expectedConfigurationName }
            if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
                    $projectConfigurationMapping.FullName,
                    $requiredMappedConfiguration
                )) {
                Add-PolicyFailure -Message (
                    "$expectedProjectPath maps solution configuration " +
                    "'$expectedConfigurationName' to '$($projectConfigurationMapping.FullName)'."
                )
            }
            if (-not $isFuzzProject -and -not $projectConfigurationMapping.IncludeInBuild) {
                Add-PolicyFailure -Message (
                    "$expectedProjectPath is excluded from solution configuration " +
                    "'$expectedConfigurationName'."
                )
            }
        }
    }
    foreach ($solutionProjectPath in $solutionProjectPaths) {
        if (-not (Test-OrdinalIgnoreCaseIdentityCollectionContains `
                -Identities $expectedProjects.Keys `
                -Candidate $solutionProjectPath)) {
            Add-PolicyFailure -Message "JpgSpinner.sln contains unapproved project '$solutionProjectPath'."
        }
    }

    $solutionConfigurationNames = @(
        $solution.SolutionConfigurations | ForEach-Object FullName
    )
    foreach ($expectedConfigurationName in $expectedConfigurationNames) {
        if ($expectedConfigurationName -cnotin $solutionConfigurationNames) {
            Add-PolicyFailure -Message "JpgSpinner.sln is missing solution configuration '$expectedConfigurationName'."
        }
    }
    foreach ($solutionConfigurationName in $solutionConfigurationNames) {
        if ($solutionConfigurationName -cnotin $expectedConfigurationNames) {
            Add-PolicyFailure -Message "JpgSpinner.sln contains unsupported solution configuration '$solutionConfigurationName'."
        }
    }

    $msixPackagingProjectCount = 0
    foreach ($expectedProject in $expectedProjects.GetEnumerator()) {
        $relativeProjectPath = $expectedProject.Key
        $requiredProjectConfigurationNames = if ($fuzzProjectPaths -ccontains $relativeProjectPath) {
            @('Fuzz|x64')
        } else {
            $expectedConfigurationNames
        }
        $projectRootElement = Read-MSBuildProjectRootElement -RelativePath $relativeProjectPath
        if ($null -eq $projectRootElement) {
            continue
        }

        $projectConfigurationNames = @(
            $projectRootElement.Items |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.ItemType,
                        'ProjectConfiguration'
                    )
                } |
                ForEach-Object Include
        )
        foreach ($expectedConfigurationName in $requiredProjectConfigurationNames) {
            if (-not (Test-OrdinalIgnoreCaseIdentityCollectionContains `
                    -Identities $projectConfigurationNames `
                    -Candidate $expectedConfigurationName)) {
                Add-PolicyFailure -Message "$relativeProjectPath is missing project configuration '$expectedConfigurationName'."
            }
        }
        foreach ($projectConfigurationName in $projectConfigurationNames) {
            if (-not (Test-OrdinalIgnoreCaseIdentityCollectionContains `
                    -Identities $requiredProjectConfigurationNames `
                    -Candidate $projectConfigurationName)) {
                Add-PolicyFailure -Message "$relativeProjectPath contains unsupported project configuration '$projectConfigurationName'."
            }
        }

        $configurationTypeValues = @(
            $projectRootElement.Properties |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.Name,
                        'ConfigurationType'
                    )
                } |
                ForEach-Object Value
        )
        if (
            $configurationTypeValues.Count -ne 1 -or
            $configurationTypeValues[0] -cne $expectedProject.Value.configurationType
        ) {
            Add-PolicyFailure -Message (
                "$relativeProjectPath must declare ConfigurationType " +
                "'$($expectedProject.Value.configurationType)' exactly once."
            )
        }

        # Root policy owns these values. Repeating one after the shared props
        # import can mask the reviewed selection even when it happens to use
        # the same text today.
        foreach ($rootOwnedPropertyName in @(
            'PlatformToolset',
            'VCToolsVersion',
            'WindowsTargetPlatformVersion',
            'UseEnv',
            'VcpkgEnabled',
            'VcpkgRoot',
            'VcpkgEnableClassic',
            'VcpkgEnableManifest',
            'VcpkgManifestInstall',
            'VcpkgAutoBootstrap',
            'VcpkgManifestRoot',
            'VcpkgApplocalDeps',
            'VcpkgAdditionalInstallOptions',
            'VcpkgTriplet'
        )) {
            $projectOwnedSelections = @(
                $projectRootElement.Properties |
                    Where-Object {
                        [System.StringComparer]::OrdinalIgnoreCase.Equals(
                            $_.Name,
                            $rootOwnedPropertyName
                        )
                    }
            )
            if ($projectOwnedSelections.Count -ne 0) {
                Add-PolicyFailure -Message (
                    "$relativeProjectPath must not redeclare root-owned property " +
                    "'$rootOwnedPropertyName'."
                )
            }
        }

        $projectDirectory = Split-Path -Parent (Get-RepositoryPath -RelativePath $relativeProjectPath)
        $actualReferencePaths = @(
            $projectRootElement.Items |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.ItemType,
                        'ProjectReference'
                    )
                } |
                ForEach-Object {
                    Get-RepositoryRelativePath -Path (
                        Join-Path $projectDirectory $_.Include
                    )
                }
        )
        foreach ($expectedReferencePath in $expectedProject.Value.references) {
            $matchingReferenceCount = @(
                $actualReferencePaths |
                    Where-Object {
                        [System.StringComparer]::OrdinalIgnoreCase.Equals($_, $expectedReferencePath)
                    }
            ).Count
            if ($matchingReferenceCount -ne 1) {
                Add-PolicyFailure -Message (
                    "$relativeProjectPath must reference '$expectedReferencePath' exactly once; " +
                    "found $matchingReferenceCount entries."
                )
            }
        }
        foreach ($actualReferencePath in $actualReferencePaths) {
            if (-not (Test-OrdinalIgnoreCaseIdentityCollectionContains `
                    -Identities $expectedProject.Value.references `
                    -Candidate $actualReferencePath)) {
                Add-PolicyFailure -Message "$relativeProjectPath contains unapproved project reference '$actualReferencePath'."
            }
        }

        $appxPackageValues = @(
            $projectRootElement.Properties |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.Name,
                        'AppxPackage'
                    )
                } |
                ForEach-Object Value
        )
        $isMsixPackagingProject = $appxPackageValues -ccontains 'true'
        if ($isMsixPackagingProject) {
            ++$msixPackagingProjectCount
        }

        if ($relativeProjectPath -ceq 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') {
            foreach ($requiredAppProperty in ([ordered]@{
                AppContainerApplication = 'false'
                AppxPackage = 'true'
                ApplicationType = 'Windows Store'
                ApplicationTypeRevision = '10.0'
                UseWinUI = 'true'
                WinUISDKReferences = 'false'
                EnableMsixTooling = 'true'
                RuntimeIdentifiers = 'win;win-x86;win-x64;win-arm64'
                WindowsAppSDKSelfContained = $null
            }).GetEnumerator()) {
                # WindowsAppSDKSelfContained is intentionally absent here: its
                # framework-dependent false value is owned by root policy.
                $matchingProperties = @(
                    $projectRootElement.Properties |
                        Where-Object {
                            [System.StringComparer]::OrdinalIgnoreCase.Equals(
                                $_.Name,
                                $requiredAppProperty.Key
                            )
                        }
                )
                if ($null -eq $requiredAppProperty.Value) {
                    if ($matchingProperties.Count -ne 0) {
                        Add-PolicyFailure -Message (
                            "$relativeProjectPath must inherit root-owned property " +
                            "'$($requiredAppProperty.Key)'."
                        )
                    }
                }
                elseif (
                    $matchingProperties.Count -ne 1 -or
                    $matchingProperties[0].Value -cne $requiredAppProperty.Value
                ) {
                    Add-PolicyFailure -Message (
                        "$relativeProjectPath must declare $($requiredAppProperty.Key) " +
                        "'$($requiredAppProperty.Value)' exactly once."
                    )
                }
            }
        }

        $windowsPackageTypeValues = @(
            $projectRootElement.Properties |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.Name,
                        'WindowsPackageType'
                    )
                } |
                ForEach-Object Value
        )
        $windowsAppSdkReferences = @(
            $projectRootElement.Items |
                Where-Object {
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.ItemType,
                        'PackageReference'
                    ) -and
                    [System.StringComparer]::OrdinalIgnoreCase.Equals(
                        $_.Include,
                        'Microsoft.WindowsAppSDK'
                    )
                }
        )
        if ($relativeProjectPath.StartsWith('src/', [System.StringComparison]::Ordinal)) {
            if ($windowsPackageTypeValues -ccontains 'None') {
                Add-PolicyFailure -Message (
                    "$relativeProjectPath is production code and must not enable the " +
                    'Windows App SDK unpackaged bootstrapper.'
                )
            }
        }
        elseif (
            $windowsAppSdkReferences.Count -gt 0 -and
            -not $isMsixPackagingProject -and
            $windowsPackageTypeValues -cnotcontains 'None'
        ) {
            Add-PolicyFailure -Message (
                "$relativeProjectPath directly consumes Windows App SDK runtime types " +
                "without package identity or the official WindowsPackageType=None test bootstrapper."
            )
        }
    }

    if ($msixPackagingProjectCount -ne 1) {
        Add-PolicyFailure -Message (
            'JpgSpinner.sln must contain exactly one single-project MSIX packaging project; ' +
            "found $msixPackagingProjectCount."
        )
    }

    # Packaged production startup receives its framework dependency through the
    # package graph. Direct bootstrapper calls would create a second, conflicting
    # initialization path and are reserved for explicitly unpackaged tests.
    $productionSourceRoot = Get-RepositoryPath -RelativePath 'src'
    if (Test-Path -LiteralPath $productionSourceRoot -PathType Container) {
        foreach ($productionSourceFile in Get-ChildItem -LiteralPath $productionSourceRoot -Recurse -File) {
            if ($productionSourceFile.Extension -cnotin @('.cpp', '.cxx', '.h', '.hpp', '.ixx')) {
                continue
            }
            $sourceText = [System.IO.File]::ReadAllText($productionSourceFile.FullName)
            if ($sourceText -match '(?i)\bMddBootstrap(?:Initialize2?|Shutdown)\b') {
                Add-PolicyFailure -Message (
                    "$(Get-RepositoryRelativePath -Path $productionSourceFile.FullName) " +
                    'must not call the Windows App SDK bootstrapper from packaged production code.'
                )
            }
        }
    }
}

function Test-NuGetConfiguration {
    $document = Read-XmlDocument -RelativePath 'NuGet.config'
    if ($null -eq $document) {
        return
    }

    $sourceNodes = @($document.SelectNodes('/configuration/packageSources/*'))
    $clearNodes = @($sourceNodes | Where-Object Name -CEQ 'clear')
    $addNodes = @($sourceNodes | Where-Object Name -CEQ 'add')
    if ($clearNodes.Count -ne 1 -or $addNodes.Count -ne 1) {
        Add-PolicyFailure -Message 'NuGet.config must clear inherited sources and declare exactly one package source.'
    }
    elseif ($addNodes[0].GetAttribute('key') -cne 'nuget.org' -or $addNodes[0].GetAttribute('value') -cne 'https://api.nuget.org/v3/index.json') {
        Add-PolicyFailure -Message 'NuGet.config must declare only the official HTTPS nuget.org v3 endpoint under key nuget.org.'
    }

    # NuGet merges disabledPackageSources independently from packageSources.
    # Requiring the section's own clear element prevents a machine- or
    # user-level entry from disabling the repository's sole approved feed.
    $disabledSourceSections = @(
        $document.SelectNodes('/configuration/disabledPackageSources')
    )
    $disabledSourceEntries = @(
        $document.SelectNodes('/configuration/disabledPackageSources/*')
    )
    $disabledSourceClearEntries = @(
        $disabledSourceEntries | Where-Object Name -CEQ 'clear'
    )
    if (
        $disabledSourceSections.Count -ne 1 -or
        $disabledSourceEntries.Count -ne 1 -or
        $disabledSourceClearEntries.Count -ne 1
    ) {
        Add-PolicyFailure -Message (
            'NuGet.config must clear inherited disabled package sources and ' +
            'declare no disabled source entries.'
        )
    }

    if ($null -ne $document.SelectSingleNode('/configuration/packageSourceCredentials')) {
        Add-PolicyFailure -Message 'NuGet.config must not contain package-source credentials.'
    }

    $mappingSources = @($document.SelectNodes('/configuration/packageSourceMapping/packageSource'))
    if ($mappingSources.Count -ne 1 -or $mappingSources[0].GetAttribute('key') -cne 'nuget.org') {
        Add-PolicyFailure -Message 'NuGet.config must map all packages exclusively to nuget.org.'
    }
    else {
        $patterns = @($mappingSources[0].SelectNodes('package') | ForEach-Object { $_.GetAttribute('pattern') })
        if ($patterns.Count -ne 1 -or $patterns[0] -cne '*') {
            Add-PolicyFailure -Message 'NuGet.config nuget.org mapping must contain exactly the wildcard package pattern.'
        }
    }
}

function Test-EditorConfiguration {
    $document = Read-EditorConfigDocument -RelativePath '.editorconfig'
    if ($null -eq $document) {
        return
    }

    Assert-ExactValue -Actual $document.preamble.root -Expected 'true' -Description '.editorconfig root'
    if (-not $document.sections.Contains('*')) {
        Add-PolicyFailure -Message '.editorconfig must declare a repository-wide [*] section.'
        return
    }

    $repositoryWideSettings = $document.sections['*']
    foreach ($requiredSetting in ([ordered]@{
        charset = 'utf-8'
        end_of_line = 'lf'
        indent_style = 'space'
        indent_size = '4'
        tab_width = '4'
        trim_trailing_whitespace = 'true'
        insert_final_newline = 'true'
        max_line_length = '120'
    }).GetEnumerator()) {
        Assert-ExactValue `
            -Actual $repositoryWideSettings[$requiredSetting.Key] `
            -Expected $requiredSetting.Value `
            -Description ".editorconfig [*] $($requiredSetting.Key)"
    }
}

function Test-ClangFormatConfiguration {
    $settings = Read-SimpleYamlMapping -RelativePath '.clang-format'
    if ($null -eq $settings) {
        return
    }

    foreach ($requiredSetting in ([ordered]@{
        BasedOnStyle = 'Microsoft'
        UseTab = 'Never'
        IndentWidth = '4'
        ContinuationIndentWidth = '4'
        ColumnLimit = '120'
        'SortIncludes.Enabled' = 'false'
    }).GetEnumerator()) {
        Assert-ExactValue `
            -Actual $settings[$requiredSetting.Key] `
            -Expected $requiredSetting.Value `
            -Description ".clang-format $($requiredSetting.Key)"
    }
}

function Test-ClangTidyNamingConfiguration {
    $settings = Read-SimpleYamlMapping -RelativePath '.clang-tidy'
    if ($null -eq $settings) {
        return
    }

    foreach ($requiredCheck in @(
        'bugprone-reserved-identifier',
        'readability-identifier-naming',
        'readability-inconsistent-declaration-parameter-name'
    )) {
        if ([string]$settings.Checks -notmatch [regex]::Escape($requiredCheck)) {
            Add-PolicyFailure -Message ".clang-tidy Checks lacks '$requiredCheck'."
        }
    }
    Assert-ExactValue -Actual $settings.WarningsAsErrors -Expected '*' -Description '.clang-tidy WarningsAsErrors'
    Assert-ExactValue -Actual $settings.FormatStyle -Expected 'file' -Description '.clang-tidy FormatStyle'
    Assert-ExactValue `
        -Actual $settings.ExcludeHeaderFilterRegex `
        -Expected '(^|[\\/])([Gg]enerated([ _-]?[Ff]iles)?)[\\/]' `
        -Description '.clang-tidy generated-header exclusion'

    foreach ($namingRule in ([ordered]@{
        'CheckOptions.readability-identifier-naming.NamespaceCase' = 'lower_case'
        'CheckOptions.readability-identifier-naming.ClassCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.StructCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.EnumCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.EnumConstantCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.TypeAliasCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.TemplateParameterCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.FunctionCase' = 'camelBack'
        'CheckOptions.readability-identifier-naming.MethodCase' = 'camelBack'
        'CheckOptions.readability-identifier-naming.VariableCase' = 'camelBack'
        'CheckOptions.readability-identifier-naming.ParameterCase' = 'camelBack'
        'CheckOptions.readability-identifier-naming.PrivateMemberCase' = 'camelBack'
        'CheckOptions.readability-identifier-naming.PrivateMemberSuffix' = '_'
    }).GetEnumerator()) {
        Assert-ExactValue `
            -Actual $settings[$namingRule.Key] `
            -Expected $namingRule.Value `
            -Description ".clang-tidy $($namingRule.Key)"
    }

    $appSettings = Read-SimpleYamlMapping -RelativePath 'src/JpgSpinner.App/.clang-tidy'
    if ($null -eq $appSettings) {
        return
    }

    Assert-ExactValue `
        -Actual $appSettings.InheritParentConfig `
        -Expected 'true' `
        -Description 'src/JpgSpinner.App/.clang-tidy InheritParentConfig'
    foreach ($appNamingRule in ([ordered]@{
        'CheckOptions.readability-identifier-naming.NamespaceIgnoredRegexp' = '^JpgSpinner$'
        'CheckOptions.readability-identifier-naming.PublicMethodCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.ProtectedMethodCase' = 'CamelCase'
        'CheckOptions.readability-identifier-naming.PrivateMethodCase' = 'camelBack'
    }).GetEnumerator()) {
        Assert-ExactValue `
            -Actual $appSettings[$appNamingRule.Key] `
            -Expected $appNamingRule.Value `
            -Description "src/JpgSpinner.App/.clang-tidy $($appNamingRule.Key)"
    }
}

function Test-GitIgnorePolicy {
    $path = Get-RepositoryPath -RelativePath '.gitignore'
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-PolicyFailure -Message 'Required configuration file is absent: .gitignore'
        return
    }

    $activePatterns = @(
        Get-Content -LiteralPath $path |
            ForEach-Object { $_.Trim() } |
            Where-Object { $_.Length -gt 0 -and -not $_.StartsWith('#') }
    )
    foreach ($requiredPattern in @(
        '.vs/',
        'artifacts/**',
        '!artifacts/release/',
        '!artifacts/release/**/',
        '!artifacts/release/*.spdx.json',
        '!artifacts/release/**/*.spdx.json',
        'build/',
        'out/',
        'vcpkg_installed/',
        'packages/',
        'AppPackages/',
        'TestResults/',
        'sanitizer-reports/',
        'fuzz-artifacts/',
        '*.pfx',
        '*.p12',
        '*.snk'
    )) {
        if ($requiredPattern -cnotin $activePatterns) {
            Add-PolicyFailure -Message ".gitignore lacks required generated-or-secret pattern '$requiredPattern'."
        }
    }

    foreach ($reviewedArtifactPath in @(
        'src/Example/packages.lock.json',
        'tests/FuzzCorpus/reviewed-input.jpg',
        'artifacts/release/2.0.0.0/JpgSpinner.spdx.json',
        'artifacts/release/2.0.0.0/_manifest/spdx_3.0/manifest.spdx.json'
    )) {
        $null = & git -C $repositoryRoot check-ignore --no-index --quiet -- $reviewedArtifactPath 2>$null
        $ignoreQueryExitCode = $LASTEXITCODE
        if ($ignoreQueryExitCode -eq 0) {
            $ignoreExplanation = & git -C $repositoryRoot check-ignore --no-index --verbose -- $reviewedArtifactPath 2>$null
            Add-PolicyFailure -Message ".gitignore unexpectedly ignores reviewed artifact '$reviewedArtifactPath': $ignoreExplanation"
        }
        elseif ($ignoreQueryExitCode -ne 1) {
            Add-PolicyFailure -Message "git check-ignore failed for reviewed artifact '$reviewedArtifactPath' with exit code $ignoreQueryExitCode."
        }
    }
}

function Test-VcpkgManifest {
    $manifest = Read-JsonDocument -RelativePath 'vcpkg.json'
    if ($null -eq $manifest) {
        return
    }

    Assert-VcpkgConfigurationRepresentationExclusivity -Manifest $manifest

    $expectedManifestPropertyNames = @(
        '$schema',
        'name',
        'version-string',
        'builtin-baseline',
        'dependencies',
        'overrides'
    )
    if ($manifest.Contains('configuration')) {
        # `configuration` is vcpkg's current embedded representation. Its
        # authority-bearing contents are validated separately below.
        $expectedManifestPropertyNames += 'configuration'
    }
    Assert-ExactJsonObjectPropertySet `
        -JsonObject $manifest `
        -ExpectedPropertyNames $expectedManifestPropertyNames `
        -Description 'vcpkg.json manifest'

    Assert-ExactValue -Actual $manifest.name -Expected 'jpg-spinner' -Description 'vcpkg manifest name'
    Assert-ExactValue -Actual $manifest.'version-string' -Expected '2.0.0' -Description 'vcpkg manifest version-string'
    Assert-ExactValue -Actual $manifest.'builtin-baseline' -Expected '118bba14b94bc040c098c0c15e63c142148c05ca' -Description 'vcpkg builtin-baseline'

    if ($manifest.Contains('configuration')) {
        Test-VcpkgResolutionAuthorityConfiguration `
            -Configuration $manifest.configuration `
            -SourceDescription 'vcpkg.json embedded configuration'
    }
    if ($manifest.Contains('vcpkg-configuration')) {
        Add-PolicyFailure -Message (
            "vcpkg.json legacy 'vcpkg-configuration' spelling is prohibited; " +
            "the repository permits no compatibility alias for configuration authority."
        )
        Test-VcpkgResolutionAuthorityConfiguration `
            -Configuration $manifest['vcpkg-configuration'] `
            -SourceDescription "vcpkg.json legacy 'vcpkg-configuration'"
    }

    $expectedVersions = [ordered]@{
        'libjpeg-turbo' = '3.2.0'
        'exiv2' = '0.28.8'
        'catch2' = '3.16.0'
    }

    $dependencies = @($manifest.dependencies)
    $overrides = @($manifest.overrides)
    $approvedDependencyNames = @($expectedVersions.Keys)

    # Check JSON types before normalizing names or versions to strings. Without
    # this boundary, PowerShell comparison can make malformed JSON values look
    # equivalent to the reviewed manifest representation.
    for ($dependencyIndex = 0; $dependencyIndex -lt $dependencies.Count; $dependencyIndex++) {
        Assert-JsonStringProperty `
            -JsonObject $dependencies[$dependencyIndex] `
            -PropertyName 'name' `
            -Description "vcpkg dependency at index $dependencyIndex"
    }
    for ($overrideIndex = 0; $overrideIndex -lt $overrides.Count; $overrideIndex++) {
        Assert-JsonStringProperty `
            -JsonObject $overrides[$overrideIndex] `
            -PropertyName 'name' `
            -Description "vcpkg override at index $overrideIndex"
        Assert-JsonStringProperty `
            -JsonObject $overrides[$overrideIndex] `
            -PropertyName 'version' `
            -Description "vcpkg override '$((Get-VcpkgManifestEntryName -Entry $overrides[$overrideIndex]))'"
    }

    $declaredDependencyNames = @(
        $dependencies | ForEach-Object { Get-VcpkgManifestEntryName -Entry $_ }
    )
    $declaredOverrideNames = @(
        $overrides | ForEach-Object { Get-VcpkgManifestEntryName -Entry $_ }
    )

    # These checks close the manifest set: the per-package checks below prove
    # every approved name appears exactly once, while these checks reject every
    # unnamed or additional direct dependency and override.
    if (@($declaredDependencyNames | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count -gt 0) {
        Add-PolicyFailure -Message 'vcpkg.json contains a direct dependency without a valid name.'
    }
    foreach ($unapprovedDependencyName in @(
        $declaredDependencyNames |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -cnotin $approvedDependencyNames } |
            Sort-Object -Unique
    )) {
        Add-PolicyFailure -Message "vcpkg.json contains unapproved direct dependency '$unapprovedDependencyName'."
    }
    if (@($declaredOverrideNames | Where-Object { [string]::IsNullOrWhiteSpace($_) }).Count -gt 0) {
        Add-PolicyFailure -Message 'vcpkg.json contains an override without a valid package name.'
    }
    foreach ($unapprovedOverrideName in @(
        $declaredOverrideNames |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -cnotin $approvedDependencyNames } |
            Sort-Object -Unique
    )) {
        Add-PolicyFailure -Message "vcpkg.json contains unapproved override '$unapprovedOverrideName'."
    }

    foreach ($dependencyName in $expectedVersions.Keys) {
        $matchingDependencies = @(
            $dependencies |
                Where-Object { (Get-VcpkgManifestEntryName -Entry $_) -ceq $dependencyName }
        )
        if ($matchingDependencies.Count -ne 1) {
            Add-PolicyFailure -Message "vcpkg.json must declare dependency '$dependencyName' exactly once."
        }
        else {
            $dependency = $matchingDependencies[0]
            $expectedDependencyPropertyNames = @('name', 'default-features')
            if ($dependencyName -ceq 'exiv2') {
                $expectedDependencyPropertyNames += 'features'
            }
            Assert-ExactJsonObjectPropertySet `
                -JsonObject $dependency `
                -ExpectedPropertyNames $expectedDependencyPropertyNames `
                -Description "vcpkg dependency '$dependencyName'"

            if (
                $dependency -isnot [System.Collections.IDictionary] -or
                -not $dependency.Contains('default-features') -or
                $dependency['default-features'] -isnot [System.Boolean] -or
                $dependency['default-features'] -ne $false
            ) {
                Add-PolicyFailure -Message (
                    "vcpkg dependency '$dependencyName' property 'default-features' " +
                    'must be the JSON boolean false.'
                )
            }
        }

        $matchingOverrides = @(
            $overrides |
                Where-Object { (Get-VcpkgManifestEntryName -Entry $_) -ceq $dependencyName }
        )
        if ($matchingOverrides.Count -ne 1) {
            Add-PolicyFailure -Message "vcpkg override '$dependencyName' must pin exact version '$($expectedVersions[$dependencyName])'."
        }
        else {
            $override = $matchingOverrides[0]
            Assert-ExactJsonObjectPropertySet `
                -JsonObject $override `
                -ExpectedPropertyNames @('name', 'version') `
                -Description "vcpkg override '$dependencyName'"

            if (
                $override -isnot [System.Collections.IDictionary] -or
                -not $override.Contains('version') -or
                [string]$override['version'] -cne $expectedVersions[$dependencyName]
            ) {
                Add-PolicyFailure -Message "vcpkg override '$dependencyName' must pin exact version '$($expectedVersions[$dependencyName])'."
            }
        }
    }

    $exiv2 = @(
        $dependencies |
            Where-Object { (Get-VcpkgManifestEntryName -Entry $_) -ceq 'exiv2' }
    )
    if ($exiv2.Count -eq 1) {
        if (
            $exiv2[0] -isnot [System.Collections.IDictionary] -or
            -not $exiv2[0].Contains('features') -or
            $exiv2[0]['features'] -isnot [System.Collections.IList]
        ) {
            Add-PolicyFailure -Message (
                "vcpkg dependency 'exiv2' property 'features' must be a JSON array " +
                "containing only the string 'xmp'."
            )
        }
        else {
            $features = @($exiv2[0]['features'])
            if (
                $features.Count -ne 1 -or
                $features[0] -isnot [string] -or
                $features[0] -cne 'xmp'
            ) {
                Add-PolicyFailure -Message (
                    "vcpkg dependency 'exiv2' property 'features' must contain " +
                    "exactly the JSON string 'xmp'."
                )
            }
        }
    }

}

function Test-VcpkgConfigurationFile {
    $configuration = Read-JsonDocument -RelativePath 'vcpkg-configuration.json'
    if ($null -eq $configuration) {
        return
    }

    Test-VcpkgResolutionAuthorityConfiguration `
        -Configuration $configuration `
        -SourceDescription 'vcpkg-configuration.json'
}

function Test-VcpkgTriplet {
    param(
        [Parameter(Mandatory)]
        [ValidateSet('x86', 'x64', 'arm64')]
        [string]$Architecture
    )

    $relativePath = "vcpkg-triplets/$Architecture-windows-static-md.cmake"
    $path = Get-RepositoryPath -RelativePath $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return
    }

    # A triplet is executable CMake, so source-line pattern matching cannot
    # establish what the interpreter consumed. CMake's versioned JSON trace is
    # the native semantic boundary: validate the executed command stream as a
    # closed six-record set with exact argument arrays.
    $traceRecords = @(
        Invoke-CMakeScriptJsonTrace -RelativePath $relativePath
    )
    Assert-CMakeTraceContainsClosedTripletSettings `
        -RelativePath $relativePath `
        -Architecture $Architecture `
        -TraceRecords $traceRecords
}

$requiredConfigurationFiles = @(
    '.vsconfig',
    '.editorconfig',
    '.clang-format',
    '.clang-tidy',
    'src/JpgSpinner.App/.clang-tidy',
    'Directory.Build.props',
    'Directory.Build.targets',
    'NuGet.config',
    'vcpkg.json',
    'vcpkg-triplets/x86-windows-static-md.cmake',
    'vcpkg-triplets/x64-windows-static-md.cmake',
    'vcpkg-triplets/arm64-windows-static-md.cmake',
    'eng/toolchain-lock.json',
    'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj',
    'eng/BuildPolicyProbe/BuildPolicyProbe.cpp',
    'JPG Spinner/JPG Spinner.vcxproj',
    'JpgSpinner.sln',
    'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj',
    'src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj',
    'src/JpgSpinner.WindowsStorage/JpgSpinner.WindowsStorage.vcxproj',
    'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj',
    'src/JpgSpinner.App/JpgSpinner.App.vcxproj',
    'tests/TestSupport/TestSupport.vcxproj',
    'tests/JpgSpinner.Domain.Tests/JpgSpinner.Domain.Tests.vcxproj',
    'tests/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.vcxproj',
    'tests/JpgSpinner.WindowsStorage.Tests/JpgSpinner.WindowsStorage.Tests.vcxproj',
    'tests/JpgSpinner.BatchProcessing.Tests/JpgSpinner.BatchProcessing.Tests.vcxproj',
    'tests/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.vcxproj',
    'tests/TestData/README.md'
)

foreach ($relativePath in $requiredConfigurationFiles) {
    Assert-RequiredFile -RelativePath $relativePath
}

Test-VisualStudioConfiguration
Test-ToolchainLock
Test-DirectoryBuildProperties
Test-DirectoryBuildTargets
Test-BuildPolicyProbeProject
Test-LegacyCppCxProjectBuildPolicyScope
Test-ModernSolutionArchitecture
Test-NuGetConfiguration
Test-EditorConfiguration
Test-ClangFormatConfiguration
Test-ClangTidyNamingConfiguration
Test-GitIgnorePolicy
Test-VcpkgManifest
Test-VcpkgConfigurationFile
Test-VcpkgTriplet -Architecture x86
Test-VcpkgTriplet -Architecture x64
Test-VcpkgTriplet -Architecture arm64

if ($policyFailures.Count -gt 0) {
    Write-Error "Repository policy failed with $($policyFailures.Count) violation(s):`n - $($policyFailures -join "`n - ")"
    exit 1
}

$verifiedToolchainLock = Read-JsonDocument -RelativePath 'eng/toolchain-lock.json'
$verifiedVcpkgManifest = Read-JsonDocument -RelativePath 'vcpkg.json'
$verifiedVcpkgVersions = @{}
foreach ($override in @($verifiedVcpkgManifest.overrides)) {
    $verifiedVcpkgVersions[[string]$override.name] = [string]$override.version
}

# Report only evidence parsed during this run. Windows App SDK, C++/WinRT, and
# SDK BuildTools enter the message in Task 2, after real project references and
# locked restore graphs exist; naming a planned pin here would be a false claim.
Write-Output (
    ('Repository policy verified: Visual Studio 2026/v145, MSVC {0}, C++20, Windows SDK 10.0.28000.0, ' +
    'libjpeg-turbo {1}, Exiv2 {2}+xmp, and Catch2 {3}.') -f
        $verifiedToolchainLock.vcToolsVersion,
        $verifiedVcpkgVersions['libjpeg-turbo'],
        $verifiedVcpkgVersions.exiv2,
        $verifiedVcpkgVersions.catch2
)
