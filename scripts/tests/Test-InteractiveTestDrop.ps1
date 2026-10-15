<# Test byte-preserving drop composition, not executable validity or deployment.
   Ordinary text sentinels intentionally stand in for binary bytes; no process,
   compiler, Windows registration or desktop is involved. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('interactive-drop-'+[Guid]::NewGuid().ToString('N'))
$binary=Join-Path $fixture 'binaries'
$inputs=@{
    'JpgSpinner.App/AppxManifest.xml'='<Package />'
    'JpgSpinner.App/JpgSpinner.exe'='application bytes, not executable test code'
    'JpgSpinner.App/Assets/Logo.png'='owned asset bytes'
    'JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe'='test-host bytes, not executable test code'
    'JpgSpinner.Presentation.Tests/Runtime.dll'='native dependency bytes'
    'JpgSpinner.Domain.Tests/unrelated.txt'='must not be included'
}
foreach ($entry in $inputs.GetEnumerator()) {
    $path=Join-Path $binary $entry.Key
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
    [IO.File]::WriteAllText($path,$entry.Value)
}
$output=Join-Path $fixture 'drop'
& (Join-Path $repositoryRoot 'scripts/New-InteractiveTestDrop.ps1') -BinaryDirectory $binary -SourceCommit ('1'*40) `
    -OutputDirectory $output -WorkingDirectory (Join-Path $fixture 'work') | Out-Null
foreach ($entry in $inputs.GetEnumerator()) {
    $copy=Join-Path $output "payload/artifacts/bin/x64/Release/$($entry.Key)"
    if ($entry.Key.StartsWith('JpgSpinner.Domain.Tests/',[StringComparison]::Ordinal)) {
        if (Test-Path -LiteralPath $copy) {throw 'Unrelated binaries entered the interactive drop.'}
    } elseif (-not [IO.File]::Exists($copy) -or [IO.File]::ReadAllText($copy) -cne $entry.Value) {
        throw "Interactive drop lost or changed required bytes: $($entry.Key)"
    }
    if ([IO.File]::ReadAllText((Join-Path $binary $entry.Key)) -cne $entry.Value) {throw 'Original binary fixture changed.'}
}
$metadata=Get-Content -LiteralPath (Join-Path $output 'build-context.json') -Raw
if (-not (Test-Json -Json $metadata -SchemaFile (Join-Path $repositoryRoot 'eng/interactive-test-drop.schema.json'))) {throw 'Interactive input context violates its schema.'}
$context=$metadata | ConvertFrom-Json
if ($context.sourceCommit -cne ('1'*40) -or $context.payloadManifestSha256 -cne (Get-FileHash -LiteralPath (Join-Path $output 'payload.json')).Hash.ToLowerInvariant()) {throw 'Interactive drop input identity differs.'}
$validator=Join-Path $repositoryRoot 'scripts/Test-InteractiveTestDrop.ps1'
$accepted=& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $fixture 'valid-input-work')
if ($accepted.RepositoryRoot -cne (Join-Path $output 'payload')) {throw 'Valid prebuilt payload root was not selected.'}
function Assert-Refused([scriptblock]$Action,[string]$ExpectedMessage) {
    $message=$null
    try {& $Action | Out-Null} catch {$message=$_.Exception.Message}
    if ($null -eq $message -or -not $message.Contains($ExpectedMessage,[StringComparison]::Ordinal)) {
        throw "Expected refusal '$ExpectedMessage', observed '$message'."
    }
}
Assert-Refused {& $validator -DropDirectory $output -ExpectedSourceCommit ('2'*40) -WorkingDirectory (Join-Path $fixture 'wrong-source-work')} 'source commit differs'
Assert-Refused {& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $output 'nested-work')} 'outside the input drop'
Assert-Refused {& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $fixture 'valid-input-work')} 'fresh working directory'
$contextPath=Join-Path $output 'build-context.json'
try {
    # Exercise raw JSON admission before PowerShell conversion can discard a
    # duplicate member or turn an array root into an enumerable object stream.
    [IO.File]::WriteAllText($contextPath,('[{"sourceCommit":"'+('1'*40)+'"}]'))
    Assert-Refused {& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $fixture 'array-root-work')} 'root'
    [IO.File]::WriteAllText($contextPath,('{"sourceCommit":"'+('1'*40)+'","sourceCommit":"'+('2'*40)+'"}'))
    Assert-Refused {& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $fixture 'duplicate-work')} 'Duplicate'
} finally {[IO.File]::WriteAllText($contextPath,$metadata)}
$inventoryPath=Join-Path $output 'payload.json'
$inventoryBytes=[IO.File]::ReadAllBytes($inventoryPath)
try {
    [IO.File]::AppendAllText($inventoryPath,' ')
    Assert-Refused {& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $fixture 'inventory-work')} 'inventory digest differs'
} finally {[IO.File]::WriteAllBytes($inventoryPath,$inventoryBytes)}
$producer=Join-Path $repositoryRoot 'scripts/New-InteractiveTestDrop.ps1'
Assert-Refused {& $producer -BinaryDirectory $binary -SourceCommit ('1'*40) -OutputDirectory $output -WorkingDirectory (Join-Path $fixture 'existing-output-work')} 'fresh and absent'
$testHost=Join-Path $binary 'JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.exe'
[IO.File]::Move($testHost,"$testHost.held")
try {
    Assert-Refused {& $producer -BinaryDirectory $binary -SourceCommit ('1'*40) -OutputDirectory (Join-Path $fixture 'missing-host-drop') -WorkingDirectory (Join-Path $fixture 'missing-host-work')} 'prerequisite is absent'
} finally {[IO.File]::Move("$testHost.held",$testHost)}
if (Test-Path -LiteralPath (Join-Path $fixture 'missing-host-drop')) {throw 'A refused producer published a partial drop.'}
$copy=Join-Path $output 'payload/artifacts/bin/x64/Release/JpgSpinner.App/JpgSpinner.exe'
[IO.File]::WriteAllText($copy,'one changed byte sequence')
$message=$null
try {& $validator -DropDirectory $output -ExpectedSourceCommit ('1'*40) -WorkingDirectory (Join-Path $fixture 'changed-input-work') | Out-Null}
catch {$message=$_.Exception.Message}
if ($message -cne 'Interactive input bytes differ from the authenticated inventory.') {throw "Changed binary bytes were accepted or refused at the wrong boundary: $message"}
Write-Output 'PASS: byte-preserving drop, exact source/inventory identity, raw JSON and fresh-output refusal controls.'
