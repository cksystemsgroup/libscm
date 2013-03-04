CC=gcc

ODIR=build
DDIR=dist

WRAP=-Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=malloc_usable_size

# for compile time options uncomment the corresponding line
# see scm.h for a description of the options

# SCM:=$(SCM) -DSCM_DESCRIPTOR_PAGE_SIZE=4096
# SCM:=$(SCM) -DSCM_DESCRIPTOR_PAGE_FREELIST_SIZE=10
# SCM:=$(SCM) -DSCM_MAX_EXPIRATION_EXTENSION=10
# SCM:=$(SCM) -DSCM_DEBUG
# SCM:=$(SCM) -DSCM_MT_DEBUG
# SCM:=$(SCM) -DSCM_PRINTMEM
# SCM:=$(SCM) -DSCM_PRINTOVERHEAD
# SCM:=$(SCM) -DSCM_PRINTLOCK
# SCM:=$(SCM) -DSCM_MAKE_MICROBENCHMARKS
# SCM:=$(SCM) -DSCM_EAGER_COLLECTION

CFLAGS=$(SCM) -Wall -fPIC -g
LFLAGS=$(CFLAGS) -lpthread

HFILES := $(wildcard *.h)
CFILES := $(wildcard *.c)

OFILES := $(patsubst %.c,$(ODIR)/%.o,$(CFILES))

$(ODIR)/%.o : %.c $(HFILES) $(CFILES)
	mkdir -p $(ODIR)
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY : libscm all clean

libscm: $(OFILES)
	mkdir -p $(DDIR)
	$(CC) $(LFLAGS) $(WRAP) -shared -o $(DDIR)/libscm.so $(OFILES)
	cp scm.h $(DDIR)
	cp scm-debug.h $(DDIR)

all: libscm

clean:
	rm -rf $(ODIR)
	rm -rf $(DDIR)