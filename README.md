# Ottersnap - Snapshot Image Viewer

**Ottersnap** is a Qt6 application for viewing images using high-performance Vulkan rendering while automatically tracking and preserving their history. When you open an image file, it is watched for changes — make an update to the image, and the old state is snapshotted onto a timeline allowing you to keep a complete collection of the image's history.

![Ottersnap](docs/images/preview.png)

## Features

- **Image Timeline** — Versions of an image are saved to a timeline, allowing you to go back to any captured version.
- **Automatic Version Capture** — Images are monitored for live changes; on change, a new snapshot is added to the timeline.
- **Effects** — Adjust the view of the image using modifiers such as grayscale and mirroring.
- **Color Picking** — Extract dominant color clusters and pick colors from the image.

## Build & Run

### Prerequisites

- **CMake ≥ 3.17**
- **Qt 6.5+**
- **libzip**
- **libzstd**
- **glslc**

### Build

```bash
cmake -S . -B build
cmake --build build
```

### Flatpak

```bash
cd flatpak
flatpak-builder --force-clean build-flatpak com.kipwisp.Ottersnap.json
flatpak install --user ../build-flatpak/repo/com.kipwisp.Ottersnap.flatpakref
```

### Tests

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## License

This project is licensed under the [GNU General Public License v3](LICENSE).
