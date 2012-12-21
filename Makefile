OBJECTDIR=build
DISTDIR=dist
WRAPPER=-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=malloc_usable_size

# for compile time options uncomment the corresponding line
# see stm.h for a description of the options

#SCM_OPTION:=$(SCM_OPTION) -DSCM_DESCRIPTOR_PAGE_SIZE=4096
#SCM_OPTION:=$(SCM_OPTION) -DSCM_DECRIPTOR_PAGE_FREELIST_SIZE=10
#SCM_OPTION:=$(SCM_OPTION) -DSCM_MAX_EXPIRATION_EXTENSION=10
#SCM_OPTION:=$(SCM_OPTION) -DSCM_DEBUG
#SCM_OPTION:=$(SCM_OPTION) -DSCM_MT_DEBUG
#SCM_OPTION:=$(SCM_OPTION) -DSCM_MEMINFO
#SCM_OPTION:=$(SCM_OPTION) -DSCM_CHECK_CONDITIONS
#SCM_OPTION:=$(SCM_OPTION) -DSCM_EAGER_COLLECTION
#SCM_OPTION:=$(SCM_OPTION) -DSCM_TEST
SCM_OPTION:=$(SCM_OPTION) -DSCM_MAX_REGIONS=10
SCM_OPTION:=$(SCM_OPTION) -DSCM_REGION_PAGE_FREELIST_SIZE=5
SCM_OPTION:=$(SCM_OPTION) -DREG_PAGE_SIZE=4092
#SCM_OPTION:=$(SCM_OPTION) -DSCM_METER
#SCM_OPTION:=$(SCM_OPTION) -DSCM_PRINTOVERHEAD
#SCM_OPTION:=$(SCM_OPTION) -DSCM_PRINTLOCK
#SCM_OPTION:=$(SCM_OPTION) -DSCM_MAKE_MICROBENCHMARKS


SOURCEFILES= \
	stm.h \
	stm-debug.h \
	descriptor_page_list.c \
	meter.h \
	meter.c \
	scm-desc.h \
	scm-desc.c \
	arch.h \
	finalizer.c \
	regmalloc.h

OBJECTFILES= \
	$(OBJECTDIR)/descriptor_page_list.o \
	$(OBJECTDIR)/meter.o \
	$(OBJECTDIR)/finalizer.o \
	$(OBJECTDIR)/scm-desc.o

LDLIBSOPTIONS=-lpthread

CFLAGS=$(SCM_OPTION) -O3 -fPIC

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
	$(CC) $(CFLAGS) $(WRAPPER) -shared -o $(DISTDIR)/libscm.so $(OBJECTFILES) $(LDLIBSOPTIONS)
	cp stm.h dist
	cp stm-debug.h dist

clean:
	rm -rf dist
	rm -rf build
