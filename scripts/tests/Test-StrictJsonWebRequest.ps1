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
Import-Module (Join-Path $repositoryRoot 'scripts/QualificationPolicy.psm1')

function Start-SingleResponseLoopbackServer {
    param(
        [Parameter(Mandatory)]
        [string]$ResponseBody,

        [Parameter()]
        [ValidateRange(0, 30000)]
        [int]$ResponseBodyDelayMilliseconds = 0,

        [hashtable]$AdditionalResponseHeaders = @{}
    )

    $pollIntervalMilliseconds = 50
    $readinessTimeoutSeconds = Get-QualificationTimeout -Category jsonTransport
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
            [int]$BodyDelayMilliseconds,
            [hashtable]$ExtraHeaders,
            [int]$PollIntervalMilliseconds
        )

        $listener = [System.Net.Sockets.TcpListener]::new(
            [System.Net.IPAddress]::Loopback,
            $Port
        )
        try {
            $listener.Start()
            Write-Output 'READY'

            # A client can reject arguments before connecting. Keep an ordinary
            # PowerShell cancellation point while awaiting that connection so
            # Stop-Job can always release this owned fixture.
            while (-not $listener.Pending()) { Start-Sleep -Milliseconds $PollIntervalMilliseconds }
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
                $extraHeaderText = ($ExtraHeaders.GetEnumerator() | ForEach-Object { "$($_.Key): $($_.Value)`r`n" }) -join ''
                $responseHeader = (
                    "HTTP/1.1 200 OK`r`n" +
                    "Content-Type: application/json; charset=utf-8`r`n" +
                    "Content-Length: $($responseBodyBytes.Length)`r`n" +
                    $extraHeaderText +
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
    } -ArgumentList $port, $ResponseBody, $ResponseBodyDelayMilliseconds, $AdditionalResponseHeaders, $pollIntervalMilliseconds

    $readinessDeadline = [DateTime]::UtcNow.AddSeconds($readinessTimeoutSeconds)
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
        Start-Sleep -Milliseconds $pollIntervalMilliseconds
    } while ([DateTime]::UtcNow -lt $readinessDeadline)

    Stop-Job -Job $serverJob
    Remove-Job -Job $serverJob -Force
    throw "Loopback JSON server did not become ready within $readinessTimeoutSeconds seconds."
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
    Accept       = 'application/json'
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

# Use real response headers and the platform's RelationLink interpretation.
# The later page must pass the same raw-JSON boundary as the first page.
foreach ($secondPageBody in @('[{"tag_name":"v2.0.0"}]', '[{"tag_name":"v2.0.0","tag_name":"v3.0.0"}]')) {
    $secondPage = Start-SingleResponseLoopbackServer -ResponseBody $secondPageBody
    $firstPage = Start-SingleResponseLoopbackServer -ResponseBody '[{"tag_name":"v1.0.0"}]' `
        -AdditionalResponseHeaders @{Link = "<$($secondPage.uri)>; rel=`"next`"" }
    try {
        $duplicatePage = $secondPageBody.Contains('v3.0.0')
        $paginationFailed = $false
        try {
            $pages = @(Invoke-JsonWebRequestWithUniqueObjectMembers -Uri $firstPage.uri -Headers $requestHeaders `
                    -SourceDescription 'paginated release metadata' -RequiredRootValueKind Array -FollowPagination)
            if ($duplicatePage -or $pages.Count -ne 2 -or $pages[1].tag_name -cne 'v2.0.0') {
                throw 'Paginated metadata was incomplete or accepted a duplicate member.'
            }
        }
        catch {
            if (-not $duplicatePage -or $_.Exception.Message -notlike "*Duplicate property 'tag_name'*") { throw }
            $paginationFailed = $true
        }
        if ($duplicatePage -and -not $paginationFailed) { throw 'The duplicate later page was not refused.' }
    }
    finally {
        Stop-SingleResponseLoopbackServer -ServerJob $firstPage.job
        Stop-SingleResponseLoopbackServer -ServerJob $secondPage.job
    }
}

$boundedPage = Start-SingleResponseLoopbackServer -ResponseBody '[]' `
    -AdditionalResponseHeaders @{Link = '<http://127.0.0.1:1/unvisited>; rel="next"' }
try {
    $boundWasRejected = $false
    try {
        [void](Invoke-JsonWebRequestWithUniqueObjectMembers -Uri $boundedPage.uri -Headers $requestHeaders `
                -SourceDescription 'bounded release metadata' -RequiredRootValueKind Array -FollowPagination -MaximumPageCount 1)
    }
    catch {
        if ($_.Exception.Message -notlike '*Pagination remains incomplete*') { throw }
        $boundWasRejected = $true
    }
    if (-not $boundWasRejected) { throw 'A reached page bound must not become a successful partial result.' }
}
finally { Stop-SingleResponseLoopbackServer -ServerJob $boundedPage.job }

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
