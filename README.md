# glfwm

[GLFW](https://github.com/glfw/glfw) + [GLFM](https://github.com/brackeen/glfm) windowing, with an optional [wgpu-native](https://github.com/gfx-rs/wgpu-native) flavor.

One codebase builds two libraries:

- **glfwm** - GLFW (with the GLFM mobile platform backend): windowing and
  input for iOS, Android, Emscripten, and macOS host development.
- **glfwmw** - glfwm + wgpu-native, merged into a single static library with
  a branded surface API (`glfwmwCreateWindowWGPUSurface`).

See [docs/libraries.md](docs/libraries.md) for the two flavors, the
`GLFWM_WGPU` build macro, and build/linking details.

## Libraries

One codebase builds two library flavors, split by the `GLFWM_WGPU`
preprocessor guard.

### Flavors

| Library | Contents | Guard macro |
|---|---|---|
| `libglfwm.a` | GLFW (static, with the GLFM platform backend) + GLFM. Windowing and input only; rendering is `GLFW_NO_API` via the native view/window. | - |
| `libglfwmw.a` | `libglfwm` + the WebGPU surface bridge (glfw3webgpu, GLFM-patched) + wgpu-native, merged into a single static library. | `GLFWM_WGPU` |

The WebGPU-specific code (surface bridge, branded surface API) is guarded by
`GLFWM_WGPU` and only compiled into glfwmw artifacts. Core sources are shared
between both flavors.

### Public header

`platform/glfm/glfwm.h` is the single public header for both flavors:

- Core (always present): entry-point bridge (`glfwmSetEntryPoint`,
  `GLFM_GLFW_APP_MAIN`) and the Apple Metal accessors
  (`glfwmGetMetalView`/`glfwmGetMetalLayer`).
- Guarded section (`#ifdef GLFWM_WGPU`): `glfwmwCreateWindowWGPUSurface()`
  plus `#include <webgpu/webgpu.h>`.

Applications include the same header either way. When building against
glfwmw, define `GLFWM_WGPU` (and include `<GLFW/glfw3.h>` before `glfwm.h`)
so the guarded section matches the linked library; a mismatch surfaces as a
link error for the surface API.

### Branded surface API

`glfwmwCreateWindowWGPUSurface(instance, window)` is implemented in
`src/glfwmw_surface.c` (flavor-neutral, compiled into every glfwmw artifact)
and forwards to the glfw3webgpu bridge, which resolves the native surface per
platform: the backend's dedicated CAMetalLayer on GLFM/Apple, the Cocoa
window's layer on desktop macOS, and so on.

### Build targets

```
make glfwm-host        # libglfwm.a + hello smoke test (build/glfm-host)
make glfwm-ios         # libglfwm.a (iOS device)
make glfwm-ios-sim     # libglfwm.a (iOS simulator)

make glfwmw-host       # libglfwmw.a + triangle example (build/glfm-host)
make glfwmw-ios        # libglfwmw.a (iOS device)
make glfwmw-ios-sim    # libglfwmw.a (iOS simulator)

make wgpu-host|ios|sim # wgpu-native static libs (prerequisites of glfwmw-*)
make all               # bridge-only dylib + desktop glfwmw shim dylib
```

`libglfwmw.a` merges the core objects, the bridge object, the branded surface
API object, and `libwgpu_native.a` with `libtool -static`, so consumers link
one library instead of three.

### Desktop glfwmw shim

`shim/libglfwmw.dylib` (renamed from `shim/libglfw3.dylib`) is the desktop
glfwmw flavor for macOS: GLFW (Cocoa) + the WebGPU surface bridge + the
branded surface API. wgpu-native is intentionally **not** linked into it;
`-Wl,-undefined,dynamic_lookup` lets it share whatever wgpu library the host
process loads (the Common Lisp FFI in `../cl-webgpu` loads `libwgpu_native`
separately). The bridge-only `shim/libglfw3webgpu.dylib` remains available
for setups that link their own GLFW.

### Linking notes

- Xcode (iOS): link `-lglfwmw` plus the frameworks listed in
  `xcode/project.yml`; define `GLFWM_WGPU=1` for app sources.
- Host: `make glfwmw-host && ./build/glfm-host/triangle`.
- glfwm-only builds must **not** define `GLFWM_WGPU`; the wgpu-native and
  webgpu-headers dependencies are still fetched but unused.

### Platform behavior notes (GLFM backend)

- **Touch → mouse**: touch 0 drives cursor position and button 0; touches
  1–7 map to buttons 1–7 (no cursor movement); touches above 7 are ignored.
  Button events carry no modifier state (GLFM provides none on touch). When
  a physical mouse is connected, GLFM reports its button number in `touch`,
  which maps unchanged.
- **Scroll**: deltas are normalized to GLFW's line convention — pixel
  deltas ÷10 (Cocoa `ScrollWheelPrecision` parity), line deltas ×1, page
  deltas ×10 (constants in `glfm_window.c`).
- **Focus/iconify**: app background/foreground maps to the GLFW iconify +
  focus events; `glfwGetWindowAttrib(GLFW_FOCUSED/ICONIFIED)` tracks them.
- **Surface lifecycle**: created/resized fire framebuffer-size **and**
  window-size events; destroyed fires a close request (treated as app quit;
  Android backgrounding needs a different mapping when that target lands).
- **Clipboard**: set is synchronous. Get is sync-emulating:
  `glfwGetClipboardString` waits up to 1s for GLFM's async delivery, then
  falls back to the cached string (also used when no clipboard is available
  or delivery is NULL). On the web, reads may require a user gesture — the
  fallback then applies.
- **Cursor mode**: `GLFW_CURSOR_DISABLED` hides the cursor but does not
  lock it (mobile has no pointer lock); maps to `glfmSetMouseCursor`.
- **Workarea**: the monitor workarea subtracts GLFM display chrome insets
  (status/navigation bars, safe area).
- **Key repeat**: virtual keyboards on iOS/Android are inconsistent;
  `GLFW_REPEAT` may not fire for all keys.
- **Host input regression**: `make glfwm-host-input` drives
  `test/glfm_hello.c` with synthetic events (test/glfm_driver.m); requires
  Accessibility permission for the calling terminal (CGEventPost).

## License

[MIT](LICENSE)
