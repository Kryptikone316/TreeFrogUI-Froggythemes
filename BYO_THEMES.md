# Bring-your-own themes and icons

Only `PrettyTreeFrog` is currently cleared for redistribution from this
repository. The release assets contain its CC0 background and system-icon
packs. The source records below are references for users who obtain other
artwork independently; they are not permission to redistribute it.

## Source links

| Pack or family | Upstream source | Current status |
|---|---|---|
| Batocera Simple Life | Source URL is still unverified | Hold; no license file found |
| Batocera SynthWave / EmuELEC Carbon | [EmuELEC carbon](https://github.com/EmuELEC/es-theme-EmuELEC-carbon) | Review; CC BY-NC-SA terms |
| muOS MuBuntu | Source URL is still unverified | Hold; license not present |
| Alekfull ARTFLIX | [Alekfull-ARTFLIX](https://github.com/fagnerpc/Alekfull-ARTFLIX) | Review; CC BY-NC-SA 4.0 |
| Alekfull NX | [Alekfull-NX](https://github.com/fagnerpc/Alekfull-NX) | Review; confirm upstream terms |
| Alekfull SOLO | [Alekfull-SOLO](https://github.com/fagnerpc/Alekfull-SOLO) | Review; confirm upstream terms |
| EmuELEC Clean Style | [EmuELEC clean-style](https://github.com/EmuELEC/es-theme-EmuELEC-clean-style) | Review; third-party credits |
| Crystal Blue | [dm2912/Crystal](https://github.com/dm2912/Crystal) | Review; non-commercial notice |
| Epic Noir | [c64-dev/es-theme-epicnoir](https://github.com/c64-dev/es-theme-epicnoir) | Review; CC BY-NC-SA 4.0 |
| Tronkyfran | [es-theme-tronkyfran](https://github.com/HerbFargus/es-theme-tronkyfran) | Review; non-commercial terms |
| EmuELEC Roleta | See [`sources/roleta-license-review.md`](sources/roleta-license-review.md) | Hold; obtain permission first |

## Convert an independently obtained source

1. Review the upstream license, credits, and any restrictions before use.
2. Download or clone the source directly from its upstream project. Do not
   treat a fork, this table, or a converted output as a license grant.
3. Extract the archive into a local folder; the converter does not unpack
   archives.
4. Run the local converter as described in
   [`converter/README.md`](converter/README.md):

   ```sh
   python3 converter/server.py
   ```

5. Use the converter's output for personal use or other uses allowed by the
   upstream terms. Do not upload the generated pack here unless its complete
   artwork, adaptations, and credits have been reviewed and cleared.

The full provenance and status record is in
[`sources/theme-sources.md`](sources/theme-sources.md).
