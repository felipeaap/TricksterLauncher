# FileListGen — Manifest Generator

FileListGen generates a `manifest.json` file listing all game files with their SHA-256 hashes and sizes. The launcher uses this manifest to detect which files need updating.

## Usage

```
FileListGen.exe <game-folder> <output-manifest.json>
```

### Example

```powershell
FileListGen.exe "C:\Game\Trickster" "C:\CDN\manifest.json"
```

## Manifest Format

```json
{
  "version": 42,
  "files": [
    {
      "FileID": 1,
      "FileHash": "a1b2c3d4e5f6...",
      "FilePath": "data/maps/town.dat",
      "FileSize": 1048576
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `version` | `int` | Incremental version number. The launcher compares this against its local `version.dat` to know if updates exist. |
| `FileID` | `int` | Sequential file identifier. |
| `FileHash` | `string` | SHA-256 hex digest of the file contents. |
| `FilePath` | `string` | Relative path from the game root directory. Uses forward slashes. |
| `FileSize` | `int64` | File size in bytes. Used for download progress calculation. |

## Workflow

1. Build the game files to a staging directory
2. Run `FileListGen.exe` against that directory
3. (Optional) Sign the manifest — see [Manifest Signing](manifest-signing.md)
4. Upload the manifest + game files to the CDN (`cdn.selenoid.com.br` or equivalent)
5. Clients will auto-detect the new version on next launch

## Versioning

Each time you regenerate the manifest, the version number should be incremented. The launcher stores the last downloaded version in `version.dat` and only re-checks files when a newer version is available on the CDN.

For a **full check** (button in the UI), the launcher ignores the version number and re-verifies all files by hash.
