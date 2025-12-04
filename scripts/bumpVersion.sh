#!/usr/bin/env bash
set -e

FILE="src/version.h"

# Nothing to do if file missing
[ -f "$FILE" ] || exit 0

############################
# 1) Bump VERSION
############################

ver=$(grep -oP '(?<=VERSION\s*=\s*")[0-9]+\.[0-9]+\.[0-9]+(?=")' "$FILE" || true)

if [ -n "$ver" ]; then
    IFS=. read -r major minor patch <<< "$ver"

    patch=$((patch + 1))
    new="$major.$minor.$patch"

    # Update VERSION line
    sed -i "s/VERSION\s*=\s*\"$ver\"/VERSION = \"$new\"/" "$FILE"
    
    git add $FILE

    echo "Auto-bumped VERSION to $new"
fi

############################
# 2) Set COMMIT string
############################

# Ensure we are in a git repo
if git rev-parse --git-dir >/dev/null 2>&1; then
    # Short hash of current HEAD (latest committed revision)
    hash=$(git rev-parse --short HEAD 2>/dev/null || echo "NOHEAD")

    # Update COMMIT line if it exists
    if grep -q 'COMMIT' "$FILE"; then
        sed -i "s/COMMIT *= *\"[^\"]*\"/COMMIT = \"$hash\"/" "$FILE"
        echo "Set COMMIT to $commit_str"
    fi

fi
