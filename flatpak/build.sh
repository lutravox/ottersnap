#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
MANIFEST="$SCRIPT_DIR/io.github.lutravox.Ottersnap.json"

cd "$REPO_ROOT"

if ! VERSION="$(git describe --tags --abbrev=0 --match 'v[0-9]*' | sed 's/^v//')"; then
    echo "error: HEAD is not tagged with a version tag (expected v<major>.<minor>.<patch>)" >&2
    exit 1
fi

# The manifest uses a {{VERSION}} placeholder; resolve it to the git tag.
RESOLVED="$(mktemp "${MANIFEST}.XXXXXX")"
trap 'rm -f "$RESOLVED"' EXIT
sed "s/{{VERSION}}/$VERSION/g" "$MANIFEST" > "$RESOLVED"

echo "Building $VERSION from $(git rev-parse --short HEAD)"
flatpak-builder "$@" "$RESOLVED"
