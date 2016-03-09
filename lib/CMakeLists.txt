add_library(wxworker defs.h wxdo.h wxworker.c conf.c dummyfd.c)
target_link_libraries(wxworker ev)