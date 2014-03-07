CFLAGS=-Wall -O3 -std=c99 -ffast-math -ftree-vectorize -funroll-loops -Xlinker -no-undefined -std=gnu99
TARGETS= haas4-jack-gtk haas4-ladspa.so reverb-jack-gtk reverb-ladspa.so sawsynth sawsynth2 polysaw apchain-jack-gtk apchain-ladspa.so

all : ${TARGETS}

clean:
	rm -f ${TARGETS}

haas4-jack-gtk : haas4.c jack-gtk-wrapper.c wrapper.h
	gcc ${CFLAGS} haas4.c jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

haas4-ladspa.so : haas4.c ladspa-wrapper.c wrapper.h 
	gcc ${CFLAGS} -fPIC -shared haas4.c ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

reverb-jack-gtk : reverb.c jack-gtk-wrapper.c wrapper.h 
	gcc ${CFLAGS} reverb.c jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

reverb-ladspa.so : reverb.c ladspa-wrapper.c wrapper.h 
	gcc ${CFLAGS} -fPIC -shared reverb.c ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

sawsynth : sawsynth.c jack-gtk-wrapper.c wrapper.h 
	gcc ${CFLAGS} sawsynth.c jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

sawsynth2 : sawsynth2.c jack-gtk-wrapper.c wrapper.h 
	gcc ${CFLAGS} sawsynth2.c jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

polysaw : polysaw.c jack-gtk-wrapper.c wrapper.h osc.h ap.h svf.h ladder.h sem_svf.h
	gcc ${CFLAGS} polysaw.c jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

apchain-jack-gtk : apchain.c jack-gtk-wrapper.c wrapper.h 
	gcc ${CFLAGS} apchain.c jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

apchain-ladspa.so : apchain.c  ladspa-wrapper.c wrapper.h
	gcc ${CFLAGS} -fPIC -shared apchain.c ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm

dummylash : dummylash.c
	gcc ${CFLAGS} $^ -o $@ -llash -lm


install-ladspa : reverb-ladspa.so
	cp $^ /usr/local/lib/ladspa/
