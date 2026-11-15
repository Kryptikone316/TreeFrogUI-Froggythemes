# Roleta license review

Review date: 2026-11-14

## Sources checked

- Roleta: [`EmuELEC/es-theme-EmuELEC-Roleta`](https://github.com/EmuELEC/es-theme-EmuELEC-Roleta), pinned at [`8d9e2dc`](https://github.com/EmuELEC/es-theme-EmuELEC-Roleta/commit/8d9e2dcd60dde4f700dc04c7bc7c502cbec99137).
- Ancestor: [`RetroHursty69/es-theme-HyperMameinaBox`](https://github.com/RetroHursty69/es-theme-HyperMameinaBox), pinned at [`33443f5`](https://github.com/RetroHursty69/es-theme-HyperMameinaBox/commit/33443f5bfe2133925353b075cfc339b467830948).

## Findings

The Roleta README says:

> es-theme-EmuELEC-Roleta based on https://github.com/RetroHursty69/es-theme-HyperMameinaBox with changes from drixplm

The pinned Roleta tree contains that README but no `LICENSE`, `COPYING`,
`NOTICE`, or credits file. The pinned HyperMameinaBox tree likewise has no
explicit license or attribution terms in its root rights files, and its GitHub
license metadata is unset. The README establishes an attribution chain, not a
redistribution license.

## Decision

`EmuELEC_Roleta` remains **Hold**. Do not include its converted backgrounds or
icons in a release, preset, or downloadable pack until the relevant rights
holders provide written redistribution terms. Keep the source fork and this
review record for provenance, and preserve the README attribution if permission
is obtained.

Permission should identify the artwork covered, allowed redistribution,
required credits, modification terms, and whether commercial use is allowed.
