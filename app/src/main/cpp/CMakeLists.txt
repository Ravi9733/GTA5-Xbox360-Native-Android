cmake_minimum_required(VERSION 3.22.1)
project(gta5native LANGUAGES CXX)

add_library(gta5native SHARED native-lib.cpp)
target_compile_features(gta5native PRIVATE cxx_std_20)
target_compile_options(gta5native PRIVATE -Wall -Wextra -Werror)
