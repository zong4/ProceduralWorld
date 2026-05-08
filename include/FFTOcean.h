#pragma once

#include <array>
#include <complex>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

class FFTOcean
{
public:
    struct Settings {
        int resolution = 128;
        float patchLength = 48.0f;
        float windSpeed = 10.0f;
        glm::vec2 windDirection = glm::normalize(glm::vec2(1.0f, 0.35f));
        float spectrumAmplitude = 0.0007f;
        float choppiness = 0.65f;
        float heightScale = 420.0f;
        float heightLimit = 1.0f;
        int cascadeCount = 3;
        std::array<float, 3> cascadePatchLengthMultipliers = { 2.2f, 0.85f, 0.28f };
        std::array<float, 3> cascadeAmplitudeMultipliers = { 0.85f, 0.38f, 0.13f };
        std::array<float, 3> cascadeHeightWeights = { 0.72f, 0.30f, 0.12f };
        std::array<float, 3> cascadeDisplacementWeights = { 0.95f, 0.45f, 0.16f };
        std::array<float, 3> cascadeSpeedMultipliers = { 0.85f, 1.08f, 1.35f };
    };

    FFTOcean() = default;

    void initialize(const Settings& settings = Settings{});
    void update(float timeSeconds);
    void release();

    GLuint heightTexture() const { return heightTexture_; }
    GLuint normalTexture() const { return normalTexture_; }
    GLuint displacementTexture() const { return displacementTexture_; }
    GLuint foldingTexture() const { return foldingTexture_; }
    GLuint detailNormalTextureA() const { return detailNormalTextureA_; }
    GLuint detailNormalTextureB() const { return detailNormalTextureB_; }
    GLuint foamNoiseTexture() const { return foamNoiseTexture_; }
    float texelSize() const { return resolution_ > 0 ? 1.0f / static_cast<float>(resolution_) : 1.0f / 128.0f; }
    const Settings& settings() const { return settings_; }

private:
    using Complex = std::complex<float>;
    static constexpr int kMaxCascadeCount = 3;

    struct CascadeState {
        float patchLength = 48.0f;
        float spectrumAmplitude = 0.0007f;
        float heightWeight = 1.0f;
        float displacementWeight = 1.0f;
        float speedMultiplier = 1.0f;
        std::vector<Complex> initialSpectrum;
        std::vector<Complex> initialSpectrumConjugate;
        std::vector<Complex> frequencySpectrum;
        std::vector<Complex> displacementSpectrumX;
        std::vector<Complex> displacementSpectrumZ;
        std::vector<Complex> spatialHeights;
        std::vector<Complex> spatialDisplacementX;
        std::vector<Complex> spatialDisplacementZ;
    };

    Settings settings_;
    int resolution_ = 0;
    GLuint heightTexture_ = 0;
    GLuint normalTexture_ = 0;
    GLuint displacementTexture_ = 0;
    GLuint foldingTexture_ = 0;
    GLuint detailNormalTextureA_ = 0;
    GLuint detailNormalTextureB_ = 0;
    GLuint foamNoiseTexture_ = 0;
    std::array<CascadeState, kMaxCascadeCount> cascades_;
    std::vector<float> heightPixels_;
    std::vector<glm::vec3> normalPixels_;
    std::vector<glm::vec2> displacementPixels_;
    std::vector<float> foldingPixels_;
    bool initialized_ = false;

    int index(int x, int y) const;
    glm::vec2 waveVector(int x, int y, float patchLength) const;
    float phillipsSpectrum(const glm::vec2& k, float spectrumAmplitude, float windSpeed) const;
    void configureCascades();
    void buildInitialSpectrum();
    void buildInitialSpectrum(CascadeState& cascade, int seedOffset);
    void buildStaticDetailTextures();
    void inverseFft2D(std::vector<Complex>& data) const;
    void fft1D(std::vector<Complex>& data, bool inverse) const;
    void uploadTextures();
};
