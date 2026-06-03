#include "PlanetProceduralData.h"
#include "PlanetTerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <chrono>
#include <map>
#include <vector>

namespace
{
struct FaceBasis {
    glm::vec3 normal;
    glm::vec3 axisU;
    glm::vec3 axisV;
};

constexpr FaceBasis kRawFaces[6] = {
    {{ 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{-1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.0f, -1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.0f,  0.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  0.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}}
};

glm::vec3 rawCubeSphereDirection(const FaceBasis& face, const glm::vec2& uv)
{
    const glm::vec2 faceUv = uv * 2.0f - 1.0f;
    return glm::normalize(face.normal + faceUv.x * face.axisU + faceUv.y * face.axisV);
}

float percentile(std::vector<float> values, float q)
{
    if (values.empty()) {
        return 0.0f;
    }
    std::sort(values.begin(), values.end());
    const std::size_t index = static_cast<std::size_t>(
        std::clamp(q, 0.0f, 1.0f) * static_cast<float>(values.size() - 1));
    return values[index];
}

float mean(const std::vector<float>& values)
{
    if (values.empty()) {
        return 0.0f;
    }
    return std::accumulate(values.begin(), values.end(), 0.0f) / static_cast<float>(values.size());
}

void writeRawHeightAtlas(const std::vector<float>& heights, int resolution, const std::string& path)
{
    const int atlasW = resolution * 3;
    const int atlasH = resolution * 2;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(atlasW * atlasH), 0);
    const auto minmax = std::minmax_element(heights.begin(), heights.end());
    const float minH = minmax.first != heights.end() ? *minmax.first : 0.0f;
    const float maxH = minmax.second != heights.end() ? *minmax.second : 1.0f;
    const float invRange = 1.0f / std::max(maxH - minH, 0.00001f);

    for (int face = 0; face < 6; ++face) {
        const int ox = (face % 3) * resolution;
        const int oy = (face / 3) * resolution;
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                const std::size_t source = static_cast<std::size_t>((face * resolution + y) * resolution + x);
                const float v = std::clamp((heights[source] - minH) * invRange, 0.0f, 1.0f);
                pixels[static_cast<std::size_t>((oy + y) * atlasW + ox + x)] =
                    static_cast<unsigned char>(std::round(v * 255.0f));
            }
        }
    }

    std::ofstream out(path, std::ios::binary);
    out << "P5\n" << atlasW << " " << atlasH << "\n255\n";
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

void analyzeRawTerrain(const PlanetRenderSettings& settings, int resolution)
{
    std::vector<float> heights;
    std::vector<float> landHeights;
    std::vector<float> mountainHeights;
    heights.reserve(static_cast<std::size_t>(resolution * resolution * 6));

    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution)
                );
                const glm::vec3 dir = rawCubeSphereDirection(kRawFaces[face], uv);
                const float h = PlanetTerrainGenerator::terrainHeight(settings, dir);
                heights.push_back(h);
                if (h > settings.seaLevelOffset) {
                    landHeights.push_back(h);
                }
                if (h > settings.seaLevelOffset + 0.18f) {
                    mountainHeights.push_back(h);
                }
            }
        }
    }

    const auto printSeries = [](const char* name, const std::vector<float>& values) {
        std::cout << name
                  << " count " << values.size()
                  << " mean " << mean(values)
                  << " p50 " << percentile(values, 0.50f)
                  << " p90 " << percentile(values, 0.90f)
                  << " p95 " << percentile(values, 0.95f)
                  << " p99 " << percentile(values, 0.99f)
                  << " max " << (values.empty() ? 0.0f : *std::max_element(values.begin(), values.end()))
                  << "\n";
    };

    std::cout << "== raw_terrain_height ==\n";
    std::cout << "resolution " << resolution
              << " total " << heights.size()
              << " landCoverage " << (heights.empty() ? 0.0f : static_cast<float>(landHeights.size()) / static_cast<float>(heights.size()))
              << " mountainCoverage " << (heights.empty() ? 0.0f : static_cast<float>(mountainHeights.size()) / static_cast<float>(heights.size()))
              << "\n";
    printSeries("all", heights);
    printSeries("land", landHeights);
    printSeries("mountain", mountainHeights);
    writeRawHeightAtlas(heights, resolution, "terrain_height_raw.pgm");
}

void writeHeightAtlas(const PlanetProceduralData& planet, const std::string& path)
{
    const auto& faces = planet.faces();
    const int n = planet.resolution();
    const int atlasW = n * 3;
    const int atlasH = n * 2;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(atlasW * atlasH), 0);
    const float minH = planet.minHeight();
    const float maxH = planet.maxHeight();
    const float invRange = 1.0f / std::max(maxH - minH, 0.00001f);

    for (int face = 0; face < 6; ++face) {
        const int ox = (face % 3) * n;
        const int oy = (face / 3) * n;
        const auto& height = faces[static_cast<std::size_t>(face)].height;
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const float h = height[static_cast<std::size_t>(y * n + x)];
                const float v = std::clamp((h - minH) * invRange, 0.0f, 1.0f);
                pixels[static_cast<std::size_t>((oy + y) * atlasW + ox + x)] =
                    static_cast<unsigned char>(std::round(v * 255.0f));
            }
        }
    }

    std::ofstream out(path, std::ios::binary);
    out << "P5\n" << atlasW << " " << atlasH << "\n255\n";
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
}

void analyze(const PlanetProceduralData& planet, const std::string& label)
{
    const auto& faces = planet.faces();
    const int n = planet.resolution();
    std::vector<float> heights;
    std::vector<float> landHeights;
    std::vector<float> neighborSteps;
    std::vector<float> curvature;
    std::vector<float> residual5;
    std::vector<float> residual9;

    heights.reserve(static_cast<std::size_t>(n * n * 6));
    landHeights.reserve(static_cast<std::size_t>(n * n * 6));

    for (int face = 0; face < 6; ++face) {
        const auto& data = faces[static_cast<std::size_t>(face)];
        heights.insert(heights.end(), data.height.begin(), data.height.end());
        for (std::size_t i = 0; i < data.height.size(); ++i) {
            if (data.waterDepth[i] <= 0.00001f) {
                landHeights.push_back(data.height[i]);
            }
        }

        for (int y = 4; y < n - 4; ++y) {
            for (int x = 4; x < n - 4; ++x) {
                const std::size_t i = static_cast<std::size_t>(y * n + x);
                if (data.waterDepth[i] > 0.00001f) {
                    continue;
                }
                const float c = data.height[i];
                float maxStep = 0.0f;
                float sum8 = 0.0f;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const float h = data.height[static_cast<std::size_t>((y + oy) * n + x + ox)];
                        maxStep = std::max(maxStep, std::abs(c - h));
                        sum8 += h;
                    }
                }
                neighborSteps.push_back(maxStep);
                curvature.push_back(std::abs(c - sum8 / 8.0f));

                float sum5 = 0.0f;
                float sum9 = 0.0f;
                int count5 = 0;
                int count9 = 0;
                for (int oy = -4; oy <= 4; ++oy) {
                    for (int ox = -4; ox <= 4; ++ox) {
                        const float h = data.height[static_cast<std::size_t>((y + oy) * n + x + ox)];
                        if (std::abs(ox) <= 2 && std::abs(oy) <= 2) {
                            sum5 += h;
                            ++count5;
                        }
                        sum9 += h;
                        ++count9;
                    }
                }
                residual5.push_back(std::abs(c - sum5 / static_cast<float>(count5)));
                residual9.push_back(std::abs(c - sum9 / static_cast<float>(count9)));
            }
        }
    }

    std::cout << "== " << label << " ==\n";
    std::cout << "resolution " << n
              << " min " << planet.minHeight()
              << " max " << planet.maxHeight()
              << " waterCoverage " << planet.waterCoverage()
              << " shoreCoverage " << planet.shoreCoverage() << "\n";
    std::cout << "height p01/p50/p99 "
              << percentile(heights, 0.01f) << " "
              << percentile(heights, 0.50f) << " "
              << percentile(heights, 0.99f) << "\n";
    std::cout << "land height p01/p50/p99 "
              << percentile(landHeights, 0.01f) << " "
              << percentile(landHeights, 0.50f) << " "
              << percentile(landHeights, 0.99f) << "\n";

    const auto printSeries = [](const char* name, const std::vector<float>& values) {
        std::cout << name
                  << " mean " << mean(values)
                  << " p50 " << percentile(values, 0.50f)
                  << " p90 " << percentile(values, 0.90f)
                  << " p95 " << percentile(values, 0.95f)
                  << " p99 " << percentile(values, 0.99f)
                  << " max " << (values.empty() ? 0.0f : *std::max_element(values.begin(), values.end()))
                  << "\n";
    };
    printSeries("neighbor_abs_step", neighborSteps);
    printSeries("curvature_abs", curvature);
    printSeries("hf_residual_5x5", residual5);
    printSeries("hf_residual_9x9", residual9);

    for (float threshold : {0.002f, 0.005f, 0.010f, 0.020f}) {
        int peaks = 0;
        int samples = 0;
        for (int face = 0; face < 6; ++face) {
            const auto& data = faces[static_cast<std::size_t>(face)];
            for (int y = 1; y < n - 1; ++y) {
                for (int x = 1; x < n - 1; ++x) {
                    const std::size_t i = static_cast<std::size_t>(y * n + x);
                    if (data.waterDepth[i] > 0.00001f) {
                        continue;
                    }
                    ++samples;
                    const float c = data.height[i];
                    float maxNeighbor = -1.0e30f;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            maxNeighbor = std::max(
                                maxNeighbor,
                                data.height[static_cast<std::size_t>((y + oy) * n + x + ox)]);
                        }
                    }
                    if (c - maxNeighbor > threshold) {
                        ++peaks;
                    }
                }
            }
        }
        std::cout << "local_peaks_gt_" << threshold
                  << " " << peaks
                  << " per_10k_land " << (samples > 0 ? static_cast<float>(peaks) * 10000.0f / static_cast<float>(samples) : 0.0f)
                  << "\n";
    }
}

PlanetProceduralData generatePlanet(PlanetRenderSettings settings, int resolution, const std::string& label)
{
    PlanetProceduralData planet;
    int lastPercent = -1;
    using Clock = std::chrono::steady_clock;
    const auto startTime = Clock::now();
    auto lastTime = startTime;
    int lastModule = -1;
    std::string lastStatus;
    std::vector<double> moduleSeconds(static_cast<std::size_t>(PlanetProceduralData::GenerationModule::Count), 0.0);
    std::map<std::string, double> statusSeconds;
    planet.generate(settings, resolution, [&](const PlanetProceduralData::GenerationProgress& progress) {
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - lastTime).count();
        if (lastModule >= 0 && lastModule < static_cast<int>(moduleSeconds.size())) {
            moduleSeconds[static_cast<std::size_t>(lastModule)] += elapsed;
        }
        if (!lastStatus.empty()) {
            statusSeconds[lastStatus] += elapsed;
        }
        lastTime = now;
        lastModule = static_cast<int>(progress.module);
        lastStatus = progress.status != nullptr ? progress.status : "";

        const int total = std::max(progress.totalSteps, 1);
        const int percent = progress.completedSteps * 100 / total;
        if (percent != lastPercent && (percent % 5 == 0 || percent == 100)) {
            lastPercent = percent;
            std::cout << "[" << label << "] " << percent << "% "
                      << progress.status << "\n" << std::flush;
        }
    });
    const auto endTime = Clock::now();
    if (lastModule >= 0 && lastModule < static_cast<int>(moduleSeconds.size())) {
        const double elapsed = std::chrono::duration<double>(endTime - lastTime).count();
        moduleSeconds[static_cast<std::size_t>(lastModule)] += elapsed;
        if (!lastStatus.empty()) {
            statusSeconds[lastStatus] += elapsed;
        }
    }
    static const char* moduleNames[] = {
        "BaseTerrain", "InitialClimate", "InitialBiomes", "BiomeTerrain", "Erosion",
        "FinalClimate", "FinalBiomes", "MeshPlanning", "Finalize"
    };
    std::cout << "[" << label << "] timing total "
              << std::chrono::duration<double>(endTime - startTime).count() << "s\n";
    for (std::size_t i = 0; i < moduleSeconds.size(); ++i) {
        std::cout << "  " << moduleNames[i] << " " << moduleSeconds[i] << "s\n";
    }
    std::cout << "[" << label << "] slow statuses\n";
    for (const auto& entry : statusSeconds) {
        if (entry.second >= 0.05) {
            std::cout << "  " << entry.second << "s " << entry.first << "\n";
        }
    }
    return planet;
}
}

int main(int argc, char** argv)
{
    const int resolution = argc > 1 ? std::clamp(std::atoi(argv[1]), 16, 512) : 96;
    const std::string mode = argc > 2 ? std::string(argv[2]) : "";
    const bool fullMode = mode == "full";
    PlanetRenderSettings settings;
    settings.terrainHeightScale = 34.0f;
    settings.terrainNoiseScale = 0.78f;
    settings.mountainMaskStrength = 1.90f;
    settings.mountainMaskScale = 3.35f;
    settings.mountainRidgeSharpness = 3.80f;
    settings.erosionIterations = 96;
    settings.erosionStrength = 0.055f;
    settings.erosionThermalStrength = 0.014f;

    if (mode == "raw") {
        analyzeRawTerrain(settings, resolution);
        return 0;
    }

    settings.erosionIterations = 0;
    settings.erosionStrength = 0.0f;
    settings.erosionThermalStrength = 0.0f;
    PlanetProceduralData noErosion = generatePlanet(settings, resolution, "no_erosion");
    analyze(noErosion, "no_erosion");
    writeHeightAtlas(noErosion, "terrain_height_no_erosion.pgm");

    if (fullMode) {
        settings.erosionIterations = 96;
        settings.erosionStrength = 0.055f;
        settings.erosionThermalStrength = 0.014f;
        PlanetProceduralData full = generatePlanet(settings, resolution, "full");
        analyze(full, "full");
        writeHeightAtlas(full, "terrain_height_full.pgm");
    }
    return 0;
}
