cmake_minimum_required(VERSION 3.14)
project(lanelet2_absorber)

find_package(autoware_cmake REQUIRED)
autoware_package()

find_package(yaml-cpp REQUIRED)
#find_package(boost REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_executable(lanelet2_absorber src/main.cpp)

target_link_libraries(lanelet2_absorber
  yaml-cpp
  #boost_system
)

#target_include_directories(lanelet2_absorber PRIVATE ${Boost_INCLUDE_DIRS})

ament_auto_package()
