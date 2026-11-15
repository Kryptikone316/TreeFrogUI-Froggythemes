# FroggyThemes Converter

FroggyThemes Converter is a small, local browser interface for turning an
already-extracted theme folder into TreeFrogUI-ready packs. It does not upload
artwork or download archives. The source archive must be extracted first.

## Dependencies

- Python 3.9 or newer (standard library only; no `pip install` is required).
- ImageMagick 6 or 7, with PNG and JPEG support.
- A modern web browser (Firefox, Chrome, Edge, or Safari).
- A theme archive that has already been extracted into a folder.

No Node.js, npm, Python packages, or internet connection are required by the
converter itself. On Linux and macOS, install Python and ImageMagick through
the normal package manager or official installers. On Windows, install Python
and ImageMagick using their official installers and ensure both are available
in `PATH`. The app looks for ImageMagick's `magick` command first and its
`convert` command second.

## Run it

1. Download the FroggyThemes repository from GitHub and extract it.
2. Install the prerequisites above.
3. Open a terminal in the downloaded repository.
4. Start the local server:

```sh
# Linux/macOS
python3 converter/server.py

# Windows PowerShell
py converter/server.py
```

5. Open `http://127.0.0.1:8765` in a browser. Stop the server with `Ctrl+C`.

Choose the already-extracted source directory, scan it, and select the main
image. Do not select the downloaded ZIP itself; archive extraction is not part
of this application.
System artwork and icon folders are optional, so a theme with only one
background can still be converted. The generated staging folders are:

```text
converter/output/theme-packs/<PackName>/
converter/output/icon-packs/<PackName>/
```

Choose the target device before starting. Backgrounds use the native panel
size: R36SX, R36 HD, and GB350 use 640×480; SF3000, SF3000 HD, SF3100, and
SF3500 use 854×480. Icons remain trimmed transparent PNGs no larger than
160×130. `SOURCE-NOTES.txt` records
the source path and reminds the user to review the original license before
redistribution. The converter does not grant a license to third-party art.

The output path can be changed in the Settings step. Select “Replace a
generated pack” only when re-running a pack that already has converter notes.
