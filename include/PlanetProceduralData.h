#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "PlanetHeightField.h"
#include "PlanetRenderer.h"

// CPU 缁旑垳鈻兼惔蹇撳閺勭喓鎮嗛弫鐗堝祦閻㈢喐鍨氶崳銊ｂ偓?
// 鐎瑰啯濡搁弫鎾暭閺勭喓鎮嗛悜妯煎壔閹?6 瀵?cube-face 妤傛ê瀹?濮樺瓨鏋?濮樻柨鈧?biome 閺佺増宓侀崶鎾呯礉
// renderer 閸愬秵濡告潻娆庣昂閺佺増宓佹稉濠佺炊娑?sampler2DArray 娓?shader 闁插洦鐗遍妴?
class PlanetProceduralData
{
public:
    // 閻㈢喐鍨氬ù浣衡柤閹峰棙鍨氭径姘嚋濡€虫健閿涘I 閻劌鐣犻弰鍓с仛鏉╂稑瀹抽弶鈥虫嫲瑜版挸澧犻梼鑸殿唽鐠囧瓨妲戦妴?
    enum class GenerationModule : int {
        BaseTerrain = 0,
        InitialClimate = 1,
        InitialBiomes = 2,
        BiomeTerrain = 3,
        Erosion = 4,
        FinalClimate = 5,
        FinalBiomes = 6,
        MeshPlanning = 7,
        Finalize = 8,
        Count = 9
    };

    // 閸氬骸褰撮悽鐔稿灇缁捐法鈻奸崥鎴滃瘜缁捐法鈻奸幎銉ユ啞閻ㄥ嫯绻樻惔锕€鎻╅悡褋鈧?
    struct GenerationProgress {
        int completedSteps = 0;
        int totalSteps = 1;
        GenerationModule module = GenerationModule::BaseTerrain;
        int moduleCompletedSteps = 0;
        int moduleTotalSteps = 1;
        const char* status = "";
    };

    using ProgressCallback = std::function<void(const GenerationProgress& progress)>;

    // 閸楁洑閲?cube face 閻ㄥ嫭澧嶉張澶屽劋閻掓瑥鐪伴妴?
    // height 閺勵垰缍婃稉鈧崠鏍彯鎼达讣绱眞aterDepth 瀹歌弓绠绘潻?terrainHeightScale閿涘苯宕熸担宥嗗复鏉╂垳绗橀悾宀€鈹栭梻杈剧幢
    // biomeWeightA/B 閻劋琚辨稉?vec4 閹垫挸瀵?8 缁粯娼楃拹銊︽綀闁插秲鈧?
    struct FaceData {
        int resolution = 0;
        std::vector<float> height;
        std::vector<float> waterDepth;
        std::vector<float> shoreMask;
        std::vector<float> erosionMask;
        std::vector<float> channelMask;
        std::vector<float> flowMask;
        std::vector<float> wearMask;
        std::vector<float> depositionMask;
        std::vector<float> temperature;
        std::vector<float> moisture;
        std::vector<float> regionId;
        std::vector<float> featureMask;
        std::vector<float> meshDensity;
        std::vector<float> geometricError;
        std::vector<glm::vec4> biomeWeightA;
        std::vector<glm::vec4> biomeWeightB;
        std::vector<glm::vec4> domainWeight;
    };

    struct TerrainChunkVertex {
        glm::vec3 sphereDir{0.0f, 1.0f, 0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec2 uv{0.0f, 0.0f};
        float height = 0.0f;
        float featureWeight = 0.0f;
    };

    struct TerrainChunk {
        int faceIndex = 0;
        int depth = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        float geometricError = 0.0f;
        float meshDensity = 0.0f;
        float featureMask = 0.0f;
        bool hasWater = false;
        bool hasShore = false;
        int flippedDiagonalCount = 0;
        int constrainedEdgeCount = 0;
        int recoveredConstraintEdgeCount = 0;
        std::vector<TerrainChunkVertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    enum class TerrainFeatureType : std::uint8_t {
        River = 0,
        Coast = 1,
        Ridge = 2,
        ErosionEdge = 3,
        Count = 4
    };

    struct TerrainFeatureSegment {
        TerrainFeatureType type = TerrainFeatureType::River;
        int faceIndex = 0;
        glm::vec2 uvA{0.0f, 0.0f};
        glm::vec2 uvB{0.0f, 0.0f};
        glm::vec3 sphereDirA{0.0f, 1.0f, 0.0f};
        glm::vec3 sphereDirB{0.0f, 1.0f, 0.0f};
        float strength = 0.0f;
    };

    void generate(const PlanetRenderSettings& settings, int faceResolution);
    void generate(const PlanetRenderSettings& settings, int faceResolution, const ProgressCallback& progressCallback);
    bool saveCache(const char* path) const;
    bool loadCache(const char* path, const PlanetRenderSettings& settings);
    void clear();

    // 閸欘亣顕扮拋鍧楁６閸ｎ煉绱濆〒鍙夌厠閸ｃ劌鎷?UI 闁俺绻冩潻娆庣昂閸戣姤鏆熼崣鏍х繁閻㈢喐鍨氱紒鎾寸亯閸滃瞼绮虹拋鈥蹭繆閹垬鈧?
    bool isGenerated() const { return generated_; }
    int resolution() const { return resolution_; }
    const PlanetRenderSettings& settings() const { return settings_; }
    const std::array<FaceData, 6>& faces() const { return faces_; }
    const std::vector<TerrainChunk>& terrainChunks() const { return terrainChunks_; }
    const std::vector<TerrainFeatureSegment>& terrainFeatureSegments() const { return terrainFeatureSegments_; }
    std::size_t terrainFeatureSegmentCount(TerrainFeatureType type) const;
    PlanetGlobalHeightField globalHeightField() const;

    float minHeight() const { return minHeight_; }
    float maxHeight() const { return maxHeight_; }
    float maxWaterDepth() const { return maxWaterDepth_; }
    float waterCoverage() const { return waterCoverage_; }
    float shoreCoverage() const { return shoreCoverage_; }

private:
    // cube face 鐏炩偓闁劌娼楅弽鍥╅兇閿涙ormal 娑撴椽娼伴張婵嗘倻閿涘畮xisU/axisV 娑撻缚顕氶棃?UV 娑撱倓閲滄潪娣偓?
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    // 鐠?cube face 閺屻儴顕楅柇璇茬湷閺冩湹濞囬悽銊ф畱閳ユ粓娼?+ 缁便垹绱╅垾婵嗙穿閻劊鈧?
    struct CellRef {
        int face = 0;
        std::size_t index = 0;
    };

    // 閸楁洜鍋ｇ粙瀣碍閸栨牠鍣伴弽椋庢畱娑擃參妫跨紒鎾寸亯閵?
    struct PlanetSample {
        float height = 0.0f;
        float waterDepth = 0.0f;
        float shoreMask = 0.0f;
        float temperature = 0.0f;
        float moisture = 0.0f;
        float slope = 0.0f;
        float channel = 0.0f;
        float flow = 0.0f;
        float wear = 0.0f;
        float deposition = 0.0f;
        glm::vec4 biomeA = glm::vec4(0.0f);
        glm::vec4 biomeB = glm::vec4(0.0f);
    };

    static const std::array<FaceBasis, 6> kFaces;

    bool generated_ = false;
    int resolution_ = 0;
    PlanetRenderSettings settings_{};
    std::array<FaceData, 6> faces_{};
    std::vector<TerrainChunk> terrainChunks_;
    std::vector<TerrainFeatureSegment> terrainFeatureSegments_;
    float minHeight_ = 0.0f;
    float maxHeight_ = 0.0f;
    float maxWaterDepth_ = 0.0f;
    float waterCoverage_ = 0.0f;
    float shoreCoverage_ = 0.0f;

    static glm::vec3 cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv);
    static int faceIndexFromDirection(const glm::vec3& dir);
    // 閹跺﹦鎮嗛棃銏℃煙閸氭垶妲х亸鍕礀閺屾劒閲?cube face 閻ㄥ嫮顬囬弫锝嗙壐鐎涙劑鈧?
    static CellRef cellFromDirection(const glm::vec3& dir, int resolution);
    // 閺€顖涘瘮鐡掑﹨绻?face 鏉堝湱鏅惃鍕仸鐏炲懏鐓＄拠顫礉閺勵垱甯寸紓婵呮叏婢跺秴鎷版笟浣冩畬閻ㄥ嫬鐔€绾偓閵?
    static CellRef neighborCell(int faceIndex, int x, int y, int resolution);
    static glm::vec3 hash3(const glm::vec3& p);
    static float gradientNoise(const glm::vec3& p);
    static float perlinNoise(const glm::vec3& p);
    static float perlinFbm(const glm::vec3& p, int octaves, float lacunarity, float gain);
    // fractal Brownian motion閿涙艾顦跨仦?gradient noise 閸欑姴濮為妴?
    static float fbm(const glm::vec3& p, int octaves, float lacunarity, float gain);
    static float ridgedFbm(const glm::vec3& p, int octaves, float lacunarity, float gain, float ridgeSharpness);

    static float altitudeBandWeight(float startAltitude, float endAltitude);
    // 閸╄櫣顢呴崷鏉胯埌妤傛ê瀹抽崙鑺ユ殶閿涙艾銇囬梽鍡愨偓浣规崳閻╁棎鈧線鐝崷鑸偓浣稿寳閼村鈧礁鍢叉い璺烘嫲濞撮攱鐭￠柈钘夋躬鏉╂瑩鍣烽崥鍫熷灇閵?
    static float terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
    static PlanetSample samplePlanetBase(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
    static PlanetSample samplePlanetBase(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height);
    void generateDemPrototype(const PlanetRenderSettings& settings, const ProgressCallback& progressCallback);
    void computeWaterClimateFields(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void removeSmallTerrainSpikes(const PlanetRenderSettings& settings, int iterations, float threshold, float blend);
    void smoothSmallTerrainBumps(const PlanetRenderSettings& settings, int iterations, float blend);
    void relaxExtremeTerrainSlopes(const PlanetRenderSettings& settings, int iterations, float maxStep, float blend);
    void refineTerrainFromBiomeWeights(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void applyErosion(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void extractPrimaryRiver(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void updateHydrologyMoisture(const PlanetRenderSettings& settings);
    void computeBiomeWeights(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void computeMeshPlanningFields(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void buildTerrainFeatureSegments(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress = {});
    void buildTerrainChunks(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress = {});
    void smoothBiomeWeights(int radius, float blend);
    void fixCubeFaceSeams();
    static float temperature(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height);
    static float moisture(const glm::vec3& sphereDir, float shoreMask);
};
