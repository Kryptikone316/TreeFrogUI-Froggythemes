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

When already inside the `converter/` directory, use either `python3
server.py` or `./server.py` on Linux/macOS. The included `server.py` has
executable permissions and uses its own shebang.

5. Open `http://127.0.0.1:8765` in a browser. Stop the server with `Ctrl+C`.

The upper-left **Stop server** button is an alternative to `Ctrl+C`. It
requires choosing **Yes** in its confirmation dialog. If conversion or
installation is active, the server cancels those jobs, stops their child
processes, waits for them to settle, and then exits. Closing the browser tab
alone does not stop the server.

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

## Integrity check and ledger

Conversion is transactional. The converter writes temporary pack folders, then
checks every generated image before publishing it under `output/`. Backgrounds
must be native-size JPEGs with no alpha channel; icons must be valid PNGs within
the configured limit. Each published pack contains a `MANIFEST.sha256` file.
The Install step remains disabled until these checks pass.

Every conversion and SD-card copy appends to `output/log.txt`. The ledger records
timestamps, job IDs, pack categories, file sizes, and SHA-256 hashes. The SD-card
workflow validates the source pack, copies into a temporary card-side folder,
validates that copy again, and only then commits it to the final TreeFrogUI
directory. If a check fails, temporary files are removed and existing packs are
left untouched. Review the readable error in the progress panel and `log.txt`,
then correct the source or settings and start a new job.

## Optional SD-card installation

After conversion, installation is user-triggered. Mount the SD card normally,
choose its mount root in the Install step, and confirm that the folder contains
the configured `frogui/` directory. **Copy packs to SD card** places generated
backgrounds in `frogui/theme-packs/` and icons in `frogui/icon-packs/`. The
tool does not mount cards or use USB/MTP. Existing generated packs require the
replacement checkbox; unrecognized folders are never overwritten.

If you prefer to copy files yourself, click **Open output folder** and use the
operating system's file manager. The two staging folders mirror the card
destinations.

## Configuration

`config.json` is the maintenance point for TreeFrogUI paths, staging folder
names, device panel sizes, source-map filenames, and image limits. If a future
TreeFrogUI release moves its pack directories, update this file before using
the converter. Restart the local server after changing it.
