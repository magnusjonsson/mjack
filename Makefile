# Flags

CFLAGS := -Wall -O3 -std=c99 -ffast-math -ftree-vectorize -funroll-loops -Xlinker -no-undefined -std=gnu99

JACK_GTK_FLAGS := ${CFLAGS} -lm $(shell pkg-config --libs --cflags gtk+-2.0 json-c jack)

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
	tanh-distortion-ladspa.so \
	mono-panner-ladspa.so \
	ms-reverb-ladspa.so \
	ms-reverb2-ladspa.so \
	ms-reverb3-ladspa.so \
	ms-gain-ladspa.so

JACK_GTK_TARGETS := \
	haas4-jack-gtk \
	reverb-jack-gtk \
	sawsynth-jack-gtk \
	sawsynth2-jack-gtk \
	polysaw-jack-gtk \
	apchain-jack-gtk \
	synth2-jack-gtk \
	ms-reverb-jack-gtk \
	ms-reverb2-jack-gtk \
	ms-reverb3-jack-gtk

TARGETS := ${LADSPA_TARGETS} ${JACK_GTK_TARGETS} ladder-filter-designer

# Toplevel rules

all : clean ${TARGETS}

clean:
	rm -f ${TARGETS}

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

# Target rules

%-ladspa.so : src/%.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

%-jack-gtk : src/%.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

# Misc

ladder-filter-designer : src/ladder-filter-designer.c
	gcc ${CFLAGS} $< -o $@ -lm

dummylash : src/dummylash.c
	gcc ${LASH_FLAGS} $^ -o $@
