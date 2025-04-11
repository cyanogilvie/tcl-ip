DESTDIR =
PREFIX = /usr/local
TCLSH = tclsh8.7
TCLSH_BENCH = cftcl
VALGRINDEXTRA =
VALGRINDARGS = --tool=memcheck --num-callers=8 --leak-resolution=high \
			   --leak-check=yes -v --suppressions=suppressions --keep-debuginfo=yes \
			   --error-exitcode=2 $(VALGRINDEXTRA)

PACKAGE_LOAD = "source ip.tcl"
PACKAGE_LOAD_EMBED = source\ ip.tcl
VER = 1.1

tm: tm/fast_ip-$(VER).tm

tm/fast_ip-$(VER).tm: ip.c tools/make_tm.tcl
	$(TCLSH_ENV) $(TCLSH) tools/make_tm.tcl tm/fast_ip-$(VER).tm

test:
	$(TCLSH_ENV) $(PKG_ENV) $(TCLSH) test.tcl $(TESTFLAGS)

valgrind:
	$(TCLSH_ENV) $(PKG_ENV) valgrind $(VALGRINDARGS) $(TCLSH) test.tcl $(TESTFLAGS)

vim-gdb:
	$(TCLSH_ENV) $(PKG_ENV) vim -c "set number" -c "set mouse=a" -c "set foldlevel=100" -c "Termdebug -ex set\ print\ pretty\ on --args $(TCLSH) test.tcl -singleproc 1 -load $(PACKAGE_LOAD_EMBED) $(TESTFLAGS)" -c "2windo set nonumber" -c "1windo set nonumber"

benchmark:
	$(TCLSH_ENV) $(PKG_ENV) $(TCLSH_BENCH) bench/run.tcl $(BENCHFLAGS) -load $(PACKAGE_LOAD)

doc: doc/ip.n README.md

doc/ip.n: doc/.build/ip.md
	pandoc --standalone --from markdown --to man doc/.build/ip.md --output doc/ip.n

README.md: doc/.build/ip.md
	pandoc --standalone --from markdown --to gfm doc/.build/ip.md --output README.md

doc/.build/ip.md: doc/ip.md.in
	@mkdir -p doc/.build
	@$(TCLSH) tools/predoc.tcl doc/ip.md.in doc/.build/ip.md

clean:
	-rm -rf doc/.build tm doc/ip.n

install: install-tm install-doc

install-tm: tm doc
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/tcl8/site-tcl
	cp tm/fast_ip-$(VER).tm $(DESTDIR)$(PREFIX)/lib/tcl8/site-tcl/

install-doc: doc
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/mann
	cp -f doc/ip.n $(DESTDIR)$(PREFIX)/share/man/mann/

.PHONY: test valgrind vim-gdb benchmark doc tm clean install install-tm install-doc
