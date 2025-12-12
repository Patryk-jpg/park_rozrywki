cmake_minimum_required(VERSION 3.28.3)
project(park_rozrywki)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

set(SHARED
        park_wspolne.cpp
)

# Executables
add_executable(park ${SHARED} park.cpp)
add_executable(klient klient.cpp ${SHARED})
add_executable(pracownik Pracownik.cpp ${SHARED})

