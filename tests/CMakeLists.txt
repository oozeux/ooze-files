qt_add_executable(tst_pathutils unit/tst_pathutils.cpp)
target_link_libraries(tst_pathutils PRIVATE ooze_core Qt6::Test)
add_test(NAME pathutils COMMAND tst_pathutils)

qt_add_executable(tst_config unit/tst_config.cpp)
target_link_libraries(tst_config PRIVATE ooze_core Qt6::Test)
add_test(NAME config COMMAND tst_config)

qt_add_executable(tst_localfs unit/tst_localfs.cpp)
target_link_libraries(tst_localfs PRIVATE ooze_core Qt6::Test)
add_test(NAME localfs COMMAND tst_localfs)

qt_add_executable(tst_proxy unit/tst_proxy.cpp)
target_link_libraries(tst_proxy PRIVATE ooze_core Qt6::Test)
add_test(NAME proxy COMMAND tst_proxy)

qt_add_executable(tst_fileoperations unit/tst_fileoperations.cpp)
target_link_libraries(tst_fileoperations PRIVATE ooze_core Qt6::Test)
add_test(NAME fileoperations COMMAND tst_fileoperations)

qt_add_executable(tst_watcher unit/tst_watcher.cpp)
target_link_libraries(tst_watcher PRIVATE ooze_core Qt6::Test)
add_test(NAME watcher COMMAND tst_watcher)

qt_add_executable(tst_theme unit/tst_theme.cpp)
target_link_libraries(tst_theme PRIVATE ooze_core Qt6::Test)
add_test(NAME theme COMMAND tst_theme)

qt_add_executable(tst_preview unit/tst_preview.cpp)
target_link_libraries(tst_preview PRIVATE ooze_core Qt6::Test)
add_test(NAME preview COMMAND tst_preview)

qt_add_executable(tst_fileicons unit/tst_fileicons.cpp)
target_link_libraries(tst_fileicons PRIVATE ooze_core Qt6::Test)
add_test(NAME fileicons COMMAND tst_fileicons)

qt_add_executable(tst_undo unit/tst_undo.cpp)
target_link_libraries(tst_undo PRIVATE ooze_core Qt6::Test)
add_test(NAME undo COMMAND tst_undo)

qt_add_executable(tst_archive unit/tst_archive.cpp)
target_link_libraries(tst_archive PRIVATE ooze_core Qt6::Test)
add_test(NAME archive COMMAND tst_archive)

qt_add_executable(tst_devices unit/tst_devices.cpp)
target_link_libraries(tst_devices PRIVATE ooze_core Qt6::Test)
add_test(NAME devices COMMAND tst_devices)

qt_add_executable(tst_trash unit/tst_trash.cpp)
target_link_libraries(tst_trash PRIVATE ooze_core Qt6::Test)
add_test(NAME trash COMMAND tst_trash)

qt_add_executable(tst_recentfiles unit/tst_recentfiles.cpp)
target_link_libraries(tst_recentfiles PRIVATE ooze_core Qt6::Test)
add_test(NAME recentfiles COMMAND tst_recentfiles)

qt_add_executable(tst_openwith unit/tst_openwith.cpp)
target_link_libraries(tst_openwith PRIVATE ooze_core Qt6::Test)
add_test(NAME openwith COMMAND tst_openwith)

qt_add_executable(tst_controller unit/tst_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ApplicationController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TabController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TabListModel.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ActionRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/app/I18n.cpp)
target_link_libraries(tst_controller PRIVATE ooze_core Qt6::Test)
add_test(NAME controller COMMAND tst_controller)

qt_add_executable(tst_tabs unit/tst_tabs.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ApplicationController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TabController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/TabListModel.cpp
    ${CMAKE_SOURCE_DIR}/src/app/ActionRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/app/I18n.cpp)
target_link_libraries(tst_tabs PRIVATE ooze_core Qt6::Test)
add_test(NAME tabs COMMAND tst_tabs)

add_test(NAME smoke-ui COMMAND ooze-files --smoke-test)
set_tests_properties(smoke-ui PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 30)

set_tests_properties(pathutils config localfs proxy fileoperations watcher theme preview fileicons undo archive devices openwith controller tabs PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
