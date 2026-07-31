# Utility sources
util_sources = files(
    'util/console_logger.cpp',
    'util/string_utils.cpp',
    'util/fft.cpp',
)

# Platform sources
platform_sources = files(
    'platform/sdl3/sdl_window.cpp',
)

# Engine sources
engine_sources = files(
    'engine/application.cpp',
    'engine/ini_config.cpp',
    'engine/map_transition_resolver.cpp',
    'engine/map_scene.cpp',
    'engine/physics.cpp',
    'engine/virtual_filesystem.cpp',
    'engine/states/state_context.cpp',
    'engine/states/intro_state.cpp',
    'engine/states/title_state.cpp',
    'engine/states/character_creation_state.cpp',
    'engine/states/loading_state.cpp',
    'engine/states/ingame_state.cpp',
    'engine/states/credits_state.cpp',
    'engine/states/load_game_state.cpp',
)

# Formats sources
formats_sources = files(
    'formats/lod_archive.cpp',
    'formats/image_lod_archive.cpp',
    'formats/sprite_lod_archive.cpp',
    'formats/game_lod_archive.cpp',
    'formats/snd_archive.cpp',
    'formats/sound_list.cpp',
    'formats/pcx_image.cpp',
    'formats/sprite_parser.cpp',
    'formats/blv_map.cpp',
    'formats/odm_map.cpp',
    'formats/tile_table.cpp',
    'formats/face_geometry.cpp',
    'formats/event_file_parser.cpp',
    'formats/global_text_parser.cpp',
    'formats/spells_parser.cpp',
    'formats/items_parser.cpp',
    'formats/placemon_parser.cpp',
    'formats/rnditems_parser.cpp',
    'formats/monsters_parser.cpp',
    'formats/mapstats_parser.cpp',
    'formats/quests_parser.cpp',
    'formats/classes_parser.cpp',
    'formats/awards_parser.cpp',
    'formats/hostile_parser.cpp',
    'formats/npcdata_parser.cpp',
    'formats/npctopic_parser.cpp',
    'formats/npctext_parser.cpp',
    'formats/npcnames_parser.cpp',
    'formats/npcprof_parser.cpp',
    'formats/npcgreet_parser.cpp',
    'formats/autonote_parser.cpp',
    'formats/credits_parser.cpp',
    'formats/two_d_events_parser.cpp',
    'formats/merchant_parser.cpp',
    'formats/frame_tables.cpp',
    'formats/evt_script_parser.cpp',
)

# Graphics sources
graphics_sources = files(
    'graphics/bitmap_font.cpp',
    'graphics/debug_text.cpp',
    'graphics/palette.cpp',
    'graphics/image.cpp',
    'graphics/image_loader.cpp',
    'graphics/sprite_decoder.cpp',
    'graphics/sprite_loader.cpp',
    'graphics/texture_manager.cpp',
    'graphics/sdl_renderer.cpp',
    'graphics/camera.cpp',
    'graphics/line_renderer.cpp',
    'graphics/world_renderer.cpp',
    'graphics/indoor_renderer.cpp',
    'graphics/outdoor_renderer.cpp',
    'graphics/visibility.cpp',
)

media_sources = files(
    'media/vid_manifest.cpp',
    'media/vid_archive.cpp',
    'media/smacker_decoder.cpp',
    'media/bink_decoder.cpp',
    'media/video_player.cpp',
)

# Game sources
game_sources = files(
    'game/character.cpp',
    'game/building_type.cpp',
    'game/party.cpp',
    'game/game_world.cpp',
    'game/generation.cpp',
    'game/event_engine.cpp',
    'game/inventory.cpp',
    'game/combat.cpp',
    'game/spells.cpp',
    'game/quest_log.cpp',
    'game/save_game.cpp',
)

# Audio sources
audio_sources = files(
    'audio/audio_system.cpp',
)

# UI sources
ui_sources = files(
    'ui/layout.cpp',
    'ui/widget.cpp',
    'ui/widgets.cpp',
    'ui/ui_manager.cpp',
    'ui/hud.cpp',
    'ui/dialogue.cpp',
    'ui/inventory_widget.cpp',
    'ui/character_stats_widget.cpp',
    'ui/spellbook_widget.cpp',
    'ui/rest_widget.cpp',
    'ui/map_widget.cpp',
)

# Dependencies
zlib_dep = dependency('zlib',
    version: '>=1.2.11',
    required: false,
    fallback: ['zlib', 'zlib_dep']
)

if not zlib_dep.found()
    error('zlib is required for LOD archive decompression')
endif

# Shaders
python = import('python').find_installation('python3')
shader_sources = files('graphics/shaders/world.vert', 'graphics/shaders/world.frag')
shaders_header = custom_target(
    'compile_shaders',
    input: shader_sources,
    output: 'shaders_compiled.hpp',
    command: [python, meson.project_source_root() / 'tools/compile_shaders.py', '--output', '@OUTPUT@', '@INPUT@'],
)

# Main executable
executable(
    'runeharbor',
    sources: [
        'main.cpp',
        shaders_header,
        util_sources,
        platform_sources,
        engine_sources,
        formats_sources,
        game_sources,
        graphics_sources,
        media_sources,
        audio_sources,
        ui_sources,
    ],
    include_directories: inc_dirs,
    dependencies: [sdl3_dep, zlib_dep, sdl_shadercross_dep],
    install: true,
)

asset_check_sources = files(
    '../tools/asset_check.cpp',
    'util/string_utils.cpp',
    'formats/image_lod_archive.cpp',
    'formats/sprite_lod_archive.cpp',
    'formats/game_lod_archive.cpp',
    'formats/snd_archive.cpp',
    'formats/sound_list.cpp',
    'formats/blv_map.cpp',
    'formats/odm_map.cpp',
    'formats/tile_table.cpp',
    'formats/face_geometry.cpp',
    'formats/evt_script_parser.cpp',
    'formats/global_text_parser.cpp',
    'formats/spells_parser.cpp',
    'formats/items_parser.cpp',
    'formats/placemon_parser.cpp',
    'formats/rnditems_parser.cpp',
    'formats/monsters_parser.cpp',
    'formats/mapstats_parser.cpp',
    'formats/quests_parser.cpp',
    'formats/classes_parser.cpp',
    'formats/awards_parser.cpp',
    'formats/hostile_parser.cpp',
    'formats/npcdata_parser.cpp',
    'formats/npctopic_parser.cpp',
    'formats/npctext_parser.cpp',
    'formats/npcnames_parser.cpp',
    'formats/npcprof_parser.cpp',
    'formats/npcgreet_parser.cpp',
    'formats/autonote_parser.cpp',
    'formats/credits_parser.cpp',
    'formats/two_d_events_parser.cpp',
    'formats/merchant_parser.cpp',
    'formats/frame_tables.cpp',
    'game/building_type.cpp',
    'media/vid_archive.cpp',
)

executable(
    'runeharbor_asset_check',
    sources: asset_check_sources,
    include_directories: inc_dirs,
    dependencies: [zlib_dep],
    install: false,
)
