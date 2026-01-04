# list_files

A high-performance file listing utility for Windows with multiple output formats and filtering options.

## Features

- Recursive directory traversal
- Multiple output formats (tree/json/list)
- File filtering (wildcard & regex)
- Display file size, modification time, type
- Relative/absolute path support
- Output to file

## Build

```bash
g++ -std=c++17 -O2 -o list_files.exe list_files.cpp
```

Requires: C++17 compatible compiler (MinGW-w64, MSVC, etc.)

## Usage

```
list_files.exe <directory> [options]
```

## Options

| Option | Description |
|--------|-------------|
| `-d, --depth <n>` | Recursion depth (default: inf) |
| `-s, --size` | Show file size |
| `-t, --time` | Show modification time |
| `-T, --type` | Show file type/extension |
| `-f, --filter <p>` | Include filter (wildcard: `*.lua` or regex: `regex:.*\.lua$`) |
| `-e, --exclude <p>` | Exclude filter |
| `--dirs-only` | List directories only |
| `--files-only` | List files only |
| `-F, --format <fmt>` | Output format: `tree` (default), `json`, `list` |
| `-r, --relative` | Use relative paths |
| `-o, --output <file>` | Output to file |
| `-h, --help` | Show help |

## Examples

```bash
# List all files in tree format
list_files.exe "C:\Users\Public"

# Show size and time, depth 2
list_files.exe "C:\Users\Public" -d 2 -s -t

# Filter by extension
list_files.exe "C:\Users\Public" -f "*.txt,*.pdf"

# JSON output for AI processing
list_files.exe "C:\Users\Public" -F json -o result.json

# Minimal output (relative paths + list format)
list_files.exe "C:\Users\Public" -F list -r
```

## Output Formats

### Tree Format (default)
Human-readable tree structure with decorations:
```
C:\Users\Public
├── [DIR]  AccountPictures
├── [DIR]  Desktop
├── [FILE] desktop.ini
```

### JSON Format
Machine-readable JSON for AI/program processing:
```json
{
  "root": "C:\\Users\\Public",
  "files": [
    {"path": "C:\\Users\\Public\\Desktop", "type": "dir", "name": "Desktop"},
    {"path": "C:\\Users\\Public\\desktop.ini", "type": "file", "name": "desktop.ini", "size": 174}
  ]
}
```

### List Format
Simple path list, one per line (minimal size):
```
C:\Users\Public\Desktop
C:\Users\Public\desktop.ini
```

## File Size Comparison (1000 files)

| Format | Size |
|--------|------|
| tree | ~80 KB |
| json | ~150 KB |
| list | ~50 KB |

## License

MIT
