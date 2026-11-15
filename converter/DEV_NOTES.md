# Converter development notes

## Design

`server.py` is a Python standard-library HTTP server bound to localhost by
default. `static/` is the browser UI. The server keeps conversion jobs in
memory and runs ImageMagick one file at a time, which makes progress and
cancellation visible without introducing a build system or a web framework.

The converter is intentionally generic: it receives an extracted source
folder, a selected main background, and optional artwork folders. It uses the
shared TreeFrogUI alias map to find per-system images by filename stem, then
applies the same native output constraints used by the maintained shell
porting scripts.

## Safety and licensing

The app is local-only and does not accept file uploads. Paths are resolved on
the machine running the server. Conversion notes are provenance, not a
permission grant. Never label third-party artwork CC0 without an explicit
license record, and preserve attribution and source terms in the pack
repository.

## Future work

- Add source-layout profiles for common Batocera, EmuELEC, muOS, and Onion
  layouts when a generic filename match is not sufficient.
- Add an optional dry-run report showing every alias that matched or skipped.
- Add image dimension/channel inspection before conversion.
- Add a native packaged launcher for Windows, macOS, and Linux.
