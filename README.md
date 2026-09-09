# glfwm

[GLFW](https://github.com/glfw/glfw) + [GLFM](https://github.com/brackeen/glfm) windowing, with an optional [wgpu-native](https://github.com/gfx-rs/wgpu-native) flavor.

One codebase builds two libraries:

- **glfwm** - GLFW (with the GLFM mobile platform backend): windowing and
  input for iOS, Android, Emscripten, and macOS host development.
- **glfwmw** - glfwm + wgpu-native, merged into a single static library with
  a branded surface API (`glfwmwCreateWindowWGPUSurface`).

See [docs/libraries.md](docs/libraries.md) for the two flavors, the
`GLFWM_WGPU` build macro, and build/linking details.

## License

[MIT](LICENSE)
