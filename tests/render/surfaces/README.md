# surfaces: native OpenGL in a screen

`affrescoui_surfaces` shows `surfaces.screen.json` with three GPU surfaces registered (`Surfaces.hpp`): a plasma
shader, a turning cube and a blur. Each is a `"surface"` node whose drawing is the host's own OpenGL.

```sh
cmake --build build --target affrescoui_surfaces
./build/affrescoui_surfaces [screen.json]
```

The rendering check draws the same surfaces: its runner (`../runner.cpp`) registers them at a fixed time for
`../scenarios/surfaces.json`, whose screen is `../screens/surfaces.screen.json`.
