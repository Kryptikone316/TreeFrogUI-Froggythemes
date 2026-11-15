# FroggyThemes

FroggyThemes is a small theme manager for TreeFrogUI on SF3000/SF3500-class
devices. It switches between theme and system-icon folders already installed
on the SD card; it does not rewrite or duplicate the selected artwork.

The app provides independent Backgrounds and Icons tabs, matching presets, a
reset-to-base-layout section, previews, and an active-pack marker. Applying an
icon pack selects TreeFrogUI's System View so the icons are visible when the
frontend returns.

## Repository layout

| Path | Purpose |
|---|---|
| `src/` | FroggyThemes source and self-contained SF3500 display/input helpers |
| `pack/` | On-device launcher, runtime libraries, and active marker |
| `theme-packs/` | Optimized TreeFrogUI background packs |
| `icon-packs/` | Optimized TreeFrogUI system-icon packs |
| `sources/` | Provenance, license status, and fork plan for imported artwork |
| `converter/` | Local browser-based converter for already-extracted source folders |

PrettyTreeFrog is the first included pack. Its backgrounds and system icons are
original project artwork released under CC0 1.0 Universal. Third-party packs
remain outside the distributable set until their upstream license and source
history are verified.

### Bring-your-own-theme candidates

`EmuELEC_Roleta` may be considered a bring-your-own-theme candidate for users
who obtain it independently. It is not distributed, packaged, or included in
our presets because its README identifies a HyperMameinaBox ancestor and
changes from `drixplm`, but neither source provides explicit redistribution
terms or a license/credits notice. Users must obtain permission from the
relevant rights holders and preserve the attribution chain before sharing or
redistributing it.

The standalone converter runs locally with Python and ImageMagick. It creates
native-size background JPEGs for the selected device family and optimized
system-icon PNGs while showing scan and conversion progress. See
[`converter/README.md`](converter/README.md).

After conversion, users may explicitly copy the generated packs to a mounted
SD card or open the output folder for manual copying. The card must be mounted
and selected through the explorer; FroggyThemes does not use USB/MTP or mount
devices automatically. TreeFrogUI paths and device definitions are maintained
in [`converter/config.json`](converter/config.json).

The converter also has an upper-left Stop server control. It requires an
explicit Yes confirmation and gracefully cancels active conversion or card-copy
work before exiting; Ctrl+C remains supported.

The Install step is gated by an integrity check. Generated packs contain
`MANIFEST.sha256`, and the converter keeps an `output/log.txt` ledger with file
sizes and SHA-256 hashes. Card deployment validates the source and the staged
card copy before committing it, so a failed check leaves the existing pack
untouched.

### Using the converter on another computer

Download the repository from GitHub and extract it. The converter requires
Python 3.9 or newer, ImageMagick 6 or 7 with PNG/JPEG support, and a modern
web browser. No Node.js, npm, Python packages, or internet connection are
needed after installation. The theme archive must already be extracted into a
folder before it is selected; the converter does not unpack archives.

From a terminal opened in the repository, run `python3 converter/server.py` on
Linux/macOS or `py converter/server.py` in Windows PowerShell, then open
`http://127.0.0.1:8765`. The converter's startup badge checks ImageMagick and
the full dependency instructions are available in
[`converter/README.md`](converter/README.md).

## Build

The target is the Datafrog MIPS32r2 toolchain with FreeType, JPEG, and PNG
development files in the sysroot:

```sh
make
```

Override `TOOLCHAIN` and `SYSROOT` for another HCLinux/Buildroot checkout. The
result is `froggythemes.mipsel`. Copy it beside the files in `pack/` under
`roms/ports/froggythemes/`.

Run `scripts/validate_packs.sh` to check the included artwork, then
`scripts/package.sh` to assemble the app and cleared packs into an SD-card
layout under `dist/FroggyThemes/`.

The launcher expects `/mnt/sdcard/frogui/theme-packs/` and
`/mnt/sdcard/frogui/icon-packs/`. The pack files are copied into those
locations by the TreeFrogUI release process or by the accompanying SD-card
deployment workflow.

At startup, FroggyThemes registers a Pack preset only when an identical folder
name exists in both directories. A background-only pack appears under Themes;
an icon-only pack appears under Icons.

## Controls

Left/Right or L1/R1 changes tabs; Up/Down selects; A applies; B/Y returns; and
Select+Start exits. The illuminated frog marks the item currently active in
TreeFrogUI.

## Licensing

The application source is GPL-3.0-or-later. See `NOTICE.md` for the bundled
helper code and runtime-library notices. Every artwork pack carries its own
license and attribution files; those terms take precedence over the app
license.
