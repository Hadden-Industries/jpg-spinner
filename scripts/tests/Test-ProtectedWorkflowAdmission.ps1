<# Frozen native-REST-shaped fixtures test residual project policy only. No
   network request, runner scheduling or protected-environment write occurs. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$test=Join-Path (Split-Path -Parent $PSScriptRoot) 'Test-ProtectedWorkflowAdmission.ps1'
$run='{"id":42,"head_sha":"1111111111111111111111111111111111111111","head_branch":"main","path":".github/workflows/continuous-integration.yml","event":"workflow_dispatch","status":"completed","conclusion":"success","repository":{"full_name":"Hadden-Industries/jpg-spinner"},"head_repository":{"full_name":"Hadden-Industries/jpg-spinner","fork":false}}'
$environment='{"name":"interactive-qualification","protection_rules":[{"type":"required_reviewers","prevent_self_review":true,"reviewers":[{"type":"User","reviewer":{"id":7}}]}]}'
foreach ($role in @('interactive-qualification','store-submission')) {
    $roleEnvironment=$environment.Replace('interactive-qualification',$role)
    $accepted=& $test -SourceRunJson $run -EnvironmentJson $roleEnvironment -TargetEnvironmentName $role -ExpectedSourceCommit ('1'*40) -ExpectedRunId 42
    if ($accepted.SourceRunId -cne '42') {throw 'Valid manual producer was not admitted.'}
    if ($accepted.Environment -cne $role) {throw 'Protected environment selection was not preserved.'}
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
        $caseEnvironment=$case.Environment.Replace('interactive-qualification',$role)
        try {& $test -SourceRunJson $case.Run -EnvironmentJson $caseEnvironment -TargetEnvironmentName $role -ExpectedSourceCommit ('1'*40) -ExpectedRunId 42 | Out-Null}
        catch {$message=$_.Exception.Message}
        if ($null -eq $message) {throw "Unsafe workflow admission: $($case.Name)"}
        Write-Output "PASS $role workflow admission refusal: $($case.Name)"
    }
    $otherRole=if($role -ceq 'store-submission'){'interactive-qualification'}else{'store-submission'}
    $message=$null
    try {& $test -SourceRunJson $run -EnvironmentJson $roleEnvironment -TargetEnvironmentName $otherRole -ExpectedSourceCommit ('1'*40) -ExpectedRunId 42 | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw 'The other protected environment role was admitted.'}
}
