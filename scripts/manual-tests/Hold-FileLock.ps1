[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'

$resolvedPath = [System.IO.Path]::GetFullPath($Path)
if (-not [System.IO.File]::Exists($resolvedPath)) {
    throw "No se encontro el archivo que se debe bloquear: $resolvedPath"
}

$stream = [System.IO.File]::Open(
    $resolvedPath,
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read,
    [System.IO.FileShare]::None)
try {
    Write-Host "Archivo bloqueado: $resolvedPath" -ForegroundColor Yellow
    Write-Host 'Mantenga esta ventana abierta y ejecute el guardado en ARTestStudio.'
    [void](Read-Host 'Presione ENTER solamente cuando termine el intento de guardado')
}
finally {
    $stream.Dispose()
    Write-Host 'Bloqueo liberado.' -ForegroundColor Green
}
