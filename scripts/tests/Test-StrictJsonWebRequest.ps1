[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$strictJsonWebRequestModulePath =
    Join-Path $repositoryRoot 'scripts/StrictJsonWebRequest.psm1'
if (-not (Test-Path -LiteralPath $strictJsonWebRequestModulePath -PathType Leaf)) {
    throw "The strict JSON web-request module is absent: $strictJsonWebRequestModulePath"
}
Import-Module -Name $strictJsonWebRequestModulePath -Force

function Start-SingleResponseLoopbackServer {
    param(
        [Parameter(Mandatory)]
        [string]$ResponseBody,

        [Parameter()]
        [ValidateRange(0, 30000)]
        [int]$ResponseBodyDelayMilliseconds = 0
    )

    # Reserve an ephemeral loopback port, release it, then let the background
    # server bind the same endpoint. The readiness handshake below closes the
    # small scheduling window before the client request is sent.
    $portReservation = [System.Net.Sockets.TcpListener]::new(
        [System.Net.IPAddress]::Loopback,
        0
    )
    $portReservation.Start()
    $port = ([System.Net.IPEndPoint]$portReservation.LocalEndpoint).Port
    $portReservation.Stop()

    $serverJob = Start-Job -ScriptBlock {
        param(
            [int]$Port,
            [string]$Body,
            [int]$BodyDelayMilliseconds
        )

        $listener = [System.Net.Sockets.TcpListener]::new(
            [System.Net.IPAddress]::Loopback,
            $Port
        )
        try {
            $listener.Start()
            Write-Output 'READY'

            $client = $listener.AcceptTcpClient()
            try {
                $stream = $client.GetStream()
                $requestReader = [System.IO.StreamReader]::new(
                    $stream,
                    [System.Text.Encoding]::ASCII,
                    $false,
                    1024,
                    $true
                )
                try {
                    while ($true) {
                        $requestLine = $requestReader.ReadLine()
                        if ([string]::IsNullOrEmpty($requestLine)) {
                            break
                        }
                    }
                }
                finally {
                    $requestReader.Dispose()
                }

                $responseBodyBytes = [System.Text.Encoding]::UTF8.GetBytes($Body)
                $responseHeader = (
                    "HTTP/1.1 200 OK`r`n" +
                    "Content-Type: application/json; charset=utf-8`r`n" +
                    "Content-Length: $($responseBodyBytes.Length)`r`n" +
                    "Connection: close`r`n`r`n"
                )
                $responseHeaderBytes = [System.Text.Encoding]::ASCII.GetBytes($responseHeader)
                $stream.Write($responseHeaderBytes, 0, $responseHeaderBytes.Length)
                $stream.Flush()
                if ($BodyDelayMilliseconds -gt 0) {
                    Start-Sleep -Milliseconds $BodyDelayMilliseconds
                }
                $stream.Write($responseBodyBytes, 0, $responseBodyBytes.Length)
                $stream.Flush()
            }
            finally {
                $client.Dispose()
            }
        }
        finally {
            $listener.Stop()
        }
    } -ArgumentList $port, $ResponseBody, $ResponseBodyDelayMilliseconds

    $readinessDeadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        $serverOutput = @(Receive-Job -Job $serverJob -Keep)
        if ($serverOutput -contains 'READY') {
            return [pscustomobject]@{
                job = $serverJob
                uri = [uri]"http://127.0.0.1:$port/metadata.json"
            }
        }
        if ($serverJob.State -in @('Failed', 'Stopped', 'Completed')) {
            throw "Loopback JSON server failed before accepting a request: $($serverOutput -join "`n")"
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $readinessDeadline)

    Stop-Job -Job $serverJob
    Remove-Job -Job $serverJob -Force
    throw 'Loopback JSON server did not become ready within ten seconds.'
}

function Stop-SingleResponseLoopbackServer {
    param(
        [Parameter(Mandatory)]
        [System.Management.Automation.Job]$ServerJob
    )

    if ($ServerJob.State -notin @('Completed', 'Failed', 'Stopped')) {
        Stop-Job -Job $ServerJob
    }
    [void](Receive-Job -Job $ServerJob -ErrorAction SilentlyContinue)
    Remove-Job -Job $ServerJob -Force
}

$requestHeaders = @{
    Accept = 'application/json'
    'User-Agent' = 'JPG-Spinner-Strict-JSON-Test/1.0'
}

$duplicateMemberServer = Start-SingleResponseLoopbackServer -ResponseBody @'
{
  "versions": ["0.0.1"],
  "versions": ["3.2.0"]
}
'@
try {
    $duplicateMemberWasRejected = $false
    try {
        [void](Invoke-JsonWebRequestWithUniqueObjectMembers `
            -Uri $duplicateMemberServer.uri `
            -Headers $requestHeaders `
            -SourceDescription 'loopback duplicate-member metadata')
    }
    catch {
        $duplicateMemberWasRejected = $true
        if (
            $_.Exception.Message.IndexOf(
                'loopback duplicate-member metadata',
                [System.StringComparison]::Ordinal
            ) -lt 0 -or
            $_.Exception.InnerException -isnot [System.Text.Json.JsonException] -or
            $_.Exception.InnerException.Message.IndexOf(
                "Duplicate property 'versions'",
                [System.StringComparison]::Ordinal
            ) -lt 0
        ) {
            throw (
                'Strict web request rejected duplicate JSON without the source-preserving ' +
                "System.Text.Json diagnostic: $($_.Exception.Message)"
            )
        }
    }
    if (-not $duplicateMemberWasRejected) {
        throw 'Strict web request accepted duplicate response members after lossy conversion.'
    }
}
finally {
    Stop-SingleResponseLoopbackServer -ServerJob $duplicateMemberServer.job
}

$validResponseServer = Start-SingleResponseLoopbackServer -ResponseBody @'
{
  "versions": ["3.2.0"]
}
'@
try {
    $validResponse = Invoke-JsonWebRequestWithUniqueObjectMembers `
        -Uri $validResponseServer.uri `
        -Headers $requestHeaders `
        -SourceDescription 'loopback valid metadata'
    if (
        @($validResponse.versions).Count -ne 1 -or
        [string]$validResponse.versions[0] -cne '3.2.0'
    ) {
        throw 'Strict web request did not preserve the valid JSON response structure.'
    }
}
finally {
    Stop-SingleResponseLoopbackServer -ServerJob $validResponseServer.job
}

$strictRequestCommand = Get-Command -Name Invoke-JsonWebRequestWithUniqueObjectMembers
if (-not $strictRequestCommand.Parameters.ContainsKey('RequiredRootValueKind')) {
    throw 'Strict web request does not expose raw JSON root-kind validation.'
}
if (-not $strictRequestCommand.Parameters.ContainsKey('ResponseDataReadTimeoutSeconds')) {
    throw 'Strict web request does not expose a response-data read timeout.'
}

$wrongRootResponseServer = Start-SingleResponseLoopbackServer -ResponseBody '[]'
try {
    $wrongRootWasRejected = $false
    try {
        [void](Invoke-JsonWebRequestWithUniqueObjectMembers `
            -Uri $wrongRootResponseServer.uri `
            -Headers $requestHeaders `
            -SourceDescription 'loopback object metadata' `
            -RequiredRootValueKind Object)
    }
    catch {
        $wrongRootWasRejected = $true
        if (
            $_.Exception.Message.IndexOf(
                'loopback object metadata root must be a JSON object',
                [System.StringComparison]::Ordinal
            ) -lt 0
        ) {
            throw (
                'Strict web request rejected a wrong JSON root without the required diagnostic: ' +
                $_.Exception.Message
            )
        }
    }
    if (-not $wrongRootWasRejected) {
        throw 'Strict web request accepted an array where the caller required an object root.'
    }
}
finally {
    Stop-SingleResponseLoopbackServer -ServerJob $wrongRootResponseServer.job
}

$stalledResponseServer = Start-SingleResponseLoopbackServer `
    -ResponseBody '{"versions":["3.2.0"]}' `
    -ResponseBodyDelayMilliseconds 3000
try {
    $requestStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $requestFailure = $null
    try {
        [void](Invoke-JsonWebRequestWithUniqueObjectMembers `
            -Uri $stalledResponseServer.uri `
            -Headers $requestHeaders `
            -SourceDescription 'loopback stalled-response metadata' `
            -ResponseDataReadTimeoutSeconds 1)
    }
    catch {
        $requestFailure = $_.Exception
    }
    finally {
        $requestStopwatch.Stop()
    }

    if ($null -eq $requestFailure) {
        throw 'Strict web request waited for a stalled response body without timing out.'
    }
    if ($requestStopwatch.ElapsedMilliseconds -lt 500) {
        throw (
            'Strict web request failed before exercising response-body transport: ' +
            $requestFailure.Message
        )
    }
    if ($requestStopwatch.ElapsedMilliseconds -ge 2500) {
        throw (
            'Strict web request did not enforce the one-second response-data timeout; ' +
            "elapsed $($requestStopwatch.ElapsedMilliseconds) ms."
        )
    }
}
finally {
    Stop-SingleResponseLoopbackServer -ServerJob $stalledResponseServer.job
}

Write-Output (
    'PASS: live JSON response bodies enforce root shapes, reject duplicate members ' +
    'before PowerShell object conversion, and time out stalled body reads.'
)
