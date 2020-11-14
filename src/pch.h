// global warnings
#pragma warning(disable: 4464)  // relative include path
#pragma warning(disable: 4820)  // padding added
#pragma warning(disable: 4623)  // implicitly defined as deleted
#pragma warning(disable: 4625)  // implicitly defined as deleted
#pragma warning(disable: 4626)  // implicitly defined as deleted
#pragma warning(disable: 5026)  // implicitly defined as deleted
#pragma warning(disable: 5027)  // implicitly defined as deleted
#pragma warning(disable: 4514)  // unreferenced inline
#pragma warning(disable: 4866)  // may not enforce left-to-right evaluation
#pragma warning(disable: 4868)  // may not enforce left-to-right evaluation
#pragma warning(disable: 4711)  // selected for automatic inline expansion
#pragma warning(disable: 4251)  // needs to have dll-interface
#pragma warning(disable: 4571)  // catch semantics
#pragma warning(disable: 4686)  // change in UDT return calling convention
#pragma warning(disable: 5045)  // spectre
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4435)  // /vd2
#pragma warning(disable: 5052)  // requires use of /std:c++latest

// popped by warnings_pop.h
#pragma warning(push, 3)
#pragma warning(disable: 4355)  // this used in initializer list
#pragma warning(disable: 4668)  // not defined as a preprocessor macro
#pragma warning(disable: 4619)  // there is no warning number 'x'
#pragma warning(disable: 5031)  // warning state pushed in different file
#pragma warning(disable: 4643)  // forward declaring in namespace std
#pragma warning(disable: 4365)  // signed/unsigned mismatch
#pragma warning(disable: 4061)  // enumerator is not explicitly handled
#pragma warning(disable: 4265)  // destructor is not virtual
#pragma warning(disable: 4623)  // default constructor implicitly deleted
#pragma warning(disable: 4266)  // no override available
#pragma warning(disable: 4267)  // conversion
#pragma warning(disable: 4774)  // format string
#pragma warning(disable: 4371)  // layout of class may have changed
#pragma warning(disable: 5039)  // throwing function passed to extern C
#pragma warning(disable: 4388)  // signed/unsigned mismatch
#pragma warning(disable: 4582)  // constructor is not implicitly called
#pragma warning(disable: 4574)  // macro is defined to be 'value'
#pragma warning(disable: 4201)  // nameless struct/union
#pragma warning(disable: 4127)  // conditional expression is constant
#pragma warning(disable: 4100)  // unreferenced parameter
#pragma warning(disable: 4242)  // possible loss of data
#pragma warning(disable: 4244)  // possible loss of data
#pragma warning(disable: 4275)  // non dll-interface base


#include <string>
#include <filesystem>
#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>
#include <mutex>
#include <regex>
#include <map>
#include <optional>
#include <atomic>
#include <functional>
#include <vector>
#include <array>
#include <set>
#include <charconv>

#include <Shlobj.h>
#include <shlwapi.h>
#include <io.h>
#include <fcntl.h>
#include <imagehlp.h>

#include <curl/curl.h>
#include <clipp.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#pragma warning(pop)

namespace mob
{

namespace fs = std::filesystem;
using hr_clock = std::chrono::high_resolution_clock;

}	// namespace
