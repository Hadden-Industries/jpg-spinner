// Observable native proof that the x64 Release policy retains distinct COMDAT
// identities. Volatile pointer reads prevent the compiler from replacing the
// comparison with a source-level constant before LINK can fold the functions.
__declspec(noinline) int firstComdatIdentityProbe() noexcept { return 7; }
__declspec(noinline) int secondComdatIdentityProbe() noexcept { return 7; }

int main() noexcept
{
    auto volatile firstIdentity = &firstComdatIdentityProbe;
    auto volatile secondIdentity = &secondComdatIdentityProbe;
    return firstIdentity == secondIdentity ? 42 : 0;
}
