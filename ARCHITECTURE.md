# ARTestStudio Architecture

## Current layers

Dependencies point inward:

1. **Domain** (`source/Domain`) contains the diagram model, geometry, and
   routing logic. It does not depend on MFC or the file system.
2. **Application** (`source/Application`) defines use cases and the contracts
   required by the application. `IDiagramStorage` is the boundary for saving
   and loading a diagram.
3. **Infrastructure** (`source/Infrastructure`) implements external contracts.
   `TextDiagramStorage` provides file persistence without exposing format
   details to the domain or MFC.
4. **Presentation** (MFC classes under `source`) coordinates user interactions.
   `CARTestStudioDoc` invokes the storage contract and presents errors, but does
   not parse or generate the file format.

To add another storage mechanism, implement `IDiagramStorage` without modifying
`DiagramModel`.

## Diagram persistence

Diagrams exclusively use the `.atd` extension (**AR Test Diagram**) and a UTF-8
text format with the `ARTESTSTUDIO_DIAGRAM` header and a version number. The
initial version is `1`.

The `.atprj` extension is reserved for the project format to be implemented
later. A project may contain multiple diagrams and their configurations. The
current diagram adapter rejects `.atprj` to keep both responsibilities separate.

Loading is transactional: a temporary model is validated and reconstructed
first. The active document is replaced only after the entire file is valid.
Unknown versions, duplicate identifiers, connections to missing nodes, invalid
ports, out-of-range dimensions, and truncated files are rejected.

`DiagramSnapshot` is the contract used to reconstruct the complete aggregate.
It preserves the original `NodeId` and `ConnectionId` values, validates every
reference before modifying the model, and calculates the next identifiers from
the highest restored values. This allows future configurations and projects to
retain stable references even when deleted elements leave gaps.

Saving is also transactional: a temporary file is generated and written in the
same directory, and Windows replaces the destination after the write completes
successfully. A failure must not leave a partially written document.

`IAtomicFileWriter` separates persistence policy from the replacement mechanism.
`WindowsAtomicFileWriter` removes stale `.tmp` files, flushes the temporary file
to disk, and only then replaces the destination. The contract can be reused by
the future `.atprj` format and allows write failures to be simulated without
depending on the file system.

`.atd` files have a 16 MB limit, and each UTF-8 label has a 64 KB limit. Total
size is checked before parsing and again during serialization. Quoted strings
are read with explicit bounds so that a hostile file cannot force an allocation
proportional to its full contents. Temporary-file and replacement failures are
reported separately; if replacement fails, the previous document remains
intact.

## Fault handling

Expected operations do not propagate exceptions to the UI. They return a
`StorageResult` containing a stable `StorageError` and optional diagnostic
details. The adapter catches allocation failures, standard-library exceptions,
and unknown exceptions at its boundary. The MFC layer converts the result into
a user-facing message and preserves the existing diagram when loading fails.

Cross-cutting failures are sent to `FaultService` through `IFaultReporter`. The
application configures `FileFaultReporter` at its composition root, so the
domain does not depend on MFC, Windows, or the logging subsystem. The global MFC
message-loop boundary records unexpected exceptions, while persistence errors
use the same channel before a message is presented to the user.

The default log is written as UTF-8 to
`%LOCALAPPDATA%\ARTestStudio\Logs\ARTestStudio.log`. After it reaches 2 MB, the
previous file is retained as `ARTestStudio.previous.log`. Failures inside the
reporter itself are suppressed so they cannot mask or compound the original
error.

## Tests

The `ARTestStudio.UnitTests` project uses Google Test to test the layers without
opening the MFC UI. Test cases are organized by responsibility under
`tests/Domain`, `tests/Application`, and `tests/Infrastructure`; shared test
doubles and utilities remain under `tests/TestSupport`.

Persistence tests cover data and UTF-8 round trips, corrupted files, missing
references, incompatible versions, and missing files. They also verify file and
label limits, truncated documents, stale temporary-file cleanup, and preservation
of the previous file when writing or replacement fails.

Google Test is obtained through `vcpkg.json`, whose baseline pins dependency
versions so that Visual Studio and scripted builds consume the same dependency.
The executable emits native XML, and the build workflow also creates a readable
HTML report under `artifacts/test-results`.
