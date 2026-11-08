#!/usr/bin/env bash
# run.sh
set -euo pipefail

# raiz
cd "$(dirname "$(readlink -f "$0")")"
root="$PWD"

# argumentos
mode=release
rebuild=0

usage() {
    cat <<USAGE
Uso: ./run.sh [opciones] [-- argumentos de ooze-files]

  -d, --debug       ejecuta build/debug/ooze-files
  -r, --release     ejecuta build/release/ooze-files (por defecto)
  -b, --build       compila antes de ejecutar
  -h, --help        muestra esta ayuda

Todo lo que siga a -- (o el primer argumento que no sea una opcion)
se pasa a ooze-files, por ejemplo una carpeta a abrir.
USAGE
}

while [ $# -gt 0 ]; do
    case "$1" in
        -d|--debug) mode=debug ;;
        -r|--release) mode=release ;;
        -b|--build) rebuild=1 ;;
        -h|--help) usage; exit 0 ;;
        --) shift; break ;;
        -*) echo "opcion desconocida: $1" >&2; usage >&2; exit 2 ;;
        *) break ;;
    esac
    shift
done

binary="$root/build/$mode/ooze-files"

# compilacion
if [ "$rebuild" = 1 ] || [ ! -x "$binary" ]; then
    "$root/build.sh" "--$mode" --no-tests
fi

# entorno nix
if [ -z "${OOZE_IN_NIX:-}" ] && [ -z "${QML_IMPORT_PATH:-}" ] && [ -e /etc/NIXOS ]; then
    export OOZE_IN_NIX=1
    if command -v nix-shell >/dev/null 2>&1; then
        exec nix-shell "$root/nix/devshell.nix" --run "$(printf '%q ' "$root/run.sh" "--$mode" -- "$@")"
    elif command -v nix >/dev/null 2>&1; then
        exec nix develop -f "$root/nix/devshell.nix" --command "$root/run.sh" "--$mode" -- "$@"
    fi
fi

# ejecucion
exec "$binary" "$@"
