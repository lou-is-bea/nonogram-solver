#!/usr/bin/env bash
# Bootstrap the nonogram-solver development environment inside Ubuntu (WSL2).
#
# Usage (from the Ubuntu shell):
#   bash /mnt/c/Users/louis/OneDrive/Documents/nonogram-solver/scripts/setup-wsl.sh
#
# What it does (idempotent, safe to re-run):
#   1. installs the toolchain listed in CONTRIBUTING.md (GCC/Clang C23, CMake, Ninja, Criterion, ...)
#   2. signs in to GitHub with the official `gh` CLI (browser flow, done by you)
#   3. clones lou-is-bea/nonogram-solver into ~/dev/nonogram-solver (Linux filesystem, not OneDrive)
#   4. if the GitHub repository is still empty, imports the project documents from the
#      OneDrive folder and pushes them as the initial commit on `main`
set -euo pipefail

REPO="lou-is-bea/nonogram-solver"
SOURCE_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
DEST_DIR="${NG_DEST_DIR:-$HOME/dev/nonogram-solver}"

say() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# --- 1. Toolchain -----------------------------------------------------------------
. /etc/os-release
say "Ubuntu ${VERSION_ID} détecté"
# Reference toolchain: GCC 14 + Clang 20 (Clang 19 = minimum supported), same versioned
# packages on 24.04 (reference) and 26.04. The distribution defaults are NOT used.
case "${VERSION_ID}" in
  24.04|26.04) ;;
  *) echo "Version d'Ubuntu non supportée : ${VERSION_ID} (attendu : 24.04 ou 26.04)" >&2; exit 1 ;;
esac
common=(build-essential cmake ninja-build pkg-config git gh rsync curl python3
        libcriterion-dev valgrind cppcheck gcovr)
extra=(gcc-14 clang-20 clang-19 clang-format-20 clang-tidy-20 libclang-rt-20-dev llvm-20)
say "Installation des paquets (sudo demandera votre mot de passe Linux)"
sudo apt-get update
sudo apt-get install -y "${common[@]}" "${extra[@]}"

# --- 2. GitHub authentication ------------------------------------------------------
if ! gh auth status --hostname github.com >/dev/null 2>&1; then
  say "Connexion à GitHub : suivez les instructions (code à saisir dans le navigateur)"
  gh auth login --hostname github.com --git-protocol https --web
fi
gh auth setup-git

if [[ -z "$(git config --global user.name || true)" ]]; then
  git config --global user.name "$(gh api user --jq '.name // .login')"
fi
if [[ -z "$(git config --global user.email || true)" ]]; then
  # GitHub no-reply address: keeps the personal e-mail out of the commit history.
  git config --global user.email "$(gh api user --jq '"\(.id)+\(.login)@users.noreply.github.com"')"
fi
git config --global init.defaultBranch main
git config --global core.autocrlf input

# --- 3. Clone into the Linux filesystem --------------------------------------------
mkdir -p "$(dirname "$DEST_DIR")"
if [[ ! -d "$DEST_DIR/.git" ]]; then
  say "Clonage de ${REPO} dans ${DEST_DIR}"
  gh repo clone "$REPO" "$DEST_DIR"
fi
cd "$DEST_DIR"

# --- 4. Initial import if the remote is empty ----------------------------------------
if ! git rev-parse --verify HEAD >/dev/null 2>&1; then
  say "Dépôt vide : import des documents depuis ${SOURCE_DIR}"
  rsync -a --exclude '.git' --exclude 'build/' "$SOURCE_DIR"/ "$DEST_DIR"/
  git checkout -B main
  git add -A
  git commit -q -F - <<'MSG'
docs: add initial project documentation

Requirements, architecture, roadmap, coding standards (C23), testing
strategy (Criterion), file formats, glossary, agent instructions and
ADR 0001-0010.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_016MvMG7e7U8ne7vcM56xBRt
MSG
  git push -u origin main
else
  say "Le dépôt contient déjà des commits : aucun import"
fi

# --- Summary ------------------------------------------------------------------------
say "Environnement prêt"
gcc-14 --version | head -1
clang-20 --version | head -1
cmake --version | head -1
pkg-config --modversion criterion | sed 's/^/criterion /'
git -C "$DEST_DIR" log --oneline -1
echo
echo "Dossier de travail : $DEST_DIR"
echo "Ouvrez-le avec : code $DEST_DIR   (VS Code + extension WSL)"
