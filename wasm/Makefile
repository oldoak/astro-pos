SET_ENV = . $(HOME)/bin/a-emcc
CC      = emcc
TARGET  = astro-pos
SRCS    = astro-pos.c

INCDIR       = -I/Users/hitesh/local/include -I/usr/local/include -I./include
LIBDIR       = -L/usr/local/lib
OBJDIR       = $(CURDIR)/web/build
BINDIR       = $(CURDIR)/web/static

DEBUG        = 
EMCCFLAGS    = --bind $(SRCS) \
               -s WASM=1 \
               -s USE_GLFW=3 \
               -s USE_WEBGL2=1 \
               -s FULL_ES3=1 \
               -s ALLOW_MEMORY_GROWTH=1 \
               --preload-file textures \
               --use-preload-plugins
CFLAGS       = -Werror -Wall -O3 $(DEBUG) $(INCDIR) $(LIBDIR)

LDLIBS       = -lglfw

.PHONY : build clean
build: $(SOURCES)
	$(CC) $(CFLAGS) $(EMCCFLAGS) $(SOURCES) -o $(BINDIR)/$(TARGET).js $(LDLIBS)

clean :
	rm -f $(BINDIR)/$(TARGET).wasm \
          $(BINDIR)/$(TARGET).data \
          $(BINDIR)/$(TARGET).js && \
    rm -rf web/__pycache__
