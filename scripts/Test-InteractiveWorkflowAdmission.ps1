<# Admit only a successful, same-candidate manual CI producer and an existing
   reviewer-protected environment. Inputs must be unfiltered native REST bodies
   fetched by the workflow with its read-only GitHub token. This local predicate
   does not authenticate arbitrary caller-supplied JSON or configure GitHub. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceRunJson,
    [Parameter(Mandatory)][string]$EnvironmentJson,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$ExpectedSourceCommit,
    [Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,18}$')][string]$ExpectedRunId
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ($SourceRunJson.Length -gt 1048576 -or $EnvironmentJson.Length -gt 1048576) {throw 'Interactive workflow admission response exceeds its size bound.'}
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$run=ConvertFrom-JsonWithUniqueObjectMembers -JsonText $SourceRunJson -RequiredRootValueKind Object -SourceDescription 'Native CI run response' -AsHashtable
$environment=ConvertFrom-JsonWithUniqueObjectMembers -JsonText $EnvironmentJson -RequiredRootValueKind Object -SourceDescription 'Native environment response' -AsHashtable
# These schemas deliberately validate the fields consumed here, not the entire
# evolving REST response. Native JSON/schema consumers retain type distinctions;
# no case-insensitive mapping or string-to-integer coercion admits the record.
$runSchema=@'
{"type":"object","required":["id","head_sha","head_branch","path","event","status","conclusion","repository","head_repository"],"properties":{"id":{"type":"integer","minimum":1},"head_sha":{"type":"string"},"head_branch":{"const":"main"},"path":{"const":".github/workflows/continuous-integration.yml"},"event":{"const":"workflow_dispatch"},"status":{"const":"completed"},"conclusion":{"const":"success"},"repository":{"type":"object","required":["full_name"],"properties":{"full_name":{"const":"Hadden-Industries/jpg-spinner"}}},"head_repository":{"type":"object","required":["full_name","fork"],"properties":{"full_name":{"const":"Hadden-Industries/jpg-spinner"},"fork":{"const":false}}}}}
'@
$environmentSchema=@'
{"type":"object","required":["name","protection_rules"],"properties":{"name":{"const":"interactive-qualification"},"protection_rules":{"type":"array","contains":{"type":"object","required":["type","prevent_self_review","reviewers"],"properties":{"type":{"const":"required_reviewers"},"prevent_self_review":{"const":true},"reviewers":{"type":"array","minItems":1,"items":{"type":"object","required":["type","reviewer"],"properties":{"type":{"enum":["User","Team"]},"reviewer":{"type":"object","required":["id"],"properties":{"id":{"type":"integer","minimum":1}}}}}}}}}}}
'@
if (-not (Test-Json -Json $SourceRunJson -Schema $runSchema -ErrorAction SilentlyContinue) -or
    -not (Test-Json -Json $EnvironmentJson -Schema $environmentSchema -ErrorAction SilentlyContinue)) {
    throw 'Interactive workflow admission refused producer or environment policy.'
}
if ($run.id.ToString([Globalization.CultureInfo]::InvariantCulture) -cne $ExpectedRunId -or
    $run.head_sha -cne $ExpectedSourceCommit) {throw 'Interactive workflow producer differs from the selected run or candidate.'}
[pscustomobject]@{SourceRunId=$ExpectedRunId;SourceCommit=$ExpectedSourceCommit;Environment=$environment.name}
