"""PlatformIO pre-build script for the pocket_marauder env.

Two arduino-esp32 3.x quirks break a stock PlatformIO build:

1. The bundled Arduino libraries (WiFi, Network, FS, ...) live in separate
   folders and cross-include one another, but their `library.properties` files
   declare no `depends=`. PlatformIO's LDF therefore compiles each in isolation
   and does not add its siblings' `src` dirs to the include path, so headers
   like <Network.h> / <FS.h> are not found. Fix: put every bundled lib `src`
   dir on the global include path.

2. `WiFi` reaches `Network` only through a *quoted* include
   (`#include "Network.h"` inside WiFiGeneric.h). LDF resolves quoted includes
   relative to the including file and never registers `Network` as a library,
   so its objects are never compiled -> "undefined reference to NetworkManager"
   at link time. Fix: build the `Network` library explicitly and link it.
"""

import os

Import("env")  # noqa: F821  (injected by PlatformIO)

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
libs_root = os.path.join(framework_dir, "libraries")

# (1) headers for every bundled library
include_dirs = []
if os.path.isdir(libs_root):
    for name in sorted(os.listdir(libs_root)):
        src = os.path.join(libs_root, name, "src")
        if os.path.isdir(src):
            include_dirs.append(src)
env.Append(CPPPATH=include_dirs)

# (2) force-compile + link the Network library that LDF misses
network_src = os.path.join(libs_root, "Network", "src")
if os.path.isdir(network_src):
    netlib = env.BuildLibrary(
        os.path.join("$BUILD_DIR", "NetworkForced"), network_src
    )
    env.Prepend(LIBS=[netlib])

print(
    "[pio_framework_libs] added %d include dirs; forced Network lib build"
    % len(include_dirs)
)
