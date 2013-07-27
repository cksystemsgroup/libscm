CC = gcc

OBJDIR = build
DISTDIR = dist

WRAP = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=malloc_usable_size

# for compile time options uncomment the corresponding line
# see libscm.h for a description of the options

# SCM:=$(SCM) -DSCM_DEBUG
# SCM:=$(SCM) -DSCM_DEBUG_THREADS
# SCM:=$(SCM) -DSCM_CHECK_CONDITIONS
# SCM:=$(SCM) -DSCM_RECORD_MEMORY_USAGE
# SCM:=$(SCM) -DSCM_PRINT_BLOCKING
# SCM:=$(SCM) -DSCM_MAKE_MICROBENCHMARKS
# SCM:=$(SCM) -DSCM_EAGER_COLLECTION

# SCM:=$(SCM) -DSCM_DESCRIPTOR_PAGE_SIZE=4096
# SCM:=$(SCM) -DSCM_DESCRIPTOR_PAGE_FREELIST_SIZE=10
# SCM:=$(SCM) -DSCM_MAX_EXPIRATION_EXTENSION=10

CFLAGS := $(SCM) -Wall -fPIC -g
LFLAGS := $(CFLAGS) -lpthread

HFILES := $(wildcard *.h)
CFILES := $(wildcard *.c)

OFILES := $(patsubst %.c,$(OBJDIR)/%.o,$(CFILES))

$(OBJDIR)/%.o : %.c $(HFILES) $(CFILES)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY : libscm all clean

libscm: $(OFILES)
	mkdir -p $(DISTDIR)
	$(CC) $(LFLAGS) $(WRAP) $(OFILES) -shared -o $(DISTDIR)/libscm.so
	cp libscm.h $(DISTDIR)

$(OFILES): | $(OBJDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

all: libscm

clean:
	rm -rf $(OBJDIR)
	rm -rf $(DISTDIR)