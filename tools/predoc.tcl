proc readfile fn {
	set h [open $fn r]
	try {read $h} finally {close $h}
}

proc writefile {fn data} {
	set h [open $fn w]
	try {puts -nonewline $h $data} finally {close $h}
}

writefile [lindex $argv 1] [string map [lrange $argv 2 end] [readfile [lindex $argv 0]]]
