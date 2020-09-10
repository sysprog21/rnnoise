CFLAGS += -Iinclude -g -Wall --warn-vla
CFLAGS += -O3
CFLAGS += -march=native -funroll-loops
CFLAGS_sndfile = $(shell pkg-config --cflags sndfile soxr)
LDFLAGS_sndfile = $(shell pkg-config --libs sndfile soxr)
LDFLAGS += -lm

OBJS = \
	src/denoise.o \
	src/rnn.o \
	src/rnn_data.o \
	src/rnn_reader.o \
	src/pitch.o \
	src/kiss_fft.o \
	src/celt_lpc.o

all: examples/rnnoise_demo examples/bertool

librnnoise.a: $(OBJS)
	$(AR) rcs $@ $^

src/denoise_training: CPPFLAGS+=-DTRAINING=1
src/denoise_training: clean $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

examples/rnnoise_demo.o: CFLAGS+=$(CFLAGS_sndfile)
examples/rnnoise_demo: LDFLAGS+=$(LDFLAGS_sndfile)
examples/rnnoise_demo: examples/rnnoise_demo.o librnnoise.a
	$(CC) -o $@ $^ $(LDFLAGS)

examples/bertool: examples/bertool.o
	$(CC) -o $@ $^ $(LDFLAGS)

scan:
	scan-build $(MAKE) clean all

gperf: LDFLAGS += -lprofiler -ltcmalloc
gperf: clean examples/rnnoise_demo examples/bertool
	CPUPROFILE_FREQUENCY=100000 CPUPROFILE=gperf.prof examples/rnnoise_demo sample.wav clean.wav
	pprof ./examples/rnnoise_demo gperf.prof --callgrind > callgrind.gperf
	gprof2dot --format=callgrind callgrind.gperf -z main | dot -T svg > gperf.svg
	ffmpeg -i clean.wav -ac 1 -ar 48000 -f s16le -acodec pcm_s16le clean.raw
	examples/bertool clean.raw target.raw

valgrind:
	valgrind examples/rnnoise_demo sample_short.wav /dev/null

check: sample.wav examples/rnnoise_demo
	examples/rnnoise_demo $< clean.wav

.PHONY: clean

clean:
	$(RM) $(OBJS) examples/*.o *.a src/denoise_training examples/rnnoise_demo
	$(RM) clean.raw clean.wav
	$(RM) gperf.svg
