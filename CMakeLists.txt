cmake_minimum_required(VERSION 3.28.3)
project(park_rozrywki)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(park_rozrywki
        Kasa.cpp
        Kasa.h
        kierownik.cpp
        kierownik.h
        klient.cpp
        klient.h
        main.cpp
        Park.cpp
        Park.h
        Pracownik.cpp
        Pracownik.h)
