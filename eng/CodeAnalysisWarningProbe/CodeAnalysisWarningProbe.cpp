// Deliberate C6011 fixture for the verification tool, never shipped or run.
// The optional SAL contract makes the unchecked dereference a native analyzer
// defect even though the sole concrete call below passes a valid pointer.
#include <sal.h>

int readUncheckedOptionalValue(_In_opt_ const int* value)
{
    return *value;
}

int main()
{
    const int value{7};
    return readUncheckedOptionalValue(&value) == 7 ? 0 : 1;
}
