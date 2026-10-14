# Qt Widgets GUI（Qt6 が見つかったときだけルートから追加される）
qt_standard_project_setup()

qt_add_executable(monopoly_gui WIN32
    src/main.cpp
    src/MainWindow.cpp src/MainWindow.hpp
    src/BoardPainter.cpp src/BoardPainter.hpp
    src/TableWidget.cpp src/TableWidget.hpp
    src/Dialogs.cpp src/Dialogs.hpp
    src/CardVisual.cpp src/CardVisual.hpp
)
target_link_libraries(monopoly_gui PRIVATE monopoly_core Qt6::Widgets)

# ---- インストール（exe + Qt DLL 一式）
include(GNUInstallDirs)
install(TARGETS monopoly_gui monopoly_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT app)

# MSVC ランタイム DLL も同梱（VC++ 再頒布パッケージが無い PC でも動くように）
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT app)
include(InstallRequiredSystemLibraries)

qt_generate_deploy_app_script(
    TARGET monopoly_gui
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script} COMPONENT app)
