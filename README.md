# Ottersnap

**Ottersnap** is a Qt6 application for viewing images rendered using Vulkan while automatically tracking and preserving their history. When you open an image file, it is watched for changes - make an update to the image, and the old state will get snapshotted onto a timeline that can be browsed inside of Ottersnap.

## Features

- **Image Timeline** — Versions of an image are saved to a timeline allowing you to go back to any captured version.
- **Automatic Version Capture** — Images can be automatically monitored for changes. On change, a new snapshot gets added to the timeline for the image.
- **Vulkan-Accelerated Viewer** — GPU-rendered image display using Vulkan.
- **Modifiers** — Adjust the view of the image using filters such as mirroring or grayscale.
- **Session Persistence** — Open tabs can be saved on exit and restored on launch.

## Build & Run

### Prerequisites

- **Qt 6.5+**
- **CMake ≥3.17**
- **glslc**

### Build

```bash
cmake --preset=release
cmake --build --preset=release
```

### Flatpak

```bash
cd flatpak
flatpak-builder --force-clean build-flatpak com.kipwisp.Ottersnap.json
flatpak install --user ../build-flatpak/repo/com.kipwisp.Ottersnap.flatpakref
```

## License

This project is licensed under the [GNU General Public License v3](LICENSE).

## Acknowledgments

Built with Qt6.
