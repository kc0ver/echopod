add_library(echo_library STATIC
    block_cache.c
    covers.c
    favorites.c
    library.c
    library_browse.c
    mp4_chapters.c
    music_fingerprint.c
    music_folders.c
    sort_initial.c
    spoken_path.c
)
target_include_directories(echo_library PUBLIC include)
if(ECHO_BUILD_SIM OR ECHO_RELEASE)
    target_compile_definitions(echo_library PRIVATE ECHO_READER_DEMO=1)
endif()
if(CMAKE_CROSSCOMPILING)
    target_compile_definitions(echo_library PRIVATE ECHO_RESIDENT_TABLES)
endif()
target_link_libraries(echo_library
    PUBLIC echo_core
    PRIVATE echo_options echo_compact
)

set(ECHO_LIBRARY_SCAN_SOURCES
    builder_sort.c
    builder_store.c
    cover_build.c
    library_builder.c
    library_builder_albums.c
    library_builder_encode.c
    library_builder_model.c
    library_builder_paths.c
    library_builder_rank.c
    library_scan.c
    library_scan_entry.c
    playlist_reader.c
    sort_fold_table.c
    sort_key.c
    tags_ape.c
    tags_asf.c
    tags_genres.c
    tags_id3.c
    tags_mp4.c
    tags_mpeg.c
    tags_qoa.c
    tags_riff.c
    tags_vorbis.c
    track_fields.c
    track_tags.c
)
if(NOT ECHO_FEATURE_EXTRA_INDEX)
    list(REMOVE_ITEM ECHO_LIBRARY_SCAN_SOURCES tags_ape.c tags_asf.c)
endif()
if(NOT ECHO_FEATURE_QOA)
    list(REMOVE_ITEM ECHO_LIBRARY_SCAN_SOURCES tags_qoa.c)
endif()
if(NOT ECHO_FEATURE_WAV AND NOT ECHO_FEATURE_EXTRA_INDEX)
    list(REMOVE_ITEM ECHO_LIBRARY_SCAN_SOURCES tags_riff.c)
endif()
add_library(echo_library_scan STATIC ${ECHO_LIBRARY_SCAN_SOURCES})
target_include_directories(echo_library_scan PUBLIC include)
target_link_libraries(echo_library_scan
    PUBLIC echo_core echo_image echo_library
    PRIVATE echo_options
)
target_link_libraries(echo_library PRIVATE echo_lto)
