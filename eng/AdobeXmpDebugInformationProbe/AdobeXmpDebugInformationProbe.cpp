#include <string>

// Adobe's documented public C++ binding requires its client string type before
// these headers. Initializing the public API pulls the real installed archive
// into the image; a probe with no referenced symbols would not test its PDBs.
#define TXMP_STRING_TYPE std::string
#include <XMP.hpp>
#include <XMP.incl_cpp>

int main()
{
    if (!SXMPMeta::Initialize())
        return 1;
    SXMPMeta::Terminate();
    return 0;
}
