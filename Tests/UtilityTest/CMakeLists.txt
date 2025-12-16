add_executable(UtilityTests)

target_sources(
    UtilityTests
    PRIVATE 
    "AlignmentUtilityTest.cpp"
    "StringUtilityTests.cpp"
)

target_link_libraries(
    UtilityTests
    PRIVATE
    Catch2::Catch2WithMain
    Utility
)


set_target_properties(
    UtilityTests
    PROPERTIES 
    FOLDER Tests
)

catch_discover_tests(UtilityTests)