<# Frozen native-REST-shaped fixtures test residual project policy only. No
   network request, runner scheduling or protected-environment write occurs. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$test=Join-Path (Split-Path -Parent $PSScriptRoot) 'Test-InteractiveWorkflowAdmission.ps1'
$run='{"id":42,"head_sha":"1111111111111111111111111111111111111111","head_branch":"main","path":".github/workflows/continuous-integration.yml","event":"workflow_dispatch","status":"completed","conclusion":"success","repository":{"full_name":"Hadden-Industries/jpg-spinner"},"head_repository":{"full_name":"Hadden-Industries/jpg-spinner","fork":false}}'
$environment='{"name":"interactive-qualification","protection_rules":[{"type":"required_reviewers","prevent_self_review":true,"reviewers":[{"type":"User","reviewer":{"id":7}}]}]}'
$accepted=& $test -SourceRunJson $run -EnvironmentJson $environment -ExpectedSourceCommit ('1'*40) -ExpectedRunId 42
if ($accepted.SourceRunId -cne '42') {throw 'Valid manual producer was not admitted.'}
$cases=@(
    @{Name='wrong-candidate';Run=$run.Replace(('1'*40),('2'*40));Environment=$environment},
    @{Name='wrong-run';Run=$run.Replace('"id":42','"id":43');Environment=$environment},
    @{Name='string-id';Run=$run.Replace('"id":42','"id":"42"');Environment=$environment},
    @{Name='failed';Run=$run.Replace('"success"','"failure"');Environment=$environment},
    @{Name='wrong-workflow';Run=$run.Replace('continuous-integration.yml','codeql.yml');Environment=$environment},
    @{Name='fork';Run=$run.Replace('"fork":false','"fork":true');Environment=$environment},
    @{Name='branch';Run=$run.Replace('"head_branch":"main"','"head_branch":"other"');Environment=$environment},
    @{Name='automatic';Run=$run.Replace('workflow_dispatch','push');Environment=$environment},
    @{Name='unprotected';Run=$run;Environment='{"name":"interactive-qualification","protection_rules":[]}'},
    @{Name='self-review';Run=$run;Environment=$environment.Replace('"prevent_self_review":true','"prevent_self_review":false')},
    @{Name='string-boolean';Run=$run;Environment=$environment.Replace('"prevent_self_review":true','"prevent_self_review":"true"')},
    @{Name='no-reviewers';Run=$run;Environment=$environment.Replace('[{"type":"User","reviewer":{"id":7}}]','[]')},
    @{Name='oversized';Run=$run;Environment=$environment+(' '*1048576)},
    @{Name='array-root';Run="[$run]";Environment=$environment},
    @{Name='duplicate';Run=$run.Replace('"id":42','"id":42,"id":42');Environment=$environment}
)
foreach ($case in $cases) {
    $message=$null
    try {& $test -SourceRunJson $case.Run -EnvironmentJson $case.Environment -ExpectedSourceCommit ('1'*40) -ExpectedRunId 42 | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw "Unsafe workflow admission: $($case.Name)"}
    Write-Output "PASS workflow admission refusal: $($case.Name)"
}
