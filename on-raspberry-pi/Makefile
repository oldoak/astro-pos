SHELL  = /bin/bash
CC     = gcc

SRCDIR   = $(CURDIR)/src
INCDIR   = -I/home/pi/local/include -I/usr/local/include -I./include
LIBDIR   = -L/usr/local/lib
OBJDIR   = $(CURDIR)/obj
BINDIR   = $(CURDIR)/bin

DEBUG    = -g
CFLAGS   = -Wall -O3 $(DEBUG) $(EXTRA_CFLAGS) $(INCDIR) $(LIBDIR)
XCFLAGS  = $(shell pkg-config --cflags cglm)

LDLIBS   = -lglfw -lGL -lGLU -lXrandr -lXi -lX11 -lXxf86vm -lpthread -ldl \
		   -lXinerama -lXcursor
LDFLAGS  = $(LDLIBS) $(shell pkg-config --libs cglm)
#SRCS     = $(wildcard $(SRCDIR)/*.c)

ASTROPOS = astro-pos
ASTROPOSSRC = $(SRCDIR)/astro-pos.c $(SRCDIR)/glad.c
ASTROPOSOBJ = $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(ASTROPOSSRC:.c=.o))

all: dirs $(ASTROPOS)

astro-pos-bin: $(ASTROPOS)

dirs:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

clean :
	rm -f $(BINDIR)/$(ASTROPOS) $(OBJDIR)/*.o

cleaner :
	rm -rf $(BINDIR) $(OBJDIR)

remake: cleaner all

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(CRATE) : $(CRATEOBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LDFLAGS)

$(ASTROPOS) : $(ASTROPOSOBJ)
	$(CC) $(CFLAGS) -o $(BINDIR)/$@ $^ $(LDFLAGS)

.PHONY : all dirs clean cleaner remake
