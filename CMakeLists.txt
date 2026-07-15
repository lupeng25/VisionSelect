cmake_minimum_required(VERSION 3.28)

project(VisionSelect VERSION 2.0.0 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.10.1 REQUIRED COMPONENTS Core Gui Widgets Sql Concurrent PrintSupport Test)
qt_standard_project_setup(REQUIRES 6.10)

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
endif()

set(VS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(VS_RESOURCE_FILES
    resources/style.qss
    resources/data/cameras.csv
    resources/data/lenses.csv
    resources/data/lights.csv
    resources/data/three_d_cameras.json
    resources/i18n/visionselect_zh_CN.qm
    resources/i18n/visionselect_en_US.qm
    resources/license/public_key.json
    resources/icons/visionselect_icon_256.png
    resources/icons/ui/assistant.png
    resources/icons/ui/calculate.png
    resources/icons/ui/camera3d.png
    resources/icons/ui/catalog.png
    resources/icons/ui/chevron-down-light.png
    resources/icons/ui/chevron-down.png
    resources/icons/ui/chevron-up-light.png
    resources/icons/ui/chevron-up.png
    resources/icons/ui/compare.png
    resources/icons/ui/error.png
    resources/icons/ui/export.png
    resources/icons/ui/info.png
    resources/icons/ui/report.png
    resources/icons/ui/requirement.png
    resources/icons/ui/results.png
    resources/icons/ui/success.png
    resources/icons/ui/warning.png
)

add_library(visionselect_core STATIC
    src/core/Localization.cpp
    src/core/SelectionTypes.cpp
    src/i18n/LanguageManager.cpp
)
target_include_directories(visionselect_core PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_core PUBLIC Qt6::Core)

add_library(visionselect_catalog STATIC src/catalog/CatalogRepository.cpp)
target_include_directories(visionselect_catalog PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_catalog PUBLIC visionselect_core Qt6::Sql)

add_library(visionselect_selection STATIC
    src/selection/CalculationAssistant.cpp
    src/selection/SelectionEngine.cpp
    src/selection/SelectionService.cpp
)
target_include_directories(visionselect_selection PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_selection PUBLIC visionselect_catalog Qt6::Core)

add_library(visionselect_three_d STATIC
    src/three_d/ThreeDCalculation.cpp
    src/three_d/ThreeDCameraMatcher.cpp
    src/three_d/ThreeDCameraRepository.cpp
    src/three_d/ThreeDCameraTypes.cpp
)
target_include_directories(visionselect_three_d PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_three_d PUBLIC visionselect_core Qt6::Core)

add_library(visionselect_license STATIC
    src/license/LicenseIssuer.cpp
    src/license/LicenseManager.cpp
)
target_include_directories(visionselect_license PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_license PUBLIC visionselect_core Qt6::Core Crypt32 Bcrypt User32)

add_library(visionselect_ui_support STATIC src/ui/UiHelpers.cpp)
target_include_directories(visionselect_ui_support PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_ui_support PUBLIC visionselect_core Qt6::Widgets)

add_library(visionselect_report STATIC src/report/PdfReportWriter.cpp)
target_include_directories(visionselect_report PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_report PUBLIC visionselect_selection visionselect_ui_support Qt6::PrintSupport)

add_library(visionselect_ui STATIC
    src/ui/CatalogDialogs.cpp
    src/ui/LicenseDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/pages/CalculationPage.cpp
    src/ui/pages/CatalogPage.cpp
    src/ui/pages/InputPage.cpp
    src/ui/pages/PureCalculationPage.cpp
    src/ui/pages/ResultsPage.cpp
    src/ui/pages/ThreeDCameraPage.cpp
)
target_include_directories(visionselect_ui PUBLIC "${VS_INCLUDE_DIR}")
target_link_libraries(visionselect_ui PUBLIC
    visionselect_catalog
    visionselect_license
    visionselect_report
    visionselect_selection
    visionselect_three_d
    visionselect_ui_support
    Qt6::Concurrent
    Qt6::Widgets
)

qt_add_executable(VisionSelect WIN32
    src/main.cpp
    resources/windows/VisionSelect.rc
)
target_link_libraries(VisionSelect PRIVATE visionselect_ui)
qt_add_resources(VisionSelect visionselect_application_resources
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES ${VS_RESOURCE_FILES}
)
if(WIN32)
    add_custom_command(TARGET VisionSelect POST_BUILD
        COMMAND "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}/windeployqt.exe"
            --compiler-runtime --no-translations
            --dir "$<TARGET_FILE_DIR:VisionSelect>"
            "$<TARGET_FILE:VisionSelect>"
        VERBATIM
        COMMENT "Deploying Qt runtime next to VisionSelect.exe"
    )
endif()

qt_add_executable(VisionSelectLicenseGenerator WIN32
    tools/license_key_generator/main.cpp
    tools/license_key_generator/LicenseGeneratorWindow.cpp
)
target_include_directories(VisionSelectLicenseGenerator PRIVATE "${VS_INCLUDE_DIR}")
target_link_libraries(VisionSelectLicenseGenerator PRIVATE visionselect_license Qt6::Widgets)

qt_add_executable(VisionSelectTests
    tests/test_selection.cpp
)
target_include_directories(VisionSelectTests PRIVATE "${VS_INCLUDE_DIR}")
target_link_libraries(VisionSelectTests PRIVATE
    visionselect_catalog
    visionselect_license
    visionselect_report
    visionselect_selection
    visionselect_three_d
    visionselect_ui_support
    Qt6::Test
    Qt6::Widgets
)
qt_add_resources(VisionSelectTests visionselect_test_resources
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES ${VS_RESOURCE_FILES}
)
add_test(NAME VisionSelectTests COMMAND VisionSelectTests)
set_tests_properties(VisionSelectTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

include(GNUInstallDirs)
install(TARGETS VisionSelect VisionSelectLicenseGenerator
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

qt_generate_deploy_app_script(
    TARGET VisionSelect
    OUTPUT_SCRIPT visionselect_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT "${visionselect_deploy_script}")
