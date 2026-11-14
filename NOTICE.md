# Notices and attribution

This repository contains software and artwork with different licenses. Do not
replace the per-component notices with the application license.

## FroggyThemes application

The FroggyThemes-specific source is Copyright (c) 2026 Kryptikone316 and is
released under the GNU General Public License, version 3 or later. The full
license text is in `LICENSE`.

## SF3500 device helpers

`src/hwdisp.c`, `src/hwdisp.h`, `src/joykey.c`, `src/joykey.h`, and
`src/tfenv.h` were copied and adapted from
[FroggySoundTuner](https://github.com/Kryptikone316/FroggySoundTuner), which
is GPL-3.0-or-later. The display path follows the technique documented in
[TreeFrogUI](https://github.com/tzubertowski/TreeFrogUI); TreeFrogUI's own
frontend remains under its upstream CC BY-NC-SA 4.0 terms.

## Runtime libraries

The MIPS shared libraries in `pack/` are bundled runtime dependencies:

- FreeType: [FreeType license](https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/LICENSE.TXT)
- libjpeg: [Independent JPEG Group license](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/LICENSE.md)
- libpng: [libpng license](https://github.com/pnggroup/libpng/blob/main/LICENSE)
- zlib: [zlib license](https://github.com/madler/zlib/blob/master/LICENSE)

Their original license terms remain applicable.

## Artwork

PrettyTreeFrog backgrounds, system icons, and the illuminated frog marker are
original project artwork released under CC0 1.0 Universal. The dedication is
repeated in each relevant asset directory.

Third-party theme and icon sources are documented in `sources/`. A pack is not
added to the distributable folders until its upstream license permits the
planned redistribution and its attribution record is complete.
