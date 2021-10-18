#pragma once

// Multi line macros always use do/while(0) to wrap multiple lines that appear as one logical statement to the
// programmer using the macro.  Using while(0) generates warning 4127 - Conditional Expression is Constant.
// The END_MULTI_LINE_MACRO macro suppresses this warning, and while(0) continues to preserve the intended
// behavior of requiring a semicolon at the end.

#define	BEGIN_MULTI_LINE_MACRO		do {

#define	END_MULTI_LINE_MACRO		__pragma(warning(push)) \
									__pragma(warning(disable:4127)) \
									} while(0) \
									__pragma(warning(pop))
