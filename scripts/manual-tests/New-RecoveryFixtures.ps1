[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'artifacts\manual-tests\diagram-recovery'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null
$fixtures = Join-Path $repositoryRoot 'quality\manual-tests\phase-03\fixtures'
$primaryFixture = Join-Path $fixtures 'MT-P3-recovery-primary.atd'
$newerFixture = Join-Path $fixtures 'MT-P3-recovery-newer.atd'

$interruptedPrimary = Join-Path $resolvedOutput 'MT-P3-interrupted.atd'
$interruptedTemporary = $interruptedPrimary + '.tmp'
Copy-Item -LiteralPath $primaryFixture -Destination $interruptedPrimary -Force
Copy-Item -LiteralPath $newerFixture -Destination $interruptedTemporary -Force
[System.IO.File]::SetLastWriteTimeUtc($interruptedPrimary, [DateTime]::UtcNow.AddMinutes(-2))
[System.IO.File]::SetLastWriteTimeUtc($interruptedTemporary, [DateTime]::UtcNow)

$corruptPrimary = Join-Path $resolvedOutput 'MT-P3-corrupt-primary.atd'
$validPrevious = Join-Path $resolvedOutput 'MT-P3-corrupt-primary.previous.atd'
[System.IO.File]::WriteAllText(
    $corruptPrimary,
    '{ "format": "ARTestStudio.Diagram", "version": 2, "nodes": [',
    [System.Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath $primaryFixture -Destination $validPrevious -Force

$invalidTemporaryPrimary = Join-Path $resolvedOutput 'MT-P3-invalid-temporary.atd'
$invalidTemporary = $invalidTemporaryPrimary + '.tmp'
Copy-Item -LiteralPath $primaryFixture -Destination $invalidTemporaryPrimary -Force
[System.IO.File]::WriteAllText(
    $invalidTemporary,
    '{ incomplete temporary content',
    [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::SetLastWriteTimeUtc($invalidTemporaryPrimary, [DateTime]::UtcNow.AddMinutes(-2))
[System.IO.File]::SetLastWriteTimeUtc($invalidTemporary, [DateTime]::UtcNow)

Write-Host 'Fixtures de recuperacion creados correctamente:' -ForegroundColor Green
Write-Host "  $interruptedPrimary"
Write-Host "  $interruptedTemporary"
Write-Host "  $corruptPrimary"
Write-Host "  $validPrevious"
Write-Host "  $invalidTemporaryPrimary"
Write-Host "  $invalidTemporary"
Write-Host ''
Write-Host 'Conserva esta ventana o copia la ruta para ejecutar los casos manuales.'
