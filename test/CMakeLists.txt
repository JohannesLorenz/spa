add_definitions(-Wall -Wextra -Werror -std=c++11 -g -ggdb -O0)

include_directories(../include)
# just for the examples:
include_directories(../include/rtosc/include)

add_executable(ringbuffer ringbuffer.cpp)
target_link_libraries(ringbuffer spa)

add_test(ringbuffer ./ringbuffer)
