# Building ARTestStudio

## Prerequisites

- Visual Studio Insiders 18 installed by default at
  `D:\Program Files\Microsoft Visual Studio\18\Insiders`.
- Desktop development with C++ workload.
- MFC for the v145 toolset.
- Windows 10 SDK.
- Internet access during the first Google Test restore through the `vcpkg`
  bundled with Visual Studio.

## From PowerShell

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

The script builds `Debug|x64`, restores Google Test when required, and runs
`ARTestStudio.UnitTests`. XML and HTML results are written to
`artifacts/test-results/x64/Debug/`, which is excluded from the repository.

The solution and MFC project are located under `source/`; tests remain separate
under `tests/`.

To select another configuration:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```

If Visual Studio Insiders is installed at another location:

```powershell
.\scripts\build.ps1 -VisualStudioPath 'E:\Microsoft Visual Studio\18\Insiders'
```

To build without running tests:

```powershell
.\scripts\build.ps1 -Configuration Debug -Platform x64 -SkipTests
```

The complete Test Explorer, filtering, and reporting guide is available in
`TESTING.md`.

## Initial quality policy

- C++20 and compiler conformance mode.
- `/W4` warning level.
- Sources compiled as UTF-8.
- Warnings do not fail the build yet; they will be promoted to errors
  incrementally after the legacy code has been cleaned up.
