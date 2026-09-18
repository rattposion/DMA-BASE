#pragma once

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <optional>
#include <expected>
#include <array>
#include <cstring>
#include <cctype>
#include <cmath>
#include <limits>
#include <functional>
#include <unordered_map>
#include <map>
#include <set>
#include <queue>
#include <stack>
#include <deque>
#include <list>
#include <forward_list>
#include <bitset>
#include <complex>
#include <valarray>
#include <random>
#include <numeric>
#include <ratio>
#include <cfenv>
#include <cinttypes>
#include <cstdint>
#include <cstddef>
#include <cstdbool>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cfloat>
#include <ciso646>
#include <clocale>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>

// DMA specific includes
#pragma warning(push)
#pragma warning(disable: 4200) // Disable zero-sized array warnings for DMA libraries
#include "libs/leechcore.h"
#include "libs/vmmdll.h"
#pragma warning(pop)

// Windows specific
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

// Standard library
using namespace std;
