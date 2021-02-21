add_definitions(-Wall -Wextra -Werror -std=c++11 -g -ggdb -O0)

include_directories(../include)
# just for the examples:
include_directories(../include/rtosc/include)
include_directories(../include/ringbuffer/include)

add_executable(osc-host osc-host.cpp)
target_link_libraries(osc-host dl spa)
add_library(osc-plugin SHARED osc-plugin.cpp)
add_library(gain SHARED gain.cpp)
#add_library(reverser SHARED reverser.cpp)
#add_library(step-repeater SHARED step-repeater.cpp)

add_test(simple-host ./osc-host libosc-plugin.so)
