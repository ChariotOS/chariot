/*
 * fft.h
 *
 * Copyright (c) 2018 Disi A
 * 
 * Author: Disi A
 * Email: adis@alum.mit.edu
 * https://www.mathworks.com/matlabcentral/profile/authors/3734620-disi-a
 * 
 * http://www.netlib.org/fftpack/doc
 * 
 * PARAMETERS:
 * * int n;          The length of the sequence to be transformed.
 * * float *x;       A real array of length n which contains the sequenceto be transformed
 * * float *wsave;   A work array which must be dimensioned at least 2*n+15.
 * *                 the same work array can be used for both rfftf and rfftb
 * *                 as long as n remains unchanged. different wsave arrays
 * *                 are required for different values of n. the contents of
 * *                 wsave must not be changed between calls of rfftf or rfftb.
 * * int *ifac;      Array containing factors of the problem dimensions.
 * *                 (initialization requirements are probably similar to wsave)
 * *                 Some docs say it needs to be 15 elements, others say n.
 * *                 Maybe n + 15 is safest.
 * 
 * More documentation from original FFTPACK
 * http://www.netlib.org/fftpack/doc
 * 
 * Some other usage reference: https://www.experts-exchange.com/questions/21416267/how-to-use-C-version-of-fftpack-a-public-domain-DFT-library.html
 */

#ifndef _fft_h
#define _fft_h

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USE_DOUBLE_PRECISION 1
#define FFT_SCALED_OUTPUT 1
#define FFT_UNSCALED_OUTPUT 0
#define FFT_IFAC 15

#if USE_DOUBLE_PRECISION
#define FFT_PRECISION double
#else
#define FFT_PRECISION float
#endif

typedef struct {

    int n;
    FFT_PRECISION * wsave;
    int * ifac;
    int scale_output; // 1 for scale and 0 for not scale

} FFTTransformer;

typedef struct {

    int n;
    FFT_PRECISION * wsave;
    int * ifac;
    int scale_output; // 1 for scale and 0 for not scale

} FFTCosqTransformer;

// Initialization real fft transform (__ogg_fdrffti)
void __fft_real_init(int n, FFT_PRECISION *wsave, int *ifac);
// Forward transform of a real periodic sequence (__ogg_fdrfftf)
void __fft_real_forward(int n,FFT_PRECISION *r,FFT_PRECISION *wsave,int *ifac);
// Real FFT backward (__ogg_fdrfftb)
void __fft_real_backward(int n, FFT_PRECISION *r, FFT_PRECISION *wsave, int *ifac); 

// Initialize cosine quarter-wave transform (__ogg_fdcosqi)
void __fft_cosq_init(int n, FFT_PRECISION *wsave, int *ifac);
// Real cosine quarter-wave forward transform (__ogg_fdcosqf)
void __fft_cosq_forward(int n,FFT_PRECISION *x,FFT_PRECISION *wsave,int *ifac);
// Real cosine quarter-wave backward transform (__ogg_fdcosqb)
void __fft_cosq_backward(int n,FFT_PRECISION *x,FFT_PRECISION *wsave,int *ifac);

// Wrapper method with all fftpack parameters properly initilized
FFTTransformer * create_fft_transformer(int signal_length, int scale_output);

void free_fft_transformer(FFTTransformer * transformer);

void fft_forward(FFTTransformer * transformer, FFT_PRECISION* input);

void fft_backward(FFTTransformer * transformer, FFT_PRECISION* input);

FFTCosqTransformer * create_fft_cosq_transformer(int signal_length, int scale_output);

void free_cosq_fft_transformer(FFTCosqTransformer * transformer);

void fft_cosq_forward(FFTCosqTransformer * transformer, FFT_PRECISION* input);

void fft_cosq_backward(FFTCosqTransformer * transformer, FFT_PRECISION* input);

#ifdef __cplusplus
}
#endif

#endif /* _fft_h */
