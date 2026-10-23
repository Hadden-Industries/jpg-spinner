<# Enforce the source-retirement boundary without parsing C++ or MSBuild.
   Git owns file selection (tracked plus non-ignored untracked files); the native
   compiler and existing effective-build policy own language/SDK semantics.
   Historical documents may describe the retired implementation, but maintained
   source must not revive its packages, copied codec or framework APIs. #>
[CmdletBinding()]
param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$failures = [Collections.Generic.List[string]]::new()

# Git otherwise silently searches a parent checkout. A fixture or wrong path
# must never inherit that parent's inventory and produce a vacuous success.
$gitRoot = @(& git -C $repositoryRoot rev-parse --show-toplevel 2>&1)
if ($LASTEXITCODE -ne 0 -or $gitRoot.Count -ne 1 -or
    -not [StringComparer]::OrdinalIgnoreCase.Equals(
        [IO.Path]::GetFullPath([string]$gitRoot[0]), $repositoryRoot)) {
    throw 'RepositoryRoot must identify the Git worktree root.'
}

# A direct existence check also catches an ignored or empty legacy directory.
# No traversal or deletion of that directory is required to refuse it.
foreach ($retiredPath in @('JPG Spinner.sln', 'JPG Spinner')) {
    if (Test-Path -LiteralPath (Join-Path $repositoryRoot $retiredPath)) {
        $failures.Add("Retired implementation remains: $retiredPath")
    }
}

# -z returns literal UTF-8 names, not Git's display-quoted path representation.
# Read the native stream intact: line splitting would corrupt legal filenames.
# https://git-scm.com/docs/git-ls-files
$startInfo = [Diagnostics.ProcessStartInfo]::new('git')
$startInfo.WorkingDirectory = $repositoryRoot
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.StandardOutputEncoding = [Text.UTF8Encoding]::new($false, $true)
foreach ($argument in @('ls-files', '-z', '--cached', '--others', '--exclude-standard')) {
    [void]$startInfo.ArgumentList.Add($argument)
}
$process = [Diagnostics.Process]::new()
$process.StartInfo = $startInfo
try {
    [void]$process.Start()
    $output = $process.StandardOutput.ReadToEndAsync()
    $errorOutput = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(30000)) {
        $process.Kill($true)
        [void]$process.WaitForExit(5000)
        throw 'Git source inventory exceeded its 30-second deadline.'
    }
    $fileNames = $output.GetAwaiter().GetResult()
    $diagnostic = $errorOutput.GetAwaiter().GetResult()
    if ($process.ExitCode -ne 0) { throw "Git source inventory failed: $diagnostic" }
}
finally { $process.Dispose() }

$retiredFileNames = @(
    'packages.config', 'jpeglib.h', 'transupp.c', 'transupp.h',
    'jpegint.h', 'jdct.h', 'jinclude.h', 'jmemsys.h'
)
$sourceExtensions = @('.cpp', '.c', '.h', '.hpp', '.xaml', '.vcxproj', '.props', '.targets')
foreach ($relativePath in ($fileNames.Split([char]0, [StringSplitOptions]::RemoveEmptyEntries) | Sort-Object -Unique)) {
    $path = Join-Path $repositoryRoot $relativePath
    # An unstaged deletion still has an index entry; it is not surviving source.
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    if ($relativePath.StartsWith('JPG Spinner/', [StringComparison]::OrdinalIgnoreCase)) { continue }
    $baseName = [IO.Path]::GetFileName($relativePath)
    if ($retiredFileNames -contains $baseName -or $relativePath -match '(?i)(^|/)jpeg-9a(/|$)') {
        $failures.Add("Retired dependency file remains: $relativePath")
    }

    # These are explicit repository lexical prohibitions, not a C++ parser.
    # In particular, '^' remains legal XOR; C++20 compilation rejects actual
    # C++/CX handles. Do not invent a second language grammar here.
    if ($relativePath -notmatch '^(src|tests|eng|fuzz)/' -or
        [IO.Path]::GetExtension($path) -notin $sourceExtensions) {
        continue
    }
    $source = [IO.File]::ReadAllText($path)
    foreach ($retiredSurface in @('ApplicationInsights', 'InstrumentationKey', 'Windows::UI::Xaml',
        'Platform::String', 'Microsoft.Toolkit')) {
        if ($source.Contains($retiredSurface, [StringComparison]::OrdinalIgnoreCase)) {
            $failures.Add("Retired source surface '$retiredSurface': $relativePath")
        }
    }
}

if ($failures.Count -ne 0) {
    throw ("Retired implementation absence policy failed:`n" + ($failures -join "`n"))
}
Write-Output 'PASS: retired implementation paths, vendored codec files and prohibited source APIs are absent.'
