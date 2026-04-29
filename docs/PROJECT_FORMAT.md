# RX14 Binary Project Format (.rx14)

## 1. Overview

The `.rx14` binary project format is a TLV (Type-Length-Value) container with
CBOR-encoded payloads and blake3 integrity checksums. It replaces the earlier
ad-hoc JSON `.rx14proj` format.

```
┌──────────────────────────────┐  0x00
│  File Header (64 bytes)      │
├──────────────────────────────┤  0x40
│  Block 0: [tag][schema]      │
│    [size][checksum][payload]  │
├──────────────────────────────┤
│  Block 1: ...                │
├──────────────────────────────┤
│  ...                         │
├──────────────────────────────┤  tocOffset
│  TOC (Table of Contents)     │
└──────────────────────────────┘  EOF
```

All multi-byte integers are **little-endian** on disk.

---

## 2. File Header (64 bytes)

| Offset | Size | Type     | Field           | Description                              |
|--------|------|----------|-----------------|------------------------------------------|
| 0x00   | 4    | u32      | magic           | `0x52583134` = ASCII `"RX14"`            |
| 0x04   | 4    | u32      | formatVersion   | Currently `1`                            |
| 0x08   | 8    | u64      | totalFileSize   | Total size of the file in bytes          |
| 0x10   | 4    | u32      | tocOffset       | Byte offset of the TOC from file start   |
| 0x14   | 4    | u32      | tocBlockCount   | Number of entries in the TOC             |
| 0x18   | 16   | u8[16]   | bodyChecksum    | blake3 hash (truncated to 16 bytes) of   |
|        |      |          |                 | all bytes from 0x40 to tocOffset         |
| 0x28   | 24   | u8[24]   | reserved        | Must be zero; reserved for future use    |

**Total: 64 bytes (0x40)**

The body starts immediately after the header at offset 0x40.

---

## 3. TLV Block

Each block consists of a 32-byte header followed by the payload bytes.

### Block Header (32 bytes)

| Offset | Size | Type     | Field          | Description                              |
|--------|------|----------|----------------|------------------------------------------|
| 0x00   | 4    | u32      | blockMagic     | 4-char ASCII tag (see table below)       |
| 0x04   | 4    | u32      | blockSchema    | Schema version for this block type       |
| 0x08   | 8    | u64      | payloadSize    | Size of the payload in bytes             |
| 0x10   | 16   | u8[16]   | blockChecksum  | blake3 hash (truncated to 16 bytes) of   |
|        |      |          |                | the payload bytes only                   |

**Total: 32 bytes (0x20)**

Payload bytes follow immediately after the block header.

### Known Block Types

| Magic        | Hex          | Purpose                                   |
|--------------|--------------|-------------------------------------------|
| `META`       | `0x4154454D` | Project metadata (name, ECU, dates, etc.) |
| `NOTS`       | `0x53544F4E` | User notes / logbook entries              |
| `ROM0`       | `0x304D4F52` | Current (modified) ROM binary             |
| `ROMO`       | `0x4F4D4F52` | Original (stock) ROM binary               |
| `MAPS`       | `0x5350414D` | Map definitions and calibration data      |
| `A2L0`       | `0x304C3241` | A2L / ASAP2 descriptor data               |
| `GRPS`       | `0x53505247` | Map group definitions                     |
| `VERS`       | `0x53524556` | Version/history snapshots                 |
| `LINK`       | `0x4B4E494C` | ROM link references                       |
| `STAR`       | `0x52415453` | Starred/favorited items                   |

Block payloads are CBOR-encoded unless noted otherwise (ROM0/ROMO carry raw
binary data).

---

## 4. Table of Contents (TOC)

The TOC is written at the end of the file, after all blocks. Its byte offset
is recorded in the file header's `tocOffset` field.

### TOC Preamble (8 bytes)

| Offset | Size | Type  | Field      | Description                   |
|--------|------|-------|------------|-------------------------------|
| 0x00   | 4    | u32   | tocMagic   | `0x434F5400` = ASCII `"TOC\0"`|
| 0x04   | 4    | u32   | reserved   | Must be zero                  |

### TOC Entry (20 bytes each)

| Offset | Size | Type  | Field       | Description                      |
|--------|------|-------|-------------|----------------------------------|
| 0x00   | 4    | u32   | blockMagic  | Same magic as the block header   |
| 0x04   | 8    | u64   | blockOffset | Byte offset of the block header  |
| 0x0C   | 8    | u64   | blockSize   | Total block size (header+payload)|

The number of entries equals `tocBlockCount` from the file header.

---

## 5. Forward Compatibility

**Unknown-block preservation rule**: When a reader encounters a block with an
unrecognized `blockMagic`, it MUST preserve the block verbatim (header +
payload) and include it in the TOC when re-saving. This allows older software
to round-trip files created by newer versions without data loss.

Readers MUST NOT reject a file solely because it contains unknown block types.

---

## 6. Versioning Policy

- `formatVersion` in the file header tracks breaking changes to the container
  layout (header structure, TOC format, checksum algorithm).
- `blockSchema` in each block header tracks breaking changes to that block's
  internal payload format.
- Incrementing `formatVersion` is a major event; prefer adding new block types
  or bumping `blockSchema` for individual blocks.

---

## 7. Atomic Save Contract

To prevent data corruption on crash or power loss, writers MUST:

1. Write to a temporary file (e.g., using `QSaveFile`).
2. Compute and write all checksums.
3. Flush and fsync the temporary file.
4. Atomically rename the temporary file over the target path.

If any step fails, the original file remains untouched.

---

## 8. Checksums

All checksums use **BLAKE3** truncated to 16 bytes (128 bits).

- **bodyChecksum** (in file header): covers all bytes from offset 0x40
  (body start) up to `tocOffset` (exclusive). This verifies the integrity
  of all blocks.
- **blockChecksum** (in each block header): covers only the payload bytes
  of that block. This allows per-block integrity verification without
  reading the entire file.
