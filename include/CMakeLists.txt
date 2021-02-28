# Rather than cmake-including sub-projects,
# we grab their files on our own here

set(src_dir "${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB rtosc_lib_src ${src_dir}/rtosc/src/*.cpp)
file(GLOB rtosc_lib_hdr ${src_dir}/rtosc/include/rtosc/*.h)

install(FILES ${rtosc_lib_hdr} DESTINATION include/rtosc)

set(rtosc_lib_src ${rtosc_lib_src} PARENT_SCOPE)
set(rtosc_lib_hdr ${rtosc_lib_hdr} PARENT_SCOPE)

install(FILES spa/spa_fwd.h spa/spa.h spa/audio_fwd.h spa/audio.h DESTINATION include/spa)

