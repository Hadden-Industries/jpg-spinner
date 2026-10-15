<# Analyze the modern native graph through Microsoft's integrated analyzer.
   The probe entry point qualifies tool wiring, not application analysis.
   Rebuild/analysis artifacts are a separate contract from incremental packaging. #>
[CmdletBinding()]
param(
    [ValidateSet('ModernSolution','ToolchainProbe','WarningControl')][string]$AnalysisScope='ModernSolution',
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateRange(1,64)][int]$MaximumBuildProcessCount=2,
    # Rebuild is the final/CI contract. Build gives focused development feedback
    # using native incremental invalidation; it is not a fresh whole-tree run.
    [ValidateSet('Build','Rebuild')][string]$BuildTarget='Rebuild'
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Static-analysis output directory must be fresh and absent.' }
[void][IO.Directory]::CreateDirectory($output)
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $root
$project = switch ($AnalysisScope) {
    'ToolchainProbe' { 'eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj' }
    'WarningControl' { 'eng/CodeAnalysisWarningProbe/CodeAnalysisWarningProbe.vcxproj' }
    default { 'JpgSpinner.sln' }
}
$arguments = @((Join-Path $root $project),'-nologo','-noAutoResponse','-nodeReuse:false',
    "-maxCpuCount:$MaximumBuildProcessCount","-t:$BuildTarget",'-p:Configuration=Release','-p:Platform=x64',
    # Public integrated-analyzer property: native targets choose /analyze and
    # artifact generation. Existing /WX remains effective; do not add WX-.
    # https://learn.microsoft.com/cpp/code-quality/using-the-cpp-core-guidelines-checkers
    # The native result-ingestion task has its own warning policy, independent
    # of compiler /WX. Bind its public property, never the private underscored
    # implementation property; a real C6011 control must make this driver fail.
    '-p:RunCodeAnalysis=true','-p:CodeAnalysisTreatWarningsAsErrors=true',
    '-restore','-p:RestoreLockedMode=true',"-p:RestoreConfigFile=$(Join-Path $root 'NuGet.config')",
    "-bl:$(Join-Path $output 'StaticAnalysis.binlog')",'-verbosity:minimal')
if ($AnalysisScope -ine 'ModernSolution') {
    # Unique probe outputs prove the analyzer produced this run's evidence;
    # never override every solution project's intermediates into one directory.
    $arguments += @("-p:IntDir=$(Join-Path $output 'obj')\","-p:OutDir=$(Join-Path $output 'bin')\")
}
Push-Location -LiteralPath $root
$previousExcludePath = [Environment]::GetEnvironmentVariable('CAExcludePath','Process')
try {
    $dependencyRoots = [Collections.Generic.List[string]]::new()
    if ($AnalysisScope -ieq 'ModernSolution') {
        # Restore the declared graph before asking the owning app project for
        # its real package root. Never guess a developer's NuGet cache path.
        & $toolchain.msBuildExecutablePath (Join-Path $root $project) -nologo -noAutoResponse -nodeReuse:false `
            -t:Restore -p:Configuration=Release -p:Platform=x64 -p:RestoreLockedMode=true `
            "-p:RestoreConfigFile=$(Join-Path $root 'NuGet.config')" -verbosity:minimal
        if ($LASTEXITCODE -ne 0) { throw 'Native analysis graph restore failed.' }
        $packageRoot = & $toolchain.msBuildExecutablePath (Join-Path $root 'src/JpgSpinner.App/JpgSpinner.App.vcxproj') `
            -nologo -noAutoResponse -p:Configuration=Release -p:Platform=x64 '-getProperty:NuGetPackageRoot'
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($packageRoot)) { throw 'Native analysis package root could not be observed.' }
        foreach ($dependencyRoot in @((Join-Path $root 'vcpkg_installed'),$packageRoot)) {
            $canonical = [IO.Path]::GetFullPath($dependencyRoot)
            $projectRelativeToDependency = [IO.Path]::GetRelativePath($canonical,$root)
            if (-not [IO.Path]::IsPathRooted($projectRelativeToDependency) -and
                $projectRelativeToDependency -ne '..' -and
                -not $projectRelativeToDependency.StartsWith('..\',[StringComparison]::Ordinal)) {
                throw 'Dependency exclusion would hide the repository source root.'
            }
            $dependencyRoots.Add($canonical)
        }
    }
    # Microsoft documents CAExcludePath for dependency headers. Scope this gate
    # to first-party sources, matching native SDK/STL exclusions; retain the
    # external diagnostic baseline and separate dependency assurance obligations.
    # Replace caller exclusions so an inherited value cannot hide our sources.
    # This is an environment input, not a global MSBuild property: native targets
    # can still append their own SDK/STL paths. Restore the parent in finally.
    # https://learn.microsoft.com/cpp/build/reference/analyze-code-analysis
    [Environment]::SetEnvironmentVariable('CAExcludePath',($dependencyRoots -join ';'),'Process')
    & $toolchain.msBuildExecutablePath @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Native static-analysis rebuild failed.' }
}
finally {
    [Environment]::SetEnvironmentVariable('CAExcludePath',$previousExcludePath,'Process')
    Pop-Location
}
