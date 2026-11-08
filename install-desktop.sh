#!/usr/bin/env bash
# install-desktop.sh
set -euo pipefail

# raiz
cd "$(dirname "$(readlink -f "$0")")"
root="$PWD"

# argumentos
mode=release
action=install
binary=""
custom=""
newflag="--new-window "
setdefault=""

usage() {
    cat <<USAGE
Uso: ./install-desktop.sh [opciones]

  -d, --debug         lanza build/debug con run.sh
  -r, --release       lanza build/release con run.sh (por defecto)
      --binary RUTA   lanza ese binario directamente (paquete de nix o /usr/bin)
      --reuse         un segundo lanzamiento reutiliza la ventana abierta
      --set-default   fija Ooze Files como gestor de carpetas (inode/directory)
  -u, --uninstall     elimina el lanzador y el icono
  -h, --help          muestra esta ayuda

Instala en ~/.local/share (o XDG_DATA_HOME) sin tocar nada fuera de tu usuario.
USAGE
}

while [ $# -gt 0 ]; do
    case "$1" in
        -d|--debug) mode=debug ;;
        -r|--release) mode=release ;;
        --binary) shift; binary="${1:?falta la ruta del binario}"; custom=1 ;;
        --reuse) newflag="" ;;
        --set-default) setdefault=1 ;;
        -u|--uninstall) action=uninstall ;;
        -h|--help) usage; exit 0 ;;
        *) echo "opcion desconocida: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# rutas
data="${XDG_DATA_HOME:-$HOME/.local/share}"
apps="$data/applications"
icons="$data/icons/hicolor/scalable/apps"
desktop="$apps/ooze-files.desktop"
icon="$icons/ooze-files.svg"

exec_path() {
    case "$1" in
        *[!A-Za-z0-9_./+:@-]*)
            printf '"%s"' "$(printf '%s' "$1" | sed -e 's/[\\"`$]/\\\\&/g')"
            ;;
        *)
            printf '%s' "$1"
            ;;
    esac
}

refresh() {
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$apps" 2>/dev/null || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$data/icons/hicolor" 2>/dev/null || true
}

# desinstalacion
if [ "$action" = uninstall ]; then
    rm -f -- "$desktop" "$icon"
    refresh
    echo "lanzador eliminado"
    exit 0
fi

# binario
if [ -z "$binary" ]; then
    binary="$root/build/$mode/ooze-files"
fi

if [ ! -x "$binary" ]; then
    echo "no existe $binary; compila antes con ./build.sh o usa --binary" >&2
    exit 1
fi

# lanzador
sep="-- "
if [ -n "$custom" ]; then
    command_line="$(exec_path "$binary")"
    sep=""
elif [ "$mode" = debug ]; then
    command_line="$(exec_path "$root/run.sh") --debug"
else
    command_line="$(exec_path "$root/run.sh")"
fi

mkdir -p "$apps" "$icons"
install -m 0644 packaging/icons/ooze-files.svg "$icon"

while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        "Exec=ooze-files --new-window %U")
            printf 'Exec=%s %s--new-window %%U\n' "$command_line" "$sep"
            ;;
        "Exec=ooze-files %U")
            printf 'Exec=%s %s%%U\n' "$command_line" "${newflag:+$sep$newflag}"
            ;;
        Icon=*)
            printf 'Icon=ooze-files\n'
            ;;
        *)
            printf '%s\n' "$line"
            ;;
    esac
done < packaging/ooze-files.desktop > "$desktop"
chmod 0644 "$desktop"

refresh

if [ -n "$setdefault" ]; then
    if command -v xdg-mime >/dev/null 2>&1; then
        xdg-mime default ooze-files.desktop inode/directory
    elif command -v gio >/dev/null 2>&1; then
        gio mime inode/directory ooze-files.desktop
    else
        echo "no hay xdg-mime ni gio; edita ~/.config/mimeapps.list a mano" >&2
    fi
fi

echo "lanzador instalado en $desktop"
