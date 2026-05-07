#pragma once

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

    Settings settings_;
    int resolution_ = 0;
    GLuint heightTexture_ = 0;
    GLuint normalTexture_ = 0;
    GLuint displacementTexture_ = 0;
    GLuint foldingTexture_ = 0;
    GLuint detailNormalTextureA_ = 0;
    GLuint detailNormalTextureB_ = 0;
    GLuint foamNoiseTexture_ = 0;
    std::vector<Complex> initialSpectrum_;
    std::vector<Complex> initialSpectrumConjugate_;
    std::vector<Complex> frequencySpectrum_;
    std::vector<Complex> displacementSpectrumX_;
    std::vector<Complex> displacementSpectrumZ_;
    std::vector<Complex> spatialHeights_;
    std::vector<Complex> spatialDisplacementX_;
    std::vector<Complex> spatialDisplacementZ_;
    std::vector<float> heightPixels_;
    std::vector<glm::vec3> normalPixels_;
    std::vector<glm::vec2> displacementPixels_;
    std::vector<float> foldingPixels_;
    bool initialized_ = false;

    int index(int x, int y) const;
    glm::vec2 waveVector(int x, int y) const;
    float phillipsSpectrum(const glm::vec2& k) const;
    void buildInitialSpectrum();
    void buildStaticDetailTextures();
    void inverseFft2D(std::vector<Complex>& data) const;
    void fft1D(std::vector<Complex>& data, bool inverse) const;
    void uploadTextures();
};
