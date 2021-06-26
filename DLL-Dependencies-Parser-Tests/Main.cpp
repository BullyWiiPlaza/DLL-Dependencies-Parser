#define BOOST_TEST_MODULE DLL References Resolver
#include <boost/test/included/unit_test.hpp>

#include "../DLLReferencesResolver.hpp"
#include <filesystem>

std::filesystem::path test_files_directory = std::filesystem::absolute("Test Files");

BOOST_AUTO_TEST_CASE(test_non_existing_file_path)
{
    try
    {
        dll_references_resolver references_resolver;
        references_resolver.executable_file_path = test_files_directory / "non-existing.exe";
        references_resolver.resolve_references();
        BOOST_REQUIRE(false);
    }
	catch(std::exception &)
	{
        BOOST_REQUIRE(true);
	}
}

BOOST_AUTO_TEST_CASE(test_utf8_file_path)
{
    dll_references_resolver references_resolver;
    references_resolver.executable_file_path = test_files_directory / "Fran\u00C7ais" / "VC_redist.x64.exe";
    references_resolver.skip_parsing_windows_dll_dependencies = true;
    const auto [dll_load_failures, missing_dlls, referenced_dlls] = references_resolver.resolve_references();
    BOOST_REQUIRE(dll_load_failures.empty());
    BOOST_REQUIRE(missing_dlls.empty());
    BOOST_REQUIRE(referenced_dlls.size() == 8);
}

BOOST_AUTO_TEST_CASE(test_bot_utilities_parsing)
{
    dll_references_resolver references_resolver;
    references_resolver.executable_file_path = test_files_directory / "Bot-Utilities.exe";
    references_resolver.skip_parsing_windows_dll_dependencies = true;
    auto [dll_load_failures, missing_dlls, referenced_dlls] = references_resolver.resolve_references();
    BOOST_REQUIRE(dll_load_failures.size() == 1);
    BOOST_REQUIRE(missing_dlls.size() == 11);
    BOOST_REQUIRE(referenced_dlls.size() == 30);
}

BOOST_AUTO_TEST_CASE(test_parsing_with_system_libraries)
{
    dll_references_resolver references_resolver;
    references_resolver.executable_file_path = test_files_directory / "Bot-Utilities.exe";
    references_resolver.skip_parsing_windows_dll_dependencies = false;

    const auto [dll_load_failures, missing_dlls, referenced_dlls] = references_resolver.resolve_references();
    BOOST_REQUIRE(dll_load_failures.size() == 1);
    BOOST_REQUIRE(missing_dlls.size() == 11);
    BOOST_REQUIRE(referenced_dlls.size() == 42);

    // Make sure running this again yields the same results
    const auto [dll_load_failures_2, missing_dlls_2, referenced_dlls_2] = references_resolver.resolve_references();
    BOOST_REQUIRE(dll_load_failures_2.size() == 1);
    BOOST_REQUIRE(missing_dlls_2.size() == 11);
    BOOST_REQUIRE(referenced_dlls_2.size() == 42);
}

BOOST_AUTO_TEST_CASE(test_jduel_links_bot_hooks_parsing)
{
    dll_references_resolver references_resolver;
    references_resolver.executable_file_path = test_files_directory / "JDuelLinksBotHooks.dll";
    references_resolver.skip_parsing_windows_dll_dependencies = true;
    const auto [dll_load_failures, missing_dlls, referenced_dlls] = references_resolver.resolve_references();
    BOOST_REQUIRE(dll_load_failures.empty());
    BOOST_REQUIRE(missing_dlls.empty());
    BOOST_REQUIRE(referenced_dlls.size() == 3);
}