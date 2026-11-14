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

PrettyTreeFrog is the first included pack. Its backgrounds and system icons are
original project artwork released under CC0 1.0 Universal. Third-party packs
remain outside the distributable set until their upstream license and source
history are verified.

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

## Controls

Left/Right or L1/R1 changes tabs; Up/Down selects; A applies; B/Y returns; and
Select+Start exits. The illuminated frog marks the item currently active in
TreeFrogUI.

## Licensing

The application source is GPL-3.0-or-later. See `NOTICE.md` for the bundled
helper code and runtime-library notices. Every artwork pack carries its own
license and attribution files; those terms take precedence over the app
license.
