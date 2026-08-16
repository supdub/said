#pragma once

// Windows' filesystem is case-insensitive. Preserve that include behavior
// when compiling Windows sources from a case-sensitive host.
#include <windows.h>
