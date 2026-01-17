cmake_minimum_required(VERSION 3.18)
project(park_rozrywki LANGUAGES CXX)

# Force C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared sources
set(SHARED
        park_wspolne.cpp
)

# Find pthreads
find_package(Threads REQUIRED)

# Executables
add_executable(park ${SHARED} park.cpp)
add_executable(klient ${SHARED} klient.cpp)
add_executable(pracownik ${SHARED} Pracownik.cpp)
add_executable(restauracja ${SHARED} kasaRestauracji.cpp)
add_executable(Kasa ${SHARED} Kasa.cpp)

# Link pthreads
target_link_libraries(park PRIVATE Threads::Threads)
target_link_libraries(klient PRIVATE Threads::Threads)
target_link_libraries(pracownik PRIVATE Threads::Threads)
target_link_libraries(restauracja PRIVATE Threads::Threads)
target_link_libraries(Kasa PRIVATE Threads::Threads)

add_dependencies(park klient pracownik Kasa restauracja)
