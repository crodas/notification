set (leveldir libs/leveldb)
set (snappydir libs/snappy)
add_definitions(-DLEVELDB_PLATFORM_POSIX)

file(GLOB level_db ${leveldir}/*/*.cc ${leveldir}/helpers/*.cc  ${snappydir}/*.cc)
file(GLOB ignore1 ${leveldir}/*/*test*.cc ${leveldir}/*/*_main.cc ${leveldir}/*/*_bench.cc ${snappydir}/*test*.cc)


set(level ${level_db} ${snappy})

list(REMOVE_ITEM level ${ignore1})


CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(stddef.h HAVE_STDDEF_H)
CHECK_INCLUDE_FILE(sys/uio.h HAVE_UIO_H)
if(HAVE_STDINT_H)
    set(ac_cv_have_stdint_h 1)
else()
    set(ac_cv_have_stdint_h 0)
endif()

if(HAVE_STDDEF_H)
    set(ac_cv_have_stddef_h 1)
else()
    set(ac_cv_have_stddef_h 0)
endif()

if(HAVE_UIO_H)
    set(ac_cv_have_sys_uio_h 1)
else()
    set(ac_cv_have_sys_uio_h 0)
endif()

if(WIN32)
    add_definitions(-DHAVE_WINDOWS_H)
endif()

configure_file(${snappydir}/snappy-stubs-public.h.in 
    ${snappydir}/snappy-stubs-public.h
    @ONLY)

file(WRITE ${snappydir}/config.h "")

include_directories(${leveldir})
include_directories(${leveldir}/include)
include_directories(${snappydir})
