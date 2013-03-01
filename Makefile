CC=gcc
OBJECTDIR=build
DISTDIR=dist
WRAPPER=-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=malloc_usable_size

# for compile time options uncomment the corresponding line
# see stm.h for a description of the options

# SCM_OPTION:=$(SCM_OPTION) -DSCM_DESCRIPTOR_PAGE_SIZE=4096
# SCM_OPTION:=$(SCM_OPTION) -DSCM_DESCRIPTOR_PAGE_FREELIST_SIZE=10
# SCM_OPTION:=$(SCM_OPTION) -DSCM_MAX_EXPIRATION_EXTENSION=10
# SCM_OPTION:=$(SCM_OPTION) -DSCM_DEBUG
# SCM_OPTION:=$(SCM_OPTION) -DSCM_MT_DEBUG
# SCM_OPTION:=$(SCM_OPTION) -DSCM_PRINTMEM
# SCM_OPTION:=$(SCM_OPTION) -DSCM_PRINTOVERHEAD
# SCM_OPTION:=$(SCM_OPTION) -DSCM_PRINTLOCK
# SCM_OPTION:=$(SCM_OPTION) -DSCM_MAKE_MICROBENCHMARKS
# SCM_OPTION:=$(SCM_OPTION) -DSCM_EAGER_COLLECTION

SOURCEFILES= \
	stm.h \
	stm-debug.h \
	descriptor_page_list.c \
	meter.h \
	meter.c \
	scm-desc.h \
	scm-desc.c \
	arch.h \
	finalizer.c

OBJECTFILES= \
	$(OBJECTDIR)/descriptor_page_list.o \
	$(OBJECTDIR)/meter.o \
	$(OBJECTDIR)/finalizer.o \
	$(OBJECTDIR)/scm-desc.o

LDLIBSOPTIONS=-lpthread

CFLAGS=$(SCM_OPTION) -Wall -fPIC -g

all: libscm

$(OBJECTDIR)/descriptor_page_list.o: $(SOURCEFILES)
	mkdir -p build
	$(CC) -c $(CFLAGS) -o $(OBJECTDIR)/descriptor_page_list.o descriptor_page_list.c

$(OBJECTDIR)/meter.o: $(SOURCEFILES)
	mkdir -p build
	$(CC) -c $(CFLAGS) -o $(OBJECTDIR)/meter.o meter.c

$(OBJECTDIR)/finalizer.o: $(SOURCEFILES)
	mkdir -p build
	$(CC) -c $(CFLAGS) -o $(OBJECTDIR)/finalizer.o finalizer.c

$(OBJECTDIR)/scm-desc.o: $(SOURCEFILES)
	mkdir -p build
	$(CC) -c $(CFLAGS) -o $(OBJECTDIR)/scm-desc.o scm-desc.c

libscm: $(OBJECTFILES)
	mkdir -p dist
	$(CC) $(CFLAGS) $(WRAPPER) -shared -o $(DISTDIR)/libscm.so -fPIC $(OBJECTFILES) $(LDLIBSOPTIONS)
	cp stm.h dist
	cp stm-debug.h dist

clean:
	rm -rf dist
	rm -rf build
