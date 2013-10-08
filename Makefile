all : haas4 reverb sawsynth apchain

clean:
	rm haas4 reverb sawsynth apchain

haas4 : haas4.c wrapper.h
	gcc -Wall -O3 $< -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm -std=gnu99

reverb : reverb.c wrapper.h
	gcc -Wall -O3 $< -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm -std=gnu99

sawsynth : sawsynth.c wrapper.h
	gcc -Wall -O3 $< -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm -std=gnu99

apchain : apchain.c wrapper.h
	gcc -Wall -O3 $< -o $@ `pkg-config --libs --cflags gtk+-2.0` -ljack -ljson-c -lm -std=gnu99

dummylash : dummylash.c
	gcc -Wall -O3 $< -o $@ -llash -lm -std=gnu99
