project(ioplugin)

add_library(lvdfilereader SHARED)
target_sources(lvdfilereader PRIVATE "lvdfileplugin.cpp" "lvdfile.cpp" "lvdfileheader.cpp")
target_link_libraries(lvdfilereader vmcore)
target_include_directories(lvdfilereader PUBLIC "lvdfileheader.h" "lvdfile.h" "lvdfileplugin.h")   # for test used

install(TARGETS lvdfilereader LIBRARY DESTINATION "lib" RUNTIME DESTINATION "bin/plugins" ARCHIVE DESTINATION "lib")