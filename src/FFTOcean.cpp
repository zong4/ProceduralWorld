#include "FFTOcean.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kGravity = 9.81f;
constexpr int kStaticTextureResolution = 256;

bool isPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

float gaussian(std::mt19937& generator)
{
    static constexpr float invSqrt2 = 0.70710678118f;
    std::normal_distribution<float> distribution(0.0f, 1.0f);
    return distribution(generator) * invSqrt2;
}

float hash2(float x, float y)
{
    return std::fmod(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f, 1.0f);
}

float fade(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

float tileNoise(float x, float y, int period)
{
    const int x0 = static_cast<int>(std::floor(x)) % period;
    const int y0 = static_cast<int>(std::floor(y)) % period;
    const int x1 = (x0 + 1) % period;
    const int y1 = (y0 + 1) % period;
    const float fx = x - std::floor(x);
    const float fy = y - std::floor(y);
    const float u = fade(fx);
    const float v = fade(fy);

    const float a = hash2(static_cast<float>(x0), static_cast<float>(y0));
    const float b = hash2(static_cast<float>(x1), static_cast<float>(y0));
    const float c = hash2(static_cast<float>(x0), static_cast<float>(y1));
    const float d = hash2(static_cast<float>(x1), static_cast<float>(y1));
    return glm::mix(glm::mix(a, b, u), glm::mix(c, d, u), v);
}

float tileFbm(float u, float v)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 4.0f;
    float total = 0.0f;

    for (int octave = 0; octave < 5; ++octave) {
        const int period = static_cast<int>(frequency);
        value += tileNoise(u * frequency, v * frequency, period) * amplitude;
        total += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / glm::max(total, 0.0001f);
}

GLuint createTexture2D(int width,
                       int height,
                       GLenum internalFormat,
                       GLenum sourceFormat,
                       GLenum sourceType,
                       const void* pixels,
                       bool generateMipmaps = false)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, sourceFormat, sourceType, pixels);
    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

GLuint loadTextureFromFile(const std::filesystem::path& path, bool grayscale)
{
    if (!std::filesystem::exists(path)) {
        return 0;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    const int requestedChannels = grayscale ? 1 : 3;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, requestedChannels);
    if (pixels == nullptr) {
        std::cerr << "[FFTOcean] Failed to load texture: " << path.string() << "\n";
        return 0;
    }

    const GLuint texture = createTexture2D(
        width,
        height,
        grayscale ? GL_R8 : GL_RGB8,
        grayscale ? GL_RED : GL_RGB,
        GL_UNSIGNED_BYTE,
        pixels,
        true
    );
    stbi_image_free(pixels);
    std::cout << "[FFTOcean] Loaded texture: " << path.string() << "\n";
    return texture;
}
}

void FFTOcean::initialize(const Settings& settings)
{
    release();

    settings_ = settings;
    if (!isPowerOfTwo(settings_.resolution)) {
        settings_.resolution = 128;
    }
    settings_.windDirection = glm::normalize(settings_.windDirection);
    resolution_ = settings_.resolution;

    const std::size_t pixelCount = static_cast<std::size_t>(resolution_ * resolution_);
    initialSpectrum_.resize(pixelCount);
    initialSpectrumConjugate_.resize(pixelCount);
    frequencySpectrum_.resize(pixelCount);
    displacementSpectrumX_.resize(pixelCount);
    displacementSpectrumZ_.resize(pixelCount);
    spatialHeights_.resize(pixelCount);
    spatialDisplacementX_.resize(pixelCount);
    spatialDisplacementZ_.resize(pixelCount);
    heightPixels_.resize(pixelCount);
    normalPixels_.resize(pixelCount);
    displacementPixels_.resize(pixelCount);
    foldingPixels_.resize(pixelCount);

    buildInitialSpectrum();

    glGenTextures(1, &heightTexture_);
    glBindTexture(GL_TEXTURE_2D, heightTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, resolution_, resolution_, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenTextures(1, &normalTexture_);
    glBindTexture(GL_TEXTURE_2D, normalTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, resolution_, resolution_, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenTextures(1, &displacementTexture_);
    glBindTexture(GL_TEXTURE_2D, displacementTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, resolution_, resolution_, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenTextures(1, &foldingTexture_);
    glBindTexture(GL_TEXTURE_2D, foldingTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, resolution_, resolution_, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    buildStaticDetailTextures();

    initialized_ = true;
    update(0.0f);
}

void FFTOcean::release()
{
    if (foldingTexture_ != 0) {
        glDeleteTextures(1, &foldingTexture_);
        foldingTexture_ = 0;
    }
    if (displacementTexture_ != 0) {
        glDeleteTextures(1, &displacementTexture_);
        displacementTexture_ = 0;
    }
    if (normalTexture_ != 0) {
        glDeleteTextures(1, &normalTexture_);
        normalTexture_ = 0;
    }
    if (heightTexture_ != 0) {
        glDeleteTextures(1, &heightTexture_);
        heightTexture_ = 0;
    }
    if (detailNormalTextureA_ != 0) {
        glDeleteTextures(1, &detailNormalTextureA_);
        detailNormalTextureA_ = 0;
    }
    if (detailNormalTextureB_ != 0) {
        glDeleteTextures(1, &detailNormalTextureB_);
        detailNormalTextureB_ = 0;
    }
    if (foamNoiseTexture_ != 0) {
        glDeleteTextures(1, &foamNoiseTexture_);
        foamNoiseTexture_ = 0;
    }

    initialized_ = false;
}

void FFTOcean::buildStaticDetailTextures()
{
    const std::filesystem::path waterTextureRoot = "assets/textures/water";

    detailNormalTextureA_ = loadTextureFromFile(waterTextureRoot / "water_detail_normal_a.png", false);
    detailNormalTextureB_ = loadTextureFromFile(waterTextureRoot / "water_detail_normal_b.png", false);
    foamNoiseTexture_ = loadTextureFromFile(waterTextureRoot / "foam_noise.png", true);

    std::vector<glm::vec3> detailNormalsA(static_cast<std::size_t>(kStaticTextureResolution * kStaticTextureResolution));
    std::vector<glm::vec3> detailNormalsB(static_cast<std::size_t>(kStaticTextureResolution * kStaticTextureResolution));
    std::vector<float> foamNoise(static_cast<std::size_t>(kStaticTextureResolution * kStaticTextureResolution));

    auto sampleHeight = [](float u, float v, float phase) {
        const float directionalRipples =
            std::sin((u * 18.0f + v * 7.0f + phase) * 2.0f * kPi) * 0.45f
          + std::sin((u * -11.0f + v * 23.0f + phase * 0.37f) * 2.0f * kPi) * 0.28f
          + std::sin((u * 37.0f + v * 19.0f - phase * 0.21f) * 2.0f * kPi) * 0.12f;
        return directionalRipples + (tileFbm(u + phase * 0.13f, v + phase * 0.07f) - 0.5f) * 0.42f;
    };

    auto buildPackedNormal = [&](int x, int y, float phase) {
            const float u = static_cast<float>(x) / static_cast<float>(kStaticTextureResolution);
            const float v = static_cast<float>(y) / static_cast<float>(kStaticTextureResolution);
            const float eps = 1.0f / static_cast<float>(kStaticTextureResolution);

            const float left = sampleHeight(u - eps, v, phase);
            const float right = sampleHeight(u + eps, v, phase);
            const float down = sampleHeight(u, v - eps, phase);
            const float up = sampleHeight(u, v + eps, phase);
            glm::vec3 normal = glm::normalize(glm::vec3(-(right - left) * 0.22f, 1.0f, -(up - down) * 0.22f));
            return normal * 0.5f + glm::vec3(0.5f);
    };

    for (int y = 0; y < kStaticTextureResolution; ++y) {
        for (int x = 0; x < kStaticTextureResolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(kStaticTextureResolution);
            const float v = static_cast<float>(y) / static_cast<float>(kStaticTextureResolution);
            detailNormalsA[static_cast<std::size_t>(y * kStaticTextureResolution + x)] = buildPackedNormal(x, y, 0.0f);
            detailNormalsB[static_cast<std::size_t>(y * kStaticTextureResolution + x)] = buildPackedNormal(x, y, 0.43f);
            const float cellular = glm::abs(tileFbm(u * 1.7f + 0.13f, v * 1.7f + 0.41f) - 0.5f) * 2.0f;
            const float wisps = tileFbm(u * 3.3f + 0.71f, v * 3.3f + 0.09f);
            foamNoise[static_cast<std::size_t>(y * kStaticTextureResolution + x)] = glm::clamp(wisps * 0.72f + cellular * 0.28f, 0.0f, 1.0f);
        }
    }

    if (detailNormalTextureA_ == 0) {
        detailNormalTextureA_ = createTexture2D(kStaticTextureResolution, kStaticTextureResolution, GL_RGB32F, GL_RGB, GL_FLOAT, detailNormalsA.data(), true);
        std::cout << "[FFTOcean] Using generated fallback: water_detail_normal_a\n";
    }
    if (detailNormalTextureB_ == 0) {
        detailNormalTextureB_ = createTexture2D(kStaticTextureResolution, kStaticTextureResolution, GL_RGB32F, GL_RGB, GL_FLOAT, detailNormalsB.data(), true);
        std::cout << "[FFTOcean] Using generated fallback: water_detail_normal_b\n";
    }
    if (foamNoiseTexture_ == 0) {
        foamNoiseTexture_ = createTexture2D(kStaticTextureResolution, kStaticTextureResolution, GL_R32F, GL_RED, GL_FLOAT, foamNoise.data(), true);
        std::cout << "[FFTOcean] Using generated fallback: foam_noise\n";
    }
}

int FFTOcean::index(int x, int y) const
{
    x = (x % resolution_ + resolution_) % resolution_;
    y = (y % resolution_ + resolution_) % resolution_;
    return y * resolution_ + x;
}

glm::vec2 FFTOcean::waveVector(int x, int y) const
{
    const int halfResolution = resolution_ / 2;
    const float kx = 2.0f * kPi * static_cast<float>(x - halfResolution) / settings_.patchLength;
    const float ky = 2.0f * kPi * static_cast<float>(y - halfResolution) / settings_.patchLength;
    return glm::vec2(kx, ky);
}

float FFTOcean::phillipsSpectrum(const glm::vec2& k) const
{
    const float kLength = glm::length(k);
    if (kLength < 0.00001f) {
        return 0.0f;
    }

    const float largestWave = settings_.windSpeed * settings_.windSpeed / kGravity;
    const float dampingLength = largestWave * 0.001f;
    const float kDotWind = glm::dot(glm::normalize(k), settings_.windDirection);
    const float directionalTerm = kDotWind * kDotWind;
    const float k2 = kLength * kLength;
    const float k4 = k2 * k2;
    const float longWaveTerm = std::exp(-1.0f / (k2 * largestWave * largestWave));
    const float shortWaveTerm = std::exp(-k2 * dampingLength * dampingLength);
    const float againstWindDamping = kDotWind < 0.0f ? 0.18f : 1.0f;

    return settings_.spectrumAmplitude
         * longWaveTerm
         * directionalTerm
         * shortWaveTerm
         * againstWindDamping
         / k4;
}

void FFTOcean::buildInitialSpectrum()
{
    std::mt19937 generator(1337);

    for (int y = 0; y < resolution_; ++y) {
        for (int x = 0; x < resolution_; ++x) {
            const glm::vec2 k = waveVector(x, y);
            const float spectrum = std::sqrt(phillipsSpectrum(k));
            const Complex h0(gaussian(generator) * spectrum, gaussian(generator) * spectrum);
            initialSpectrum_[index(x, y)] = h0;
        }
    }

    for (int y = 0; y < resolution_; ++y) {
        for (int x = 0; x < resolution_; ++x) {
            initialSpectrumConjugate_[index(x, y)] = std::conj(initialSpectrum_[index(resolution_ - x, resolution_ - y)]);
        }
    }
}

void FFTOcean::fft1D(std::vector<Complex>& data, bool inverse) const
{
    const int n = static_cast<int>(data.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (int length = 2; length <= n; length <<= 1) {
        const float angle = (inverse ? 2.0f : -2.0f) * kPi / static_cast<float>(length);
        const Complex root(std::cos(angle), std::sin(angle));
        for (int start = 0; start < n; start += length) {
            Complex twiddle(1.0f, 0.0f);
            const int halfLength = length >> 1;
            for (int offset = 0; offset < halfLength; ++offset) {
                const Complex even = data[start + offset];
                const Complex odd = data[start + offset + halfLength] * twiddle;
                data[start + offset] = even + odd;
                data[start + offset + halfLength] = even - odd;
                twiddle *= root;
            }
        }
    }

    if (inverse) {
        const float scale = 1.0f / static_cast<float>(n);
        for (Complex& value : data) {
            value *= scale;
        }
    }
}

void FFTOcean::inverseFft2D(std::vector<Complex>& data) const
{
    std::vector<Complex> line(static_cast<std::size_t>(resolution_));

    for (int y = 0; y < resolution_; ++y) {
        for (int x = 0; x < resolution_; ++x) {
            line[static_cast<std::size_t>(x)] = data[index(x, y)];
        }
        fft1D(line, true);
        for (int x = 0; x < resolution_; ++x) {
            data[index(x, y)] = line[static_cast<std::size_t>(x)];
        }
    }

    for (int x = 0; x < resolution_; ++x) {
        for (int y = 0; y < resolution_; ++y) {
            line[static_cast<std::size_t>(y)] = data[index(x, y)];
        }
        fft1D(line, true);
        for (int y = 0; y < resolution_; ++y) {
            data[index(x, y)] = line[static_cast<std::size_t>(y)];
        }
    }
}

void FFTOcean::update(float timeSeconds)
{
    if (!initialized_) {
        return;
    }

    for (int y = 0; y < resolution_; ++y) {
        for (int x = 0; x < resolution_; ++x) {
            const glm::vec2 k = waveVector(x, y);
            const float omega = std::sqrt(kGravity * glm::length(k));
            const Complex phase(std::cos(omega * timeSeconds), std::sin(omega * timeSeconds));
            const int pixelIndex = index(x, y);
            frequencySpectrum_[pixelIndex] = initialSpectrum_[pixelIndex] * phase
                                           + initialSpectrumConjugate_[pixelIndex] * std::conj(phase);

            const float kLength = glm::length(k);
            if (kLength > 0.00001f) {
                const Complex minusI(0.0f, -1.0f);
                displacementSpectrumX_[pixelIndex] = minusI * (k.x / kLength) * frequencySpectrum_[pixelIndex];
                displacementSpectrumZ_[pixelIndex] = minusI * (k.y / kLength) * frequencySpectrum_[pixelIndex];
            } else {
                displacementSpectrumX_[pixelIndex] = Complex(0.0f, 0.0f);
                displacementSpectrumZ_[pixelIndex] = Complex(0.0f, 0.0f);
            }
        }
    }

    spatialHeights_ = frequencySpectrum_;
    spatialDisplacementX_ = displacementSpectrumX_;
    spatialDisplacementZ_ = displacementSpectrumZ_;
    inverseFft2D(spatialHeights_);
    inverseFft2D(spatialDisplacementX_);
    inverseFft2D(spatialDisplacementZ_);

    for (int y = 0; y < resolution_; ++y) {
        for (int x = 0; x < resolution_; ++x) {
            const int pixelIndex = index(x, y);
            const float checkerboardSign = ((x + y) & 1) != 0 ? -1.0f : 1.0f;
            float height = spatialHeights_[index(x, y)].real();
            height *= checkerboardSign;
            height *= settings_.heightScale;
            heightPixels_[pixelIndex] = glm::clamp(height, -settings_.heightLimit, settings_.heightLimit);

            const float displacementX = spatialDisplacementX_[pixelIndex].real() * checkerboardSign * settings_.heightScale * 0.35f;
            const float displacementZ = spatialDisplacementZ_[pixelIndex].real() * checkerboardSign * settings_.heightScale * 0.35f;
            displacementPixels_[pixelIndex] = glm::vec2(displacementX, displacementZ);
        }
    }

    const float texelWorldSize = settings_.patchLength / static_cast<float>(resolution_);
    const float choppyNormalScale = settings_.choppiness;
    for (int y = 0; y < resolution_; ++y) {
        for (int x = 0; x < resolution_; ++x) {
            const float left = heightPixels_[index(x - 1, y)];
            const float right = heightPixels_[index(x + 1, y)];
            const float down = heightPixels_[index(x, y - 1)];
            const float up = heightPixels_[index(x, y + 1)];
            const glm::vec2 displacementLeft = displacementPixels_[index(x - 1, y)];
            const glm::vec2 displacementRight = displacementPixels_[index(x + 1, y)];
            const glm::vec2 displacementDown = displacementPixels_[index(x, y - 1)];
            const glm::vec2 displacementUp = displacementPixels_[index(x, y + 1)];

            const glm::vec3 tangentDelta(
                2.0f * texelWorldSize + (displacementRight.x - displacementLeft.x) * choppyNormalScale,
                right - left,
                (displacementRight.y - displacementLeft.y) * choppyNormalScale
            );
            const glm::vec3 bitangentDelta(
                (displacementUp.x - displacementDown.x) * choppyNormalScale,
                up - down,
                2.0f * texelWorldSize + (displacementUp.y - displacementDown.y) * choppyNormalScale
            );
            normalPixels_[index(x, y)] = glm::normalize(glm::cross(bitangentDelta, tangentDelta));

            const float dDxDx = (displacementRight.x - displacementLeft.x) / (2.0f * texelWorldSize);
            const float dDxDz = (displacementUp.x - displacementDown.x) / (2.0f * texelWorldSize);
            const float dDzDx = (displacementRight.y - displacementLeft.y) / (2.0f * texelWorldSize);
            const float dDzDz = (displacementUp.y - displacementDown.y) / (2.0f * texelWorldSize);
            const float jacobian = (1.0f + choppyNormalScale * dDxDx) * (1.0f + choppyNormalScale * dDzDz)
                                 - (choppyNormalScale * dDxDz) * (choppyNormalScale * dDzDx);
            foldingPixels_[index(x, y)] = glm::clamp((1.0f - jacobian) * 1.5f, 0.0f, 1.0f);
        }
    }

    uploadTextures();
}

void FFTOcean::uploadTextures()
{
    glBindTexture(GL_TEXTURE_2D, heightTexture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution_, resolution_, GL_RED, GL_FLOAT, heightPixels_.data());

    glBindTexture(GL_TEXTURE_2D, normalTexture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution_, resolution_, GL_RGB, GL_FLOAT, normalPixels_.data());

    glBindTexture(GL_TEXTURE_2D, displacementTexture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution_, resolution_, GL_RG, GL_FLOAT, displacementPixels_.data());

    glBindTexture(GL_TEXTURE_2D, foldingTexture_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, resolution_, resolution_, GL_RED, GL_FLOAT, foldingPixels_.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
