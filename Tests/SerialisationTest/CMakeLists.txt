add_executable(SerialisationTests)

target_sources(
    SerialisationTests
    PRIVATE 
    "BitReaderAndBitWriterTests.cpp"
    "ReadStreamAndWriteStreamTests.cpp"
    "SerialiseBitTests.cpp"
    "SerialiseTests.cpp"
)

target_link_libraries(
    SerialisationTests
	PRIVATE
	Catch2::Catch2WithMain
	Serialisation
    Utility
)

set_target_properties(
    SerialisationTests
    PROPERTIES 
    FOLDER Tests
)

catch_discover_tests(SerialisationTests)