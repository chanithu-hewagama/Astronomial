cmake_minimum_required(VERSION 3.20)
project(astronomial CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(TgBot REQUIRED)

function(configure_astronomial_target target)
  target_include_directories(${target} PRIVATE ${SWE_DIR})
  target_link_libraries(${target} PRIVATE TgBot::TgBot ${SWE_LIB})
  if(WIN32)
    target_link_libraries(${target} PRIVATE ws2_32)
  endif()
endfunction()

add_executable(astro main.cpp)
configure_astronomial_target(astro)

if(WIN32)
  add_executable(astronomial WIN32 main.cpp)
  target_compile_definitions(astronomial PRIVATE ASTRONOMIAL_GUI)
  configure_astronomial_target(astronomial)
  target_link_libraries(astronomial PRIVATE user32 gdi32)
endif()
