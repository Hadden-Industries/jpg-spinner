<# A literal, non-execution provenance fixture exercises the record contract.
   These constants are test data, never claims that fixture tools were run. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$validator = Join-Path $root 'scripts/Test-BuildProvenance.ps1'
$fixtureRoot = Join-Path $OwnedOutputRoot ('provenance-contract-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
$record = [ordered]@{
    schemaVersion=1;sourceCommit=('1'*40);sourceTreeIsClean=$true;architecture='x64';configuration='Release'
    runnerImage=@{label='windows-2025-vs2026';version='20261014.1.0'}
    operatingSystemVersion='10.0.26300.0';vcToolsVersion='14.51.36231';windowsSdkVersion='10.0.28000.0'
    selectedTools=@{}
    nugetLocks=@{'src/JpgSpinner.App/packages.lock.json'=('a'*64)}
    vcpkgBaseline=('2'*40);vcpkgManifestSha256=('b'*64)
    actionCommits=@{
        'actions/checkout'='3d3c42e5aac5ba805825da76410c181273ba90b1'
        'actions/cache'='55cc8345863c7cc4c66a329aec7e433d2d1c52a9'
        'actions/upload-artifact'='cf430e030ddbb5b0abf93d22962f4752f3646cd9'
        'github/codeql-action'='24c54180a607b1449ed407dd24f251e4e9147c8d'
    }
}
foreach ($name in @('msBuild','compiler','linker','makeAppx')) {
    $record.selectedTools[$name]=@{path="<visual-studio>/$name.exe";fileVersion='1.2.3.4';sha256=('c'*64)}
}
$valid = Join-Path $fixtureRoot 'valid.json'
[IO.File]::WriteAllText($valid,($record | ConvertTo-Json -Depth 10),[Text.UTF8Encoding]::new($false))
& $validator -Path $valid
foreach ($case in @('empty','wrong-type','absolute-path','duplicate','stale-action','array-root')) {
    $copy = $record | ConvertTo-Json -Depth 10 | ConvertFrom-Json -AsHashtable
    switch ($case) {
        'empty' { $json='{}' }
        'wrong-type' { $copy.sourceTreeIsClean='true';$json=$copy | ConvertTo-Json -Depth 10 }
        'absolute-path' { $copy.selectedTools.compiler.path='C:/Users/example/private/cl.exe';$json=$copy | ConvertTo-Json -Depth 10 }
        'duplicate' {
            $json=($record | ConvertTo-Json -Depth 10).TrimEnd()
            $json=$json.Substring(0,$json.LastIndexOf('}')) + ',"schemaVersion":1}'
        }
        'stale-action' { $copy.actionCommits['actions/checkout']='3'*40;$json=$copy | ConvertTo-Json -Depth 10 }
        'array-root' { $json='[]' }
    }
    $path = Join-Path $fixtureRoot ($case+'.json')
    [IO.File]::WriteAllText($path,$json,[Text.UTF8Encoding]::new($false))
    $message=$null
    try { & $validator -Path $path } catch { $message=$_.Exception.Message }
    if ($null -eq $message) { throw "Provenance validator accepted $case." }
    if ($case -eq 'duplicate' -and $message -notmatch '(?i)duplicate') { throw "Duplicate-member fixture did not reach native duplicate rejection: $message" }
    if ($case -eq 'empty') {
        foreach ($field in $record.Keys) {
            if (-not $message.Contains($field,[StringComparison]::Ordinal)) { throw "Empty-record diagnostic omitted $field : $message" }
        }
    }
    Write-Output "PASS provenance refusal: $case"
}
