DESTDIR =
PREFIX = /usr/local
TCLSH = tclsh
TCLSH_BENCH = tclsh
VALGRINDEXTRA =
VALGRINDARGS = --tool=memcheck --num-callers=8 --leak-resolution=high \
			   --leak-check=yes -v --suppressions=suppressions --keep-debuginfo=yes \
			   --error-exitcode=2 $(VALGRINDEXTRA)

SCAN_BUILD = scan-build-22
CLANG = clang-22
RE2C = re2c
TCL_INCLUDE = /opt/tcl9g/include
SCANDIR = .scan-build
SCANFLAGS =

PACKAGE_LOAD = "source ip.tcl"
PACKAGE_LOAD_EMBED = source\ ip.tcl
PACKAGE_NAME = fast_ip
VER = $(file < version)

all: tm doc

tm: tm/$(PACKAGE_NAME)-$(VER).tm

tm/$(PACKAGE_NAME)-$(VER).tm: ip.c tools/make_tm.tcl
	$(TCLSH_ENV) $(TCLSH) tools/make_tm.tcl tm/$(PACKAGE_NAME)-$(VER).tm

test:
	$(TCLSH_ENV) $(PKG_ENV) $(TCLSH) test.tcl $(TESTFLAGS)

valgrind:
	$(TCLSH_ENV) $(PKG_ENV) valgrind $(VALGRINDARGS) $(TCLSH) test.tcl $(TESTFLAGS)

vim-gdb:
	$(TCLSH_ENV) $(PKG_ENV) vim -c "set number" -c "set mouse=a" -c "set foldlevel=100" -c "Termdebug -ex set\ print\ pretty\ on --args $(TCLSH) test.tcl -singleproc 1 -load $(PACKAGE_LOAD_EMBED) $(TESTFLAGS)" -c "2windo set nonumber" -c "1windo set nonumber"

benchmark:
	$(TCLSH_ENV) $(PKG_ENV) $(TCLSH_BENCH) bench/run.tcl $(BENCHFLAGS) -load $(PACKAGE_LOAD)

scan-build:
	@mkdir -p $(SCANDIR)
	$(RE2C) --case-ranges --tags --no-debug-info -o $(SCANDIR)/ip.c ip.c
	$(SCAN_BUILD) --use-cc=$(CLANG) -o $(SCANDIR)/reports $(SCANFLAGS) \
		$(CLANG) -c -std=c17 -Wall \
			-I$(TCL_INCLUDE) -I. -Iteabase \
			$(SCANDIR)/ip.c -o $(SCANDIR)/ip.o

doc: doc/ip.n README.md

doc/ip.n: doc/.build/ip.md
	pandoc --standalone --from markdown --to man doc/.build/ip.md --output doc/ip.n

README.md: doc/.build/ip.md
	pandoc --standalone --from markdown --to gfm doc/.build/ip.md --output README.md

doc/.build/ip.md: doc/ip.md.in version Makefile
	@mkdir -p doc/.build
	@$(TCLSH) tools/predoc.tcl doc/ip.md.in doc/.build/ip.md @PACKAGE_NAME@ "$(PACKAGE_NAME)" @PACKAGE_VERSION@ "$(VER)"

clean:
	-rm -rf doc/.build tm doc/ip.n $(SCANDIR)

install: install-tm install-doc

install-tm: tm
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/tcl9/site-tcl
	cp tm/$(PACKAGE_NAME)-$(VER).tm $(DESTDIR)$(PREFIX)/lib/tcl8/site-tcl/

install-doc: doc
	@mkdir -p $(DESTDIR)$(PREFIX)/share/man/mann
	cp -f doc/ip.n $(DESTDIR)$(PREFIX)/share/man/mann/

.PHONY: test valgrind vim-gdb benchmark scan-build doc tm clean install install-tm install-doc
