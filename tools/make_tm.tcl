proc readfile fn {
	set h	[open $fn r]
	try {read $h} finally {close $h}
}

proc writebin {fn bytes} {
	set h	[open $fn wb]
	try {puts -nonewline $h $bytes} finally {close $h}
}

set dir	[file dirname [file normalize [lindex $argv 0]]]
file mkdir $dir
set tm	{package require jitc
namespace eval ::fast_ip {namespace export *}
apply {{} {
	set h		[open [info script] rb]
	set data	[try {read $h} finally {close $h}]
	regsub {^.*?\x1A\n} $data {} c_source_cmp
	jitc::bind ::fast_ip::ip [list options {-Wall -Werror -std=c17 -g} filter {jitc::re2c --case-ranges --tags --no-debug-info} code [encoding convertfrom utf-8 [zlib inflate $c_source_cmp]]] ip
}}
}
set c_source_cmp	[zlib deflate [encoding convertto utf-8 [readfile ip.c]] 9]
writebin [lindex $argv 0] [encoding convertto utf-8 $tm]\x1A\n$c_source_cmp

