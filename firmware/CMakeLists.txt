cmake_minimum_required(VERSION 3.16)

# Load IDF project/target configuration from the activated toolchain environment.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# IDF discovers the main component and generates the target image from this project.
project(balancing_robot)
