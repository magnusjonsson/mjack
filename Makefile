CFLAGS=-Wall -O3 -std=c99 -ffast-math -ftree-vectorize -funroll-loops -D_GNU_SOURCE
TARGETS= haas4-jack-gtk haas4-ladspa.so reverb-jack-gtk reverb-ladspa.so sawsynth apchain-jack-gtk apchain-ladspa.so

all : ${TARGETS}

clean:
	rm -f ${TARGETS}

haas4-jack-gtk : haas4.c wrapper.h jack-gtk-wrapper.c 
	gcc ${CFLAGS} $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

haas4-ladspa.so : haas4.c wrapper.h ladspa-wrapper.c
	gcc ${CFLAGS} -fPIC -shared $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

reverb-jack-gtk : reverb.c wrapper.h jack-gtk-wrapper.c
	gcc ${CFLAGS} $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

reverb-ladspa.so : reverb.c wrapper.h ladspa-wrapper.c
	gcc ${CFLAGS} -fPIC -shared $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

sawsynth : sawsynth.c wrapper.h jack-gtk-wrapper.c
	gcc ${CFLAGS} $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

apchain-jack-gtk : apchain.c wrapper.h jack-gtk-wrapper.c
	gcc ${CFLAGS} $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

apchain-ladspa.so : apchain.c wrapper.h ladspa-wrapper.c
	gcc ${CFLAGS} -fPIC -shared $^ -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

dummylash : dummylash.c
	gcc ${CFLAGS} $^ -o $@ -llash -lm


install-ladspa : reverb-ladspa.so
	cp $^ /usr/local/lib/ladspa/
