# trv
A command-line utility written in C that traverses directories and displays file extensions alongside their actual file signatures, allowing users to identify mismatches between file names and actual file contents.
## Installation
### Clone the repository
```bash
git clone https://github.com/rmasondev/trv.git
cd trv
```
### Compile source code
```bash
gcc trv.c -o trv
```
### Install the executable
```bash
sudo install -m 755 trv /usr/local/bin/trv
```
## Usage
```bash
trv <path> [options]
```
## Output Fields
|Field    |Description      |
|---------|-----------------|
|Path     |Path of the file or directory        |
|Extension|Extension extracted from the filename|
|Signature|Detected file type                   |
|Level    |Depth relative to the starting point |
|Directory|Whether the entry is a directory     |

An asterisk (`*`) indicates that the file type was inferred from character patterns rather than identified using magic numbers.
## Options
|Flag     |Description      |
|---------|-----------------|
|`--no-e` |Exclude Extension|
|`--no-s` |Exclude Signature|
|`--no-l` |Exclude Level    |
|`--no-d` |Exclude Directory|
## Examples
```bash
trv .
trv ~/Documents
trv Documents
trv ~/Documents --no-e --no-l
```
## Supported File Types
### Signature
- JPEG
- PNG
- GIF
- PDF
- ZIP
- SQLite
### Pattern-based detection
- HTML

## Limitations
- Maximum of 1000 entries per traversal
- Maximum path length of 499 bytes
