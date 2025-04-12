#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

// Only included for inline-editor linting
#include <tcl.h>
#include <tclDecls.h>
#include "tclstuff.h"

#define LITSTRS \
	X( L_TRUE,		"1" ) \
	X( L_FALSE,		"0" ) \
	X( L_IPV4,		"ipv4" ) \
	X( L_IPV6,		"ipv6" )
enum {
#define X(sym, str)	sym,
	LITSTRS
#undef X
	L_size
};
static const char*	lit_str[L_size] = {
#define X(sym, str)	str,
	LITSTRS
#undef X
};
static Tcl_Obj*		lit[L_size] = {0};

struct ll_intreps {
	struct ll_intreps*	next;
	struct ll_intreps*	prev;
	Tcl_Obj*			obj;
};
static Tcl_HashTable	g_intreps;
struct ll_intreps		ll_intreps_head = {0};
struct ll_intreps		ll_intreps_tail = {0};

static void register_intrep(Tcl_Obj* obj) //<<<
{
	Tcl_HashEntry*		he = NULL;
	int					new = 0;
	struct ll_intreps*	lle = NULL;

	lle = ckalloc(sizeof(*lle));
	*lle = (struct ll_intreps){
		.prev	= ll_intreps_tail.prev,
		.next	= &ll_intreps_tail,
		.obj	= obj,
	};
	ll_intreps_tail.prev->next	= lle;
	ll_intreps_tail.prev		= lle;

	he = Tcl_CreateHashEntry(&g_intreps, obj, &new);
	if (!new) Tcl_Panic("register_intrep: already registered");
	Tcl_SetHashValue(he, lle);
}

//>>>
void forget_intrep(Tcl_Obj* obj) //<<<
{
	Tcl_HashEntry*		he = Tcl_FindHashEntry(&g_intreps, obj);
	if (!he) Tcl_Panic("forget_intrep: not registered");
	struct ll_intreps*	lle = Tcl_GetHashValue(he);
	lle->prev->next = lle->next;
	lle->next->prev = lle->prev;
	Tcl_DeleteHashEntry(he);
	ckfree(lle);
	lle = NULL;
}

//>>>

// ip_objtype <<<
struct ip_info {
	int		af;		// AF_INET (ipv4) or AF_INET6 (ipv6)
	int16_t	netbits;
	uint8_t	normalized;
	union {
		struct in_addr	ipv4;
		struct in6_addr	ipv6;
	};
	uint64_t	skey;
};

static void free_ip_info(struct ip_info* ip) //<<<
{
	if (ip) {
		// Free anything dynamic in ip_info (nothing yet)
		ckfree(ip);
		ip = NULL;
	}
}

//>>>

static void free_ip_internal_rep(Tcl_Obj* obj);
static void dup_ip_internal_rep(Tcl_Obj* src, Tcl_Obj* dst);
static void update_ip_string_rep(Tcl_Obj* obj);

static Tcl_ObjType ip_objtype = {
	.name				= "ip",
	.freeIntRepProc		= free_ip_internal_rep,
	.dupIntRepProc		= dup_ip_internal_rep,
	.updateStringProc	= update_ip_string_rep
};

static void free_ip_internal_rep(Tcl_Obj* obj) //<<<
{
	Tcl_ObjInternalRep*	ir = Tcl_FetchInternalRep(obj, &ip_objtype);
	forget_intrep(obj);
	free_ip_info((struct ip_info*)ir->twoPtrValue.ptr1);
}

//>>>
static void dup_ip_internal_rep(Tcl_Obj* src, Tcl_Obj* dst) //<<<
{
	Tcl_ObjInternalRep*	ir = Tcl_FetchInternalRep(src, &ip_objtype);
	struct ip_info* src_ip = (struct ip_info*)ir->twoPtrValue.ptr1;
	
	// Create a copy of the ip_info structure
	struct ip_info* dst_ip = ckalloc(sizeof(*dst_ip));
	*dst_ip = *src_ip;  // Copy all fields
	
	// Store the new internal rep in the destination object
	Tcl_StoreInternalRep(dst, &ip_objtype, &(Tcl_ObjInternalRep){.twoPtrValue.ptr1 = dst_ip});
	register_intrep(dst); // Register the new object in the intrep table
}

//>>>
static void update_ip_string_rep(Tcl_Obj* obj) //<<<
{
	Tcl_ObjInternalRep*	ir = Tcl_FetchInternalRep(obj, &ip_objtype);
	struct ip_info*		ip = ir->twoPtrValue.ptr1;
	const int			buflen = INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN;
	Tcl_DString			ds;

	Tcl_DStringInit(&ds);
	Tcl_DStringSetLength(&ds, buflen + 4);	// Max length possible - longest addr string rep + / + 3 digit netbits
	char*	buf = Tcl_DStringValue(&ds);

	if (NULL == inet_ntop(ip->af, ip->af == AF_INET ? &ip->ipv4 : (struct in_addr*)&ip->ipv6, buf, buflen)) {
		int	err = Tcl_GetErrno();
		Tcl_Panic("inet_ntop failed: %s - %s" , Tcl_ErrnoId(), Tcl_ErrnoMsg(err));
	}
	size_t	bufend = strlen(buf);

	if (
			ip->netbits >= 0 &&
			ip->netbits < (ip->af == AF_INET ? 32 : 128)
	) {
		// Add the slash for the netbits suffix - make sure to include it in the buffer
		buf[bufend] = '/';
		bufend++;

		// Add the netbits value
		bufend += sprintf(buf + bufend, "%d", ip->netbits);
	}

	// Set the final string length
	Tcl_DStringSetLength(&ds, bufend);

	// Initialize the string representation
	Tcl_InitStringRep(obj, Tcl_DStringValue(&ds), bufend);
	Tcl_DStringFree(&ds);
	ip->normalized = 1;
}

//>>>

struct Tcl_ObjType networks_objtype;
static int GetIPFromObj(Tcl_Interp* interp, Tcl_Obj* obj, struct ip_info** ipPtr) //<<<
{
	int					code = TCL_OK;
	struct ip_info*		ip = NULL;
	Tcl_ObjInternalRep*	ir = NULL;

	ir = Tcl_FetchInternalRep(obj, &ip_objtype);

	if (!ir) {
		Tcl_ObjInternalRep*	net_ir = Tcl_FetchInternalRep(obj, &networks_objtype);
		if (net_ir) {
			unsigned long	count		= net_ir->ptrAndLongRep.value;
			Tcl_Obj**		networks	= net_ir->ptrAndLongRep.ptr;
			if (count == 1) {
				// We have a networks object with a single element, so we can
				// just use that element as the IP object
				ir = Tcl_FetchInternalRep(networks[0], &ip_objtype);
			}
		}
	}

	if (!ir) {
		ip = ckalloc(sizeof(*ip));
		*ip = (struct ip_info){0};

		const unsigned char*	s = (const unsigned char*)Tcl_GetString(obj);
		const unsigned char		*ns, *ae, *YYMARKER;
		/*!stags:re2c format = "const unsigned char* @@;\n"; */
		for (;;) {
			/*!re2c
				re2c:yyfill:enable		= 0;
				re2c:define:YYCTYPE		= "unsigned char";
				re2c:define:YYCURSOR	= "s";
				re2c:flags:tags			= 1;

				end			= [\x00];
				digit		= [0-9];
				hexdigit	= [0-9a-fA-F];
				dec_octet
					= digit
					| [1-9] digit
					| "1" digit{2}
					| "2" [0-4] digit
					| "25" [0-5];
				ipv4address	= dec_octet "." dec_octet "." dec_octet "." dec_octet;
				h16			= hexdigit{1,4};
				ls32		= h16 ":" h16 | ipv4address;
				ipv6address
					=                            (h16 ":"){6} ls32
					|                       "::" (h16 ":"){5} ls32
					| (               h16)? "::" (h16 ":"){4} ls32
					| ((h16 ":"){0,1} h16)? "::" (h16 ":"){3} ls32
					| ((h16 ":"){0,2} h16)? "::" (h16 ":"){2} ls32
					| ((h16 ":"){0,3} h16)? "::"  h16 ":"     ls32
					| ((h16 ":"){0,4} h16)? "::"              ls32
					| ((h16 ":"){0,5} h16)? "::"              h16
					| ((h16 ":"){0,6} h16)? "::";
				netbits		= "/" @ns digit{1,3};

				ipv4address @ae netbits? end	{ ip->af = AF_INET;  break; }
				ipv6address @ae netbits? end	{ ip->af = AF_INET6; break; }
				* { THROW_PRINTF_LABEL(finally, code, "Can't parse IP \"%s\"", Tcl_GetString(obj)); }
			*/
		}

		// Extract just the address part (without netbits) using the ae tag
		char	ip_part[INET6_ADDRSTRLEN > INET_ADDRSTRLEN ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN];
		size_t	addr_len = ae - (const unsigned char*)Tcl_GetString(obj);

		if (addr_len >= sizeof(ip_part))
			THROW_PRINTF_LABEL(finally, code, "IP address too long");

		memcpy(ip_part, Tcl_GetString(obj), addr_len);
		ip_part[addr_len] = '\0';

		if (1 != inet_pton(ip->af, ip_part, ip->af == AF_INET ? &ip->ipv4 : (struct in_addr*)&ip->ipv6))
			THROW_PRINTF_LABEL(finally, code, "inet_pton failed to parse \"%s\"", ip_part);

		if (ns) {
			const char*	netbits_str = (const char*)ns;
			ip->netbits = (int)strtol(netbits_str, NULL, 10);

			// Validate netbits range based on address family
			if (ip->af == AF_INET) {
				if (ip->netbits < 0 || ip->netbits > 32)
					THROW_PRINTF_LABEL(finally, code, "Invalid netbits for IPv4: %d (must be 0-32)", ip->netbits);
			} else if (ip->af == AF_INET6) {
				if (ip->netbits < 0 || ip->netbits > 128)
					THROW_PRINTF_LABEL(finally, code, "Invalid netbits for IPv6: %d (must be 0-128)", ip->netbits);
			}
		} else {
			ip->netbits = ip->af == AF_INET ? 32 : 128; // Default to host bits
		}

		// Convert IPv4-in-IPv6 address to IPv4.  The alternative would be to convert all IPv4 addresses to IPv6
		// which would simplify the later comparisons but would be less efficient for the IPv4 case, which is
		// overwhelmingly more common.
		if (
				ip->af == AF_INET6 &&
				ip->netbits >= 96 &&
				*(uint64_t*)ip->ipv6.s6_addr == 0 &&
				ip->ipv6.s6_addr[8] == 0 &&
				ip->ipv6.s6_addr[9] == 0 &&
				ip->ipv6.s6_addr[10] == 0xff &&
				ip->ipv6.s6_addr[11] == 0xff
		) {
			memcpy(&ip->ipv4, &ip->ipv6.s6_addr[12], sizeof(ip->ipv4));
			ip->af = AF_INET;
			ip->netbits -= 96;
		}

		// Generate the sort keys
		if (ip->af == AF_INET)
			ip->skey = (uint32_t)ntohl(ip->ipv4.s_addr);

		Tcl_StoreInternalRep(obj, &ip_objtype, &(Tcl_ObjInternalRep){.twoPtrValue.ptr1 = ip});
		ip = NULL;	// transfer ownership to the obj intrep
		register_intrep(obj);
		ir = Tcl_FetchInternalRep(obj, &ip_objtype);
	}

	*ipPtr = ir->twoPtrValue.ptr1;

finally:
	if (ip) {
		ckfree(ip);
		ip = NULL;
	}
	return code;
}

//>>>
// ip_objtype >>>
// networks_objtype <<<
static void free_networks_internal_rep(Tcl_Obj* obj);
static void dup_networks_internal_rep(Tcl_Obj* src, Tcl_Obj* dst);
static void update_networks_string_rep(Tcl_Obj* obj);

struct Tcl_ObjType networks_objtype = {
	.name				= "ip_networks",
	.freeIntRepProc		= free_networks_internal_rep,
	.dupIntRepProc		= dup_networks_internal_rep,
	.updateStringProc	= update_networks_string_rep
};

static void free_networks_internal_rep(Tcl_Obj* obj) //<<<
{
	Tcl_ObjInternalRep*	ir = Tcl_FetchInternalRep(obj, &networks_objtype);
	Tcl_Obj**			networks	= ir->ptrAndLongRep.ptr;
	size_t				count		= ir->ptrAndLongRep.value;

#if 1
	for (size_t i=0; i<count; i++) {
		//if (i > 10) break;
		replace_tclobj(&networks[i], NULL);
	}
#endif

	forget_intrep(obj);
	if (networks) {
		ckfree(networks);
		networks = NULL;
	}
}

//>>>
static void dup_networks_internal_rep(Tcl_Obj* src, Tcl_Obj* dst) //<<<
{
	Tcl_ObjInternalRep*	ir = Tcl_FetchInternalRep(src, &networks_objtype);
	size_t				count		= ir->ptrAndLongRep.value;
	Tcl_Obj**			networks	= ir->ptrAndLongRep.ptr;

	// Create a copy of the networks array
	Tcl_Obj** new_networks = ckalloc(count * sizeof(Tcl_Obj*));
	memset(new_networks, 0, count * sizeof(Tcl_Obj*));
	for (size_t i=0; i<count; i++)
		replace_tclobj(&new_networks[i], networks[i]);

	// Store the new internal rep in the destination object
	Tcl_StoreInternalRep(dst, &networks_objtype, &(Tcl_ObjInternalRep){
			.ptrAndLongRep.ptr		= new_networks,
			.ptrAndLongRep.value	= count
	});
	register_intrep(dst); // Register the new object in the intrep table
}

//>>>
static void update_networks_string_rep(Tcl_Obj* obj) //<<<
{
	Tcl_ObjInternalRep*	ir = Tcl_FetchInternalRep(obj, &networks_objtype);
	Tcl_Obj**			networks	= ir->ptrAndLongRep.ptr;
	size_t				count		= ir->ptrAndLongRep.value;
	Tcl_DString			ds;

	Tcl_DStringInit(&ds);

	// Create a string representation of the networks: A Tcl list of IP objects
	for (size_t i=0; i<count; i++)
		Tcl_DStringAppendElement(&ds, Tcl_GetString(networks[i]));

	Tcl_InitStringRep(obj, Tcl_DStringValue(&ds), Tcl_DStringLength(&ds));
	Tcl_DStringFree(&ds);
}

//>>>

static uint32_t netmask4(int netbits) //<<<
{
	if (netbits < 0 || netbits > 32) Tcl_Panic("Invalid netbits: %d", netbits);
	return (uint32_t)(~0UL << (32 - netbits));
}

//>>>
static int64_t cmp_network4(const struct ip_info* addr, const struct ip_info* net) //<<<
{
	const int16_t	netbits = net->netbits;
	const uint32_t	mask = netmask4(netbits);
	if ((addr->skey & mask) == (net->skey & mask)) return 0;
	return addr->skey - net->skey;
}

//>>>
static int64_t cmp_network6(const struct ip_info* addr, const struct ip_info* net) //<<<
{
	const uint16_t	netbits = net->netbits;
	if (netbits < 0 || netbits > 128) Tcl_Panic("Invalid netbits: %d", netbits);

	int i;
	for (i=0; i<netbits/8; i++) {
		const int diff = net->ipv6.s6_addr[i] - addr->ipv6.s6_addr[i];
		if (diff != 0) return diff;
	}

	if (netbits % 8) {
		const uint8_t mask = (uint8_t)(~0U << (8 - netbits % 8));
		const int diff = (addr->ipv6.s6_addr[i] & mask) - (net->ipv6.s6_addr[i] & mask);
		return diff;
	}

	return 0;
}

//>>>
static int64_t _compare_ip(Tcl_Obj* ip1Obj, Tcl_Obj* ip2Obj) //<<<
{
	struct ip_info*	ip1 = NULL;
	struct ip_info*	ip2 = NULL;

	if (TCL_OK != GetIPFromObj(NULL, ip1Obj, &ip1)) Tcl_Panic("compare_ip: ip1 isn't valid: \"%s\"", Tcl_GetString(ip1Obj));
	if (TCL_OK != GetIPFromObj(NULL, ip2Obj, &ip2)) Tcl_Panic("compare_ip: ip2 isn't valid: \"%s\"", Tcl_GetString(ip2Obj));

	if (ip1->af != ip2->af)
		return ip1->af == AF_INET ? -1 : 1;	// Sort all IPv4 addresses before IPv6 addresses

	// return 0 (equal) if ip1 is within ip2 (considering the netbits of ip2)
	return ip1->af == AF_INET ?
			cmp_network4(ip1, ip2) :
			cmp_network6(ip1, ip2);
}

//>>>
static int compare_ip(const void* a, const void* b) //<<<
{
	const int64_t res = _compare_ip(*(Tcl_Obj**)a, *(Tcl_Obj**)b);
	return res < 0 ? -1 : res > 0 ? 1 : 0;		// res could be outside of the range of int (ie. 0 - (2**32-1) or the other way around), so we need to normalize to {-1, 0, 1}
}

//>>>
static int GetNetworksFromObj(Tcl_Interp* interp, Tcl_Obj* obj, size_t* count, Tcl_Obj*** networksPtr) //<<<
{
	int					code = TCL_OK;
	Tcl_Obj**			networks = NULL;
	Tcl_ObjInternalRep*	ir = NULL;
	int					oc = 0;

	ir = Tcl_FetchInternalRep(obj, &networks_objtype);

	if (!ir) {
		Tcl_ObjInternalRep*	ip_ir = Tcl_FetchInternalRep(obj, &ip_objtype);
		if (ip_ir) {
			// We have an IP object, so we need to upconvert it to a networks object of one element (a duplicate of the IP object to avoid a circular reference)
			networks = ckalloc(sizeof(Tcl_Obj*));
			memset(networks, 0, sizeof(Tcl_Obj*));
			replace_tclobj(&networks[0], Tcl_DuplicateObj(obj));
			oc = 1;
			Tcl_StoreInternalRep(obj, &networks_objtype, &(Tcl_ObjInternalRep){
					.ptrAndLongRep.ptr		= networks,
					.ptrAndLongRep.value	= oc
			});
			register_intrep(obj);
			ir = Tcl_FetchInternalRep(obj, &networks_objtype);
			networks = NULL;	// transfer ownership to the obj intrep
		}
	}

	if (!ir) {
		// networks stringrep is a Tcl list of ip objects
		Tcl_Obj**	ov = NULL;
		TEST_OK_LABEL(finally, code, Tcl_ListObjGetElements(interp, obj, &oc, &ov));

		if (oc > 0) {
			networks = ckalloc(oc * sizeof(Tcl_Obj*));
			memset(networks, 0, oc * sizeof(Tcl_Obj*));

			for (int i=0; i<oc; i++) {
				struct ip_info*	ip = NULL;
				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, ov[i], &ip));	// Ensure all the list elements are valid IPs
				replace_tclobj(&networks[i], ov[i]);
			}

			// Sort the networks by address family and address
			if (oc > 1)
				qsort(networks, oc, sizeof(Tcl_Obj*), compare_ip);
		}
		Tcl_StoreInternalRep(obj, &networks_objtype, &(Tcl_ObjInternalRep){
				.ptrAndLongRep.ptr		= networks,
				.ptrAndLongRep.value	= oc
		});

		networks = NULL;	// transfer ownership to the obj intrep
		register_intrep(obj);
		ir = Tcl_FetchInternalRep(obj, &networks_objtype);
	}

	*networksPtr	= ir->ptrAndLongRep.ptr;
	*count			= ir->ptrAndLongRep.value;

finally:
	if (networks) {
		for (int i=0; i<oc; i++) replace_tclobj(&networks[i], NULL);
		ckfree(networks);
		networks = NULL;
	}
	return code;
}

//>>>
// networks_objtype >>>

INIT { //<<<
	ll_intreps_head.next = &ll_intreps_tail;
	ll_intreps_tail.prev = &ll_intreps_head;
	Tcl_InitHashTable(&g_intreps, TCL_ONE_WORD_KEYS);
	for (int i=0; i<L_size; i++) replace_tclobj(&lit[i], Tcl_NewStringObj(lit_str[i], -1));
	return TCL_OK;
}

//>>>
RELEASE { //<<<
	for (int i=0; i<L_size; i++) replace_tclobj(&lit[i], NULL);

	const char* hashstats = Tcl_HashStats(&g_intreps);
	ckfree(hashstats);

	// Since new entries are added to the tail of the list, we can just walk
	// the list and free all the entries - any new entries added during this
	// process will be processed before reaching the end.  Can't use lle->next
	// (even saved) since the Tcl_FreeInternalRep handler could have freed the
	// corresponding object too.
	for (struct ll_intreps* lle = ll_intreps_head.next; lle->next; lle = ll_intreps_head.next) {
		Tcl_GetString(lle->obj);
		Tcl_FreeInternalRep(lle->obj);	// Calls Tcl_DeleteHashEntry on this entry and unlinks lle, frees it
	}
	if (
			ll_intreps_head.next != &ll_intreps_tail ||
			ll_intreps_tail.prev != &ll_intreps_head
	) Tcl_Panic("ll_intreps_head->next != ll_intreps_tail");

	Tcl_DeleteHashTable(&g_intreps);
}

//>>>
OBJCMD(ip) //<<<
{
	int		code = TCL_OK;
	static const char* ops[] = {
		"type",
		"normalize",
		"valid",
		"eq",
		"contained",
		"lookup",
		NULL
	};
	enum {
		OP_TYPE,
		OP_NORMALIZE,
		OP_VALID,
		OP_EQ,
		OP_CONTAINED,
		OP_LOOKUP,
	} op;
	Tcl_Obj*	tmp = NULL;
	Tcl_Obj*	res = NULL;

	enum {A_cmd, A_OP, A_args};
	CHECK_MIN_ARGS_LABEL(finally, code, "op ?arg ...?");

	int	opidx;
	TEST_OK_LABEL(finally, code, Tcl_GetIndexFromObj(interp, objv[A_OP], ops, "op", TCL_EXACT, &opidx));
	op = opidx;
	switch (op) {
		case OP_TYPE: //<<<
			{
				enum {A_cmd=1, A_IP, A_objc};
				CHECK_ARGS_LABEL(finally, code, "ip");
				struct ip_info*	ip = NULL;
				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, objv[A_IP], &ip));
				switch (ip->af) {
					case AF_INET:	Tcl_SetObjResult(interp, lit[L_IPV4]);	break;
					case AF_INET6:	Tcl_SetObjResult(interp, lit[L_IPV6]);	break;
					default:		THROW_ERROR_LABEL(finally, code, "Invalid address type");
				}
				break;
			}
			//>>>
		case OP_NORMALIZE: //<<<
			{
				enum {A_cmd=1, A_IP, A_objc};
				CHECK_ARGS_LABEL(finally, code, "ip");
				struct ip_info*		ip = NULL;

				// Validate the IP format by retrieving its internal representation
				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, objv[A_IP], &ip));

				Tcl_Obj*	res = NULL;
				if (Tcl_HasStringRep(objv[A_IP]) && !ip->normalized) {
					res = Tcl_DuplicateObj(objv[A_IP]);	// Create a duplicate that we can manipulate
					Tcl_InvalidateStringRep(res);		// Invalidate the string representation to force regeneration
				} else {
					res = objv[A_IP];
				}
				Tcl_SetObjResult(interp, res);
				break;
			}
			//>>>
		case OP_VALID: //<<<
			{
				enum {A_cmd=1, A_IP, A_objc};
				CHECK_ARGS_LABEL(finally, code, "ip");
				struct ip_info*	ip = NULL;
				const int		is_valid = TCL_OK == GetIPFromObj(interp, objv[A_IP], &ip);
				Tcl_ResetResult(interp);
				Tcl_SetObjResult(interp, lit[is_valid ? L_TRUE : L_FALSE]);
				break;
			}
			//>>>
		case OP_EQ: //<<<
			{
				enum {A_cmd=1, A_IP1, A_IP2, A_objc};
				CHECK_ARGS_LABEL(finally, code, "ip ip");
				struct ip_info*	ip1 = NULL;
				struct ip_info*	ip2 = NULL;
				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, objv[A_IP1], &ip1));
				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, objv[A_IP2], &ip2));
				Tcl_SetObjResult(interp, lit[
						ip1->af			== ip2->af &&
						ip1->netbits	== ip2->netbits &&
						(
							(ip1->af == AF_INET  && ip1->ipv4.s_addr == ip2->ipv4.s_addr == 0) ||
							(ip1->af == AF_INET6 && memcmp(&ip1->ipv6.s6_addr, &ip2->ipv6.s6_addr, sizeof(ip1->ipv6.s6_addr)) == 0)
						)
						? L_TRUE : L_FALSE
				]);
				break;
			}
			//>>>
		case OP_CONTAINED: //<<<
			{
				enum {A_cmd=1, A_NETWORKS, A_IP, A_objc};
				CHECK_ARGS_LABEL(finally, code, "networks ip");
				Tcl_Obj**			networks = NULL;
				size_t				count = 0;
				struct ip_info*		ip = NULL;

				// Must get the networks intrep first, since A_NETWORKS and A_IP may alias each other
				// and GetNetworksFromObj will shimmer an ip_objtype to networks_objtype, invalidating
				// the ip_objtype intrep pointer we would be holding if we'd fetched that one first.
				TEST_OK_LABEL(finally, code, GetNetworksFromObj(interp, objv[A_NETWORKS], &count, &networks));
				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, objv[A_IP], &ip));	// Not directly required, but rather do it here where proper errors can be thrown, rather than Tcl_Panicing in _compare_ip

				// Perform binary search
				Tcl_Obj* result = bsearch(&objv[A_IP], networks, count, sizeof(Tcl_Obj*), compare_ip);
				
				Tcl_SetObjResult(interp, lit[result != NULL ? L_TRUE : L_FALSE]);
				break;
			}
			//>>>
			case OP_LOOKUP: //<<<
			{
				Tcl_Obj**			networks = NULL;
				size_t				count = 0;
				struct ip_info*		ip = NULL;
				Tcl_DictSearch		search;
				Tcl_Obj				*k, *v;
				int					done;

				enum {A_cmd=1, A_NETWORK_SETS, A_IP, A_objc};
				CHECK_ARGS_LABEL(finally, code, "network_sets ip");

				TEST_OK_LABEL(finally, code, GetIPFromObj(interp, objv[A_IP], &ip));	// Not directly required, but rather do it here where proper errors can be thrown, rather than Tcl_Panicing in _compare_ip
				ip = NULL;	// Could be invalidated by later GetNetworksFromObj

				replace_tclobj(&res, Tcl_NewListObj(0, NULL));
				TEST_OK_LABEL(finally, code, Tcl_DictObjFirst(interp, objv[A_NETWORK_SETS], &search, &k, &v, &done));
				for (; !done; Tcl_DictObjNext(&search, &k, &v, &done)) {
					replace_tclobj(&tmp, v);
					TEST_OK_LABEL(donesearch, code, GetNetworksFromObj(interp, tmp, &count, &networks));
					if (NULL != bsearch(&objv[A_IP], networks, count, sizeof(Tcl_Obj*), compare_ip))
						TEST_OK_LABEL(donesearch, code, Tcl_ListObjAppendElement(interp, res, k));
				}
			donesearch:
				Tcl_DictObjDone(&search);
				if (code != TCL_OK) goto finally;

				Tcl_SetObjResult(interp, res);
				break;
			}

			//>>>
		default: THROW_ERROR_LABEL(finally, code, "Unhandled op");
	}

finally:
	replace_tclobj(&tmp, NULL);
	replace_tclobj(&res, NULL);
	return code;
}

//>>>

// vim: foldmethod=marker foldmarker=<<<,>>> ts=4 shiftwidth=4
