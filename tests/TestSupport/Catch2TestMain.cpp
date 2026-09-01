#include <catch2/catch_session.hpp>

int main(int argumentCount, char* argumentValues[])
{
    // One reviewed entry point keeps every native test executable consistent
    // while each test project contributes only its subject-specific cases.
    return Catch::Session().run(argumentCount, argumentValues);
}
