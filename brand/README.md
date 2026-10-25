# The AffrescoUI logo

<img src="affrescoui-mark.svg" width="126" alt="The AffrescoUI mark: an arched window of four panes, a brush painting the last one">

An arched Renaissance window cut into four panes. Each pane is a *giornata*, the patch of fresh plaster a fresco painter
finishes in a day, and also a UI panel: a sidebar, a header and two content panels. A brush paints the last pane, from
the bottom up: the application is being changed while it shows, which is AffrescoUI's idea.

The mark has no text. Every file is plain filled paths with real gaps (no masks, clips or transforms), so it renders the
same in browsers, design tools and AffrescoUI's own SVG reader.

## Files

| File | Use |
|---|---|
| `affrescoui-mark.svg` | The mark, transparent: documentation, headers, anywhere on a page. |
| `affrescoui-mark-mono.svg` | One colour (`currentColor`): embossing, single-colour print, system tray. |
| `affrescoui-icon.svg` | The app icon: the mark on a white rounded tile, on the macOS 1024 grid (an 824 tile). |
| `favicon.svg` | The browser favicon, drawn on a 32-pixel grid so its panes stay sharp. |
| `favicon.ico` | The favicon for older browsers: 16, 32 and 48 pixels (16 and 32 drawn on their pixel grids). |
| `apple-touch-icon.png` | iOS home screen, 180 × 180, on white (iOS rounds the corners). |
| `icon-192.png`, `icon-512.png` | Web app manifest icons (`"purpose": "any"`), transparent. |
| `icon-maskable-512.png` | Web app manifest icon (`"purpose": "maskable"`): on white, inside the safe zone. |
| `AffrescoUI.icns` | The macOS app icon, 16 to 1024 pixels. |
| `affrescoui.ico` | The Windows app icon, 16 to 256 pixels. |
| `png/` | The mark and the app icon as PNG, 16 to 1024 pixels. |

Where the logo is used: the Launcher's sidebar (`examples/launcher`), the web demo's favicon and header (`web/`), the
documentation site's logo and favicon (`website/`) and the Linux AppImage's icon (`packaging/appimage`).

## Colours

| | Hex | |
|---|---|---|
| Lapis | `#2F4B9C` | the sidebar pane |
| Ochre | `#E3A33B` | the arch's top pane |
| Terra rossa | `#C4553D` | the third pane, the brush's handle |
| Verdigris | `#3F9A89` | the last pane (28% where it isn't painted yet), the bristles |
| Ferrule | `#C9C2B4` | the brush's metal band |

## Using it

- Keep clear space around the mark of at least a quarter of its height.
- Below 32 pixels use the files drawn for small sizes (`favicon.svg`, `favicon.ico`, `png/affrescoui-mark-16.png` and
  `-32.png`), not the master scaled down.
- Don't recolour the panes, rotate the mark, move the brush or add effects; on a busy or coloured background, use the app
  icon (on its white tile) or the one-colour mark.

The AffrescoUI name and logo identify this project. The Apache License 2.0 covers the code and doesn't grant the right
to use them as a trademark (its section 6); refer to the project with them freely.
