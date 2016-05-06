add_library(wxworker STATIC defs.h wxworker.c conf.c dummyfd.c env.c shm.c)
target_link_libraries(wxworker ev)