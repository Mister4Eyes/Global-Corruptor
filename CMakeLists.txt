cmake_minimum_required(VERSION 3.5)
project(Global_Corruptor)

include_directories(${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include)

if(WIN32)
	set(Global_Corruptor_SRC stdafx.cpp BaseAddressGetter.cpp)
else()
	set(Global_Corruptor_SRC BaseAddressGetter.cpp)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
	add_compile_options("-std=gnu++14")
endif()

add_executable(GlobalCorruptor ${Global_Corruptor_SRC})
