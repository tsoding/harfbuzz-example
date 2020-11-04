PKGS=harfbuzz freetype2
CFLAGS=-Wall `pkg-config --cflags $(PKGS)`
LIBS=`pkg-config --libs $(PKGS)` -lm

hb-probe: main.c
	$(CC) $(CFLAGS) -o hb-probe main.c $(LIBS)
