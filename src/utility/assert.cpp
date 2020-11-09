#include "pch.h"
#include "assert.h"
#include "../core/context.h"

namespace mob
{

void mob_assertion_failed(
  const char* message,
  const char* exp, const wchar_t* file, int line, const char* func)
{
  if (message)
  {
	gcx().error(context::generic,
	  "assertion failed: {}:{} {}: {} ({})",
	  std::wstring(file), line, func, message, exp);
  }
  else
  {
	gcx().error(context::generic,
	  "assertion failed: {}:{} {}: '{}'",
	  std::wstring(file), line, func, exp);
  }

  if (IsDebuggerPresent())
	DebugBreak();
}

} // namespace
