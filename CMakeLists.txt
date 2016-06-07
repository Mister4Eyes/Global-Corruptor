cmake_minimum_required(VERSION 3.5)
project(Global_Corruptor)

if(WIN32)
	set(Global_Corruptor_SRC stdafx.cpp BaseAddressGetter.cpp)
else()
	set(Global_Corruptor_SRC BaseAddressGetter.cpp)
	
	find_package(Boost REQUIRED COMPONENTS system filesystem)
	set(Boost_DEBUG OFF)
	set(Boost_USE_STATIC_LIBS ON)
	set(Boost_USE_MULTITHREADED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
	add_compile_options("-std=gnu++14")
endif()

add_executable(GlobalCorruptor ${Global_Corruptor_SRC})

if(NOT WIN32)
	target_link_libraries(GlobalCorruptor ${Boost_LIBRARIES})
endif()
