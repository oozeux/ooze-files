#!/usr/bin/env bash
# build.sh
set -euo pipefail

# raiz
cd "$(dirname "$(readlink -f "$0")")"

# argumentos
original_args=("$@")
mode=release
clean=0
run_tests=0
build_tests=ON
jobs=""

usage() {
    cat <<USAGE
Uso: ./build.sh [opciones]

  -d, --debug       compila en Debug (build/debug)
  -r, --release     compila en RelWithDebInfo (build/release, por defecto)
  -c, --clean       borra el directorio de compilacion antes de compilar
  -t, --test        ejecuta ctest al terminar (fuerza tests activados)
      --no-tests    no compila los tests
  -j, --jobs N      numero de trabajos en paralelo
  -h, --help        muestra esta ayuda
USAGE
}

while [ $# -gt 0 ]; do
    case "$1" in
        -d|--debug) mode=debug ;;
        -r|--release) mode=release ;;
        -c|--clean) clean=1 ;;
        -t|--test) run_tests=1; build_tests=ON ;;
        --no-tests) build_tests=OFF ;;
        -j|--jobs) shift; jobs="${1:?falta el numero de trabajos}" ;;
        -h|--help) usage; exit 0 ;;
        *) echo "opcion desconocida: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [ "$run_tests" = 1 ] && [ "$build_tests" = OFF ]; then
    echo "--test y --no-tests son incompatibles" >&2
    exit 2
fi

# entorno nix
if [ -z "${IN_NIX_SHELL:-}" ] && [ -z "${OOZE_IN_NIX:-}" ]; then
    export OOZE_IN_NIX=1
    if command -v nix-shell >/dev/null 2>&1; then
        exec nix-shell nix/devshell.nix --run "$(printf '%q ' "$PWD/build.sh" "${original_args[@]}")"
    elif command -v nix >/dev/null 2>&1; then
        exec nix develop -f nix/devshell.nix --command "$PWD/build.sh" "${original_args[@]}"
    fi
fi

for tool in cmake ninja; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "falta '$tool'; instala las dependencias o usa: nix-shell nix/devshell.nix" >&2
        exit 1
    }
done

# configuracion
preset="linux-$mode"
dir="build/$mode"

if [ "$clean" = 1 ]; then
    rm -rf -- "$dir"
fi

cmake --preset "$preset" -DOOZE_FILES_BUILD_TESTS="$build_tests"

# compilacion
build_args=(--build --preset "$preset")
if [ -n "$jobs" ]; then
    build_args+=(--parallel "$jobs")
else
    build_args+=(--parallel)
fi
cmake "${build_args[@]}"

# tests
if [ "$run_tests" = 1 ]; then
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM_TESTS:-offscreen}" \
        ctest --test-dir "$dir" --output-on-failure
fi

echo "binario: $dir/ooze-files"
