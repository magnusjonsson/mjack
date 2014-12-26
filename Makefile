CFLAGS := -Wall -O3 -std=c99 -ffast-math -ftree-vectorize -funroll-loops -Xlinker -no-undefined -std=gnu99

LADSPA_TARGETS := haas4-ladspa.so reverb-ladspa.so apchain-ladspa.so hpf-ladspa.so lpf-ladspa.so parametric-ladspa.so tanh-distortion-ladspa.so mono-panner-ladspa.so ms-reverb-ladspa.so ms-reverb2-ladspa.so ms-reverb3-ladspa.so ms-gain-ladspa.so

JACK_GTK_TARGETS := haas4-jack-gtk reverb-jack-gtk sawsynth sawsynth2 polysaw apchain-jack-gtk synth2 ms-reverb-jack-gtk ms-reverb2-jack-gtk ms-reverb3-jack-gtk

TARGETS := ${LADSPA_TARGETS} ${JACK_GTK_TARGETS} ladder_filter_designer

all : ${TARGETS}

clean:
	rm -f ${TARGETS}

haas4-jack-gtk : src/haas4.c src/jack-gtk-wrapper.c src/wrapper.h
	gcc ${CFLAGS} src/haas4.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

haas4-ladspa.so : src/haas4.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} -fPIC -shared src/haas4.c src/ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

hpf-ladspa.so : src/hpf.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/hpf.c src/ladspa-wrapper.c -o $@ -lm

lpf-ladspa.so : src/lpf.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/lpf.c src/ladspa-wrapper.c -o $@ -lm

parametric-ladspa.so : src/parametric.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/parametric.c src/ladspa-wrapper.c -o $@ -lm

tanh-distortion-ladspa.so : src/tanh_distortion.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/tanh_distortion.c src/ladspa-wrapper.c -o $@ -lm

mono-panner-ladspa.so : src/mono_panner.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/mono_panner.c src/ladspa-wrapper.c -o $@ -lm

reverb-jack-gtk : src/reverb.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} src/reverb.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

reverb-ladspa.so : src/reverb.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} -fPIC -shared src/reverb.c src/ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

sawsynth : src/sawsynth.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} src/sawsynth.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

sawsynth2 : src/sawsynth2.c src/jack-gtk-wrapper.c src/wrapper.h
	gcc ${CFLAGS} src/sawsynth2.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

polysaw : src/polysaw.c src/jack-gtk-wrapper.c src/wrapper.h src/osc.h src/ap.h src/svf.h src/ladder.h src/sem_svf.h src/ms20_filter.h src/lfo.h src/adsr_env.h src/simple_env.h
	gcc ${CFLAGS} src/polysaw.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

synth2 : src/synth2.c src/jack-gtk-wrapper.c src/wrapper.h src/osc.h src/ap.h src/svf.h src/ladder.h src/sem_svf.h src/ms20_filter.h src/lfo.h src/adsr_env.h src/simple_env.h src/meantone.h
	gcc ${CFLAGS} src/synth2.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

apchain-jack-gtk : src/apchain.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} src/apchain.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

apchain-ladspa.so : src/apchain.c  src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/apchain.c src/ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

dummylash : src/dummylash.c
	gcc ${CFLAGS} $^ -o $@ -llash -lm

ms-reverb-jack-gtk : src/ms_reverb.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} src/ms_reverb.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm
ms-reverb2-jack-gtk : src/ms_reverb2.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} src/ms_reverb2.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

ms-reverb3-jack-gtk : src/ms_reverb3.c src/jack-gtk-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} src/ms_reverb3.c src/jack-gtk-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

ms-reverb-ladspa.so : src/ms_reverb.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} -fPIC -shared src/ms_reverb.c src/ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

ms-reverb2-ladspa.so : src/ms_reverb2.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} -fPIC -shared src/ms_reverb2.c src/ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

ms-reverb3-ladspa.so : src/ms_reverb3.c src/ladspa-wrapper.c src/wrapper.h 
	gcc ${CFLAGS} -fPIC -shared src/ms_reverb3.c src/ladspa-wrapper.c -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack `pkg-config --libs --cflags json-c` -lm

ms-gain-ladspa.so : src/ms_gain.c src/ladspa-wrapper.c src/wrapper.h
	gcc ${CFLAGS} -fPIC -shared src/ms_gain.c src/ladspa-wrapper.c -o $@ -lm

install-ladspa : ${LADSPA_TARGETS}
	cp $^ /usr/local/lib/ladspa/

ladder_filter_designer : src/ladder_filter_designer.c
	gcc ${CFLAGS} $< -o $@ -lm
