cmake_minimum_required(VERSION 3.24)
project(nirisnap VERSION 0.1.0 LANGUAGES C CXX)
include(GNUInstallDirs)
include(CTest)
set(CMAKE_AUTOMOC ON)

find_package(PkgConfig REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Test Widgets)
find_package(LayerShellQt REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PROTOCOL_XMLS
  ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-screencopy-unstable-v1.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-virtual-pointer-unstable-v1.xml
)
set(PROTOCOL_SOURCES)
foreach(PROTOCOL_XML IN LISTS PROTOCOL_XMLS)
  get_filename_component(PROTOCOL_NAME "${PROTOCOL_XML}" NAME_WE)
  set(PROTOCOL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${PROTOCOL_NAME}-client-protocol.h")
  set(PROTOCOL_CODE "${CMAKE_CURRENT_BINARY_DIR}/${PROTOCOL_NAME}-protocol.c")
  add_custom_command(
    OUTPUT "${PROTOCOL_HEADER}" "${PROTOCOL_CODE}"
    COMMAND "${WAYLAND_SCANNER}" client-header "${PROTOCOL_XML}" "${PROTOCOL_HEADER}"
    COMMAND "${WAYLAND_SCANNER}" private-code "${PROTOCOL_XML}" "${PROTOCOL_CODE}"
    DEPENDS "${PROTOCOL_XML}"
    VERBATIM
  )
  list(APPEND PROTOCOL_SOURCES "${PROTOCOL_HEADER}" "${PROTOCOL_CODE}")
endforeach()

add_library(nirisnap-core STATIC
  src/cli-path.cpp
  src/cli-path.hpp
  src/icons.cpp
  src/icons.hpp
  src/instance-lock.cpp
  src/instance-lock.hpp
  src/niri-ipc.cpp
  src/niri-ipc.hpp
  src/pin-file.cpp
  src/pin-file.hpp
  src/pin-layout.cpp
  src/pin-layout.hpp
  src/capture.cpp
  src/capture.hpp
  src/cut.cpp
  src/cut.hpp
  src/palette-config.cpp
  src/palette-config.hpp
  src/background-config.cpp
  src/background-config.hpp
  src/recent-snaps.cpp
  src/recent-snaps.hpp
  src/output-config.cpp
  src/output-config.hpp
  src/overlay-chrome.cpp
  src/overlay-chrome.hpp
  src/eyedropper.cpp
  src/eyedropper.hpp
  src/surface-capture.cpp
  src/stitch.cpp
  src/stitch.hpp
  src/text-band.cpp
  src/text-band.hpp
  src/auto-capture.cpp
  src/auto-capture.hpp
  src/scroll-inject.cpp
  src/scroll-inject.hpp
  src/startup-timing.cpp
  src/startup-timing.hpp
  ${PROTOCOL_SOURCES}
  src/editor.cpp
  src/editor.hpp
)
qt_add_resources(nirisnap-core "capture-fonts"
  PREFIX "/fonts"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/JetBrainsMono-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/InterDisplay-SemiBold.ttf"
)

# CXX-only: nirisnap-core also compiles Wayland protocol .c files.
target_precompile_headers(nirisnap-core PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:<QtWidgets/QWidget$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtWidgets/QApplication$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QPainter$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QPainterPath$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QImage$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QPixmap$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QFontDatabase$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QMouseEvent$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QKeyEvent$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtGui/QWheelEvent$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtConcurrent/QtConcurrentRun$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QFutureWatcher$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QProcess$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QFile$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QDir$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QFileInfo$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QJsonObject$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QJsonArray$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QJsonDocument$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QTimer$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QString$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<QtCore/QVector$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<cmath$<ANGLE-R>>"
)

target_include_directories(nirisnap-core PUBLIC
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(nirisnap-core PUBLIC cxx_std_23)
target_compile_options(nirisnap-core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(nirisnap-core PUBLIC
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  LayerShellQt::Interface
  PkgConfig::WaylandClient
)

qt_add_executable(nirisnap src/main.cpp src/pin.cpp src/pin.hpp)
target_compile_options(nirisnap PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(nirisnap PRIVATE NIRISNAP_VERSION="${PROJECT_VERSION}")
target_link_libraries(nirisnap PRIVATE nirisnap-core)
target_precompile_headers(nirisnap REUSE_FROM nirisnap-core)

if(BUILD_TESTING)
qt_add_executable(nirisnap-smoke
  tests/editor-smoke.cpp
  tests/clipboard-smoke.cpp
  tests/clipboard-smoke.hpp
  tests/instance-lock-smoke.cpp
  tests/instance-lock-smoke.hpp
  tests/transform-smoke.cpp
  tests/transform-smoke.hpp
  tests/cut-smoke.cpp
  tests/cut-smoke.hpp
  tests/cut-mapping-smoke.cpp
  tests/cut-mapping-smoke.hpp
  tests/palette-config-smoke.cpp
  tests/palette-config-smoke.hpp
  tests/niri-ipc-smoke.cpp
  tests/niri-ipc-smoke.hpp
  src/cli-path.cpp
  src/recent-snaps.cpp
  src/recent-snaps.hpp
  src/cli-path.hpp
  src/capture.cpp
  src/capture.hpp
  src/niri-ipc.cpp
  src/niri-ipc.hpp
  src/overlay-chrome.cpp
  src/overlay-chrome.hpp
  tests/surface-capture-smoke.cpp
  tests/surface-capture-smoke.hpp
  tests/pin-lifecycle-smoke.cpp
  tests/pin-lifecycle-smoke.hpp
  tests/pin-layout-smoke.cpp
  tests/pin-layout-smoke.hpp
  src/stitch.cpp
  src/stitch.hpp
  src/auto-capture.cpp
  src/auto-capture.hpp
  tests/stitch-smoke.cpp
  tests/stitch-smoke.hpp
)
target_include_directories(nirisnap-smoke PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
target_compile_options(nirisnap-smoke PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(nirisnap-smoke PRIVATE nirisnap-core Qt6::Test)
target_precompile_headers(nirisnap-smoke REUSE_FROM nirisnap-core)
add_test(NAME nirisnap-smoke COMMAND nirisnap-smoke "${CMAKE_CURRENT_BINARY_DIR}/nirisnap-smoke-output")
set_tests_properties(nirisnap-smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

qt_add_executable(stitch-replay
  src/stitch-replay.cpp
  src/stitch.cpp
  src/stitch.hpp
)
target_include_directories(stitch-replay PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(stitch-replay PRIVATE cxx_std_23)
target_compile_options(stitch-replay PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(stitch-replay PRIVATE Qt6::Core Qt6::Gui)

install(TARGETS nirisnap
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(FILES assets/OFL.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/nirisnap"
  RENAME Neucha-OFL.txt
)
install(FILES assets/JetBrainsMono-OFL.txt assets/Inter-OFL.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/nirisnap"
)
install(FILES nirisnap.desktop
  DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)
