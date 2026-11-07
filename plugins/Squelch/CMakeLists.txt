juce_add_plugin(Squelch
    COMPANY_NAME "Homebrew"
    PLUGIN_MANUFACTURER_CODE Hmbr
    PLUGIN_CODE Sqlc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Squelch"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(Squelch PRIVATE Source/PluginProcessor.cpp)

target_compile_definitions(Squelch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Squelch
    PRIVATE
        synth_shared
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
