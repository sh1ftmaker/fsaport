#!/bin/bash
# Commit a built site (tools/build_pages.sh) to the gh-pages branch. Does not push.
#
#   tools/publish_pages.sh SITE_DIR [WORKTREE_DIR]   (default: build/gh-pages)
#   git -C build/gh-pages push origin gh-pages        (then, to publish)
#
# Creates the gh-pages branch (an orphan: no source history in it) and a git
# worktree for it on first use, replaces its whole content with SITE_DIR and
# commits when something changed. The branch holds only the site.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SITE=$(cd "${1:?usage: tools/publish_pages.sh SITE_DIR [WORKTREE_DIR]}" && pwd)
WT=${2:-$ROOT/build/gh-pages}
BRANCH=gh-pages

for f in index.html fsa.js fsa.wasm .nojekyll; do
  [ -e "$SITE/$f" ] || { echo "$SITE has no $f: not a site from tools/build_pages.sh" >&2; exit 1; }
done
[ ! -e "$SITE/fsa.map" ] || { echo "$SITE has fsa.map: rebuild it with tools/build_pages.sh" >&2; exit 1; }

if [ ! -e "$WT/.git" ]; then
  if git -C "$ROOT" show-ref --verify --quiet "refs/heads/$BRANCH"; then
    git -C "$ROOT" worktree add "$WT" "$BRANCH"
  elif git -C "$ROOT" show-ref --verify --quiet "refs/remotes/origin/$BRANCH"; then
    git -C "$ROOT" worktree add -b "$BRANCH" "$WT" "origin/$BRANCH"
  else
    git -C "$ROOT" worktree add --orphan -b "$BRANCH" "$WT"
  fi
fi
[ "$(git -C "$WT" symbolic-ref --short HEAD)" = "$BRANCH" ] || { echo "$WT is not on $BRANCH" >&2; exit 1; }

# Replace everything but .git with the site.
find "$WT" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
cp -a "$SITE/." "$WT/"
git -C "$WT" add -A
if git -C "$WT" diff --cached --quiet; then
  echo "gh-pages: no change"
  exit 0
fi
SRC=$(git -C "$ROOT" rev-parse --short HEAD)
git -C "$WT" commit -q -m "Site built from $SRC (tools/build_pages.sh)" ${FSA_COMMIT_TRAILER:+-m "$FSA_COMMIT_TRAILER"}
git -C "$WT" log --oneline -1
echo "committed to $BRANCH in $WT (not pushed): git -C $WT push origin $BRANCH"
