# Ottersnap - Snapshot Image Viewer

[![CI](https://github.com/lutravox/ottersnap/actions/workflows/ci.yml/badge.svg)](https://github.com/lutravox/ottersnap/actions/workflows/ci.yml)
[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)

**Ottersnap** is a Qt6 image viewer that uses high-performance Vulkan rendering while automatically tracking and preserving image history. When you open an image file, it is watched for changes — make an update to the image, and the old state is snapshotted onto a timeline allowing you to keep a complete collection of the image's history.

![Ottersnap](docs/images/preview.png)

> **Note:** Ottersnap is intended only for tracking changes to a working image — **it is not a backup solution**.

## Features

- **Image Timeline** — Versions of an image are saved to a timeline, allowing you to go back to any captured version.
- **Automatic Version Capture** — Images are monitored for live changes; on change, a new snapshot is added to the timeline.
- **Effects** — Adjust the view of the image using effects such as grayscale and mirroring.
- **Color Picking** — View dominant color clusters and pick colors from the image.

## Install

Ottersnap is currently only available for **Linux** only.

Prebuilt releases are published on the [releases page](https://github.com/lutravox/ottersnap/releases).

## Build & Run

### Build Dependencies


Install build dependencies for your distribution:

#### Arch Linux

```bash
sudo pacman -S cmake ninja pkgconf qt6-base qt6-shadertools shaderc libzip zstd vulkan-headers libglvnd
```

#### Debian (Trixie)

```bash
sudo apt-get install cmake ninja-build pkgconf qt6-base-dev qt6-shadertools-dev \
  glslc libzip-dev libzstd-dev libvulkan-dev libgl-dev
```

#### Fedora 44

```bash
sudo dnf install cmake ninja-build pkgconf qt6-qtbase-devel qt6-qtshadertools-devel \
  glslc libzip-devel libzstd-devel vulkan-headers mesa-libGL-devel
```

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Flatpak Build

```bash
cd flatpak

# Build and run from the build sandbox
flatpak-builder --user --install-deps-from=flathub --force-clean --run \
  build-flatpak io.github.lutravox.Ottersnap.json

# Or export to a local repo and install into your user account
flatpak-builder --user --install-deps-from=flathub --force-clean \
  --repo=../repo build-flatpak io.github.lutravox.Ottersnap.json
flatpak remote-add --if-not-exists --user --no-gpg-verify ottersnap-local \
  "file://$(pwd)/../repo"
flatpak install --user ottersnap-local io.github.lutravox.Ottersnap
```

### Testing

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## License

This project is licensed under the [GNU General Public License v3](LICENSE).
