ADD_DEFINITIONS(-Wall -O2)
ADD_LIBRARY(extractor_fb2 SHARED extractor_fb2.c)
TARGET_LINK_LIBRARIES(extractor_fb2 expat extractor zip)
