# ARTestStudio Diagram (`.atd`) format

## Current format

New diagram files use UTF-8 JSON with the discriminator `ARTestStudio.Diagram`
and schema version `2`. The `.atd` extension identifies a single diagram; the
reserved `.atprj` extension will represent a multi-diagram project in a future
phase.

```json
{
  "format": "ARTestStudio.Diagram",
  "version": 2,
  "nodes": [
    {
      "id": 1,
      "kind": "rectangle",
      "position": { "x": 100, "y": 100 },
      "size": { "width": 150, "height": 100 },
      "label": "PowerON()"
    }
  ],
  "connections": [
    {
      "id": 1,
      "from": { "nodeId": 1, "port": "bottom" },
      "to": { "nodeId": 2, "port": "top" },
      "route": [
        { "x": 175, "y": 200 },
        { "x": 175, "y": 260 }
      ]
    }
  ]
}
```

## Stable values

- Node kinds: `rectangle`, `diamond`.
- Port names: `top`, `right`, `bottom`, `left`.
- Node and connection identifiers are positive unsigned 64-bit values.
- Coordinates and dimensions are integers.
- Labels are UTF-8 strings.
- `route` contains intermediate points only; endpoints are derived from ports.

## Compatibility and migration

- The reader detects JSON v2 and the legacy `ARTESTSTUDIO_DIAGRAM 1` text
  format without relying on the filename.
- Legacy v1 files remain readable and retain their node and connection IDs.
- Loading v1 does not rewrite the file. The next successful user save writes
  JSON v2 through the existing atomic-file writer.
- Unknown JSON or legacy versions return `UnsupportedVersion`; they are never
  partially loaded.
- Parsing and domain validation complete in a temporary model before replacing
  the active diagram.

## Safety limits

- Maximum file size: 16 MB.
- Maximum label size: 64 KB in UTF-8.
- Maximum nodes: 10,000.
- Maximum connections: 20,000.
- Maximum intermediate route points per connection: 1,000.
- Coordinates: -10,000,000 through 10,000,000.
- Node dimensions: 1 through 100,000.

Saving remains atomic: content is written and flushed to `<diagram>.atd.tmp`,
then replaces the destination with `MoveFileExW`. A failed serialization or
replacement leaves the previous document unchanged.
