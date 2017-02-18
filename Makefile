# Flags

CFLAGS := -Wall -O2 -ftree-vectorize -ffast-math -Xlinker -no-undefined -std=gnu99 -fvisibility=hidden
LDFLAGS := -lm

JACK_GTK_CFLAGS := ${CFLAGS} $(shell pkg-config --cflags gtk+-2.0 json-c jack)
JACK_GTK_LDFLAGS := ${LDFLAGS} $(shell pkg-config --libs  gtk+-2.0 json-c jack)

LADSPA_CFLAGS := ${CFLAGS} -fPIC -shared
LADSPA_LDFLAGS := ${LDFLAGS}

LASH_CFLAGS := ${CFLAGS}
LASH_LDFLAGS := ${LDFLAGS} -llash

LV2_CFLAGS := ${CFLAGS}
LV2_LDFLAGS := ${LDFLAGS} -fPIC -shared

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

LV2_TARGETS := lv2/synth.so validate_lv2

LV2_INSTALL_DIR := /usr/lib/lv2/mjack.lv2

TARGETS := ${LV2_TARGETS} ${JACK_GTK_TARGETS} ${LADSPA_TARGETS} ladder-filter-designer

# Toplevel rules

all : clean ${TARGETS}

clean:
	rm -f ${TARGETS} *.o

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

install-lv2 : ${LV2_TARGETS}
	mkdir -p ${LV2_INSTALL_DIR}
	install lv2/*.ttl lv2/*.so ${LV2_INSTALL_DIR}/

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

lv2/synth.so : lv2/synth.c
	gcc ${LV2_CFLAGS} $^ ${LV2_LDFLAGS} -o $@

# Misc

ladder-filter-designer : src/ladder-filter-designer.c
	gcc ${CFLAGS} $< -o $@ -lm

dummylash : src/dummylash.c
	gcc ${LASH_FLAGS} $^ -o $@

validate_lv2:
	sord_validate -l \
		/usr/lib/lv2/schemas.lv2/*.ttl \
		/usr/lib/lv2/lv2core.lv2/lv2core.ttl \
		/usr/lib/lv2/urid.lv2/urid.ttl \
		/usr/lib/lv2/atom.lv2/atom.ttl \
		/usr/lib/lv2/ui.lv2/ui.ttl \
		/usr/lib/lv2/midi.lv2/midi.ttl \
		/usr/lib/lv2/event.lv2/event.ttl \
		lv2/*.ttl
