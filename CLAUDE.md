# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build/Test Commands
- Build: `make`
- Test: `make test`
- Memory check: `make valgrind`
- Tests matching a pattern: `make test TESTFLAGS="-match glob"`, where glob is a glob pattern matching the test name

## Code Style Guidelines
- C code follows C17 standard with compiler flags: `-Wall -Werror -std=c17 -g`
- Uses re2c for lexical parsing with tags enabled
- Folding markers: `//<<<` for opening, `//>>>` for closing
- Indentation: 4-space tabs (ts=4 shiftwidth=4)
- Function comments use the folding markers pattern
- Error handling: Use `THROW_*` macros for error propagation
- Memory management: Free allocated resources in cleanup sections
- Naming: snake_case for functions and variables
- Hash table indexed doubly-linked list used for object representation tracking
- Tcl integration follows standard Tcl C API patterns
- IP address parsing supports both IPv4 and IPv6 formats
- No trailing whitespace on lines.  Empty lines should be 0-length

## Project Architecture
- The code implements an accelerated facility for Tcl scripts to test whether a given IP is within a network or list of networks
- Custom Tcl_ObjType used to hold lists of non-overlapping networks in sorted order
- Network membership testing uses O(log2(n)) binary search instead of O(n) linear search
