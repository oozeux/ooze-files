{ pkgs ? import <nixpkgs> { } }:

let
  qt = pkgs.qt6;
  qtRuntime = [
    qt.qtbase
    qt.qtdeclarative
    qt.qtwayland
    qt.qtsvg
  ];
in
pkgs.mkShell {
  name = "ooze-files-dev";

  packages = [
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
    pkgs.gcc
    qt.qtbase
    qt.qtbase.dev
    qt.qtdeclarative
    qt.qtdeclarative.dev
    qt.qtsvg
    qt.qtsvg.dev
    qt.qttools
    pkgs.glib
    pkgs.glib.dev
    pkgs.ffmpeg
    pkgs.squashfsTools
    pkgs.poppler-utils
  ] ++ pkgs.lib.optional (pkgs ? libsysprof-capture) pkgs.libsysprof-capture;

  shellHook = ''
    export OOZE_SHELL_QT_QML="${pkgs.lib.makeSearchPath "lib/qt-6/qml" qtRuntime}"
    export QML_IMPORT_PATH="$OOZE_SHELL_QT_QML"
    export QML2_IMPORT_PATH="$OOZE_SHELL_QT_QML"
    export QT_PLUGIN_PATH="${pkgs.lib.makeSearchPath "lib/qt-6/plugins" qtRuntime}"
    export QT_QPA_PLATFORM_PLUGIN_PATH="${pkgs.lib.makeSearchPath "lib/qt-6/plugins/platforms" qtRuntime}"
    echo "ooze-files dev shell"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
  '';
}
