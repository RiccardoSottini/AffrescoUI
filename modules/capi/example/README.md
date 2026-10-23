# embed: AffrescoUI inside a host's OpenGL

A host application with its own GLFW window, OpenGL 4.1 context and scene (a turning quad) that embeds a screen,
`embed.screen.json`, through the C API (`affresco/affresco.h`, mode `AFR_MODE_EMBEDDED`):

- by default the screen is drawn over the scene each frame (`afr_screen_frame`), and the host forwards its input;
- with `--texture` it is rendered into a texture (`afr_screen_render_texture`) that the quad shows as it turns.
  Input isn't mapped onto the quad, so this mode only shows.

The buttons "Slower" and "Faster" are native actions the host answers (it sets `state.speed`); "Pause" is a step of
the document (`state.paused`), which the host reads when the screen reports a revision. The host loads OpenGL with
glad, the same loader it gives the screen.

```sh
cmake --build build --target affrescoui_embed
./build/affrescoui_embed            # over the scene
./build/affrescoui_embed --texture  # on the quad
AFFRESCOUI_SCREENSHOT=out/embed.ppm ./build/affrescoui_embed   # one frame, hidden, then exit
```
