# Developer tools (off by default): cmake -B build -DHOMEBREW_BUILD_TESTS=ON
juce_add_console_app(DivaConvert PRODUCT_NAME "DivaConvert")
target_sources(DivaConvert PRIVATE DivaConvert.cpp ../plugins/Mira/Source/DivaImporter.cpp ../plugins/Mira/Source/MiraPresets.cpp)
target_include_directories(DivaConvert PRIVATE ../plugins/Mira/Source)
target_compile_definitions(DivaConvert PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DivaConvert PRIVATE juce::juce_audio_processors PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(GeneratorTest PRODUCT_NAME "GeneratorTest")
target_sources(GeneratorTest PRIVATE GeneratorTest.cpp)
target_include_directories(GeneratorTest PRIVATE ../plugins/Squelch/Source)
target_compile_definitions(GeneratorTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(GeneratorTest PRIVATE juce::juce_core PUBLIC juce::juce_recommended_config_flags)
