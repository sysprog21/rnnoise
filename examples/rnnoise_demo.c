/* Copyright (c) 2020 National Cheng Kung University, Taiwan.
 * Copyright (c) 2018 Gregor Richards.
 * Copyright (c) 2017 Mozilla
 */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sndfile.h>
#include <sox.h>
#include <soxr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rnnoise.h"

#define DENOISE_SAMPLE_RATE 48000
#define DENOISE_FRAMES (DENOISE_SAMPLE_RATE / 1000 * 10)

size_t resample(soxr_t soxr, const int16_t *in_buf, const int in_frames,
                int16_t *out_buf, const int out_frames) {
  size_t odone;
  const soxr_error_t error =
      soxr_process(soxr, in_buf, in_frames, NULL, out_buf, out_frames, &odone);
  return error ? -1 : odone;
}

void denoise(soxr_t soxr_in, soxr_t soxr_out, int16_t *snd_buf,
             const int in_frames, int16_t *resample_buf, float *denoise_buf,
             DenoiseState *st) {
  resample(soxr_in, snd_buf, in_frames, resample_buf, DENOISE_FRAMES);

  for (int i = 0; i < DENOISE_FRAMES; i++)
    denoise_buf[i] = (float)resample_buf[i];

  rnnoise_process_frame(st, denoise_buf, denoise_buf);

  for (int i = 0; i < DENOISE_FRAMES; i++)
    resample_buf[i] = denoise_buf[i];

  resample(soxr_out, resample_buf, DENOISE_FRAMES, snd_buf, in_frames);
}

uint64_t usecs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * (uint64_t)1000000 + tv.tv_usec);
}

int main(int argc, char **argv) {
  DenoiseState *st;
  SNDFILE *in_sf, *out_sf;
  SF_INFO in_info = {0}, out_info = {0};
  soxr_error_t error;

  if (argc != 3) {
    fprintf(stderr, "usage: %s <noisy speech> <output denoised>\n", argv[0]);
    return 1;
  }

  if (access(argv[1], F_OK) == -1) {
    fprintf(stderr, "FATAL: can not access file %s.\n", argv[0]);
    return -1;
  }

  soxr_quality_spec_t const q_spec = soxr_quality_spec(SOXR_HQ, 0);
  soxr_io_spec_t const io_spec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
  soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(sox_false);

  st = rnnoise_create(NULL);
  in_sf = sf_open(argv[1], SFM_READ, &in_info);
  out_info = (SF_INFO){.samplerate = in_info.samplerate,
                       .channels = 1,
                       .format = in_info.format};
  out_sf = sf_open(argv[2], SFM_WRITE, &out_info);

  sf_command(in_sf, SFC_SET_NORM_FLOAT, NULL, SF_FALSE);
  sf_command(out_sf, SFC_SET_NORM_FLOAT, NULL, SF_FALSE);

  const int in_frames = in_info.samplerate / 1000 * 10;
  int16_t *snd_buf = malloc(in_frames * 2);
  int16_t resample_buf[DENOISE_FRAMES];
  float denoise_buf[DENOISE_FRAMES];

  soxr_t soxr_in = soxr_create(in_info.samplerate, DENOISE_SAMPLE_RATE, 1,
                               &error, &io_spec, &q_spec, &runtime_spec);
  soxr_t soxr_out = soxr_create(DENOISE_SAMPLE_RATE, out_info.samplerate, 1,
                                &error, &io_spec, &q_spec, &runtime_spec);

  const sf_count_t remain_frames = in_info.frames % in_frames;
  uint64_t t0 = usecs();
  double runtime = 0.0;
  while (1) {
    const sf_count_t read_samples = sf_readf_short(in_sf, snd_buf, in_frames);
    if (read_samples < in_frames)
      break;

    denoise(soxr_in, soxr_out, snd_buf, in_frames, resample_buf, denoise_buf,
            st);
    sf_writef_short(out_sf, snd_buf, in_frames);
    runtime += 0.01; // 480 samples at 48Khz mono -> 5ms
  }
  if (remain_frames > 0) {
    memset(snd_buf + (in_frames - remain_frames), 0, remain_frames);
    denoise(soxr_in, soxr_out, snd_buf, in_frames, resample_buf, denoise_buf,
            st);
    sf_writef_short(out_sf, snd_buf, remain_frames);
    runtime += 0.01; // 480 samples at 48Khz mono -> 5ms
  }
  double elapsed = (usecs() - t0) / 1000000.0;
  fprintf(stdout,
          "processed %3.3f seconds in %3.3f seconds (%3.2fx realtime) \n",
          runtime, elapsed, runtime / elapsed);

  free(snd_buf);
  soxr_delete(soxr_in);
  soxr_delete(soxr_out);
  rnnoise_destroy(st);

  sf_close(in_sf);
  sf_close(out_sf);
  return 0;
}
