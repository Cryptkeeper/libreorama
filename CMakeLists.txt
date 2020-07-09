cmake_minimum_required(VERSION 3.16)

project(libreorama
        VERSION 1.0.0
        DESCRIPTION "Open-source Light-O-Rama sequence player software"
        HOMEPAGE_URL "https://github.com/Cryptkeeper/libreorama"
        LANGUAGES C)

set(CMAKE_C_STANDARD 99)

if (APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif ()

add_executable(libreorama src/main.c src/player.h src/player.c src/err.h src/err.c src/file.c src/file.h src/sequence.c src/sequence.h src/seqtypes/lormedia.c src/seqtypes/lormedia.h src/encode.h src/encode.c src/frame.h src/channel.c src/channel.h src/frame.c src/audio.c src/audio.h src/effect.h src/lbrerr.c src/lbrerr.h)

if (APPLE)
    target_link_libraries(libreorama liblightorama.a libserialport.a libalut.a libxml2.a)
    target_link_libraries(libreorama "-framework OpenAL")
endif ()

install(TARGETS libreorama RUNTIME DESTINATION bin)