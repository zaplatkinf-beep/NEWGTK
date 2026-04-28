cmake_minimum_required(VERSION 3.10)
project(iec61850_client_gui)

set(CMAKE_C_STANDARD 11)

# Находим необходимые пакеты
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(LIBIEC61850 REQUIRED libiec61850)
find_package(Threads REQUIRED)

# Выводим информацию
message(STATUS "GTK4 include dirs: ${GTK4_INCLUDE_DIRS}")
message(STATUS "libiec61850 include dirs: ${LIBIEC61850_INCLUDE_DIRS}")

# Добавляем заголовочные файлы
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${LIBIEC61850_INCLUDE_DIRS}
    ${LIBIEC61850_INCLUDE_DIRS}/libiec61850
    ${GTK4_INCLUDE_DIRS}
)

# Только GUI исходники (БЕЗ client_example_log.c!)
set(SOURCES
    src/main.c
    src/client_gui.c
    src/client_wrapper.c
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES})

# Флаги компиляции
target_compile_options(${PROJECT_NAME} PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${LIBIEC61850_CFLAGS_OTHER}
    -pthread
)

# Линковка
target_link_libraries(${PROJECT_NAME}
    ${GTK4_LIBRARIES}
    ${LIBIEC61850_LIBRARIES}
    Threads::Threads
)

# Опции линковки
target_link_options(${PROJECT_NAME} PRIVATE
    ${GTK4_LDFLAGS_OTHER}
)
