# FroggyThemes Converter

FroggyThemes Converter is a small, local browser interface for turning an
already-extracted theme folder into TreeFrogUI-ready packs. It does not upload
artwork or download archives. The source archive must be extracted first.

## Dependencies

- Python 3.9 or newer (standard library only; no `pip install` is required).
- ImageMagick 6 or 7, with PNG and JPEG support.

Linux and macOS users can install ImageMagick from their normal package
manager. Windows users can use the official ImageMagick installer; the app
looks for `magick` first and `convert` second. The browser itself may be any
modern browser. Internet access is not needed after the dependencies are
installed.

## Run it

From the FroggyThemes repository:

```text
python3 converter/server.py
```

Open `http://127.0.0.1:8765` in a browser. Stop the server with `Ctrl+C`.

Choose the extracted source directory, scan it, and select the main image.
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
