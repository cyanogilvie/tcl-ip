package require jitc 0.5.6

proc readfile fn {
	set h	[open $fn r]
	try {read $h} finally {close $h}
}

set debug	{}
if 1 {
	set dpath	/tmp/ip_in_range
	file mkdir $dpath
	file delete {*}[glob -nocomplain -types f [file join $dpath *]]
	lappend debug	debug $dpath
}

set debugmode	{}
if 0 {
	lappend debugmode	define	{DEBUG 1}	include_path	[file join [file dirname [file normalize [info script]]] teabase] \
}

namespace eval ::fast_ip {namespace export *}
jitc::bind ::fast_ip::ip [list {*}$debug {*}$debugmode \
	options	{-Wall -Werror -std=c17 -g} \
	filter	{jitc::re2c --case-ranges --tags --no-debug-info} \
	code	[string cat \
		"#ifndef Tcl_FetchInternalRep\n" \
		"#define TIP445_SHIM 1\n" \
		"#endif\n" \
		[readfile tip445.h] \
		[readfile ip.c] \
	] \
] ip

