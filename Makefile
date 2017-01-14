# Flags

CFLAGS := -Wall -O3 -std=c99 -Xlinker -no-undefined -std=gnu99 -fvisibility=hidden -g

JACK_GTK_FLAGS := ${CFLAGS} $(shell pkg-config --libs --cflags gtk+-2.0 json-c jack) -lm

LADSPA_FLAGS := ${CFLAGS} -fPIC -shared -lm

LASH_FLAGS := ${CFLAGS} -llash -lm

# Targets

LADSPA_TARGETS := \
	haas4-ladspa.so \
	reverb-ladspa.so \
	apchain-ladspa.so \
	hpf-ladspa.so \
	lpf-ladspa.so \
	parametric-ladspa.so \
	parametric2-ladspa.so \
	tanh-distortion-ladspa.so \
	exp-distortion-ladspa.so \
	mono-panner-ladspa.so \
	ms-reverb-ladspa.so \
	ms-reverb2-ladspa.so \
	ms-reverb3-ladspa.so \
	ms-gain-ladspa.so \
	compressor-ladspa.so \
	distbox-ladspa.so \
	knee-ladspa.so \
	monoroom-ladspa.so \
	x2-distortion-ladspa.so \

JACK_GTK_TARGETS := \
	haas4-jack-gtk \
	reverb-jack-gtk \
	sawsynth-jack-gtk \
	sawsynth2-jack-gtk \
	sawsynth3-jack-gtk \
	sawsynth4-jack-gtk \
	polysaw-jack-gtk \
	apchain-jack-gtk \
	synth2-jack-gtk \
	monosynth-jack-gtk \
	ms-reverb-jack-gtk \
	ms-reverb2-jack-gtk \
	ms-reverb3-jack-gtk \
	compressor-jack-gtk \
	kick-jack-gtk \
	kick2-jack-gtk \
	kick3-jack-gtk \
	monoroom-jack-gtk \
	fm-jack-gtk \
	dc-click-jack-gtk \


TARGETS := ${LADSPA_TARGETS} ${JACK_GTK_TARGETS} ladder-filter-designer

# Toplevel rules

all : clean ${TARGETS}

clean:
	rm -f ${TARGETS} *.o

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

# Target rules

%-ladspa.so : src/plugins/%.c ladspa-wrapper.o
	gcc ${LADSPA_FLAGS} $^ -o $@

%-jack-gtk : src/plugins/%.c jack-gtk-wrapper.o
	gcc ${JACK_GTK_FLAGS} $^ -o $@

ladspa-wrapper.o : src/wrappers/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} -c $^ -o $@

jack-gtk-wrapper.o : src/wrappers/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} -c $^ -o $@

# Misc

ladder-filter-designer : src/ladder-filter-designer.c
	gcc ${CFLAGS} $< -o $@ -lm

dummylash : src/dummylash.c
	gcc ${LASH_FLAGS} $^ -o $@
