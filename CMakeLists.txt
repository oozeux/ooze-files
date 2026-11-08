cmake_minimum_required(VERSION 3.21)

project(ooze-files
    VERSION 0.1.46
    DESCRIPTION "Administrador de archivos minimalista para Wayland"
    HOMEPAGE_URL "https://example.invalid/ooze-files"
    LANGUAGES CXX
)

# Source Path Check
if(CMAKE_SOURCE_DIR MATCHES "[ ()]")
    message(FATAL_ERROR
        "La ruta del proyecto contiene espacios o paréntesis:\n  ${CMAKE_SOURCE_DIR}\n"
        "qt_add_translations de Qt 6 no los soporta. Mueve el proyecto a una ruta sin ellos.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OOZE_FILES_WITH_ARCHIVES "Soporte de archivos comprimidos (fase futura)" OFF)
option(OOZE_FILES_WITH_REMOTE "Backends remotos vía gvfs (fase futura)" OFF)
option(OOZE_FILES_WITH_UDISKS "Detalles de dispositivos vía UDisks2 (fase futura)" OFF)
option(OOZE_FILES_WITH_XDG_THUMBNAILERS "Miniaturas externas (fase futura)" OFF)
option(OOZE_FILES_BUILD_TESTS "Compilar tests" ON)

foreach(reserved_feature ARCHIVES REMOTE UDISKS XDG_THUMBNAILERS)
    if(OOZE_FILES_WITH_${reserved_feature})
        message(FATAL_ERROR
            "OOZE_FILES_WITH_${reserved_feature} corresponde a una fase no implementada todavía")
    endif()
endforeach()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick DBus Svg LinguistTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
qt_standard_project_setup(REQUIRES 6.5)

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_library(ooze_core STATIC
    src/core/CancelToken.h
    src/core/PathUtils.h
    src/core/PathUtils.cpp
    src/core/DirectoryLoader.h
    src/core/DirectoryLoader.cpp
    src/core/DirectoryWatcher.h
    src/core/DirectoryWatcher.cpp
    src/core/FilePreview.h
    src/core/FilePreview.cpp
    src/core/ThumbnailCache.h
    src/core/ThumbnailCache.cpp
    src/core/FileIcons.h
    src/core/FileIcons.cpp
    src/core/OpenWith.h
    src/core/OpenWith.cpp
    src/core/Devices.h
    src/core/Devices.cpp
    src/core/Archive.h
    src/core/Archive.cpp
    src/core/UndoStack.h
    src/core/UndoStack.cpp
    src/core/FileOperations.h
    src/core/FileOperations.cpp
    src/core/LoaderSignals.h
    src/core/FileListModel.h
    src/core/FileListModel.cpp
    src/core/FileListProxyModel.h
    src/core/FileListProxyModel.cpp
    src/core/Places.h
    src/core/Places.cpp
    src/core/RecentFiles.h
    src/core/RecentFiles.cpp
    src/core/TrashService.h
    src/core/TrashService.cpp
    src/core/TerminalLauncher.h
    src/core/TerminalLauncher.cpp
    src/settings/Config.h
    src/settings/Config.cpp
    src/theme/ThemeStore.h
    src/theme/ThemeStore.cpp
    src/theme/PortalSettings.h
    src/theme/PortalSettings.cpp
    src/vfs/VfsTypes.h
    src/vfs/VfsBackend.h
    src/vfs/VfsRegistry.h
    src/vfs/VfsRegistry.cpp
    src/vfs/LocalVfs.h
    src/vfs/LocalVfs.cpp
    src/vfs/RecentVfs.h
    src/vfs/RecentVfs.cpp
    src/vfs/TrashVfs.h
    src/vfs/TrashVfs.cpp
)

target_include_directories(ooze_core PUBLIC src)
target_link_libraries(ooze_core PUBLIC Qt6::Core Qt6::Gui Qt6::DBus Qt6::Svg PkgConfig::GIO)
target_compile_definitions(ooze_core PUBLIC OOZE_FILES_VERSION="${PROJECT_VERSION}")

qt_add_executable(ooze-files
    src/app/main.cpp
    src/app/ApplicationController.h
    src/app/ApplicationController.cpp
    src/app/TabController.h
    src/app/TabController.cpp
    src/app/TabListModel.h
    src/app/TabListModel.cpp
    src/app/SingleInstance.h
    src/app/SingleInstance.cpp
    src/app/IconProvider.h
    src/app/IconProvider.cpp
    src/app/DragPreviewProvider.h
    src/app/DragPreviewProvider.cpp
    src/app/ThumbnailProvider.h
    src/app/ThumbnailProvider.cpp
    src/app/PdfPreviewProvider.h
    src/app/PdfPreviewProvider.cpp
    src/app/Tools.h
    src/app/Tools.cpp
    src/app/SearchModel.h
    src/app/SearchModel.cpp
    src/app/IconTheme.h
    src/app/IconTheme.cpp
    src/app/I18n.h
    src/app/I18n.cpp
    src/app/Logging.h
    src/app/Logging.cpp
    src/app/ActionRegistry.h
    src/app/ActionRegistry.cpp
)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(ooze-files
    URI Ooze.Files
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/FileIcon.qml
        qml/components/IconButton.qml
        qml/components/ScrollIndicator.qml
        qml/components/SearchField.qml
        qml/components/MenuPanel.qml
        qml/components/PopupMenu.qml
        qml/components/DetailsList.qml
        qml/components/InfoPanel.qml
        qml/components/PropertiesDialog.qml
        qml/components/PreviewOverlay.qml
        qml/components/SearchOverlay.qml
        qml/components/BulkRenameDialog.qml
        qml/components/PromptDialog.qml
        qml/components/CompressDialog.qml
        qml/components/ConflictDialog.qml
        qml/components/ConfirmDialog.qml
        qml/components/CommandPalette.qml
        qml/components/MarqueeSelection.qml
        qml/panels/HeaderBar.qml
        qml/panels/TabBar.qml
        qml/panels/Sidebar.qml
        qml/panels/StatusBar.qml
        qml/pages/SettingsWindow.qml
        qml/views/FileGridView.qml
        qml/views/FileListView.qml
)

qt_add_translations(ooze-files
    TS_FILES i18n/ooze-files_en.ts
    RESOURCE_PREFIX /i18n
)

target_link_libraries(ooze-files PRIVATE ooze_core Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Svg)

# Qt Includes
foreach(qt_target Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
    get_target_property(qt_include_dirs ${qt_target} INTERFACE_INCLUDE_DIRECTORIES)
    foreach(qt_include_dir IN LISTS qt_include_dirs)
        get_filename_component(qt_include_name "${qt_include_dir}" NAME)
        if(qt_include_name MATCHES "^Qt[A-Z][A-Za-z0-9]*$")
            get_filename_component(qt_include_root "${qt_include_dir}" DIRECTORY)
            target_include_directories(ooze-files SYSTEM PRIVATE "${qt_include_root}")
        endif()
    endforeach()
endforeach()

set_target_properties(ooze-files PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

install(TARGETS ooze-files RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/ooze-files.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/icons/ooze-files.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(OOZE_FILES_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
