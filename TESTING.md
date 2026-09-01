# ARTestStudio Automated Tests

## Structure

The `tests/ARTestStudio.UnitTests.vcxproj` project uses Google Test 1.18 and is
included in `source/ARTestStudio.sln`.

- `Domain/DiagramModelTests.cpp`: invariants, identifiers, and connections.
- `Domain/OrthogonalRouterTests.cpp`: orthogonal routes and node avoidance.
- `Application/FaultServiceTests.cpp`: fault-reporting boundary and delegation.
- `Application/InteractionControllerTests.cpp`: interaction states, selection,
  and undo/redo.
- `Application/UnexpectedCloseRecoveryTests.cpp`: session-catalog validation and
  safe detection of a previously terminated process.
- `Infrastructure/TextDiagramStorageTests.cpp`: `.atd` format, limits, and atomic
  saving.
- `Infrastructure/FileFaultReporterTests.cpp`: log writes and rotation.
- `TestSupport/TestSupport.h`: temporary files and shared test doubles.

There are currently 57 test cases across 7 suites.

## Running from the terminal

From the repository root:

```powershell
.\scripts\build.ps1 -Configuration Debug -Platform x64
```

For pre-release validation:

```powershell
.\scripts\build.ps1 -Configuration Release -Platform x64
```

The command returns a nonzero exit code when the build, any test, verdict
validation, or report generation fails.

## Running from Visual Studio Test Explorer

1. Open `source/ARTestStudio.sln` with Visual Studio Insiders.
2. Select the `x64` platform and either the `Debug` or `Release` configuration.
3. Open **Test > Test Explorer**.
4. Build the solution with **Build > Build Solution**.
5. Wait for 57 tests to appear under the Google Test suites.
6. Select **Run All Tests**, or run an individual suite or test case.

The Google Test adapter is included in Visual Studio's C++ workload. No
additional extension is required for this configuration.

## Reports

Each `scripts/build.ps1` run generates:

- `artifacts/test-results/<Platform>/<Configuration>/ARTestStudio.UnitTests.xml`:
  native Google Test output for continuous integration.
- `artifacts/test-results/<Platform>/<Configuration>/ARTestStudio.UnitTests.html`:
  a visual summary containing suite, test case, status, duration, and failure
  details.

Reports are local artifacts and are excluded from Git. Attach them as evidence
when an implementation requires automated traceability.

Before generating the actual report, the workflow runs a regression test for
the report generator using synthetic `PASSED`, `FAILED`, and `SKIPPED` cases.
Each report also compares the test and failure counts declared at the XML root
with the verdicts calculated per test case. If the two levels disagree, the
report is not generated and the build terminates with an error.

## Adding a test case

1. Select the file that corresponds to the responsible layer.
2. Declare the case with `TEST(SuiteName, ExpectedBehavior)`.
3. Keep the temporary arrangement, file, or service scoped to the test case.
4. Run both Debug and Release before completing the feature.
5. Confirm that the new case appears in Test Explorer and in both the XML and
   HTML reports.
