-include Makefile-automake

.PHONY: run-autoconf
run-autoconf:
	libtoolize
	aclocal
	automake --add-missing --copy
	autoconf
	./configure
	$(MAKE)

RELEASE_SOURCES = \
	ChangeLog.txt \
	configure.ac \
	conspy.1 \
	conspy.c \
	conspy.html \
	conspy.spec \
	Makefile \
	Makefile-automake.am \
	Makefile.release \
	README.txt

include Makefile.release
