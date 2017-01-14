# Flags

CFLAGS := -Wall -O2 -ftree-vectorize -ffast-math -std=c99 -Xlinker -no-undefined -std=gnu99 -fvisibility=hidden
LDFLAGS := -lm

JACK_GTK_CFLAGS := ${CFLAGS} $(shell pkg-config --cflags gtk+-2.0 json-c jack)
JACK_GTK_LDFLAGS := ${LDFLAGS} $(shell pkg-config --libs  gtk+-2.0 json-c jack)

LADSPA_CFLAGS := ${CFLAGS} -fPIC -shared
LADSPA_LDFLAGS := ${LDFLAGS}

LASH_CFLAGS := ${CFLAGS}
LASH_LDFLAGS := ${LDFLAGS} -llash

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


TARGETS := ${JACK_GTK_TARGETS} ${LADSPA_TARGETS} ladder-filter-designer

# Toplevel rules

all : clean ${TARGETS}

clean:
	rm -f ${TARGETS} *.o

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

# Target rules

%-ladspa.so : src/plugins/%.c ladspa-wrapper.o
	gcc ${LADSPA_CFLAGS} $^ ${LADSPA_LDFLAGS} -o $@

%-jack-gtk : src/plugins/%.c jack-gtk-wrapper.o scala.o
	gcc ${JACK_GTK_CFLAGS} $^ ${JACK_GTK_LDFLAGS}  -o $@

ladspa-wrapper.o : src/wrappers/ladspa-wrapper.c
	gcc ${LADSPA_CFLAGS} -c $^ -o $@

jack-gtk-wrapper.o : src/wrappers/jack-gtk-wrapper.c
	gcc ${JACK_GTK_CFLAGS} -c $^ -o $@

scala.o : src/tuning/scala.c
	gcc ${JACK_GTK_CFLAGS} -c $^ -o $@

# Misc

ladder-filter-designer : src/ladder-filter-designer.c
	gcc ${CFLAGS} $< -o $@ -lm

dummylash : src/dummylash.c
	gcc ${LASH_FLAGS} $^ -o $@
