cmake_minimum_required(VERSION 3.17)
project(ffmpegexamples)

set(CMAKE_CXX_STANDARD 14)

include_directories(${CMAKE_SOURCE_DIR}/3rdParty/include/ffmpeg)
link_directories(${CMAKE_SOURCE_DIR}/3rdParty/lib)

add_executable(videoExamples getAudioMetadata.cpp)
#add_executable(videoExamples transcode.cpp)
target_link_libraries(videoExamples 
       avcodec
       avutil
       avformat 
       avfilter 
       avdevice
       swresample 
       swscale
       )

