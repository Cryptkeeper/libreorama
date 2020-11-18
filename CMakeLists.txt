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

add_executable(libreorama src/main.c src/player/player.h src/player/player.c src/err/al.h src/err/al.c src/err/sp.h src/err/sp.c src/file.c src/file.h src/player/sequence.h src/seqtypes/lormedia.c src/seqtypes/lormedia.h src/lorinterface/encode.h src/lorinterface/encode.c src/lorinterface/frame.h src/lorinterface/channel.c src/lorinterface/channel.h src/lorinterface/frame.c src/lorinterface/effect.h src/err/lbr.c src/err/lbr.h src/interval.c src/interval.h src/lorinterface/minify.c src/lorinterface/minify.h src/seqtypes/lorparse.h src/seqtypes/lorparse.c src/seqtypes/loreffect.c src/seqtypes/loreffect.h)

if (APPLE)
    target_link_libraries(libreorama liblightorama.a libserialport.a libalut.a libxml2.a)
    target_link_libraries(libreorama "-framework OpenAL")
endif ()

install(TARGETS libreorama RUNTIME DESTINATION bin)
