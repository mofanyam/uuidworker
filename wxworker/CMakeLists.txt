add_library(wxworker STATIC defs.h wxworker.c dummyfd.c shm.c wxworker.c env.c conf.c spinlock.c ip.c)
target_link_libraries(wxworker ev)