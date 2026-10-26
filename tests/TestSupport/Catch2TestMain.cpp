#include <catch2/catch_session.hpp>
#include <catch2/catch_user_config.hpp>

// Enforce the selected library's generated configuration for every test host.
// Reporter/file destinations must come from our explicit CLI, never ambient
// Bazel variables evaluated before the harness/test cases gain control.
#ifndef CATCH_CONFIG_NO_GETENV
#error The selected Catch2 dependency must disable ambient environment configuration.
#endif

int main(int argumentCount, char *argumentValues[])
{
    // One reviewed entry point keeps every native test executable consistent
    // while each test project contributes only its subject-specific cases.
    return Catch::Session().run(argumentCount, argumentValues);
}
