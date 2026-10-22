# glad

OpenGL function loader, **generated** (not hand-written) by [glad](https://github.com/Dav1dde/glad) 0.1.36 with:

```
--profile="core" --api="gl=4.1" --generator="c" --spec="gl" --extensions=""
```

Web generator equivalent: https://glad.dav1d.de/#profile=core&language=c&specification=gl&loader=on&api=gl%3D4.1

It's vendored so the build needs no Python step. Regenerate only to change the OpenGL version or extensions.
The generated code is public domain. `include/KHR/khrplatform.h` is © The Khronos Group under the MIT-style
license in its header.

## OpenGL ES 3.0 (`gles3/`)

The loader of the `gles3` backend (`AFFRESCOUI_GL_BACKEND=gles3`: ANGLE, Mesa's EGL, WebGL2), generated the same way
by glad 0.1.36 with:

```
--profile="core" --api="gles2=3.0" --generator="c" --spec="gl" --extensions=""
```

Web generator equivalent: https://glad.dav1d.de/#profile=core&language=c&specification=gl&loader=on&api=gles2%3D3.0

It has the same header name, `glad/glad.h`, so the build compiles one loader or the other; `gles3/include` comes
first on the include path, and `include/KHR/khrplatform.h` serves both.
