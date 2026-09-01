[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Complete', 'ToolchainProbe', 'ModernSolution', IgnoreCase = $false)]
    [string]$EvidenceScope = 'Complete',

    [Parameter()]
    [ValidateSet('Debug', 'Release', IgnoreCase = $false)]
    [string[]]$ToolchainProbeConfigurations = @('Debug', 'Release'),

    [Parameter()]
    [ValidateSet('Win32', 'x64', 'ARM64', IgnoreCase = $false)]
    [string[]]$ToolchainProbePlatforms = @('Win32', 'x64', 'ARM64'),

    [Parameter()]
    [ValidateSet('Debug', 'Release', IgnoreCase = $false)]
    [string[]]$ModernSolutionConfigurations = @('Debug', 'Release'),

    [Parameter()]
    [ValidateSet('Win32', 'x64', 'ARM64', IgnoreCase = $false)]
    [string[]]$ModernSolutionPlatforms = @('x64')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$probeProjectPath = Join-Path $repositoryRoot 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj'
$modernSolutionPath = Join-Path $repositoryRoot 'JpgSpinner.sln'
$repositoryNuGetConfigurationPath = Join-Path $repositoryRoot 'NuGet.config'
$resolverPath = Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1'
$artifactsRoot = Join-Path $repositoryRoot 'artifacts/build-policy-probe'
$modernSolutionArtifactsRoot = Join-Path $repositoryRoot 'artifacts/build-policy-modern-solution'
$policyFailures = [System.Collections.Generic.List[string]]::new()

$dumpbinImageMitigationMetadataModulePath =
    Join-Path $PSScriptRoot 'DumpbinImageMitigationMetadata.psm1'
Import-Module -Name $dumpbinImageMitigationMetadataModulePath -Force
$jsonObjectMemberValidationModulePath =
    Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1'
Import-Module -Name $jsonObjectMemberValidationModulePath -Force

function Add-BuildPolicyFailure {
    param(
        [Parameter(Mandatory)]
        [string]$Message
    )

    [void]$policyFailures.Add($Message)
}

function ConvertTo-MsvcSlashPrefixedOptionToken {
    param(
        [Parameter(Mandatory)]
        [string]$OptionToken
    )

    # CL and LINK both document '/' and '-' as equivalent option specifiers.
    # Normalize only that leading character: option-name case has different
    # semantics in the compiler and linker and must remain untouched.
    if ($OptionToken.Length -gt 1 -and $OptionToken[0] -ceq '-') {
        return "/$($OptionToken.Substring(1))"
    }

    return $OptionToken
}

function ConvertFrom-MsvcProcessArgumentString {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$ArgumentString
    )

    # Microsoft C/C++ startup code splits arguments on spaces and tabs, keeps
    # quoted spans together, and gives backslashes special meaning only when
    # they immediately precede a quote. Implement those documented rules here
    # instead of treating the rendered command as a whitespace-delimited list.
    $arguments = [System.Collections.Generic.List[string]]::new()
    $argumentStringIndex = 0
    while ($argumentStringIndex -lt $ArgumentString.Length) {
        while (
            $argumentStringIndex -lt $ArgumentString.Length -and
            ($ArgumentString[$argumentStringIndex] -ceq ' ' -or $ArgumentString[$argumentStringIndex] -ceq "`t")
        ) {
            $argumentStringIndex++
        }
        if ($argumentStringIndex -ge $ArgumentString.Length) {
            break
        }

        $argumentBuilder = [System.Text.StringBuilder]::new()
        $insideQuotedSpan = $false
        while ($argumentStringIndex -lt $ArgumentString.Length) {
            $currentCharacter = $ArgumentString[$argumentStringIndex]
            if (
                -not $insideQuotedSpan -and
                ($currentCharacter -ceq ' ' -or $currentCharacter -ceq "`t")
            ) {
                break
            }

            if ($currentCharacter -ceq '\') {
                $backslashRunStartIndex = $argumentStringIndex
                while (
                    $argumentStringIndex -lt $ArgumentString.Length -and
                    $ArgumentString[$argumentStringIndex] -ceq '\'
                ) {
                    $argumentStringIndex++
                }
                $backslashCount = $argumentStringIndex - $backslashRunStartIndex

                if (
                    $argumentStringIndex -lt $ArgumentString.Length -and
                    $ArgumentString[$argumentStringIndex] -ceq '"'
                ) {
                    [void]$argumentBuilder.Append([char]92, [int]($backslashCount / 2))
                    if (($backslashCount % 2) -eq 1) {
                        [void]$argumentBuilder.Append('"')
                        $argumentStringIndex++
                    }
                    else {
                        if (
                            $insideQuotedSpan -and
                            ($argumentStringIndex + 1) -lt $ArgumentString.Length -and
                            $ArgumentString[$argumentStringIndex + 1] -ceq '"'
                        ) {
                            [void]$argumentBuilder.Append('"')
                            $argumentStringIndex += 2
                        }
                        else {
                            $insideQuotedSpan = -not $insideQuotedSpan
                            $argumentStringIndex++
                        }
                    }
                }
                else {
                    [void]$argumentBuilder.Append([char]92, $backslashCount)
                }
                continue
            }

            if ($currentCharacter -ceq '"') {
                if (
                    $insideQuotedSpan -and
                    ($argumentStringIndex + 1) -lt $ArgumentString.Length -and
                    $ArgumentString[$argumentStringIndex + 1] -ceq '"'
                ) {
                    [void]$argumentBuilder.Append('"')
                    $argumentStringIndex += 2
                }
                else {
                    $insideQuotedSpan = -not $insideQuotedSpan
                    $argumentStringIndex++
                }
                continue
            }

            [void]$argumentBuilder.Append($currentCharacter)
            $argumentStringIndex++
        }

        [void]$arguments.Add($argumentBuilder.ToString())
    }

    return $arguments.ToArray()
}

function Get-MsvcOptionNameComparison {
    param(
        [Parameter(Mandatory)]
        [ValidateSet('CL', 'LINK')]
        [string]$MsvcTool
    )

    if ($MsvcTool -ceq 'CL') {
        return [System.StringComparison]::Ordinal
    }

    return [System.StringComparison]::OrdinalIgnoreCase
}

function Test-MsvcCommandFileArgumentsAbsent {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [string]$Context
    )

    foreach ($commandArgument in $CommandArguments) {
        if ($commandArgument.StartsWith('@', [System.StringComparison]::Ordinal)) {
            Add-BuildPolicyFailure -Message (
                "$Context command line contains command-file argument '$commandArgument'; " +
                'effective options cannot be proven without captured command-file contents.'
            )
        }
    }
}

function Test-MsvcCommandArgumentsContainExactOption {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [string]$Option,

        [Parameter(Mandatory)]
        [ValidateSet('CL', 'LINK')]
        [string]$MsvcTool
    )

    $optionNameComparison = Get-MsvcOptionNameComparison -MsvcTool $MsvcTool
    foreach ($commandArgument in $CommandArguments) {
        $slashPrefixedCommandArgument =
            ConvertTo-MsvcSlashPrefixedOptionToken -OptionToken $commandArgument
        if ($MsvcTool -ceq 'CL' -and $slashPrefixedCommandArgument -ceq '/link') {
            break
        }
        if ($slashPrefixedCommandArgument.Equals($Option, $optionNameComparison)) {
            return $true
        }
    }

    return $false
}

function Test-RequiredMsvcCommandOption {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [string]$Option,

        [Parameter(Mandatory)]
        [ValidateSet('CL', 'LINK')]
        [string]$MsvcTool,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if (-not (Test-MsvcCommandArgumentsContainExactOption `
            -CommandArguments $CommandArguments `
            -Option $Option `
            -MsvcTool $MsvcTool
        )) {
        Add-BuildPolicyFailure -Message "$Context lacks effective option '$Option'."
    }
}

function Test-ProhibitedMsvcCommandOption {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [string]$Option,

        [Parameter(Mandatory)]
        [ValidateSet('CL', 'LINK')]
        [string]$MsvcTool,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if (Test-MsvcCommandArgumentsContainExactOption `
            -CommandArguments $CommandArguments `
            -Option $Option `
            -MsvcTool $MsvcTool
    ) {
        Add-BuildPolicyFailure -Message "$Context contains prohibited option '$Option'."
    }
}

function Test-ProhibitedMsvcCommandOptionFamily {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [string]$OptionFamilyPattern,

        [Parameter(Mandatory)]
        [string]$FamilyDescription,

        [Parameter(Mandatory)]
        [ValidateSet('CL', 'LINK')]
        [string]$MsvcTool,

        [Parameter(Mandatory)]
        [string]$Context
    )

    $optionFamilyRegexOptions = [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    if ($MsvcTool -ceq 'LINK') {
        $optionFamilyRegexOptions = $optionFamilyRegexOptions -bor
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    }
    $optionFamilyMatcher = [regex]::new($OptionFamilyPattern, $optionFamilyRegexOptions)

    foreach ($commandLineToken in $CommandArguments) {
        $slashPrefixedOptionToken =
            ConvertTo-MsvcSlashPrefixedOptionToken -OptionToken $commandLineToken
        if ($MsvcTool -ceq 'CL' -and $slashPrefixedOptionToken -ceq '/link') {
            break
        }
        if ($optionFamilyMatcher.IsMatch($slashPrefixedOptionToken)) {
            Add-BuildPolicyFailure -Message (
                "$Context contains incompatible $FamilyDescription option '$commandLineToken'; " +
                "the required '/utf-8' option cannot be combined with a separate character-set option."
            )
        }
    }
}

function Test-RequiredEffectiveCommandOptionFamily {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [string]$OptionFamilyPattern,

        [Parameter(Mandatory)]
        [string]$RequiredOption,

        [Parameter()]
        [string]$AcceptedEffectiveOptionPattern,

        [Parameter(Mandatory)]
        [string]$FamilyDescription,

        [Parameter(Mandatory)]
        [ValidateSet('CL', 'LINK')]
        [string]$MsvcTool,

        [Parameter(Mandatory)]
        [string]$Context
    )

    # MSVC documents that, when options in the same family conflict, the
    # rightmost member controls the compilation or link. Presence alone is
    # therefore insufficient: `/std:c++20 ... /std:c++17` is effectively C++17.
    # CL option names are case-sensitive (except HELP), while an individual
    # option may still define distinct semantics for its argument. LINK option
    # names and keyword arguments are case-insensitive. Keep those grammars
    # distinct even though both tools use rightmost/last-processed precedence.
    $optionNameComparison = Get-MsvcOptionNameComparison -MsvcTool $MsvcTool
    $optionFamilyRegexOptions = [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
    if ($MsvcTool -ceq 'LINK') {
        $optionFamilyRegexOptions = $optionFamilyRegexOptions -bor
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase
    }
    $optionFamilyMatcher = [regex]::new($OptionFamilyPattern, $optionFamilyRegexOptions)
    $acceptedEffectiveOptionMatcher = if ([string]::IsNullOrWhiteSpace($AcceptedEffectiveOptionPattern)) {
        $null
    }
    else {
        # Some documented effective states have multiple equivalent spellings
        # or option-specific argument case rules. Let the family declaration
        # model those semantics without weakening the tool-wide name grammar.
        [regex]::new($AcceptedEffectiveOptionPattern, $optionFamilyRegexOptions)
    }
    $orderedFamilyOptions = @(
        foreach ($commandLineToken in $CommandArguments) {
            $slashPrefixedOptionToken =
                ConvertTo-MsvcSlashPrefixedOptionToken -OptionToken $commandLineToken
            if ($MsvcTool -ceq 'CL' -and $slashPrefixedOptionToken -ceq '/link') {
                break
            }
            if ($optionFamilyMatcher.IsMatch($slashPrefixedOptionToken)) {
                $commandLineToken
            }
        }
    )

    if ($orderedFamilyOptions.Count -eq 0) {
        Add-BuildPolicyFailure -Message (
            "$Context lacks a $FamilyDescription option; required '$RequiredOption'."
        )
        return
    }

    $effectiveFamilyOption = $orderedFamilyOptions[-1]
    $slashPrefixedEffectiveFamilyOption =
        ConvertTo-MsvcSlashPrefixedOptionToken -OptionToken $effectiveFamilyOption
    $effectiveOptionSatisfiesRequirement = if ($null -ne $acceptedEffectiveOptionMatcher) {
        $acceptedEffectiveOptionMatcher.IsMatch($slashPrefixedEffectiveFamilyOption)
    }
    else {
        $slashPrefixedEffectiveFamilyOption.Equals($RequiredOption, $optionNameComparison)
    }
    if (-not $effectiveOptionSatisfiesRequirement) {
        Add-BuildPolicyFailure -Message (
            "$Context uses effective $FamilyDescription option '$effectiveFamilyOption'; " +
            "required '$RequiredOption'."
        )
    }
}

function Test-CompilerCommandPolicy {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [ValidateSet('Debug', 'Release')]
        [string]$Configuration,

        [Parameter(Mandatory)]
        [string]$Context
    )

    Test-MsvcCommandFileArgumentsAbsent `
        -CommandArguments $CommandArguments `
        -Context $Context

    foreach ($requiredCompilerOption in @('/utf-8', '/Brepro')) {
        Test-RequiredMsvcCommandOption `
            -CommandArguments $CommandArguments `
            -Option $requiredCompilerOption `
            -MsvcTool CL `
            -Context $Context
    }

    # The locked v145 compiler diagnoses /utf-8 combined with either separate
    # charset option as D8016, independent of token order. Report the policy
    # conflict directly rather than relying on compiler diagnostic wording.
    Test-ProhibitedMsvcCommandOptionFamily `
        -CommandArguments $CommandArguments `
        -OptionFamilyPattern '^/source-charset:.+$' `
        -FamilyDescription 'source-character-set' `
        -MsvcTool CL `
        -Context $Context
    Test-ProhibitedMsvcCommandOptionFamily `
        -CommandArguments $CommandArguments `
        -OptionFamilyPattern '^/execution-charset:.+$' `
        -FamilyDescription 'execution-character-set' `
        -MsvcTool CL `
        -Context $Context

    foreach ($compilerOptionFamilyRequirement in @(
        @{ pattern = '^/std:[^\s]+$'; requiredOption = '/std:c++20'; description = 'language-standard' },
        @{ pattern = '^/permissive-?$'; requiredOption = '/permissive-'; description = 'conformance' },
        @{ pattern = '^/(?:W[0-4]|Wall|w)$'; requiredOption = '/W4'; description = 'warning-level' },
        @{ pattern = '^/WX-?$'; requiredOption = '/WX'; description = 'warnings-as-errors' },
        @{ pattern = '^/sdl-?$'; requiredOption = '/sdl'; description = 'SDL-check' },
        @{
            pattern = '^/guard:(?i:cf)-?$'
            acceptedPattern = '^/guard:(?i:cf)$'
            requiredOption = '/guard:cf'
            description = 'compiler Control Flow Guard'
        },
        @{ pattern = '^/Qspectre-?$'; requiredOption = '/Qspectre'; description = 'Spectre-v1-mitigation' },
        @{ pattern = '^/Zc:__cplusplus-?$'; requiredOption = '/Zc:__cplusplus'; description = 'updated-__cplusplus-macro' }
    )) {
        Test-RequiredEffectiveCommandOptionFamily `
            -CommandArguments $CommandArguments `
            -OptionFamilyPattern $compilerOptionFamilyRequirement.pattern `
            -AcceptedEffectiveOptionPattern $compilerOptionFamilyRequirement['acceptedPattern'] `
            -RequiredOption $compilerOptionFamilyRequirement.requiredOption `
            -FamilyDescription $compilerOptionFamilyRequirement.description `
            -MsvcTool CL `
            -Context $Context
    }

    if ($Configuration -ceq 'Release') {
        foreach ($releaseCompilerOptionFamilyRequirement in @(
            @{ pattern = '^/O(?:d|1|2|x)$'; requiredOption = '/O2'; description = 'optimization' },
            @{ pattern = '^/GL-?$'; requiredOption = '/GL'; description = 'whole-program-optimization' }
        )) {
            Test-RequiredEffectiveCommandOptionFamily `
                -CommandArguments $CommandArguments `
                -OptionFamilyPattern $releaseCompilerOptionFamilyRequirement.pattern `
                -RequiredOption $releaseCompilerOptionFamilyRequirement.requiredOption `
                -FamilyDescription $releaseCompilerOptionFamilyRequirement.description `
                -MsvcTool CL `
                -Context $Context
        }
    }
}

function Test-LinkerCommandPolicy {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$CommandArguments,

        [Parameter(Mandatory)]
        [ValidateSet('Debug', 'Release')]
        [string]$Configuration,

        [Parameter(Mandatory)]
        [ValidateSet('Win32', 'x64', 'ARM64')]
        [string]$Platform,

        [Parameter(Mandatory)]
        [string]$Context
    )

    Test-MsvcCommandFileArgumentsAbsent `
        -CommandArguments $CommandArguments `
        -Context $Context
    Test-RequiredMsvcCommandOption `
        -CommandArguments $CommandArguments `
        -Option '/Brepro' `
        -MsvcTool LINK `
        -Context $Context
    Test-RequiredEffectiveCommandOptionFamily `
        -CommandArguments $CommandArguments `
        -OptionFamilyPattern '^/GUARD:(?:CF|NO)$' `
        -RequiredOption '/guard:cf' `
        -FamilyDescription 'linker Control Flow Guard' `
        -MsvcTool LINK `
        -Context $Context

    if ($Configuration -ceq 'Release') {
        Test-RequiredEffectiveCommandOptionFamily `
            -CommandArguments $CommandArguments `
            -OptionFamilyPattern '^/LTCG(?::[^\s]+)?$' `
            -AcceptedEffectiveOptionPattern '^/LTCG(?::(?:INCREMENTAL|NOSTATUS|STATUS))?$' `
            -RequiredOption '/LTCG' `
            -FamilyDescription 'link-time-code-generation' `
            -MsvcTool LINK `
            -Context $Context
    }

    if ($Platform -ceq 'x64') {
        Test-RequiredEffectiveCommandOptionFamily `
            -CommandArguments $CommandArguments `
            -OptionFamilyPattern '^/CETCOMPAT(?::NO)?$' `
            -RequiredOption '/CETCOMPAT' `
            -FamilyDescription 'CET-compatibility' `
            -MsvcTool LINK `
            -Context $Context
    }
    else {
        Test-ProhibitedMsvcCommandOption `
            -CommandArguments $CommandArguments `
            -Option '/CETCOMPAT' `
            -MsvcTool LINK `
            -Context $Context
    }
}

function ConvertTo-NormalizedLibraryDirectoryPath {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$DirectoryPath
    )

    $trimmedDirectoryPath = $DirectoryPath.Trim().Trim('"').TrimEnd('\', '/')
    if ([string]::IsNullOrWhiteSpace($trimmedDirectoryPath)) {
        return $null
    }

    if ([System.IO.Path]::IsPathFullyQualified($trimmedDirectoryPath)) {
        return [System.IO.Path]::GetFullPath($trimmedDirectoryPath).TrimEnd('\', '/')
    }

    return $trimmedDirectoryPath
}

function Test-SpectreMitigatedLibrarySearchPrecedence {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$LinkerCommandArguments,

        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$EvaluatedLibraryPath,

        [Parameter(Mandatory)]
        [string]$ExpectedSpectreMitigatedRuntimeLibraryDirectoryPath,

        [Parameter(Mandatory)]
        [string]$UnmitigatedRuntimeLibraryDirectoryPath,

        [Parameter(Mandatory)]
        [string]$Context
    )

    # Microsoft documents that every /LIBPATH directory is searched first, in
    # command-line order, followed by the directories in the LIB environment
    # path. MSBuild derives LIB from evaluated LibraryPath when UseEnv is false.
    # Model that combined order so retaining a Spectre directory somewhere in
    # the list cannot hide an ordinary runtime directory that resolves first.
    $effectiveLibrarySearchDirectoryPaths =
        [System.Collections.Generic.List[string]]::new()
    foreach ($linkerCommandArgument in $LinkerCommandArguments) {
        $slashPrefixedLinkerArgument =
            ConvertTo-MsvcSlashPrefixedOptionToken -OptionToken $linkerCommandArgument
        if (-not $slashPrefixedLinkerArgument.StartsWith(
                '/LIBPATH:',
                [System.StringComparison]::OrdinalIgnoreCase
            )) {
            continue
        }

        $additionalLibraryDirectoryPath = ConvertTo-NormalizedLibraryDirectoryPath `
            -DirectoryPath $slashPrefixedLinkerArgument.Substring('/LIBPATH:'.Length)
        if ($null -ne $additionalLibraryDirectoryPath) {
            [void]$effectiveLibrarySearchDirectoryPaths.Add($additionalLibraryDirectoryPath)
        }
    }
    foreach ($libraryDirectoryPath in ($EvaluatedLibraryPath -split ';')) {
        $normalizedLibraryDirectoryPath = ConvertTo-NormalizedLibraryDirectoryPath `
            -DirectoryPath $libraryDirectoryPath
        if ($null -ne $normalizedLibraryDirectoryPath) {
            [void]$effectiveLibrarySearchDirectoryPaths.Add($normalizedLibraryDirectoryPath)
        }
    }

    $normalizedExpectedSpectreDirectoryPath = ConvertTo-NormalizedLibraryDirectoryPath `
        -DirectoryPath $ExpectedSpectreMitigatedRuntimeLibraryDirectoryPath
    $normalizedUnmitigatedDirectoryPath = ConvertTo-NormalizedLibraryDirectoryPath `
        -DirectoryPath $UnmitigatedRuntimeLibraryDirectoryPath
    $firstSpectreDirectoryIndex = -1
    $firstUnmitigatedDirectoryIndex = -1
    for (
        $libraryDirectoryIndex = 0;
        $libraryDirectoryIndex -lt $effectiveLibrarySearchDirectoryPaths.Count;
        $libraryDirectoryIndex++
    )
    {
        $effectiveLibraryDirectoryPath = $effectiveLibrarySearchDirectoryPaths[$libraryDirectoryIndex]
        if (
            $firstSpectreDirectoryIndex -lt 0 -and
            $effectiveLibraryDirectoryPath.Equals(
                $normalizedExpectedSpectreDirectoryPath,
                [System.StringComparison]::OrdinalIgnoreCase
            )
        ) {
            $firstSpectreDirectoryIndex = $libraryDirectoryIndex
        }
        if (
            $firstUnmitigatedDirectoryIndex -lt 0 -and
            $effectiveLibraryDirectoryPath.Equals(
                $normalizedUnmitigatedDirectoryPath,
                [System.StringComparison]::OrdinalIgnoreCase
            )
        ) {
            $firstUnmitigatedDirectoryIndex = $libraryDirectoryIndex
        }
    }

    if ($firstSpectreDirectoryIndex -lt 0) {
        Add-BuildPolicyFailure -Message (
            "$Context does not select an architecture-specific Spectre-mitigated " +
            "MSVC library path '$ExpectedSpectreMitigatedRuntimeLibraryDirectoryPath'."
        )
        return
    }
    if (
        $firstUnmitigatedDirectoryIndex -ge 0 -and
        $firstUnmitigatedDirectoryIndex -lt $firstSpectreDirectoryIndex
    ) {
        Add-BuildPolicyFailure -Message (
            "$Context effective library search order resolves unmitigated MSVC runtime directory " +
            "'$UnmitigatedRuntimeLibraryDirectoryPath' before required Spectre-mitigated directory " +
            "'$ExpectedSpectreMitigatedRuntimeLibraryDirectoryPath'."
        )
    }
}

function Get-ObservedMsvcToolCommand {
    param(
        [Parameter(Mandatory)]
        [string]$DiagnosticLog,

        [Parameter(Mandatory)]
        [string]$ExecutablePath,

        [Parameter(Mandatory)]
        [string]$RequiredOperand,

        [Parameter()]
        [AllowEmptyCollection()]
        [string[]]$RequiredArgumentSubstrings = @(),

        [Parameter(Mandatory)]
        [string]$Context
    )

    # Diagnostic verbosity renders the actual ToolTask command as a line that
    # begins with the invoked executable. Other lines can repeat option-shaped
    # text (notably FileTracker), so bind evidence to the exact locked tool path
    # and expected input/output operand instead of searching or joining the log.
    $executableName = [System.IO.Path]::GetFileName($ExecutablePath)
    $observedToolCommands = [System.Collections.Generic.List[object]]::new()
    foreach ($diagnosticLogLine in ($DiagnosticLog -split "`r?`n")) {
        $trimmedDiagnosticLogLine = $diagnosticLogLine.TrimStart()
        foreach ($renderedExecutablePrefix in @($ExecutablePath, "`"$ExecutablePath`"")) {
            if (-not $trimmedDiagnosticLogLine.StartsWith(
                    $renderedExecutablePrefix,
                    [System.StringComparison]::OrdinalIgnoreCase
                )) {
                continue
            }
            if (
                $trimmedDiagnosticLogLine.Length -gt $renderedExecutablePrefix.Length -and
                $trimmedDiagnosticLogLine[$renderedExecutablePrefix.Length] -cnotin @(' ', "`t")
            ) {
                continue
            }

            $argumentString =
                $trimmedDiagnosticLogLine.Substring($renderedExecutablePrefix.Length).TrimStart()
            $argumentString = [regex]::Replace(
                $argumentString,
                '\s+\(TaskId:\d+\)\s*$',
                '',
                [System.Text.RegularExpressions.RegexOptions]::CultureInvariant
            )
            $commandArguments = @(ConvertFrom-MsvcProcessArgumentString -ArgumentString $argumentString)
            $containsRequiredOperand = @(
                $commandArguments |
                    Where-Object {
                        $_.IndexOf($RequiredOperand, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
                    }
            ).Count -gt 0
            $containsEveryRequiredArgumentSubstring = $true
            foreach ($requiredArgumentSubstring in $RequiredArgumentSubstrings) {
                $containsRequiredArgumentSubstring = @(
                    $commandArguments |
                        Where-Object {
                            $_.IndexOf(
                                $requiredArgumentSubstring,
                                [System.StringComparison]::OrdinalIgnoreCase
                            ) -ge 0
                        }
                ).Count -gt 0
                if (-not $containsRequiredArgumentSubstring) {
                    $containsEveryRequiredArgumentSubstring = $false
                    break
                }
            }
            if ($containsRequiredOperand -and $containsEveryRequiredArgumentSubstring) {
                [void]$observedToolCommands.Add([pscustomobject]@{
                    executablePath = $ExecutablePath
                    argumentString = $argumentString
                    commandArguments = [string[]]$commandArguments
                })
            }
            break
        }
    }

    if ($observedToolCommands.Count -eq 0) {
        Add-BuildPolicyFailure -Message (
            "$Context diagnostic log contains no $executableName task command for $RequiredOperand " +
            "at locked path '$ExecutablePath'."
        )
        return $null
    }
    if ($observedToolCommands.Count -ne 1) {
        Add-BuildPolicyFailure -Message (
            "$Context diagnostic log contains $($observedToolCommands.Count) $executableName task commands " +
            "for $RequiredOperand; expected exactly one."
        )
        return $null
    }

    return $observedToolCommands[0]
}

function Invoke-NormalizedChildProcess {
    param(
        [Parameter(Mandatory)]
        [string]$ExecutablePath,

        [Parameter(Mandatory)]
        [string[]]$ArgumentList,

        [Parameter(Mandatory)]
        [string]$WorkingDirectoryPath
    )

    # Some automation hosts inject both PATH and Path into the process environment.
    # Windows treats those names as equivalent, but MSBuild's managed CL task stores
    # inherited variables in a case-insensitive dictionary and rejects the duplicate.
    #
    # CL, _CL_, LINK, and _LINK_ are command-option injection surfaces documented by
    # the MSVC toolchain. UseEnv and the VC directory variables can replace MSBuild's
    # evaluated include and library search paths with caller-owned state. Excluding
    # both surfaces is part of the verifier's trust boundary: repository policy must
    # be measured independently of a caller's ambient tool options and directories.
    # Build a child-only environment with one canonical Path entry and none of those
    # ambient inputs. Path remains necessary for ordinary child-tool discovery, while
    # the locked MSBuild executable and repository policy select the tools under test.
    $inheritedEnvironment =
        [Environment]::GetEnvironmentVariables([EnvironmentVariableTarget]::Process)
    $normalizedEnvironment = [System.Collections.Generic.Dictionary[string, string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    $excludedEnvironmentVariableNames = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    foreach ($excludedEnvironmentVariableName in @(
        'Path',
        'CL',
        '_CL_',
        'LINK',
        '_LINK_',
        'UseEnv',
        'LIB',
        'LIBPATH',
        'INCLUDE',
        'EXTERNAL_INCLUDE'
    )) {
        [void]$excludedEnvironmentVariableNames.Add($excludedEnvironmentVariableName)
    }

    foreach ($environmentVariableName in $inheritedEnvironment.Keys) {
        if ($excludedEnvironmentVariableNames.Contains([string]$environmentVariableName)) {
            continue
        }

        $normalizedEnvironment[[string]$environmentVariableName] =
            [string]$inheritedEnvironment[$environmentVariableName]
    }
    $normalizedEnvironment['Path'] = [string]$env:PATH
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $ExecutablePath
    # MSVC resolves repository-relative response-file and other tool inputs
    # against the child process working directory. Pin that directory to the
    # repository being verified so results cannot depend on the caller's CWD.
    $startInfo.WorkingDirectory = $WorkingDirectoryPath
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $ArgumentList) {
        [void]$startInfo.ArgumentList.Add($argument)
    }

    $startInfo.Environment.Clear()
    foreach ($environmentVariable in $normalizedEnvironment.GetEnumerator()) {
        $startInfo.Environment[$environmentVariable.Key] = $environmentVariable.Value
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        [void]$process.Start()
        $standardOutputTask = $process.StandardOutput.ReadToEndAsync()
        $standardErrorTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()

        return [pscustomobject]@{
            exitCode = $process.ExitCode
            standardOutput = $standardOutputTask.GetAwaiter().GetResult()
            standardError = $standardErrorTask.GetAwaiter().GetResult()
        }
    }
    finally {
        $process.Dispose()
    }
}

function Get-EvaluatedBuildPolicyProperties {
    param(
        [Parameter(Mandatory)]
        [string]$MSBuildExecutablePath,

        [Parameter(Mandatory)]
        [string]$ProjectPath,

        [Parameter(Mandatory)]
        [string]$Configuration,

        [Parameter(Mandatory)]
        [string]$Platform,

        [Parameter(Mandatory)]
        [string]$WorkingDirectoryPath,

        [Parameter(Mandatory)]
        [string]$Context
    )

    # MSBuild 17.8 and later provides -getProperty specifically for evaluated
    # state. Reuse that engine-owned semantic boundary instead of scraping
    # diagnostic prose that can contain candidate, reassigned, and final values.
    $evaluationArguments = @(
        $ProjectPath,
        '/nologo',
        '-noAutoResponse',
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        '-getProperty:LibraryPath,WindowsTargetPlatformVersion,UseEnv'
    )
    $evaluationResult = Invoke-NormalizedChildProcess `
        -ExecutablePath $MSBuildExecutablePath `
        -ArgumentList $evaluationArguments `
        -WorkingDirectoryPath $WorkingDirectoryPath
    if ($evaluationResult.exitCode -ne 0) {
        Add-BuildPolicyFailure -Message (
            "$Context MSBuild property evaluation failed with exit code $($evaluationResult.exitCode): " +
            $evaluationResult.standardError.Trim()
        )
        return $null
    }

    try {
        $evaluationDocument = ConvertFrom-JsonWithUniqueObjectMembers `
            -JsonText $evaluationResult.standardOutput `
            -SourceDescription "$Context MSBuild evaluated properties" `
            -RequiredRootValueKind Object `
            -MaximumDepth 20
    }
    catch {
        Add-BuildPolicyFailure -Message (
            "$Context MSBuild property evaluation did not return strict object-root JSON: " +
            $_.Exception.Message
        )
        return $null
    }

    if ($null -eq $evaluationDocument.Properties) {
        Add-BuildPolicyFailure -Message "$Context MSBuild property evaluation lacks its Properties object."
        return $null
    }

    return $evaluationDocument.Properties
}

if (-not (Test-Path -LiteralPath $probeProjectPath -PathType Leaf)) {
    throw "Build-policy probe project is absent: $probeProjectPath"
}
if (-not (Test-Path -LiteralPath $resolverPath -PathType Leaf)) {
    throw "MSBuild toolchain resolver is absent: $resolverPath"
}

$resolvedToolchain = & $resolverPath -RepositoryRoot $repositoryRoot
[void](New-Item -ItemType Directory -Path $artifactsRoot -Force)

$requiredWindowsSdkVersion = '10.0.28000.0'
$probeConfigurations = if ($EvidenceScope -ceq 'ModernSolution') {
    @()
}
else {
    @($ToolchainProbeConfigurations)
}
$probePlatforms = if ($EvidenceScope -ceq 'ModernSolution') {
    @()
}
else {
    @($ToolchainProbePlatforms)
}
foreach ($configuration in $probeConfigurations) {
    foreach ($platform in $probePlatforms) {
        $configurationName = "$configuration|$platform"
        $safeConfigurationName = "$($configuration.ToLowerInvariant())-$($platform.ToLowerInvariant())"
        $binaryLogPath = Join-Path $artifactsRoot "$safeConfigurationName.binlog"
        $diagnosticLogPath = Join-Path $artifactsRoot "$safeConfigurationName.log"
        $evaluatedBuildPolicyProperties = Get-EvaluatedBuildPolicyProperties `
            -MSBuildExecutablePath $resolvedToolchain.msBuildExecutablePath `
            -ProjectPath $probeProjectPath `
            -Configuration $configuration `
            -Platform $platform `
            -WorkingDirectoryPath $repositoryRoot `
            -Context $configurationName
        if (
            $null -ne $evaluatedBuildPolicyProperties -and
            [string]$evaluatedBuildPolicyProperties.WindowsTargetPlatformVersion -cne
            $requiredWindowsSdkVersion
        ) {
            Add-BuildPolicyFailure -Message (
                "$configurationName uses effective Windows SDK version " +
                "'$($evaluatedBuildPolicyProperties.WindowsTargetPlatformVersion)'; " +
                "required '$requiredWindowsSdkVersion'."
            )
        }
        if (
            $null -ne $evaluatedBuildPolicyProperties -and
            [string]::Equals(
                [string]$evaluatedBuildPolicyProperties.UseEnv,
                'true',
                [System.StringComparison]::OrdinalIgnoreCase
            )
        ) {
            Add-BuildPolicyFailure -Message (
                "$configurationName uses effective UseEnv=true; deterministic VC directory " +
                'selection requires UseEnv=false.'
            )
        }

        $msBuildArguments = @(
            $probeProjectPath,
            '/nologo',
            '-noAutoResponse',
            '/m:1',
            '/t:Rebuild',
            '/v:minimal',
            "/p:Configuration=$configuration",
            "/p:Platform=$platform",
            "/bl:$binaryLogPath",
            '/fl',
            "/flp:LogFile=$diagnosticLogPath;Verbosity=diagnostic"
        )

        $buildResult = Invoke-NormalizedChildProcess `
            -ExecutablePath $resolvedToolchain.msBuildExecutablePath `
            -ArgumentList $msBuildArguments `
            -WorkingDirectoryPath $repositoryRoot
        if ($buildResult.exitCode -ne 0) {
            $combinedBuildOutput = @(
                $buildResult.standardOutput
                $buildResult.standardError
            ) -join "`n"
            $retainedOutput = ($combinedBuildOutput -split "`r?`n" | Select-Object -Last 30) -join "`n"
            Add-BuildPolicyFailure -Message "$configurationName probe build failed with exit code $($buildResult.exitCode).`n$retainedOutput"
            # MSBuild still emits structured task command events and a binary
            # log for most tool failures. Retain the build failure, then inspect
            # any available command evidence to report the violated policy as
            # well as the downstream compiler or linker symptom.
        }

        foreach ($expectedLogPath in @($binaryLogPath, $diagnosticLogPath)) {
            if (-not (Test-Path -LiteralPath $expectedLogPath -PathType Leaf)) {
                Add-BuildPolicyFailure -Message "$configurationName probe did not produce required evidence file '$expectedLogPath'."
            }
        }
        if (-not (Test-Path -LiteralPath $diagnosticLogPath -PathType Leaf)) {
            continue
        }

        $diagnosticLog = Get-Content -LiteralPath $diagnosticLogPath -Raw
        $expectedTargetArchitecture = switch ($platform) {
            'Win32' { 'x86' }
            'x64' { 'x64' }
            'ARM64' { 'arm64' }
        }
        $expectedTargetTools = $resolvedToolchain.targetTools[$expectedTargetArchitecture]
        $compilerToolCommand = Get-ObservedMsvcToolCommand `
            -DiagnosticLog $diagnosticLog `
            -ExecutablePath $expectedTargetTools.compilerExecutablePath `
            -RequiredOperand 'BuildPolicyProbe.cpp' `
            -Context $configurationName
        $linkerToolCommand = Get-ObservedMsvcToolCommand `
            -DiagnosticLog $diagnosticLog `
            -ExecutablePath $expectedTargetTools.linkerExecutablePath `
            -RequiredOperand 'BuildPolicyProbe.exe' `
            -Context $configurationName
        [string[]]$compilerCommandArguments = @()
        if ($null -ne $compilerToolCommand) {
            $compilerCommandArguments = @($compilerToolCommand.commandArguments)
        }
        [string[]]$linkerCommandArguments = @()
        if ($null -ne $linkerToolCommand) {
            $linkerCommandArguments = @($linkerToolCommand.commandArguments)
        }

        $expectedSpectreMitigatedRuntimeLibraryDirectoryPath = Join-Path `
            $resolvedToolchain.vcToolsDirectoryPath `
            "lib/spectre/$expectedTargetArchitecture"
        $unmitigatedRuntimeLibraryDirectoryPath = Join-Path `
            $resolvedToolchain.vcToolsDirectoryPath `
            "lib/$expectedTargetArchitecture"
        if ($null -ne $evaluatedBuildPolicyProperties) {
            Test-SpectreMitigatedLibrarySearchPrecedence `
                -LinkerCommandArguments $linkerCommandArguments `
                -EvaluatedLibraryPath ([string]$evaluatedBuildPolicyProperties.LibraryPath) `
                -ExpectedSpectreMitigatedRuntimeLibraryDirectoryPath (
                    $expectedSpectreMitigatedRuntimeLibraryDirectoryPath
                ) `
                -UnmitigatedRuntimeLibraryDirectoryPath $unmitigatedRuntimeLibraryDirectoryPath `
                -Context $configurationName
        }

        Test-CompilerCommandPolicy `
            -CommandArguments $compilerCommandArguments `
            -Configuration $configuration `
            -Context "$configurationName compiler"
        Test-LinkerCommandPolicy `
            -CommandArguments $linkerCommandArguments `
            -Configuration $configuration `
            -Platform $platform `
            -Context "$configurationName linker"

        $builtExecutablePath = Join-Path $artifactsRoot "$platform/$configuration/BuildPolicyProbe.exe"
        if (-not (Test-Path -LiteralPath $builtExecutablePath -PathType Leaf)) {
            Add-BuildPolicyFailure -Message "$configurationName did not produce expected executable '$builtExecutablePath'."
            continue
        }

        if ($platform -ceq 'x64') {
            $dumpbinExecutablePath = Join-Path (Split-Path -Parent $expectedTargetTools.linkerExecutablePath) 'dumpbin.exe'
            $peInspectionOutput = & $dumpbinExecutablePath /headers /loadconfig $builtExecutablePath 2>&1
            if ($LASTEXITCODE -ne 0) {
                Add-BuildPolicyFailure -Message (
                    "$configurationName dumpbin image-mitigation inspection failed with exit code $LASTEXITCODE."
                )
            }
            else {
                $imageMitigationMetadata = ConvertFrom-DumpbinImageMitigationMetadata `
                    -DumpbinOutput ($peInspectionOutput -join "`n")
                if (-not $imageMitigationMetadata.advertisesControlFlowGuard) {
                    Add-BuildPolicyFailure -Message "$configurationName PE headers do not advertise Control Flow Guard."
                }
                if (-not $imageMitigationMetadata.isControlFlowGuardInstrumented) {
                    Add-BuildPolicyFailure -Message (
                        "$configurationName PE load configuration does not report CF Instrumented."
                    )
                }
                if (-not $imageMitigationMetadata.hasControlFlowGuardFunctionIdTable) {
                    Add-BuildPolicyFailure -Message (
                        "$configurationName PE load configuration does not report FID table present."
                    )
                }
                if (-not $imageMitigationMetadata.isCetCompatible) {
                    Add-BuildPolicyFailure -Message "$configurationName PE headers do not advertise CET compatibility."
                }
            }
        }
    }
}

if ($EvidenceScope -cne 'ToolchainProbe') {
    if (-not (Test-Path -LiteralPath $modernSolutionPath -PathType Leaf)) {
        throw "Modern solution is absent: $modernSolutionPath"
    }
    if (-not (Test-Path -LiteralPath $repositoryNuGetConfigurationPath -PathType Leaf)) {
        throw "Repository NuGet configuration is absent: $repositoryNuGetConfigurationPath"
    }

    [void](New-Item -ItemType Directory -Path $modernSolutionArtifactsRoot -Force)

    # Each evidence record selects a source command that belongs to exactly one
    # project. The project-name qualifier binds shared sources such as
    # Catch2TestMain.cpp to the project-specific /Fo or /Fd path produced by
    # that project's IntDir. Application records also select their final LINK
    # command through the project-specific output directory.
    $modernProjectCommandEvidence = @(
        [pscustomobject]@{
            projectName = 'JpgSpinner.Domain'
            compilerOperand = 'DomainModule.cpp'
            linkerOperand = $null
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.JpegTransformation'
            compilerOperand = 'JpegTransformationModule.cpp'
            linkerOperand = $null
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.WindowsStorage'
            compilerOperand = 'WindowsStorageModule.cpp'
            linkerOperand = $null
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.BatchProcessing'
            compilerOperand = 'BatchProcessingModule.cpp'
            linkerOperand = $null
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.App'
            compilerOperand = 'pch.cpp'
            linkerOperand = 'JpgSpinner.exe'
        },
        [pscustomobject]@{
            projectName = 'TestSupport'
            compilerOperand = 'TemporaryDirectory.cpp'
            linkerOperand = $null
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.Domain.Tests'
            compilerOperand = 'Catch2TestMain.cpp'
            linkerOperand = 'JpgSpinner.Domain.Tests.exe'
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.JpegTransformation.Tests'
            compilerOperand = 'TestSupportContracts.cpp'
            linkerOperand = 'JpgSpinner.JpegTransformation.Tests.exe'
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.WindowsStorage.Tests'
            compilerOperand = 'Catch2TestMain.cpp'
            linkerOperand = 'JpgSpinner.WindowsStorage.Tests.exe'
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.BatchProcessing.Tests'
            compilerOperand = 'Catch2TestMain.cpp'
            linkerOperand = 'JpgSpinner.BatchProcessing.Tests.exe'
        },
        [pscustomobject]@{
            projectName = 'JpgSpinner.Presentation.Tests'
            compilerOperand = 'Catch2TestMain.cpp'
            linkerOperand = 'JpgSpinner.Presentation.Tests.exe'
        }
    )

    # The portable probe validates every locked compiler/linker architecture.
    # Real-project evidence has a separate, explicit matrix because a packaged
    # C++/WinUI ARM64 build additionally requires Visual Studio's optional
    # Microsoft.VisualStudio.Component.UWP.VC.ARM64 component. Task 2's build
    # contract is x64, so the default real-project matrix covers both policy-
    # relevant configurations on x64. Callers with that optional component can
    # request ARM64 explicitly without weakening or silently skipping evidence.
    foreach ($configuration in $ModernSolutionConfigurations) {
            foreach ($platform in $ModernSolutionPlatforms) {
                $configurationName = "$configuration|$platform"
                $modernSolutionContext = "$configurationName modern solution"
                $safeConfigurationName = (
                    "$($configuration.ToLowerInvariant())-$($platform.ToLowerInvariant())"
                )
                $binaryLogPath = Join-Path `
                    $modernSolutionArtifactsRoot `
                    "$safeConfigurationName.binlog"
                $diagnosticLogPath = Join-Path `
                    $modernSolutionArtifactsRoot `
                    "$safeConfigurationName.log"

                $modernSolutionBuildArguments = @(
                    $modernSolutionPath,
                    '/nologo',
                    '-noAutoResponse',
                    '/m:1',
                    '/restore',
                    '/t:Rebuild',
                    '/v:minimal',
                    "/p:Configuration=$configuration",
                    "/p:Platform=$platform",
                    "/p:RestoreConfigFile=$repositoryNuGetConfigurationPath",
                    '/p:RestoreLockedMode=true',
                    '/p:ContinuousIntegrationBuild=true',
                    "/bl:$binaryLogPath",
                    '/fl',
                    "/flp:LogFile=$diagnosticLogPath;Verbosity=diagnostic"
                )
                $modernSolutionBuildResult = Invoke-NormalizedChildProcess `
                    -ExecutablePath $resolvedToolchain.msBuildExecutablePath `
                    -ArgumentList $modernSolutionBuildArguments `
                    -WorkingDirectoryPath $repositoryRoot
                if ($modernSolutionBuildResult.exitCode -ne 0) {
                    $combinedBuildOutput = @(
                        $modernSolutionBuildResult.standardOutput
                        $modernSolutionBuildResult.standardError
                    ) -join "`n"
                    $retainedOutput = (
                        $combinedBuildOutput -split "`r?`n" |
                            Select-Object -Last 50
                    ) -join "`n"
                    Add-BuildPolicyFailure -Message (
                        "$modernSolutionContext build failed with exit code " +
                        "$($modernSolutionBuildResult.exitCode).`n$retainedOutput"
                    )
                }

                foreach ($expectedLogPath in @($binaryLogPath, $diagnosticLogPath)) {
                    if (-not (Test-Path -LiteralPath $expectedLogPath -PathType Leaf)) {
                        Add-BuildPolicyFailure -Message (
                            "$modernSolutionContext did not produce required evidence " +
                            "file '$expectedLogPath'."
                        )
                    }
                }
                if (-not (Test-Path -LiteralPath $diagnosticLogPath -PathType Leaf)) {
                    continue
                }

                $diagnosticLog = Get-Content -LiteralPath $diagnosticLogPath -Raw
                $expectedTargetArchitecture = switch ($platform) {
                    'Win32' { 'x86' }
                    'x64' { 'x64' }
                    'ARM64' { 'arm64' }
                }
                $expectedTargetTools =
                    $resolvedToolchain.targetTools[$expectedTargetArchitecture]

                foreach ($projectCommandEvidence in $modernProjectCommandEvidence) {
                    $projectContext = (
                        "$configurationName $($projectCommandEvidence.projectName)"
                    )
                    $compilerToolCommand = Get-ObservedMsvcToolCommand `
                        -DiagnosticLog $diagnosticLog `
                        -ExecutablePath $expectedTargetTools.compilerExecutablePath `
                        -RequiredOperand $projectCommandEvidence.compilerOperand `
                        -RequiredArgumentSubstrings @($projectCommandEvidence.projectName) `
                        -Context $projectContext
                    [string[]]$compilerCommandArguments = @()
                    if ($null -ne $compilerToolCommand) {
                        $compilerCommandArguments = @($compilerToolCommand.commandArguments)
                    }
                    Test-CompilerCommandPolicy `
                        -CommandArguments $compilerCommandArguments `
                        -Configuration $configuration `
                        -Context "$projectContext compiler"

                    if ($null -eq $projectCommandEvidence.linkerOperand) {
                        continue
                    }

                    $linkerRequiredArgumentSubstrings = @(
                        $projectCommandEvidence.projectName
                    )
                    if ($projectCommandEvidence.projectName -ceq 'JpgSpinner.App') {
                        # The packaged C++/WinUI targets intentionally perform
                        # an intermediate /WINMD:ONLY link followed by the
                        # application-producing /WINMD:NO link. Policy for the
                        # shipped executable must bind to the latter command.
                        $linkerRequiredArgumentSubstrings += '/WINMD:NO'
                    }
                    $linkerToolCommand = Get-ObservedMsvcToolCommand `
                        -DiagnosticLog $diagnosticLog `
                        -ExecutablePath $expectedTargetTools.linkerExecutablePath `
                        -RequiredOperand $projectCommandEvidence.linkerOperand `
                        -RequiredArgumentSubstrings $linkerRequiredArgumentSubstrings `
                        -Context $projectContext
                    [string[]]$linkerCommandArguments = @()
                    if ($null -ne $linkerToolCommand) {
                        $linkerCommandArguments = @($linkerToolCommand.commandArguments)
                    }
                    Test-LinkerCommandPolicy `
                        -CommandArguments $linkerCommandArguments `
                        -Configuration $configuration `
                        -Platform $platform `
                        -Context "$projectContext linker"
                }
        }
    }
}

if ($policyFailures.Count -gt 0) {
    Write-Error "Effective build policy failed with $($policyFailures.Count) violation(s):`n - $($policyFailures -join "`n - ")"
    exit 1
}

$verifiedCommandScopeDescriptions = [System.Collections.Generic.List[string]]::new()
if ($EvidenceScope -cne 'ModernSolution') {
    [void]$verifiedCommandScopeDescriptions.Add(
        'toolchain probes for ' +
        "$($ToolchainProbeConfigurations -join '/') on " +
        "$($ToolchainProbePlatforms -join '/')"
    )
}
if ($EvidenceScope -cne 'ToolchainProbe') {
    [void]$verifiedCommandScopeDescriptions.Add(
        'every modern production/test project command for ' +
        "$($ModernSolutionConfigurations -join '/') on " +
        "$($ModernSolutionPlatforms -join '/')"
    )
}

Write-Output (
    "Effective build policy scope '$EvidenceScope' verified " +
    "$($verifiedCommandScopeDescriptions -join '; ') with VCToolsVersion " +
    "$($resolvedToolchain.vcToolsVersion) and Windows SDK $requiredWindowsSdkVersion. " +
    'ToolTask evidence proves the rightmost effective options in every selected command.'
)
