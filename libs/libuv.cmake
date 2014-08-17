# This is a big hack. Someday I'll fix it to work properly
# So far it works for linux

set(LIBUVDIR libs/libuv)

include_directories(${LIBUVDIR}/include ${LIBUVDIR}/src)
set(UV
  ${LIBUVDIR}/src/fs-poll.c
  ${LIBUVDIR}/src/inet.c
  ${LIBUVDIR}/src/uv-common.c
  ${LIBUVDIR}/src/threadpool.c
  ${LIBUVDIR}/src/version.c)

if(WIN32)
  # FIXME: This is probably wrong
  add_definitions(-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0600 -D_CRT_SECURE_NO_WARNINGS)
  include_directories(${LIBUVDIR}/src/win)
  file(GLOB UV_OS ${LIBUVDIR}/src/win/*.c)
  set(UV ${UV} ${UV_OS})
else()
  include_directories(${LIBUVDIR}/src/unix)
  file(GLOB UV_OS ${LIBUVDIR}/src/unix/*.c)

  set(BASE ${CMAKE_CURRENT_SOURCE_DIR}/${LIBUVDIR})

  #for linux FIXME!
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/sunos.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/pthread-fixes.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/openbsd.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/android-ifaddrs.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/darwin.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/darwin-proctitle.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/kqueue.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/netbsd.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/freebsd.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/fsevents.c)
  list(REMOVE_ITEM UV_OS ${BASE}/src/unix/aix.c)

  set(uv ${UV} ${UV_OS})
endif()

