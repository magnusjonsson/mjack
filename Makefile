# Flags

CFLAGS := -Wall -O3 -std=c99 -ffast-math -ftree-vectorize -funroll-loops -Xlinker -no-undefined -std=gnu99

JACK_GTK_FLAGS := ${CFLAGS} -lm $(shell pkg-config --libs --cflags gtk+-2.0 json-c jack)

LADSPA_FLAGS := ${CFLAGS} -fPIC -shared -lm

LASH_FLAGS := ${CFLAGS} -llash -lm

# Targets

LADSPA_TARGETS := haas4-ladspa.so reverb-ladspa.so apchain-ladspa.so hpf-ladspa.so lpf-ladspa.so parametric-ladspa.so tanh-distortion-ladspa.so mono-panner-ladspa.so ms-reverb-ladspa.so ms-reverb2-ladspa.so ms-reverb3-ladspa.so ms-gain-ladspa.so

JACK_GTK_TARGETS := haas4-jack-gtk reverb-jack-gtk sawsynth sawsynth2 polysaw apchain-jack-gtk synth2 ms-reverb-jack-gtk ms-reverb2-jack-gtk ms-reverb3-jack-gtk

TARGETS := ${LADSPA_TARGETS} ${JACK_GTK_TARGETS} ladder_filter_designer

# Toplevel rules

all : clean ${TARGETS}

clean:
	rm -f ${TARGETS}

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

# Target rules

haas4-jack-gtk : src/haas4.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

haas4-ladspa.so : src/haas4.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

hpf-ladspa.so : src/hpf.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

lpf-ladspa.so : src/lpf.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} $^ -o $@

parametric-ladspa.so : src/parametric.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

tanh-distortion-ladspa.so : src/tanh_distortion.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

mono-panner-ladspa.so : src/mono_panner.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

reverb-jack-gtk : src/reverb.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

reverb-ladspa.so : src/reverb.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

sawsynth : src/sawsynth.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

sawsynth2 : src/sawsynth2.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

polysaw : src/polysaw.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

synth2 : src/synth2.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

apchain-jack-gtk : src/apchain.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

apchain-ladspa.so : src/apchain.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

ms-reverb-jack-gtk : src/ms_reverb.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

ms-reverb2-jack-gtk : src/ms_reverb2.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

ms-reverb3-jack-gtk : src/ms_reverb3.c src/jack-gtk-wrapper.c
	gcc ${JACK_GTK_FLAGS} $^ -o $@

ms-reverb-ladspa.so : src/ms_reverb.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

ms-reverb2-ladspa.so : src/ms_reverb2.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

ms-reverb3-ladspa.so : src/ms_reverb3.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

ms-gain-ladspa.so : src/ms_gain.c src/ladspa-wrapper.c
	gcc ${LADSPA_FLAGS} $^ -o $@

# Misc

ladder_filter_designer : src/ladder_filter_designer.c
	gcc ${CFLAGS} $< -o $@ -lm

dummylash : src/dummylash.c
	gcc ${LASH_FLAGS} $^ -o $@
