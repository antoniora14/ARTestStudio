[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'x86')]
    [string]$Platform = 'x64',

    [string]$VisualStudioPath = 'D:\Program Files\Microsoft Visual Studio\18\Insiders',

    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $repositoryRoot 'source\ARTestStudio.sln'
$msbuildPath = Join-Path $VisualStudioPath 'MSBuild\Current\Bin\MSBuild.exe'

if (-not (Test-Path -LiteralPath $msbuildPath)) {
    throw "No se encontro MSBuild de Visual Studio Insiders en: $msbuildPath"
}

& $msbuildPath $solutionPath /m "/p:Configuration=$Configuration" "/p:Platform=$Platform" /verbosity:minimal
if ($LASTEXITCODE -ne 0) {
    throw "La compilacion fallo con codigo $LASTEXITCODE."
}

if (-not $SkipTests) {
    $testExecutable = Join-Path $repositoryRoot "artifacts\bin\$Platform\$Configuration\ARTestStudio.Tests.exe"
    if (-not (Test-Path -LiteralPath $testExecutable)) {
        throw "No se encontro el ejecutable de pruebas: $testExecutable"
    }

    & $testExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "Las pruebas fallaron con codigo $LASTEXITCODE."
    }
}