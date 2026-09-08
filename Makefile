# glfwm - GLFW + WebGPU + Mobile
#
# Builds:
#   - glfw3webgpu bridge library (GLFW -> WebGPU surface creation)
#   - GLFW (static, from source) and a combined GLFW + glfw3webgpu shared lib
#   - wgpu-native (from source, requires cargo)
#   - GLFW with the GLFM platform backend (mobile targets + macOS host dev)
#
# Common Lisp bindings live in ../cl-webgpu (the webgpu_shim layer).

# Detect platform
UNAME_S := $(shell uname -s)

# Compiler settings
CC ?= cc
CFLAGS ?= -O2 -fPIC -Wall -Wextra
LDFLAGS ?= -shared

# Output library names
ifeq ($(UNAME_S),Darwin)
    GLFW_COMBINED_LIB = shim/libglfw3.dylib
    GLFW_WEBGPU_LIB = shim/libglfw3webgpu.dylib
    GLFW_STATIC = deps/glfw/build/src/libglfw3.a
    WGPU_NATIVE_LIB = libwgpu_native.dylib
    WGPU_NATIVE_TARGET = deps/wgpu-native/target/release/$(WGPU_NATIVE_LIB)
    UNDEFINED_FLAGS = -Wl,-undefined,dynamic_lookup
else ifeq ($(OS),Windows_NT)
    GLFW_COMBINED_LIB = shim/glfw3.dll
    GLFW_STATIC = deps/glfw/build/src/libglfw3.a
    WGPU_NATIVE_LIB = wgpu_native.dll
    WGPU_NATIVE_TARGET = deps/wgpu-native/target/release/$(WGPU_NATIVE_LIB)
else
    GLFW_COMBINED_LIB = shim/libglfw3.so
    GLFW_STATIC = deps/glfw/build/src/libglfw3.a
    WGPU_NATIVE_LIB = libwgpu_native.so
    WGPU_NATIVE_TARGET = deps/wgpu-native/target/release/$(WGPU_NATIVE_LIB)
endif

# Include paths for headers
CFLAGS += -Ideps -Ideps/glfw/include -Ideps/glfw3webgpu

# Platform-specific flags
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -dynamiclib $(UNDEFINED_FLAGS)
    GLFW_LDFLAGS = -framework Cocoa -framework IOKit -framework CoreFoundation -framework CoreVideo -framework QuartzCore
    GLFW_DEFINES = -D_GLFW_COCOA
else ifeq ($(OS),Windows_NT)
    GLFW_DEFINES = -D_GLFW_WIN32
else
    # Assume Linux - could be X11 or Wayland, default to X11 for now
    GLFW_DEFINES = -D_GLFW_X11
endif

GLFW3WEBGPU_SRC = deps/glfw3webgpu/glfw3webgpu.c

.PHONY: all clean libwgpu-native libglfw

all: $(GLFW_WEBGPU_LIB)

# Build glfw3webgpu bridge library.
# Uses -undefined dynamic_lookup so it shares whatever GLFW instance is
# already loaded in the process (e.g. from cl-glfw3 / homebrew GLFW).
$(GLFW_WEBGPU_LIB): $(GLFW3WEBGPU_SRC)
	@mkdir -p shim
ifeq ($(UNAME_S),Darwin)
	$(CC) -x objective-c $(CFLAGS) $(GLFW_DEFINES) -dynamiclib \
	  -I deps/glfw/include -I deps/webgpu -I deps/glfw3webgpu \
	  -DGLFW_EXPOSE_NATIVE_COCOA \
	  $(GLFW3WEBGPU_SRC) \
	  $(UNDEFINED_FLAGS) \
	  -framework Cocoa -framework IOKit -framework QuartzCore \
	  -install_name @rpath/libglfw3webgpu.dylib \
	  -o $@
else
	$(CC) $(CFLAGS) $(GLFW_DEFINES) -shared \
	  -I deps/glfw/include -I deps/webgpu -I deps/glfw3webgpu \
	  $(GLFW3WEBGPU_SRC) \
	  $(UNDEFINED_FLAGS) \
	  -o $@
endif

# Build combined GLFW + glfw3webgpu shared lib (requires GLFW static build first)
$(GLFW_COMBINED_LIB): $(GLFW_STATIC)
	@mkdir -p shim
ifeq ($(UNAME_S),Darwin)
	$(CC) -x objective-c $(CFLAGS) $(GLFW_DEFINES) -dynamiclib \
	  -I deps/glfw/include -I deps/webgpu -I deps/glfw3webgpu \
	  -DGLFW_EXPOSE_NATIVE_COCOA \
	  $(GLFW3WEBGPU_SRC) \
	  -x none -all_load $(GLFW_STATIC) \
	  $(GLFW_LDFLAGS) \
	  $(UNDEFINED_FLAGS) \
	  -o $@
else
	$(CC) $(CFLAGS) $(GLFW_DEFINES) -shared \
	  -I deps/glfw/include -I deps/webgpu -I deps/glfw3webgpu \
	  $(GLFW3WEBGPU_SRC) \
	  -Wl,--whole-archive $(GLFW_STATIC) -Wl,--no-whole-archive \
	  $(GLFW_LDFLAGS) \
	  $(UNDEFINED_FLAGS) \
	  -o $@
endif

# Build GLFW as a static library from source
libglfw: $(GLFW_STATIC)

$(GLFW_STATIC):
	@echo "Building GLFW (static) from source..."
	mkdir -p deps/glfw/build
	cd deps/glfw/build && cmake .. \
	  -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF \
	  -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
	$(MAKE) -C deps/glfw/build -j$$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Build wgpu-native from source in deps/wgpu-native
# Requires Rust toolchain with cargo.
libwgpu-native:
	@echo "Building wgpu-native from source..."
	cd deps/wgpu-native && cargo build --release
	@echo "Built: $(WGPU_NATIVE_TARGET)"

# Convenience target: build all dependencies and libraries
build-all: libglfw libwgpu-native all

clean:
	rm -f $(GLFW_WEBGPU_LIB) $(GLFW_COMBINED_LIB)

# --- GLFM platform backend ---------------------------------------------------
# Builds GLFW with the GLFM platform backend. GLFM supports macOS natively, so
# this also serves as the host development target (no simulator needed).

GLFM_DIR = platform/glfm
GLFM_BUILD_DIR = build/glfm-host
GLFM_HOST_LIB = $(GLFM_BUILD_DIR)/libglfw3_glfm.a

GLFM_INCLUDES = -Ideps/glfw/include -Ideps/glfw/src -Ideps/glfm -I$(GLFM_DIR)
GLFM_CFLAGS = -D_GLFW_GLFM -O2 -Wall -Wextra -fPIC

# GLFW core sources for the GLFM platform build. The Null platform sources are
# included because platform.c always references _glfwConnectNull; the result is
# a binary supporting both GLFW_PLATFORM_GLFM and GLFW_PLATFORM_NULL.
GLFW_GLFM_CORE_SRCS = \
	init.c \
	context.c \
	input.c \
	monitor.c \
	platform.c \
	window.c \
	vulkan.c \
	egl_context.c \
	osmesa_context.c \
	macos_time.c \
	posix_thread.c \
	posix_module.c \
	null_joystick.c \
	null_init.c \
	null_monitor.c \
	null_window.c

GLFM_BACKEND_SRCS = \
	glfm_init.c \
	glfm_monitor.c \
	glfm_window.c \
	glfm_entry.c

GLFM_HOST_OBJS = \
	$(addprefix $(GLFM_BUILD_DIR)/glfw_,$(GLFW_GLFM_CORE_SRCS:.c=.o)) \
	$(addprefix $(GLFM_BUILD_DIR)/glfm_backend_,$(GLFM_BACKEND_SRCS:.c=.o)) \
	$(GLFM_BUILD_DIR)/glfm_apple.o

.PHONY: glfm-host clean-glfm

glfm-host: $(GLFM_BUILD_DIR)/hello_glfm

$(GLFM_BUILD_DIR):
	mkdir -p $(GLFM_BUILD_DIR)

$(GLFM_BUILD_DIR)/glfw_%.o: deps/glfw/src/%.c | $(GLFM_BUILD_DIR)
	$(CC) $(GLFM_CFLAGS) $(GLFM_INCLUDES) -c $< -o $@

$(GLFM_BUILD_DIR)/glfm_backend_%.o: $(GLFM_DIR)/%.c | $(GLFM_BUILD_DIR)
	$(CC) $(GLFM_CFLAGS) $(GLFM_INCLUDES) -c $< -o $@

$(GLFM_BUILD_DIR)/glfm_apple.o: deps/glfm/glfm_apple.m | $(GLFM_BUILD_DIR)
	$(CC) $(GLFM_CFLAGS) $(GLFM_INCLUDES) -x objective-c -c $< -o $@

$(GLFM_HOST_LIB): $(GLFM_HOST_OBJS)
	libtool -static -o $@ $^

$(GLFM_BUILD_DIR)/hello_glfm.o: test/glfm_hello.c | $(GLFM_BUILD_DIR)
	$(CC) -DGLFM_GLFW_PLATFORM -O2 -Wall -Wextra -Ideps/glfw/include -I$(GLFM_DIR) -c $< -o $@

$(GLFM_BUILD_DIR)/hello_glfm: $(GLFM_BUILD_DIR)/hello_glfm.o $(GLFM_HOST_LIB)
	$(CC) $^ -o $@ \
	  -framework AppKit -framework Foundation -framework Metal -framework MetalKit \
	  -framework IOKit -framework Carbon -framework CoreFoundation -framework QuartzCore

clean-glfm:
	rm -rf $(GLFM_BUILD_DIR)
