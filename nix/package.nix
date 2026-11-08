{ lib
, stdenv
, cmake
, ninja
, pkg-config
, glib
, qt6
, ffmpeg
, squashfsTools
, poppler_utils
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "ooze-files";
  version = "0.1.46";

  src = lib.cleanSourceWith {
    src = ../.;
    filter = path: type:
      let
        base = baseNameOf path;
      in
      !(builtins.elem base [ "build" "result" ".git" ".cache" ])
      && !(lib.hasPrefix "build-" base)
      && !(lib.hasPrefix "result-" base);
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    qt6.wrapQtAppsHook
    qt6.qttools
  ];

  buildInputs = [
    glib
    glib.dev
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtwayland
    qt6.qtsvg
    ffmpeg
    squashfsTools
    poppler_utils
  ];

  qtWrapperArgs = [
    "--prefix PATH : ${lib.makeBinPath [ ffmpeg squashfsTools poppler_utils ]}"
  ];

  cmakeFlags = [
    "-DOOZE_FILES_BUILD_TESTS=OFF"
    "-DCMAKE_BUILD_TYPE=Release"
  ];

  doCheck = false;

  meta = {
    description = "Administrador de archivos minimalista para Wayland";
    homepage = "https://example.invalid/ooze-files";
    license = lib.licenses.gpl3Plus;
    mainProgram = "ooze-files";
    platforms = lib.platforms.linux;
  };
})
