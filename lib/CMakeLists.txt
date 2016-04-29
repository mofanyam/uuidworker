add_library(wxworker defs.h wxworker.c conf.c dummyfd.c env.c)
target_link_libraries(wxworker ev)