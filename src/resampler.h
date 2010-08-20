
#ifndef RSD_RESAMPLER
#define RSD_RESAMPLER

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*resampler_cb_t) (void *cb_data, float **data);

typedef struct resampler
{
   float *data;
   float ratio;
   size_t data_ptr;
   size_t data_size;
   void *cb_data;
   int channels;
   resampler_cb_t func;
   uint64_t sum_output_frames;
   uint64_t sum_input_frames;
} resampler_t;


resampler_t* resampler_new(resampler_cb_t func, float ratio, int channels, void* cb_data);
ssize_t resampler_cb_read(resampler_t *state, size_t frames, float *data);
void resampler_free(resampler_t* state);

void resampler_float_to_s16(int16_t * restrict out, const float * restrict in, size_t samples);
void resampler_s16_to_float(float * restrict out, const int16_t * restrict in, size_t samples);

#ifdef __cplusplus
}
#endif

#endif
