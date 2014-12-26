CFLAGS := -Wall -O3 -std=c99 -ffast-math -ftree-vectorize -funroll-loops -Xlinker -no-undefined -std=gnu99

JACK_GTK_FLAGS := ${CFLAGS} -lm $(shell pkg-config --libs --cflags gtk+-2.0 json-c jack)
LADSPA_FLAGS := ${CFLAGS} -fPIC -shared -lm
LASH_FLAGS := ${CFLAGS} -llash -lm

LADSPA_TARGETS := haas4-ladspa.so reverb-ladspa.so apchain-ladspa.so hpf-ladspa.so lpf-ladspa.so parametric-ladspa.so tanh-distortion-ladspa.so mono-panner-ladspa.so ms-reverb-ladspa.so ms-reverb2-ladspa.so ms-reverb3-ladspa.so ms-gain-ladspa.so

JACK_GTK_TARGETS := haas4-jack-gtk reverb-jack-gtk sawsynth sawsynth2 polysaw apchain-jack-gtk synth2 ms-reverb-jack-gtk ms-reverb2-jack-gtk ms-reverb3-jack-gtk

TARGETS := ${LADSPA_TARGETS} ${JACK_GTK_TARGETS} ladder_filter_designer

all : clean ${TARGETS}

clean:
	rm -f ${TARGETS}

haas4-jack-gtk : src/haas4.c src/jack-gtk-wrapper.c src/wrapper.h
	gcc ${JACK_GTK_FLAGS} src/haas4.c src/jack-gtk-wrapper.c -o $@

haas4-ladspa.so : src/haas4.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} -fPIC -shared src/haas4.c src/ladspa-wrapper.c -o $@

hpf-ladspa.so : src/hpf.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/hpf.c src/ladspa-wrapper.c -o $@

lpf-ladspa.so : src/lpf.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/lpf.c src/ladspa-wrapper.c -o $@

parametric-ladspa.so : src/parametric.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/parametric.c src/ladspa-wrapper.c -o $@

tanh-distortion-ladspa.so : src/tanh_distortion.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/tanh_distortion.c src/ladspa-wrapper.c -o $@

mono-panner-ladspa.so : src/mono_panner.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/mono_panner.c src/ladspa-wrapper.c -o $@

reverb-jack-gtk : src/reverb.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} src/reverb.c src/jack-gtk-wrapper.c -o $@

reverb-ladspa.so : src/reverb.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${LADSPA_FLAGS} src/reverb.c src/ladspa-wrapper.c -o $@

sawsynth : src/sawsynth.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} src/sawsynth.c src/jack-gtk-wrapper.c -o $@

sawsynth2 : src/sawsynth2.c src/jack-gtk-wrapper.c src/wrapper.h
	gcc ${JACK_GTK_FLAGS} src/sawsynth2.c src/jack-gtk-wrapper.c -o $@

polysaw : src/polysaw.c src/jack-gtk-wrapper.c src/wrapper.h src/osc.h src/ap.h src/svf.h src/ladder.h src/sem_svf.h src/ms20_filter.h src/lfo.h src/adsr_env.h src/simple_env.h
	gcc ${JACK_GTK_FLAGS} src/polysaw.c src/jack-gtk-wrapper.c -o $@

synth2 : src/synth2.c src/jack-gtk-wrapper.c src/wrapper.h src/osc.h src/ap.h src/svf.h src/ladder.h src/sem_svf.h src/ms20_filter.h src/lfo.h src/adsr_env.h src/simple_env.h src/meantone.h
	gcc ${JACK_GTK_FLAGS} src/synth2.c src/jack-gtk-wrapper.c -o $@

apchain-jack-gtk : src/apchain.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} src/apchain.c src/jack-gtk-wrapper.c -o $@

apchain-ladspa.so : src/apchain.c  src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/apchain.c src/ladspa-wrapper.c -o $@

dummylash : src/dummylash.c
	gcc ${LASH_FLAGS} $^ -o $@

ms-reverb-jack-gtk : src/ms_reverb.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} src/ms_reverb.c src/jack-gtk-wrapper.c -o $@
ms-reverb2-jack-gtk : src/ms_reverb2.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} src/ms_reverb2.c src/jack-gtk-wrapper.c -o $@

ms-reverb3-jack-gtk : src/ms_reverb3.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${JACK_GTK_FLAGS} src/ms_reverb3.c src/jack-gtk-wrapper.c -o $@

ms-reverb-ladspa.so : src/ms_reverb.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/ms_reverb.c src/ladspa-wrapper.c -o $@

ms-reverb2-ladspa.so : src/ms_reverb2.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${LADSPA_FLAGS} src/ms_reverb2.c src/ladspa-wrapper.c -o $@

ms-reverb3-ladspa.so : src/ms_reverb3.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${LADSPA_FLAGS} src/ms_reverb3.c src/ladspa-wrapper.c -o $@

ms-gain-ladspa.so : src/ms_gain.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${LADSPA_FLAGS} src/ms_gain.c src/ladspa-wrapper.c -o $@

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

ladder_filter_designer : src/ladder_filter_designer.c
	gcc ${CFLAGS} $< -o $@ -lm
