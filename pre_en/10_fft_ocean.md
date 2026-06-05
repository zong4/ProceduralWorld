# 10. FFT Ocean Waves

## Matching Images
- ../images_en/08_ocean_fft.svg
- ../images_en/22_flow_10_fft_ocean.svg

## Goal
Frequency-domain wave spectra generate height, normal, displacement and folding textures.

## Key Code Locations
- src/FFTOcean.cpp
- include/FFTOcean.h

## Explanation Flow
1. initialize() sets cascades and resolution
2. buildInitialSpectrum() creates a Phillips spectrum
3. Initialize frequency data from wind, amplitude and gravity
4. update(time) advances spectral phases
5. IFFT converts data to spatial wave fields
6. Compute height, normal, displacement and folding
7. uploadTextures() sends results to GPU
8. Ocean shader samples textures for animated waves

## Speaking Script
> The FFT ocean module makes the water surface move.

> It first builds a wave spectrum in the frequency domain using wind, amplitude and gravity parameters.

> Every frame, the frequency phases advance with time. An inverse FFT converts the result back into spatial wave textures.

> The output includes height, normals, horizontal displacement and folding information.

> The ocean shader samples these textures across multiple cascades to produce continuous waves at different scales.

## Possible Follow-up Answer
You do not need to derive the math; explain that FFT lets complex wave patterns be generated efficiently.