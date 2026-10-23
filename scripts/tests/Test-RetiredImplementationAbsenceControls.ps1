<# Exercise the actual retirement gate in small, independently initialized Git
   fixtures. The caller supplies a preallocated HISEW working group;
   this test retains every fixture for native evidence promotion/disposition. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FixtureRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath($FixtureRoot)
if (-not (Test-Path -LiteralPath $workspace -PathType Container)) {
    throw 'FixtureRoot must be an existing, exclusively owned working directory.'
}
$fixtureRoot = Join-Path $workspace ([guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
$policyPath = Join-Path (Split-Path -Parent $PSScriptRoot) 'Test-RetiredImplementationAbsence.ps1'
$powerShellPath = (Get-Process -Id $PID).Path

# Create one independent Git fixture, exercise the real child-process gate and
# assert its expected status/diagnostic. Keep its bytes and raw result for the
# caller's native evidence/preservation lifecycle, including on assertion error.
function Invoke-RetirementFixture {
    param([string]$Name, [string]$RelativePath, [string]$Content, [string]$ExpectedDiagnostic)

    $fixture = Join-Path $fixtureRoot $Name
    [void][IO.Directory]::CreateDirectory($fixture)
    & git -C $fixture init --quiet
    if ($LASTEXITCODE -ne 0) { throw "Cannot initialize fixture: $Name" }
    $sourcePath = Join-Path $fixture 'src/Valid XOR.cpp'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $sourcePath))
    [IO.File]::WriteAllText($sourcePath, 'int result(int a, int b) { return a ^ b; }')
    if ($RelativePath) {
        $mutationPath = Join-Path $fixture $RelativePath
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $mutationPath))
        [IO.File]::WriteAllText($mutationPath, $Content)
    }
    $output = & $powerShellPath -NoProfile -File $policyPath -RepositoryRoot $fixture 2>&1
    $exitCode = $LASTEXITCODE
    $text = $output -join "`n"
    [IO.File]::WriteAllText((Join-Path $fixtureRoot "$Name.log"), "Exit: $exitCode`n$text")
    if ($ExpectedDiagnostic) {
        if ($exitCode -eq 0 -or -not $text.Contains($ExpectedDiagnostic, [StringComparison]::Ordinal)) {
            throw "Fixture '$Name' did not refuse the intended contract: $text"
        }
    } elseif ($exitCode -ne 0) {
        throw "Valid fixture '$Name' was refused: $text"
    }
}

# The positive control protects ordinary C++ XOR and literal filenames with
# spaces; no hand-written C++ grammar may mistake those for C++/CX handles.
Invoke-RetirementFixture -Name valid -RelativePath '' -Content '' -ExpectedDiagnostic ''
Invoke-RetirementFixture -Name legacy-solution -RelativePath 'JPG Spinner.sln' -Content '' `
    -ExpectedDiagnostic 'Retired implementation remains: JPG Spinner.sln'
Invoke-RetirementFixture -Name legacy-tree -RelativePath 'JPG Spinner/Scenario.cpp' -Content '' `
    -ExpectedDiagnostic 'Retired implementation remains: JPG Spinner'
Invoke-RetirementFixture -Name package-config -RelativePath 'src/packages.config' -Content '<packages />' `
    -ExpectedDiagnostic 'Retired dependency file remains: src/packages.config'
Invoke-RetirementFixture -Name copied-codec -RelativePath 'src/copied/jpeglib.h' -Content '' `
    -ExpectedDiagnostic 'Retired dependency file remains: src/copied/jpeglib.h'
Invoke-RetirementFixture -Name old-codec-directory -RelativePath 'src/jpeg-9a/renamed.c' -Content '' `
    -ExpectedDiagnostic 'Retired dependency file remains: src/jpeg-9a/renamed.c'
Invoke-RetirementFixture -Name retired-xaml -RelativePath 'src/Window.cpp' `
    -Content 'using namespace Windows::UI::Xaml;' `
    -ExpectedDiagnostic "Retired source surface 'Windows::UI::Xaml': src/Window.cpp"
Invoke-RetirementFixture -Name retired-telemetry -RelativePath 'src/Telemetry.cpp' `
    -Content 'ApplicationInsights::TelemetryClient client;' `
    -ExpectedDiagnostic "Retired source surface 'ApplicationInsights': src/Telemetry.cpp"

Write-Output (
    'PASS: valid source plus seven independent retired-source refusal controls; fixtures retained for native cleanup.'
)
