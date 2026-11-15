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

The device selector is explicit: R36SX/R36 HD/GB350 map to 640×480, while
SF3000/SF3000 HD/SF3100/SF3500 map to 854×480. Only background output
dimensions change; icon normalization remains capped at 160×130.

`config.json` is the single maintenance point for those device definitions,
TreeFrogUI card paths, staging paths, source maps, and image limits. The
The browser loads its device list and default device from `/api/config`; restart
the server after editing the file.

The optional deployment step only copies after an explicit button click. It
requires a selected mounted card root containing the configured TreeFrogUI
directory. Manual copying remains available through the OS file-explorer
button.

## Integrity and transaction boundaries

Conversion and deployment are single active jobs. A browser reload asks
`/api/current-job` for the queued, running, or most recent terminal job and
continues polling it instead of starting another conversion. New conversion or
deployment requests are rejected while a job is active.

Generated packs are built below a job-specific temporary directory. The server
writes `MANIFEST.sha256`, checks hashes, image format, native dimensions,
channels, and icon limits, and only then atomically promotes the pack. SD-card
deployment repeats validation after copying to a card-side staging directory;
existing generated packs are moved aside only during the final commit and are
restored if that commit fails. Temporary directories are removed on success or
failure. `output/log.txt` is an append-only human-readable ledger containing
job events and SHA-256 records for generated and copied files.

Shutdown is also explicit. `/api/shutdown` accepts only the exact confirmation
string `Yes`; it marks the server as closing, cancels queued/running jobs,
waits up to ten seconds for ImageMagick or copy work to settle, and then calls
the HTTP server shutdown. `Ctrl+C` uses the same cancellation and wait path.

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
