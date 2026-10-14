cmake_minimum_required(VERSION 3.21)
project(MonopolyProject VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # ソースを UTF-8 として扱う（日本語コメント・文字列の文字化け防止）
    add_compile_options(/utf-8)
endif()

add_subdirectory(core)
add_subdirectory(cli)

option(MONOPOLY_BUILD_TESTS "Build core tests" ON)
if(MONOPOLY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- GUI（Qt6 が無ければスキップして CLI だけビルドする）
option(MONOPOLY_BUILD_GUI "Build Qt GUI" ON)
if(MONOPOLY_BUILD_GUI)
    find_package(Qt6 6.5 QUIET COMPONENTS Widgets)
    if(Qt6_FOUND)
        message(STATUS "Qt6 ${Qt6_VERSION} found: building GUI")
        add_subdirectory(gui)

        # ---- インストーラ (CPack + Qt Installer Framework)
        set(CPACK_PACKAGE_NAME "BusinessMonopolyRecorder")
        set(CPACK_PACKAGE_VENDOR "riku")
        set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
        set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Business Monopoly Recorder")
        set(CPACK_PACKAGE_FILE_NAME "BusinessMonopolyRecorder-${PROJECT_VERSION}-Setup")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "BusinessMonopolyRecorder")
        set(CPACK_GENERATOR "IFW")
        set(CPACK_COMPONENTS_ALL app)
        set(CPACK_IFW_PACKAGE_TITLE "Business Monopoly Recorder")
        set(CPACK_IFW_PACKAGE_PUBLISHER "riku")
        set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "Business Monopoly Recorder")
        # ユーザー単位でインストール（管理者権限なし）
        set(CPACK_IFW_TARGET_DIRECTORY "@HomeDir@/AppData/Local/Programs/BusinessMonopolyRecorder")
        set(CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME "Uninstall")
        set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
        include(CPack)
        include(CPackIFW)
        cpack_add_component(app DISPLAY_NAME "Business Monopoly Recorder" REQUIRED)
        cpack_ifw_configure_component(app
            DISPLAY_NAME "Business Monopoly Recorder"
            DESCRIPTION "記録・再生アプリ本体 (GUI + CLI)"
            SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/installer/installscript.qs
            FORCED_INSTALLATION
        )
    else()
        message(STATUS "Qt6 not found: GUI skipped (set CMAKE_PREFIX_PATH to e.g. C:/Qt/6.x.x/msvc2022_64)")
    endif()
endif()
