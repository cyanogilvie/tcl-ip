proc readfile fn {
	set h [open $fn r]
	try {read $h} finally {close $h}
}

proc writefile {fn data} {
	set h [open $fn w]
	try {puts -nonewline $h $data} finally {close $h}
}

writefile [lindex $argv 1] [string map [list \
	@PACKAGE_NAME@		fast_ip \
	@PACKAGE_VERSION@	[string trim [readfile version]] \
] [readfile [lindex $argv 0]]]
