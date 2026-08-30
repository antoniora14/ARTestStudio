# ARTestStudio diagram recovery

ARTestStudio protects each saved diagram with three distinct files:

| Role | Example | Purpose |
|---|---|---|
| Primary | `sequence.atd` | Current confirmed document |
| Previous valid version | `sequence.previous.atd` | Last valid primary before replacement |
| Interrupted save | `sequence.atd.tmp` | Write-through temporary file used by atomic replacement |

## Normal save

1. Serialize and validate the in-memory model before touching the filesystem.
2. If the existing primary is valid, copy its exact content atomically to
   `*.previous.atd`.
3. If the backup cannot be written, abort the save and leave the primary unchanged.
4. Write the new content to `*.atd.tmp`, flush it, close it, and atomically replace
   the primary.
5. If replacement fails, the old primary and the previous-version backup remain valid.

An invalid primary is never promoted into `*.previous.atd`; an existing valid backup
is preserved instead.

## Recovery inspection

Opening a diagram probes the primary, previous-version backup, and interrupted-save
temporary file independently. Every candidate must pass the same size, UTF-8, schema,
version, reference, and domain-invariant validation as a normal `.atd`.

A valid candidate is offered only when:

- the primary is missing;
- the primary is invalid; or
- the candidate has a later modification time than the valid primary.

If more than one candidate qualifies, the newest valid candidate is selected.
Incomplete or corrupt recovery artifacts are logged and never offered.

## Controlled recovery

The UI presents **Yes / No / Cancel**:

- **Yes** revalidates the candidate, protects an interrupted save durably, replaces
  the primary atomically, and then publishes the recovered model.
- **No** records the rejection and attempts to open the primary normally.
- **Cancel** leaves the files and current application state unchanged.

The active model is updated only after the recovered primary has been written
successfully. A failed recovery cannot partially replace the in-memory diagram.

## Unexpected application close

MFC remains responsible for serializing unsaved in-memory edits every minute. After
each successful autosave, ARTestStudio also persists a small session catalog containing
the Restart Manager identifier and the owning process identifier. This catalog closes
a gap in the standard MFC flow: Task Manager can terminate the process before Windows
invokes the recovery callback that normally persists the document-to-autosave mapping.

On the next ordinary launch, ARTestStudio checks the recorded process. It offers the
standard MFC recovery only when that process no longer exists; a second application
instance never steals the active instance's autosave. A normal shutdown removes the
catalog. Windows Restart Manager remains supported, but automatic OS relaunch is no
longer required for recovery after **End task**.

This mechanism is separate from `*.atd.tmp`: the session autosave protects edits that
have not reached Save, while the temporary file protects a Save operation interrupted
during filesystem replacement.

## Fault log events

Recovery uses the existing fault handling service and records:

- `RECOVERY_CANDIDATE_DETECTED`
- `RECOVERY_INVALID_PREVIOUS_VERSION`
- `RECOVERY_INVALID_INTERRUPTED_SAVE`
- `RECOVERY_INSPECTION_FAILED`
- `RECOVERY_DECLINED`
- `RECOVERY_CANCELLED`
- `RECOVERY_STARTED`
- `RECOVERY_COMPLETED`
- `RECOVERY_FAILED`
- `RECOVERY_UNEXPECTED_CLOSE_RESTARTED`
- `RECOVERY_UNEXPECTED_CLOSE_DETECTED`
- `RECOVERY_UNEXPECTED_CLOSE_FAILED`
- `RECOVERY_AUTOSAVE_FAILED`
- `RECOVERY_AUTOSAVE_CATALOG_FAILED`
- `RECOVERY_AUTOSAVE_CATALOG_INVALID`
