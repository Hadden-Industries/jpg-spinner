<# Emit an independently specified byte sequence through the native stdout
   stream. Invalid UTF-8 and embedded NUL distinguish bytes from text output. #>
[CmdletBinding()]
param([switch]$Fail)

$bytes = [byte[]]@(0, 255, 128, 195, 40, 13, 10, 65, 0, 254)
$output = [Console]::OpenStandardOutput()
$output.Write($bytes, 0, $bytes.Length)
$output.Flush()
[Console]::Error.WriteLine('owned-binary-child-stderr')
if ($Fail) { exit 7 }
