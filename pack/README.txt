Froggythemes for TreeFrogUI / SF3500

The launcher carries MIPS copies of FreeType and zlib because some cards have
ARM copies of those generic library names under cubegm/lib. It also carries
libjpeg for the theme preview pane, libpng for the icon contact sheet, and a
small RGBA frog marker for the active theme.

Open Froggythemes in the Ports list.
Left/Right or L1/R1 switches between background, system icon, and preset tabs.
Up/Down selects, A applies the active tab's setting, B or Y returns, and
Select+Start exits.

The selected background preview is shown on the right side of the screen. The
icon tab shows a six-icon contact sheet from the selected pack.
Applying an icon pack also selects TreeFrogUI System View so the icons are
visible when the frontend returns.
The Presets tab restores the base Vertical, Horizontal, or System View layout,
or applies a matching background + icon combination. Pack presets are
registered automatically at startup when the exact same folder name exists in
both frogui/theme-packs and frogui/icon-packs; one-sided packs remain in their
individual Themes or Icons tab.
Themes and icons can still be selected independently, and presets never delete
installed packs.
The illuminated frog before a pack name marks the item currently used by
TreeFrogUI.

The app uses the existing folders in frogui/theme-packs and updates frogui/settings.txt.
