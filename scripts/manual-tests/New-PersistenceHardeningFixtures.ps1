[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'artifacts\manual-tests\persistence-hardening'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null

$oversizedPath = Join-Path $resolvedOutput 'PH-oversized-16mb.atd'
$stream = [System.IO.File]::Open(
    $oversizedPath,
    [System.IO.FileMode]::Create,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None)
try {
    $stream.SetLength((16MB) + 1)
}
finally {
    $stream.Dispose()
}

$labelPath = Join-Path $resolvedOutput 'PH-label-over-64kb.atd'
$oversizedLabel = 'A' * ((64KB) + 1)
$labelDocument = @(
    'ARTESTSTUDIO_DIAGRAM 1'
    'NODES 1'
    ('NODE 1 0 0 0 150 100 "' + $oversizedLabel + '"')
    'CONNECTIONS 0'
    'END'
) -join [Environment]::NewLine
[System.IO.File]::WriteAllText(
    $labelPath,
    $labelDocument,
    [System.Text.UTF8Encoding]::new($false))

$truncatedPath = Join-Path $resolvedOutput 'PH-truncated.atd'
$truncatedDocument = @(
    'ARTESTSTUDIO_DIAGRAM 1'
    'NODES 1'
    'NODE 1 0 0 0 150 100 "unterminated'
) -join [Environment]::NewLine
[System.IO.File]::WriteAllText(
    $truncatedPath,
    $truncatedDocument,
    [System.Text.UTF8Encoding]::new($false))

$staleDestinationPath = Join-Path $resolvedOutput 'PH-stale-temporary.atd'
$staleTemporaryPath = $staleDestinationPath + '.tmp'
[System.IO.File]::WriteAllText(
    $staleTemporaryPath,
    'STALE_TEMPORARY_CONTENT',
    [System.Text.UTF8Encoding]::new($false))

Write-Host 'Fixtures de persistencia creados correctamente:' -ForegroundColor Green
Write-Host "  $oversizedPath"
Write-Host "  $labelPath"
Write-Host "  $truncatedPath"
Write-Host "  $staleTemporaryPath"
Write-Host ''
Write-Host 'Conserva esta ventana o copia la ruta para ejecutar los casos manuales.'
