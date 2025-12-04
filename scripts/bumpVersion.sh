#!/usr/bin/env bash
set -e

FILE="src/version.h"

# Nothing to do if file missing
[ -f "$FILE" ] || exit 0


ver=$(grep -oP 'VERSION\s*=\s*"\K[0-9]+\.[0-9]+\.[0-9]+' "$FILE" || true)

if [ -n "$ver" ]; then
    IFS=. read -r major minor patch <<< "$ver"

    patch=$((patch + 1))
    new="$major.$minor.$patch"

    # Update VERSION line
    sed -i "s/VERSION\s*=\s*\"$ver\"/VERSION = \"$new\"/" "$FILE"

    git add $FILE

    echo "Auto-bumped VERSION to $new"
fi
