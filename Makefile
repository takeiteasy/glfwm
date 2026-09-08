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
# Builds GLFW with the GLFM platform backend plus the glfw3webgpu bridge for:
#   glfm-host     macOS host development (GLFM runs natively; no simulator)
#   glfm-ios      iOS device (arm64)
#   glfm-ios-sim  iOS simulator (arm64)
#
# wgpu-native static libs are built by cargo (wgpu-host / wgpu-ios / wgpu-sim).

GLFM_DIR = platform/glfm
GLFM_MIN_IOS = 15.0
GLFM_HOST_DIR = build/glfm-host

GLFM_CFLAGS = -D_GLFW_GLFM -DGLES_SILENCE_DEPRECATION -O2 -Wall -Wextra -fPIC
GLFM_INCLUDES = -Ideps/glfw/include -Ideps/glfw/src -Ideps/glfm -I$(GLFM_DIR) \
                -Ideps/webgpu -Ideps/glfw3webgpu -Ideps

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
	glfm_entry.c \
	glfm_surface.c

# Per-config build rules: $(1) = config name, $(2) = build dir, $(3) = extra flags,
# $(4) = wgpu-native lib key (HOST/IOS/SIM)
define GLFM_config

$(1)_OBJS = $(addprefix $(2)/glfw_,$(GLFW_GLFM_CORE_SRCS:.c=.o)) \
            $(addprefix $(2)/glfm_backend_,$(GLFM_BACKEND_SRCS:.c=.o)) \
            $(2)/glfm_apple.o

$(2):
	mkdir -p $(2)

$(2)/glfw_%.o: deps/glfw/src/%.c | $(2)
	$$(CC) $$(GLFM_CFLAGS) $$(GLFM_INCLUDES) $(3) \
	  -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers \
	  -c $$< -o $$@

$(2)/glfm_backend_%.o: $(GLFM_DIR)/%.c | $(2)
	$$(CC) $$(GLFM_CFLAGS) $$(GLFM_INCLUDES) $(3) -c $$< -o $$@

# glfm_surface.c contains Objective-C on Apple targets; compile the whole
# file as Objective-C so it also builds unchanged for future non-Apple
# GLFM targets (Emscripten/Android), where it falls back to stubs.
$(2)/glfm_backend_glfm_surface.o: $(GLFM_DIR)/glfm_surface.c | $(2)
	$$(CC) $$(GLFM_CFLAGS) $$(GLFM_INCLUDES) $(3) -x objective-c -c $$< -o $$@

$(2)/glfm_apple.o: deps/glfm/glfm_apple.m | $(2)
	$$(CC) $$(GLFM_CFLAGS) $$(GLFM_INCLUDES) $(3) -x objective-c -c $$< -o $$@

$(2)/libglfw3_glfm.a: $$($(1)_OBJS)
	libtool -static -o $$@ $$^

# glfw3webgpu bridge for the GLFM platform (CAMetalLayer surface source)
$(2)/glfw3webgpu_glfm.o: deps/glfw3webgpu/glfw3webgpu.c | $(2)
	$$(CC) $$(GLFM_CFLAGS) -DGLFW_INCLUDE_NONE -DGLFW_EXPOSE_NATIVE_GLFM $$(GLFM_INCLUDES) $(3) -x objective-c -c $$< -o $$@

$(2)/libglfw3webgpu_glfm.a: $(2)/glfw3webgpu_glfm.o
	libtool -static -o $$@ $$^

clean-$(1):
	rm -rf $(2)

.PHONY: clean-$(1)

endef

GLFM_SDK_DEVICE = $(shell xcrun --sdk iphoneos --show-sdk-path)
GLFM_SDK_SIM = $(shell xcrun --sdk iphonesimulator --show-sdk-path)

$(eval $(call GLFM_config,glfm-host,$(GLFM_HOST_DIR),,HOST))
$(eval $(call GLFM_config,glfm-ios,build/ios-device,-target arm64-apple-ios$(GLFM_MIN_IOS) -isysroot $(GLFM_SDK_DEVICE),IOS))
$(eval $(call GLFM_config,glfm-ios-sim,build/ios-sim,-target arm64-apple-ios$(GLFM_MIN_IOS)-simulator -isysroot $(GLFM_SDK_SIM),SIM))

.PHONY: glfm-host glfm-ios glfm-ios-sim clean-glfm

glfm-host: $(GLFM_HOST_DIR)/hello_glfm \
           $(GLFM_HOST_DIR)/libglfw3_glfm.a $(GLFM_HOST_DIR)/libglfw3webgpu_glfm.a
glfm-ios: build/ios-device/libglfw3_glfm.a build/ios-device/libglfw3webgpu_glfm.a build/ios-device/libwgpu_native.a
glfm-ios-sim: build/ios-sim/libglfw3_glfm.a build/ios-sim/libglfw3webgpu_glfm.a build/ios-sim/libwgpu_native.a

clean-glfm: clean-glfm-host clean-glfm-ios clean-glfm-ios-sim

# --- wgpu-native (cargo) -----------------------------------------------------

WGPU_DIR = deps/wgpu-native
# Minimal feature set: Metal backend + WGSL input (sufficient for all glfwm
# targets; the C API is unaffected, so this can be changed without ABI churn).
WGPU_FEATURES = --no-default-features --features metal,wgsl

WGPU_HOST_LIB = $(WGPU_DIR)/target/release/libwgpu_native.a
WGPU_IOS_LIB = $(WGPU_DIR)/target/aarch64-apple-ios/release/libwgpu_native.a
WGPU_SIM_LIB = $(WGPU_DIR)/target/aarch64-apple-ios-sim/release/libwgpu_native.a

# Route through the toolchain pinned by deps/wgpu-native/rust-toolchain.toml:
# the rustc/cargo in PATH may be a Homebrew build that ignores the toolchain
# file and does not ship iOS std libraries.
WGPU_CARGO = $(shell cd $(WGPU_DIR) && rustup which cargo)
WGPU_RUSTC = $(shell cd $(WGPU_DIR) && rustup which rustc)

.PHONY: wgpu-host wgpu-ios wgpu-sim

wgpu-host:
	cd $(WGPU_DIR) && RUSTC="$(WGPU_RUSTC)" "$(WGPU_CARGO)" build --release $(WGPU_FEATURES)
wgpu-ios:
	cd $(WGPU_DIR) && RUSTC="$(WGPU_RUSTC)" "$(WGPU_CARGO)" build --release --target aarch64-apple-ios $(WGPU_FEATURES)
wgpu-sim:
	cd $(WGPU_DIR) && RUSTC="$(WGPU_RUSTC)" "$(WGPU_CARGO)" build --release --target aarch64-apple-ios-sim $(WGPU_FEATURES)

# Stage the wgpu-native static libs next to the GLFW libs for the Xcode app
# targets (which link everything from build/ios-*).
build/ios-device/libwgpu_native.a: $(WGPU_IOS_LIB)
	cp $< $@
build/ios-sim/libwgpu_native.a: $(WGPU_SIM_LIB)
	cp $< $@

# --- GLFM host smoke test and WebGPU example ---------------------------------

GLFM_HOST_FRAMEWORKS = \
	-framework AppKit -framework Foundation -framework Metal -framework MetalKit \
	-framework IOKit -framework Carbon -framework CoreFoundation -framework QuartzCore

$(GLFM_HOST_DIR)/hello_glfm.o: test/glfm_hello.c | $(GLFM_HOST_DIR)
	$(CC) -DGLFM_GLFW_PLATFORM -O2 -Wall -Wextra -Ideps/glfw/include -I$(GLFM_DIR) -c $< -o $@

$(GLFM_HOST_DIR)/hello_glfm: $(GLFM_HOST_DIR)/hello_glfm.o $(GLFM_HOST_DIR)/libglfw3_glfm.a
	$(CC) $^ -o $@ $(GLFM_HOST_FRAMEWORKS)

$(GLFM_HOST_DIR)/triangle.o: examples/triangle.c | $(GLFM_HOST_DIR)
	$(CC) -DGLFM_GLFW_PLATFORM -DGLFW_INCLUDE_NONE $(GLFM_CFLAGS) $(GLFM_INCLUDES) -c $< -o $@

$(GLFM_HOST_DIR)/triangle: $(GLFM_HOST_DIR)/triangle.o \
		$(GLFM_HOST_DIR)/libglfw3webgpu_glfm.a \
		$(GLFM_HOST_DIR)/libglfw3_glfm.a \
		$(WGPU_HOST_LIB)
	$(CC) $^ -o $@ $(GLFM_HOST_FRAMEWORKS)

.PHONY: glfm-host-triangle
glfm-host-triangle: $(GLFM_HOST_DIR)/triangle

# --- Xcode project (xcodegen) ------------------------------------------------
# Generates the iOS example project and builds/runs it on the simulator.
# Device builds: open the generated project, set your DEVELOPMENT_TEAM, and run.

SIM_NAME ?= iPhone 17 Pro

.PHONY: xcodegen sim-build sim-run

xcodegen:
	@mkdir -p build/xcode
	@xcodegen generate --spec xcode/project.yml --project build/xcode 2>/dev/null || { \
	  echo "xcodegen: retrying (staging copy can be swept by temp cleanup)"; \
	  xcodegen generate --spec xcode/project.yml --project build/xcode; \
	}

sim-build: wgpu-sim glfm-ios-sim xcodegen
	xcodebuild -project build/xcode/GlfwmTriangle.xcodeproj \
	  -scheme Triangle-Sim -configuration Release \
	  -destination 'generic/platform=iOS Simulator' \
	  -derivedDataPath build/xcode-build build

sim-run: sim-build
	xcrun simctl boot "$(SIM_NAME)" 2>/dev/null || true
	xcrun simctl install booted build/xcode-build/Build/Products/Release-iphonesimulator/triangle.app
	xcrun simctl launch --console-pty booted net.takeiteasy.glfwm.triangle
