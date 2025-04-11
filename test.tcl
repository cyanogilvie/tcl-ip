package require tcltest
tcltest::configure {*}$argv

source ip.tcl
interp alias {} ip {} ::fast_ip::ip

proc readfile fn {
	set h	[open $fn r]
	try {read $h} finally {close $h}
}

namespace eval ::ip::test {
	namespace import ::tcltest::*

	# Helper proc to verify IP parsing
	proc test_ip_parse {testname ip expected_type {expected_error ""}} {
		if {$expected_error eq ""} {
			test $testname "Testing IP parsing of $ip" -body {
				ip type $ip
			} -result $expected_type
		} else {
			test $testname "Testing IP parsing failure of $ip" -body {
				catch {ip type $ip} err
				set err
			} -match glob -result "*$expected_error*"
		}
	}

	# IPv4 format tests
	test_ip_parse ipv4-standard			192.168.1.1		ipv4
	test_ip_parse ipv4-localhost		127.0.0.1		ipv4
	test_ip_parse ipv4-broadcast		255.255.255.255	ipv4
	test_ip_parse ipv4-zeros			0.0.0.0			ipv4
	test_ip_parse ipv4-with-netbits-0	192.168.1.1/0	ipv4
	test_ip_parse ipv4-with-netbits-8	192.168.1.1/8	ipv4
	test_ip_parse ipv4-with-netbits-16	192.168.1.1/16	ipv4
	test_ip_parse ipv4-with-netbits-24	192.168.1.1/24	ipv4
	test_ip_parse ipv4-with-netbits-32	192.168.1.1/32	ipv4
	test_ip_parse ipv4-google			8.15.202.0/24	ipv4

	# IPv4 invalid tests
	test_ip_parse ipv4-invalid-format	192.168.1		{} "Can't parse IP"
	test_ip_parse ipv4-invalid-octet	192.168.1.256	{} "Can't parse IP"
	test_ip_parse ipv4-invalid-netbits	192.168.1.1/33	{} "Invalid netbits for IPv4"
	test_ip_parse ipv4-negative-netbits	192.168.1.1/-1	{} "Can't parse IP"

	# IPv6 format tests - standard format
	test_ip_parse ipv6-standard			2001:db8::1								ipv6
	test_ip_parse ipv6-loopback			::1										ipv6
	test_ip_parse ipv6-unspecified		::										ipv6
	test_ip_parse ipv6-full				2001:0db8:0000:0000:0000:0000:0000:0001	ipv6
	test_ip_parse ipv6-with-netbits-64	2001:db8::1/64							ipv6
	test_ip_parse ipv6-with-netbits-128	2001:db8::1/128							ipv6

	# IPv6 with different compression patterns
	test_ip_parse ipv6-compress-start	::2001:db8:1	ipv6
	test_ip_parse ipv6-compress-middle	2001:db8::1:2	ipv6
	test_ip_parse ipv6-compress-end		2001:db8::		ipv6

	# IPv6 with embedded IPv4
	test_ip_parse ipv6-ipv4mapped	::ffff:192.168.1.1	ipv4

	# IPv6 invalid tests
	test_ip_parse ipv6-invalid-format		2001:db8:1				{} "Can't parse IP"
	test_ip_parse ipv6-invalid-netbits		2001:db8::1/129			{} "Invalid netbits for IPv6"
	test_ip_parse ipv6-negative-netbits		2001:db8::1/-1			{} "Can't parse IP"
	test_ip_parse ipv6-too-many-segments	2001:db8:1:2:3:4:5:6:7	{} "Can't parse IP"
	test_ip_parse ipv6-invalid-chars		2001:db8::xyz			{} "Can't parse IP"

	# String representation tests
	test string-repr-ipv4			"Test IPv4 string representation"				{ip normalize 192.168.1.1}		192.168.1.1
	test string-repr-ipv4-netbits	"Test IPv4 with netbits string representation"	{ip normalize 192.168.1.1/24}	192.168.1.1/24
	test string-repr-ipv6			"Test IPv6 string representation"				{ip normalize 2001:db8::1}		2001:db8::1
	test string-repr-ipv6-netbits	"Test IPv6 with netbits string representation"	{ip normalize 2001:db8::1/64}	2001:db8::1/64
	test normalize-ipv6				"Test normalize for IPv6"						{ip normalize 2001:0db8:0000:0000:0000:0000:0000:0001}	2001:db8::1
	test normalize-ipv6-zeros		"Test normalize for IPv6 with multiple zeros"	{ip normalize 2001:0:0:0:0:0:0:1}	2001::1
	test normalize-ipv6-netbits		"Test normalize for IPv6 with netbits"			{ip normalize 2001:db8::1/64}		2001:db8::1/64

	# Test that IPv4 mappings are normalized correctly
	test normalize-ipv4mapped	"Test normalize for IPv4-mapped IPv6 address"	{ip normalize ::ffff:192.168.1.1}	192.168.1.1

	# Edge cases with IP boundaries
	test ipv4-edge-max-octets		"Test IP with max octet values"	{ip type 255.255.255.255}	ipv4
	test ipv4-edge-netbits-boundary	"Test IPv4 with boundary netbits values"	{list [ip type 192.168.1.1/0] [ip type 192.168.1.1/32]}	{ipv4 ipv4}
	test ipv6-edge-netbits-boundary	"Test IPv6 with boundary netbits values"	{list [ip type 2001:db8::1/0] [ip type 2001:db8::1/128]}	{ipv6 ipv6}

	# Performance tests (commenting out to avoid slowing down the test suite)
	# test performance-ipv4 "IPv4 parsing performance" -body {
	#     set start [clock microseconds]
	#     for {set i 0} {$i < 10000} {incr i} {
	#         ip type 192.168.1.1
	#     }
	#     set end [clock microseconds]
	#     expr {($end - $start) / 10000.0}
	# } -match <regexp {^[0-9]+\.[0-9]+$}

	# Tests for the 'contained' operation
	test contained-basic-ipv4				"Test basic IPv4 containment"					{ip contained 192.168.1.0/24	192.168.1.10}	1
	test contained-negative-ipv4			"Test IPv4 not contained"						{ip contained 192.168.1.0/24	10.0.0.1}		0
	test contained-multiple-networks-ipv4	"Test IPv4 in multiple networks"				{ip contained {192.168.1.0/24 10.0.0.0/8 172.16.0.0/12}	10.1.2.3}	1
	test contained-none-match-ipv4			"Test IPv4 matching none of multiple networks"	{ip contained {192.168.1.0/24 10.0.0.0/8 172.16.0.0/12}	8.8.8.8}	0
	test contained-exact-match-ipv4			"Test exact IPv4 address match"					{ip contained 192.168.1.1/32	192.168.1.1}	1
	test contained-network-boundary-ipv4-1	"Test IPv4 network boundary cases"				{ip contained 192.168.1.0/24	192.168.1.0}	1
	test contained-network-boundary-ipv4-2	"Test IPv4 network boundary cases"				{ip contained 192.168.1.0/24	192.168.1.255}	1
	test contained-netbits-0-ipv4			"Test IPv4 with 0 netbits (all IPs match)"		{ip contained 0.0.0.0/0			8.8.8.8}		1

	# IPv6 tests
	test contained-basic-ipv6				"Test basic IPv6 containment"				{ip contained 2001:db8::/64 2001:db8::1}	1
	test contained-negative-ipv6			"Test IPv6 not contained"					{ip contained 2001:db8::/64 2001:db9::1}	0
	test contained-multiple-networks-ipv6	"Test IPv6 in multiple networks"			{ip contained {2001:db8::/64 2001:db9::/64 fd00::/8} fd00::1}	1
	test contained-exact-match-ipv6			"Test exact IPv6 address match"				{ip contained 2001:db8::1/128 2001:db8::1}	1
	test contained-netbits-0-ipv6			"Test IPv6 with 0 netbits (all IPs match)"	{ip contained ::/0 2001:db8::1}				1

	# Mixed IPv4/IPv6 tests
	test contained-mixed-networks-1			"Test mixed IPv4/IPv6 networks"	{ip contained {192.168.1.0/24 2001:db8::/64} 192.168.1.10}	1
	test contained-mixed-networks-2			"Test mixed IPv4/IPv6 networks"	{ip contained {192.168.1.0/24 2001:db8::/64} 2001:db8::10}	1
	test contained-ipv4-mapped-ipv6			"Test IPv4-mapped IPv6 address"	{ip contained 192.168.1.0/24 ::ffff:192.168.1.10}			1
	test contained-ipv4-mapped-ipv6-network "Test IPv4-mapped IPv6 network containing IPv4"	{ip contained ::ffff:192.168.1.0/120 192.168.1.10}	1
	test contained-bad-ipv4-mapped			"Test invalid format" -body {
		ip contained 10.0.0.0/8 ::ffff::10.0.1.1
	} -returnCodes error -result {Can't parse IP "::ffff::10.0.1.1"}

	# Edge cases
	test contained-empty-networks			"Test with empty networks list"					{ip contained {} 192.168.1.1}			0
	test contained-network-with-no-netbits	"Test containment with no netbits specified"	{ip contained 192.168.1.1 192.168.1.1}	1

	test contained-sorting-order-1	"Test sorting order affects result"	{ip contained {10.0.0.0/8 192.168.1.0/24}	192.168.1.10}	1
	test contained-sorting-order-2	"Test sorting order affects result"	{ip contained {192.168.1.0/24 10.0.0.0/8}	192.168.1.10}	1

	# Performance tests (commenting out to avoid slowing down the test suite)
	# test contained-performance "Test containment performance with large network list" -body {
	#     # Create a list of 1000 networks
	#     set networks {}
	#     for {set i 0} {$i < 1000} {incr i} {
	#         lappend networks "10.[expr {$i / 256}].[expr {$i % 256}].0/24"
	#     }
	#     
	#     # Time 1000 lookups
	#     set start [clock microseconds]
	#     for {set i 0} {$i < 1000} {incr i} {
	#         ip contained $networks 8.8.8.8
	#     }
	#     set end [clock microseconds]
	#     expr {($end - $start) / 1000.0}
	# } -match <regexp {^[0-9]+\.[0-9]+$}

	# Test lookup
	if 1 {
	set network_sets [dict create \
		alibaba			[readfile alibaba.networks] \
		facebook		[readfile facebook.networks] \
		google			[readfile google.networks] \
		googlebot		[readfile googlebot.networks] \
		itregion		[readfile itregion.networks] \
		tencent			[readfile tencent.networks] \
		trafficforce	[readfile trafficforce.networks] \
	]
	} else {
	set network_sets [dict create \
		google			[readfile google.networks] \
	]
	}
	test lookup-1.1 "Test lookup, found"		{ip lookup $network_sets 66.249.68.131}	{google googlebot}
	test lookup-2.1 "Test lookup, not found"	{ip lookup $network_sets 1.1.1.2}		{}
	test lookup-bad-ipv4-mapped			"Test invalid format" -body {
		ip lookup {bad 10.0.0.0/8} ::ffff::10.0.1.1
	} -returnCodes error -result {Can't parse IP "::ffff::10.0.1.1"}

	# Clean up and report results
	cleanupTests
}
