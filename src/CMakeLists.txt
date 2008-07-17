ADD_DEFINITIONS(-Wall -O2)
ADD_LIBRARY(extractor_fb2 SHARED extractor_fb2.c)
TARGET_LINK_LIBRARIES(fb2plug3 expat extractor zip)
