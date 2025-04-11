package require chantricks
package require rl_http
package require parse_args
package require rl_json

namespace eval radb {
	namespace export *
	namespace ensemble create -prefixes no
	namespace path {::parse_args ::rl_json}

	proc _req query { #<<<
		package require chantricks
		chantricks with_chan sock {socket whois.radb.net 43} {
			chan configure $sock -translation crlf -buffering line
			puts $sock $query
			read $sock
		}
	}

	#>>>
	proc routes args { #<<<
		parse_args $args {
			-types	{-default ipv4 -enum {ipv4 ipv6 both}}
			-asn	{-required -# {ASN, with "AS" prefix or just as an integer}}
		}

		if {[string is integer -strict $asn]} {
			set asn	AS$asn
		}

		set query "-i origin $asn"
		switch -exact -- $types {
			ipv4	{set query "-T route $query"}
			ipv6	{set query "-T route6 $query"}
		}

		set resp	[_req $query]
		lmap {- route} [switch -exact -- $types {
			ipv4 {regexp -all -inline -line {^route:\s*(.*?)\s*$} $resp}
			ipv6 {regexp -all -inline -line {^route6:\s*(.*?)\s*$} $resp}
			both {regexp -all -inline -line {^route6?:\s*(.*?)\s*$} $resp}
			default {error "Invalid types"}
		}] {set route}
	}

	#>>>
	proc ip ip { #<<<
		set resp	[_req "-l $ip"]
		set res	{{}}

		foreach line [split [string trim $resp] \n] {
			switch -regexp -matchvar m -- $line {
				{^origin:\s*(.*?)\s*$}			{json set res origin		[json string [lindex $m 1]]}
				{^route:\s*(.*?)\s*$}			{json set res route			[json string [lindex $m 1]]}
				{^created:\s*(.*?)\s*$}			{json set res created		[json string [lindex $m 1]]}
				{^last-modified:\s*(.*?)\s*$}	{json set res last-modified	[json string [lindex $m 1]]}
				{^descr:\s*(.*?)\s*$}			{json set res descr			[json string [lindex $m 1]]}
			}
		}

		set res
	}

	#>>>
}

# vim: ft=tcl foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
