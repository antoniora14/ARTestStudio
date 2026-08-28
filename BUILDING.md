# Compilar ARTestStudio

## Requisitos

- Visual Studio Insiders 18 instalado por defecto en
  `D:\Program Files\Microsoft Visual Studio\18\Insiders`.
- Desarrollo para el escritorio con C++.
- MFC para el toolset v145.
- Windows 10 SDK.
- Acceso a Internet durante la primera restauracion de Google Test mediante el
  `vcpkg` incluido con Visual Studio.

## Desde PowerShell

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

El script compila `Debug|x64`, restaura Google Test si es necesario y ejecuta
`ARTestStudio.UnitTests`. Los resultados XML y HTML se escriben en
`artifacts/test-results/x64/Debug/`, que no forma parte del repositorio.

La solución y el proyecto MFC se encuentran bajo `source/`; las pruebas permanecen
separadas en `tests/`.

Para seleccionar otra configuración:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```

Si Visual Studio Insiders está instalado en otra ubicación:

```powershell
.\scripts\build.ps1 -VisualStudioPath 'E:\Microsoft Visual Studio\18\Insiders'
```

Para compilar sin ejecutar pruebas:

```powershell
.\scripts\build.ps1 -Configuration Debug -Platform x64 -SkipTests
```

La guia completa para Test Explorer, filtros y reportes esta en
`TESTING.md`.

## Política inicial de calidad

- C++20 y modo de conformidad del compilador.
- Nivel de advertencias `/W4`.
- Fuentes compiladas como UTF-8.
- Las advertencias aún no fallan la compilación; se convertirán gradualmente en
  errores después de sanear el código heredado.
