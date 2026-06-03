uniform float seaLevelOffset;
uniform float planetRadius;
uniform vec3 terrainLowlandColor;
uniform vec3 terrainForestColor;
uniform vec3 terrainDesertColor;
uniform vec3 terrainRockColor;
uniform vec3 terrainBeachColor;
uniform vec3 terrainSnowColor;
uniform float terrainBeachWidth;
uniform float terrainRockSlopeStart;
uniform float terrainRockSlopeEnd;
uniform float terrainSnowStart;
uniform float terrainSnowEnd;
uniform float terrainMaterialNoiseScale;
uniform float terrainMaterialNoiseStrength;
uniform float timeSeconds;
uniform int renderRivers;
uniform float riverVisibility;
uniform float riverWidth;
uniform float riverShine;
uniform float riverRefractionStrength;
uniform vec3 riverColor;
uniform float oceanShoreBlendWidth;
uniform float heightScale;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralTemperatureTexture;
uniform sampler2DArray proceduralMoistureTexture;

// 地形材质合成。
// 输入为 CPU 烘焙的高度/水深/温湿度/侵蚀/biome 权重和运行时法线，
// 输出一个 SurfaceData.baseColor，后续由 terrain_lighting.glsl 做光照。

#include "planet_sampling.glsl"

float hash31(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

float valueNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);
    return mix(nxy0, nxy1, u.z);
}

float fbm3(vec3 p)
{
    // 材质层使用 fBM 打散颜色和 biome 边界，避免大块纯色。
    float value = 0.0;
    float amplitude = 0.5;
    float total = 0.0;

    for (int i = 0; i < 4; ++i) {
        value += valueNoise(p) * amplitude;
        total += amplitude;
        p *= 2.03;
        amplitude *= 0.5;
    }

    return value / max(total, 0.0001);
}

float sharpenBiomeWeight(float weight, float exponent)
{
    return pow(clamp(weight, 0.0, 1.0), exponent);
}

float coastalShelter(vec3 sphereDir)
{
    vec3 p = sphereDir * 3.7;
    float broad = fbm3(p + vec3(12.3, 4.7, 8.1));
    float pocket = fbm3(p * 2.35 + vec3(5.7, 17.9, 2.8));
    float notch = 1.0 - abs(fbm3(p * 5.2 + vec3(31.4, 7.6, 19.3)) * 2.0 - 1.0);
    float sheltered = broad * 0.50 + pocket * 0.30 + notch * 0.20;
    return smoothstep(0.42, 0.78, sheltered);
}

float runtimeShoreMask(float h)
{
    float signedWaterDepth = (seaLevelOffset - h) * heightScale;
    return 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth, 0.001), abs(signedWaterDepth));
}

struct PlanetSample
{
    float height;
    float bakedHeight;
    float waterDepth;
    float bakedWaterDepth;
    float runtimeWaterDepth;
    float shoreMask;
    float signedHeightFromSea;
    float temperature;
    float moisture;
    vec4 erosionData;
    vec4 biomeA;
    vec4 biomeB;
    vec4 domainWeight;
};

PlanetSample samplePlanet(vec3 sphereDir, float finalHeight)
{
    // 汇总当前片元需要的所有程序化数据层。
    // runtimeWaterDepth 来自最终高度，bakedWaterDepth 来自 CPU 烘焙，两者组合更稳定。
    PlanetSample planetSample;
    planetSample.height = finalHeight;
    planetSample.bakedHeight = sampleFloatArraySeamlessNarrow(proceduralHeightTexture, sphereDir);
    planetSample.bakedWaterDepth = max(sampleFloatArraySeamless(proceduralWaterDepthTexture, sphereDir), 0.0);
    planetSample.runtimeWaterDepth = max((seaLevelOffset - finalHeight) * heightScale, 0.0);
    planetSample.waterDepth = max(planetSample.runtimeWaterDepth, planetSample.bakedWaterDepth * 0.15);
    planetSample.shoreMask = runtimeShoreMask(finalHeight);
    planetSample.signedHeightFromSea = (finalHeight - seaLevelOffset) * heightScale;
    planetSample.temperature = clamp(sampleFloatArraySeamless(proceduralTemperatureTexture, sphereDir), 0.0, 1.0);
    planetSample.moisture = clamp(sampleFloatArraySeamless(proceduralMoistureTexture, sphereDir), 0.0, 1.0);
    planetSample.erosionData = sampleVec4ArraySeamless(proceduralErosionMaskTexture, sphereDir);
    planetSample.biomeA = sampleVec4ArraySeamless(proceduralBiomeWeightATexture, sphereDir);
    planetSample.biomeB = sampleVec4ArraySeamless(proceduralBiomeWeightBTexture, sphereDir);
    planetSample.domainWeight = sampleVec4ArraySeamless(proceduralDomainWeightTexture, sphereDir);
    return planetSample;
}

SurfaceData sampleSurfaceData(float height, vec3 worldPos, vec3 shadingNormal, vec3 sphereDir)
{
    SurfaceData surface;
    surface.riverMask = 0.0;
    surface.riverSpecular = 0.0;
    surface.specularStrength = 0.010;
    PlanetSample planet = samplePlanet(sphereDir, height);

    // slope 由真实法线和径向方向计算，后面用于裸岩/雪线/植被剔除。
    vec3 radialUp = normalize(worldPos);
    surface.radialAlignment = clamp(dot(normalize(shadingNormal), radialUp), 0.0, 1.0);
    surface.slope = 1.0 - surface.radialAlignment;
    surface.height01 = height * 0.5 + 0.5;
    float temperature = planet.temperature;
    float moisture = planet.moisture;
    vec4 erosionData = planet.erosionData;
    float channelMask = clamp(erosionData.r, 0.0, 1.0);
    float flowMask = clamp(erosionData.g, 0.0, 1.0);
    float wearMask = clamp(erosionData.b, 0.0, 1.0);
    float depositionMask = clamp(erosionData.a, 0.0, 1.0);
    float upliftMask = clamp(planet.domainWeight.x, 0.0, 1.0);
    float geomChannel = clamp(max(planet.domainWeight.y, channelMask), 0.0, 1.0);
    float geomSlope = clamp(max(planet.domainWeight.z, surface.slope), 0.0, 1.0);
    float geomLand = clamp(planet.domainWeight.w, 0.0, 1.0);
    float waterDepth = planet.waterDepth;
    float signedHeightFromSea = planet.signedHeightFromSea;
    float relativeHeight = height - seaLevelOffset;
    float runtimeShore = planet.shoreMask;
    float runtimeLand = smoothstep(0.0, max(oceanShoreBlendWidth * 0.40, 0.001), signedHeightFromSea);
    float runtimeWater = smoothstep(0.0, max(oceanShoreBlendWidth * 0.45, 0.001), -signedHeightFromSea);
    float coastShelter = coastalShelter(sphereDir);
    float coastExposure = 1.0 - coastShelter;

    float mHeight = clamp(relativeHeight / 0.42, 0.0, 1.0);
    float mHigh = smoothstep(0.34, 0.82, mHeight);
    float mAlpine = smoothstep(0.58, 0.96, mHeight);
    float mPeak = smoothstep(0.80, 1.0, mHeight);
    float mCliff = smoothstep(0.24, 0.70, geomSlope + wearMask * 0.20 + upliftMask * 0.08);
    float mValley = smoothstep(0.10, 0.76, geomChannel + flowMask * 0.24);
    float mShore = runtimeShore * runtimeLand;

    float mMacro = fbm3(radialUp * 3.6 + vec3(19.0, 4.0, 11.0));
    float mPatch = fbm3(radialUp * 9.5 + vec3(2.7, 41.0, 6.0));
    float mVegNoise = fbm3(radialUp * 30.0 + vec3(7.0, 13.0, 29.0));
    float mMineral = fbm3(radialUp * 68.0 + vec3(31.0, 4.0, 18.0));
    float mGrainA = fbm3(radialUp * 220.0 + vec3(5.0, 17.0, 23.0));
    float mGrainB = fbm3(radialUp * 680.0 + vec3(43.0, 3.0, 9.0));
    float mCrack = 1.0 - abs(fbm3(radialUp * 150.0 + vec3(12.0, 8.0, 41.0)) * 2.0 - 1.0);
    float mMaterialNoise = fbm3(radialUp * (terrainMaterialNoiseScale * 200.0));
    float mColorVariation = mix(1.0 - terrainMaterialNoiseStrength, 1.0 + terrainMaterialNoiseStrength, mMaterialNoise);

    float mDry = clamp((1.0 - moisture) * 0.74 + temperature * 0.34 + mMacro * 0.34, 0.0, 1.0);
    float mLush = clamp(moisture * 0.78 + (1.0 - temperature) * 0.10 + (1.0 - mMacro) * 0.24 + mPatch * 0.18, 0.0, 1.0);
    float mCold = clamp((1.0 - temperature) * 0.90 + mHigh * 0.22, 0.0, 1.0);
    float mPlain = runtimeLand * (1.0 - mCliff) * (1.0 - mAlpine * 0.65);
    float mForest = smoothstep(0.34, 0.62, mLush) * smoothstep(0.12, 0.52, mVegNoise) * mPlain;
    float mSavanna = smoothstep(0.34, 0.62, mDry) * smoothstep(0.18, 0.58, moisture + mPatch * 0.24) * mPlain;
    float mArid = smoothstep(0.46, 0.72, mDry) * (1.0 - smoothstep(0.30, 0.62, moisture + mVegNoise * 0.16)) * mPlain;
    float mTundra = smoothstep(0.48, 0.78, mCold) * (1.0 - mPeak) * mPlain;
    float mWet = mValley * runtimeLand * smoothstep(0.34, 0.82, moisture);
    float mRiverBed = clamp(max(channelMask, flowMask * 0.74) * runtimeLand, 0.0, 1.0);

    vec3 mLowGrass = vec3(0.20, 0.67, 0.16);
    vec3 mMeadow = vec3(0.64, 0.78, 0.16);
    vec3 mForestDark = vec3(0.030, 0.23, 0.070);
    vec3 mForestWarm = vec3(0.11, 0.43, 0.09);
    vec3 mSavannaColor = vec3(0.76, 0.66, 0.20);
    vec3 mDrySoil = vec3(0.68, 0.36, 0.15);
    vec3 mOchre = vec3(0.92, 0.60, 0.22);
    vec3 mWetGreen = vec3(0.08, 0.35, 0.25);
    vec3 mTundraColor = vec3(0.55, 0.63, 0.42);
    vec3 mBrownSlope = vec3(0.50, 0.30, 0.16);
    vec3 mRedSoil = vec3(0.62, 0.22, 0.10);
    vec3 mRockWarm = vec3(0.55, 0.43, 0.32);
    vec3 mRockCool = vec3(0.52, 0.56, 0.50);
    vec3 mRockDark = vec3(0.18, 0.17, 0.15);
    vec3 mPaleStone = vec3(0.78, 0.76, 0.62);
    vec3 mSnow = vec3(0.93, 0.96, 0.94);
    vec3 mBeach = vec3(0.78, 0.66, 0.38);
    vec3 mRiverBedColor = vec3(0.19, 0.16, 0.11);
    vec3 mShallowSeabed = vec3(0.39, 0.49, 0.42);
    vec3 mDeepSeabed = vec3(0.12, 0.15, 0.17);

    vec3 mEco = mix(mLowGrass, mMeadow, smoothstep(0.08, 0.36, mHeight));
    mEco = mix(mEco, mForestDark, mForest * 0.94);
    mEco = mix(mEco, mForestWarm, mForest * (1.0 - mCold) * 0.34);
    mEco = mix(mEco, mSavannaColor, mSavanna * 0.86);
    mEco = mix(mEco, mix(mDrySoil, mOchre, mMacro), mArid * 0.96);
    mEco = mix(mEco, mTundraColor, mTundra * 0.82);
    mEco = mix(mEco, mWetGreen, mWet * 0.78);

    float mSoilStripe = smoothstep(0.35, 0.78, mCrack + wearMask * 0.30 + channelMask * 0.18);
    float mMineralStripe = smoothstep(0.40, 0.82, mMineral + mCliff * 0.22 + upliftMask * 0.10);
    float mScree = clamp(mCliff * 0.62 + mAlpine * 0.28 + wearMask * 0.38, 0.0, 1.0) * runtimeLand;
    float mCapRock = smoothstep(0.42, 0.88, mHeight + geomSlope * 0.32 + upliftMask * 0.12) * runtimeLand;
    vec3 mSoil = mix(mBrownSlope, mRedSoil, mDry * 0.62 + mMacro * 0.22);
    vec3 mRock = mix(mRockWarm, mRockCool, mCold * 0.45 + mMineral * 0.35);
    mRock = mix(mRock, mRockDark, mSoilStripe * mCliff * 0.35);
    mRock = mix(mRock, mPaleStone, mCapRock * mMineralStripe * 0.36);

    vec3 mLandColor = mEco;
    mLandColor = mix(mLandColor, mSoil, clamp(mScree * 0.34 + mSoilStripe * mHigh * 0.22, 0.0, 0.62));
    mLandColor = mix(mLandColor, mRock, clamp(mScree * 0.60 + mCapRock * 0.34, 0.0, 0.86));
    mLandColor = mix(mLandColor, mRiverBedColor, mRiverBed * 0.62);
    mLandColor = mix(mLandColor, mSnow, mPeak * smoothstep(0.52, 0.86, mCold) * 0.56);
    mLandColor = mix(mLandColor, mBeach, mShore * (1.0 - mCliff) * 0.82);

    vec3 mDetail = vec3(1.0);
    mDetail *= mix(vec3(0.86), vec3(1.22), mGrainA);
    mDetail *= mix(vec3(1.0), vec3(0.62, 0.84, 0.52), mForest * smoothstep(0.50, 0.88, mGrainB) * 0.34);
    mDetail *= mix(vec3(1.0), vec3(1.30, 1.08, 0.66), mArid * smoothstep(0.42, 0.84, mGrainB) * 0.34);
    mDetail *= mix(vec3(1.0), vec3(0.54, 0.52, 0.48), mScree * smoothstep(0.38, 0.84, mCrack) * 0.38);
    mDetail *= mix(vec3(1.0), vec3(0.82, 0.94, 0.76), mWet * 0.18);

    vec3 mVivid = mLandColor * mDetail;
    float mLum = dot(mVivid, vec3(0.299, 0.587, 0.114));
    mVivid = mix(vec3(mLum), mVivid, 1.34);
    vec3 mSeabed = mix(mShallowSeabed, mDeepSeabed, smoothstep(0.2, 5.0, waterDepth));
    mSeabed = mix(mSeabed, mRiverBedColor, depositionMask * 0.34 + wearMask * 0.18);
    float mLand = clamp(runtimeLand * max(geomLand, 0.85), 0.0, 1.0);
    vec3 mColor = mix(mSeabed, mVivid, mLand);
    mColor = mix(mColor, mWetGreen * mix(0.80, 1.14, mGrainA), mRiverBed * 0.28);
    mColor = mix(mColor, mLandColor * vec3(0.82, 0.88, 0.96), mPeak * 0.08);
    mColor = clamp(mColor * mix(0.98, 1.16, mVegNoise), vec3(0.0), vec3(2.2));

    surface.riverMask = 0.0;
    surface.riverSpecular = 0.0;
    surface.specularStrength = mix(0.006, 0.016, mCapRock * 0.45 + mWet * 0.20);
    surface.specularStrength = mix(surface.specularStrength, 0.014, mRiverBed * 0.30);
    surface.baseColor = clamp(mColor * mColorVariation, vec3(0.0), vec3(2.0));
    return surface;

#if 0
    vec4 biomeA = planet.biomeA;
    vec4 biomeB = planet.biomeB;
    float beachWeight = clamp(biomeA.r, 0.0, 1.0);
    float grassWeight = clamp(biomeA.g, 0.0, 1.0);
    float forestWeight = clamp(biomeA.b, 0.0, 1.0);
    float desertWeight = clamp(biomeA.a, 0.0, 1.0);
    float rockWeight = clamp(biomeB.r, 0.0, 1.0);
    float snowWeight = clamp(biomeB.g, 0.0, 1.0);
    float wetlandWeight = clamp(biomeB.b, 0.0, 1.0);
    float shallowWaterWeight = clamp(biomeB.a, 0.0, 1.0);
    // CPU 生成的 beach/rock 权重之外，再根据运行时海岸和坡度动态补强。
    float runtimeBeach = runtimeShore
                       * runtimeLand
                       * coastShelter
                       * (1.0 - smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope));
    float beachPocket = smoothstep(
        0.58,
        0.78,
        fbm3(radialUp * 46.0 + vec3(17.2, 5.8, 39.4)) * 0.64 + coastShelter * 0.22 + beachWeight * 0.22
    );
    float beachShelf = (1.0 - smoothstep(terrainBeachWidth * 0.65, terrainBeachWidth * 2.35, relativeHeight))
                     * smoothstep(0.0, max(terrainBeachWidth * 0.16, 0.0001), relativeHeight)
                     * coastShelter
                     * beachPocket
                     * (1.0 - smoothstep(terrainRockSlopeStart * 0.75, terrainRockSlopeEnd, surface.slope))
                     * (1.0 - clamp(wearMask * 0.55, 0.0, 0.65));
    float authoredBeach = beachWeight;
    runtimeBeach = max(runtimeBeach * beachPocket * 0.58, beachShelf) * smoothstep(0.025, 0.18, authoredBeach);
    float runtimeRockCoast = runtimeShore
                           * runtimeLand
                           * coastExposure
                           * smoothstep(terrainRockSlopeStart * 0.42, terrainRockSlopeEnd, surface.slope + wearMask * 0.24);
    float runtimeShallowWater = runtimeWater
                              * (1.0 - smoothstep(max(oceanShoreBlendWidth, 0.001), max(oceanShoreBlendWidth * 4.0, 0.004), waterDepth));
    beachWeight = max(beachWeight, runtimeBeach * 0.42);
    rockWeight = max(rockWeight, runtimeRockCoast * 0.78);
    shallowWaterWeight = max(shallowWaterWeight, runtimeShallowWater * 0.75);

    float beachDominance = clamp(beachWeight * 0.72, 0.0, 0.85);
    grassWeight *= 1.0 - beachDominance;
    forestWeight *= 1.0 - beachDominance;
    desertWeight *= 1.0 - beachDominance * 0.85;
    wetlandWeight *= 1.0 - beachDominance * 0.60;
    rockWeight *= 1.0 - beachDominance * 0.35;

    float mountainDesertCull = smoothstep(0.115, 0.245, relativeHeight + surface.slope * 0.46 + wearMask * 0.20);
    // 沙漠只保留在低坡低海拔平原；被挤出的权重转给草地或岩石。
    float desertPlain = (1.0 - smoothstep(0.12, 0.34, surface.slope))
                      * (1.0 - smoothstep(0.14, 0.32, relativeHeight))
                      * (1.0 - mountainDesertCull)
                      * (1.0 - clamp(channelMask * 0.70 + flowMask * 0.55 + wearMask * 0.35, 0.0, 0.85));
    float displacedDesert = desertWeight * (1.0 - desertPlain);
    desertWeight *= mix(0.20, 1.0, desertPlain);
    rockWeight += displacedDesert * smoothstep(0.18, 0.46, surface.slope + relativeHeight * 0.20) * 0.72;
    grassWeight += displacedDesert * (1.0 - smoothstep(0.18, 0.46, surface.slope + relativeHeight * 0.20)) * 0.34;

    float alpineVegetationCull = smoothstep(0.105, 0.235, relativeHeight + surface.slope * 0.32);
    // 森林/草地随海拔、坡度、侵蚀剔除，剔除部分转为岩石或草地过渡。
    float forestSlopeViability = 1.0 - smoothstep(0.14, 0.32, surface.slope);
    forestSlopeViability *= 1.0 - smoothstep(0.095, 0.205, relativeHeight);
    forestSlopeViability *= 1.0 - smoothstep(0.115, 0.255, relativeHeight + surface.slope * 0.70);
    forestSlopeViability *= 1.0 - alpineVegetationCull * 0.96;
    forestSlopeViability *= 1.0 - clamp(wearMask * 0.28 + channelMask * 0.16, 0.0, 0.52);
    float displacedForest = forestWeight * (1.0 - forestSlopeViability);
    forestWeight *= mix(0.05, 1.0, forestSlopeViability);
    rockWeight += displacedForest * smoothstep(0.10, 0.32, surface.slope + wearMask * 0.18) * 0.72;
    grassWeight += displacedForest * (1.0 - smoothstep(0.10, 0.32, surface.slope + wearMask * 0.18)) * 0.30;

    float highlandForestCull = smoothstep(0.115, 0.255, relativeHeight + surface.slope * 0.55);
    float culledForest = forestWeight * highlandForestCull;
    forestWeight -= culledForest * 0.85;
    rockWeight += culledForest * 0.65;
    grassWeight += culledForest * (1.0 - smoothstep(0.12, 0.30, surface.slope)) * (1.0 - alpineVegetationCull) * 0.10;

    float grassPlainViability = (1.0 - smoothstep(0.10, 0.28, surface.slope))
                              * (1.0 - smoothstep(0.105, 0.235, relativeHeight + surface.slope * 0.24));
    float displacedGrass = grassWeight * (1.0 - grassPlainViability);
    grassWeight *= mix(0.18, 1.0, grassPlainViability);
    rockWeight += displacedGrass * smoothstep(0.18, 0.42, surface.slope + relativeHeight * 0.15) * 0.52;

    float alpineMaterialMask = smoothstep(0.14, 0.38, relativeHeight + surface.slope * 0.42);
    // 陡坡、高地、冲刷区和干燥裸地都会提高岩石权重。
    float steepRockMask = smoothstep(0.20, 0.50, surface.slope);
    float dryRockPlain = smoothstep(0.04, 0.20, relativeHeight)
                       * (1.0 - smoothstep(0.38, 0.76, relativeHeight))
                       * (1.0 - smoothstep(0.22, 0.48, surface.slope))
                       * (1.0 - smoothstep(0.34, 0.62, moisture))
                       * smoothstep(0.52, 0.74, fbm3(radialUp * 9.4 + vec3(21.3, 6.7, 42.8)));
    float ridgeRockMask = smoothstep(0.08, 0.30, relativeHeight)
                        * smoothstep(0.16, 0.38, surface.slope);
    float erodedOutcropMask = wearMask
                            * smoothstep(0.12, 0.34, surface.slope + relativeHeight * 0.22);
    float exposedRockMask = clamp(
        max(steepRockMask * (0.42 + alpineMaterialMask * 0.58), ridgeRockMask)
      + erodedOutcropMask * 0.58,
        0.0,
        1.0
    );
    exposedRockMask *= runtimeLand * (1.0 - beachWeight * 0.58) * (1.0 - wetlandWeight * 0.45);
    rockWeight *= mix(0.76, 1.0, clamp(alpineMaterialMask + steepRockMask * 0.55, 0.0, 1.0));
    rockWeight = max(rockWeight, exposedRockMask * 0.82);
    rockWeight = max(rockWeight, dryRockPlain * 0.68);
    rockWeight += wearMask * steepRockMask * max(alpineMaterialMask, ridgeRockMask) * 0.48;
    grassWeight *= 1.0 - exposedRockMask * 0.42;
    forestWeight *= 1.0 - exposedRockMask * 0.56;
    desertWeight *= 1.0 - exposedRockMask * 0.30;
    grassWeight *= 1.0 - dryRockPlain * 0.48;
    forestWeight *= 1.0 - dryRockPlain * 0.62;
    desertWeight *= 1.0 - dryRockPlain * 0.28;
    grassWeight *= 1.0 - alpineVegetationCull * 0.94;
    forestWeight *= 1.0 - alpineVegetationCull * 0.98;
    snowWeight *= smoothstep(0.28, 0.56, relativeHeight)
                * smoothstep(0.42, 0.72, 1.0 - temperature);
    snowWeight += alpineMaterialMask
                * smoothstep(0.36, 0.66, 1.0 - temperature)
                * smoothstep(0.28, 0.58, relativeHeight)
                * 0.20;

    float biomeBreakup = fbm3(radialUp * 32.0 + vec3(11.7, 4.3, 19.2));
    grassWeight *= mix(0.88, 1.10, fbm3(radialUp * 24.0 + vec3(2.1, 8.4, 5.7)));
    forestWeight *= mix(0.82, 1.16, biomeBreakup);
    desertWeight *= mix(0.84, 1.18, fbm3(radialUp * 18.0 + vec3(31.2, 6.6, 14.8)));
    wetlandWeight *= mix(0.76, 1.20, fbm3(radialUp * 38.0 + vec3(5.6, 22.4, 7.1)));

    float dominantLandWeight = max(max(max(beachWeight, grassWeight), max(forestWeight, desertWeight)), max(max(rockWeight, snowWeight), wetlandWeight));
    // 主导 biome 越明确，权重曲线越锐利；边界处保持柔和混合。
    float biomeContrast = mix(1.32, 2.32, smoothstep(0.16, 0.52, dominantLandWeight));
    beachWeight = sharpenBiomeWeight(beachWeight, biomeContrast * 0.88);
    grassWeight = sharpenBiomeWeight(grassWeight, biomeContrast);
    forestWeight = sharpenBiomeWeight(forestWeight, biomeContrast * 0.92);
    desertWeight = sharpenBiomeWeight(desertWeight, biomeContrast * 1.08);
    rockWeight = sharpenBiomeWeight(rockWeight, biomeContrast * 0.95);
    snowWeight = sharpenBiomeWeight(snowWeight, biomeContrast * 0.78);
    wetlandWeight = sharpenBiomeWeight(wetlandWeight, biomeContrast * 0.92);

    float treeLineBlend = smoothstep(0.02, 0.22, snowWeight) * smoothstep(0.02, 0.28, forestWeight);
    float snowFeather = snowWeight * treeLineBlend * 0.38;
    snowWeight -= snowFeather;
    forestWeight -= forestWeight * treeLineBlend * 0.22;
    grassWeight += snowFeather * 0.34;
    rockWeight += snowFeather * 0.28;

    float landWeight = beachWeight + grassWeight + forestWeight + desertWeight + rockWeight + snowWeight + wetlandWeight;
    if (landWeight + shallowWaterWeight <= 0.0001) {
        // 缓存缺失或权重被极端参数压空时的兜底材质分类。
        float landMask = smoothstep(0.0, max(terrainBeachWidth, 0.0001), relativeHeight);
        float seabedMask = smoothstep(0.0001, 0.08, waterDepth);
        float fallbackBeach = (1.0 - smoothstep(terrainBeachWidth * 0.35, terrainBeachWidth, abs(relativeHeight)))
                            * landMask
                            * coastShelter;
        float fallbackRock = max(
            smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope) * landMask,
            coastExposure * smoothstep(terrainRockSlopeStart * 0.60, terrainRockSlopeEnd, surface.slope) * landMask
        );
        float fallbackSnow = max(
            smoothstep(terrainSnowStart, terrainSnowEnd, surface.height01),
            smoothstep(0.68, 0.86, 1.0 - temperature)
        ) * landMask;
        float fallbackDesert = smoothstep(0.55, 0.75, temperature)
                             * (1.0 - smoothstep(0.25, 0.45, moisture))
                             * landMask;
        float fallbackForest = smoothstep(0.45, 0.70, moisture)
                             * smoothstep(0.25, 0.45, temperature)
                             * landMask;
        beachWeight = clamp(fallbackBeach, 0.0, 1.0);
        rockWeight = clamp(fallbackRock, 0.0, 1.0);
        snowWeight = clamp(fallbackSnow, 0.0, 1.0);
        desertWeight = clamp(fallbackDesert, 0.0, 1.0);
        forestWeight = clamp(fallbackForest, 0.0, 1.0);
        grassWeight = clamp(landMask, 0.0, 1.0);
        shallowWaterWeight = max(shallowWaterWeight, seabedMask);
        landWeight = beachWeight + grassWeight + forestWeight + desertWeight + rockWeight + snowWeight;
    }

    float materialNoise = fbm3(radialUp * (terrainMaterialNoiseScale * 200.0));
    float colorVariation = mix(1.0 - terrainMaterialNoiseStrength, 1.0 + terrainMaterialNoiseStrength, materialNoise);

    vec3 grassColor = mix(terrainLowlandColor, vec3(0.16, 0.48, 0.12), 0.34);
    vec3 forestColor = mix(terrainForestColor, vec3(0.035, 0.18, 0.055), 0.34);
    vec3 desertColor = mix(terrainDesertColor, vec3(0.86, 0.66, 0.28), 0.38);
    vec3 rockColor = mix(terrainRockColor, vec3(0.36, 0.37, 0.36), 0.42);
    vec3 beachColor = mix(terrainBeachColor, vec3(0.82, 0.74, 0.48), 0.28);
    vec3 snowColor = mix(terrainSnowColor, vec3(0.96, 0.98, 1.0), 0.30);
    vec3 lowlandTint = grassColor * mix(0.88, 1.10, fbm3(radialUp * 18.0 + 4.1));
    vec3 wetlandColor = mix(grassColor, forestColor, 0.68) * vec3(0.62, 0.88, 0.72);
    vec3 alpineColor = mix(forestColor, mix(rockColor, snowColor, 0.42), 0.58);
    vec3 shallowShelfColor = mix(terrainBeachColor, terrainRockColor, 0.25);
    vec3 sedimentColor = vec3(0.42, 0.36, 0.27);
    vec3 abyssalClayColor = vec3(0.18, 0.16, 0.15);
    vec3 seabedRockColor = terrainRockColor * 0.78;
    vec3 shallowWaterColor = mix(shallowShelfColor, sedimentColor, smoothstep(0.25, 2.5, waterDepth));
    // 浅水海床颜色受深度、冲刷和沉积影响。
    shallowWaterColor = mix(shallowWaterColor, abyssalClayColor, smoothstep(2.0, 10.0, waterDepth));
    shallowWaterColor = mix(shallowWaterColor, seabedRockColor, clamp(wearMask * 0.35 + surface.slope * 0.18, 0.0, 0.55));
    shallowWaterColor = mix(shallowWaterColor, sedimentColor, depositionMask * 0.35);
    vec3 color = (
        beachColor * beachWeight
      + lowlandTint * grassWeight
      + forestColor * forestWeight
      + desertColor * desertWeight
      + rockColor * rockWeight
      + snowColor * snowWeight
      + wetlandColor * wetlandWeight
      + shallowWaterColor * shallowWaterWeight
    ) / max(landWeight + shallowWaterWeight, 0.0001);

    color = mix(color, rockColor * mix(0.86, 1.08, fbm3(radialUp * 42.0 + 9.3)), exposedRockMask * 0.28);
    color = mix(color, rockColor, wearMask * smoothstep(0.26, 0.62, surface.slope) * 0.34);
    color = mix(color, vec3(0.52, 0.43, 0.30), depositionMask * 0.28);
    float viewDistance = length(cameraPos - worldPos);
    float nearDetail = 0.0;
    float finePebble = fbm3(radialUp * 820.0 + vec3(7.3, 19.1, 3.6));
    float fineGrain = fbm3(radialUp * 1450.0 + vec3(31.2, 4.7, 22.5));
    float crackNoise = 1.0 - abs(fbm3(radialUp * 560.0 + vec3(12.4, 8.8, 41.6)) * 2.0 - 1.0);
    float rockFine = smoothstep(0.48, 0.88, finePebble) * max(rockWeight, exposedRockMask);
    float grassFine = smoothstep(0.42, 0.82, fineGrain) * (grassWeight + forestWeight * 0.45);
    float sandFine = smoothstep(0.36, 0.78, finePebble * 0.65 + fineGrain * 0.35) * (desertWeight + beachWeight);
    float crackFine = pow(clamp(crackNoise, 0.0, 1.0), 5.0) * max(rockWeight, snowWeight * 0.35);
    vec3 closeDetailTint = vec3(1.0);
    closeDetailTint *= mix(vec3(1.0), vec3(0.76, 0.78, 0.74), rockFine * 0.24);
    closeDetailTint *= mix(vec3(1.0), vec3(0.70, 0.82, 0.60), grassFine * 0.14);
    closeDetailTint *= mix(vec3(1.0), vec3(1.16, 1.08, 0.86), sandFine * 0.18);
    closeDetailTint *= mix(vec3(1.0), vec3(0.52, 0.50, 0.47), crackFine * 0.28);
    color *= mix(vec3(1.0), closeDetailTint, nearDetail * terrainMaterialNoiseStrength);
    // 侵蚀/水流 mask 作为最终 tint：河道更暗更湿，沉积更偏土色。
    color = mix(color, vec3(0.06, 0.16, 0.10), channelMask * 0.12);
    color = mix(color, vec3(0.08, 0.20, 0.11), flowMask * 0.22);
    float riverNoise = fbm3(radialUp * 180.0 + vec3(41.2, 9.7, 63.5));
    float riverWidthSafe = max(riverWidth, 0.05);
    float riverCore = smoothstep(0.34, 0.82 / riverWidthSafe, channelMask);
    float riverThread = smoothstep(0.22, 0.72 / riverWidthSafe, channelMask + flowMask * 0.08);
    float riverMask = clamp(max(riverCore, riverThread * channelMask * 0.62), 0.0, 1.0);
    riverMask *= runtimeLand;
    riverMask *= 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth * 2.0, 0.002), waterDepth);
    riverMask *= smoothstep(-0.015, 0.095, relativeHeight + 0.055);
    riverMask *= mix(0.78, 1.20, riverNoise);
    float riversEnabled = renderRivers != 0 ? 1.0 : 0.0;
    riverMask = clamp(riverMask * riverVisibility * riversEnabled, 0.0, 1.0);
    float riverBank = clamp(flowMask * 0.56 + channelMask * 0.72, 0.0, 1.0) * runtimeLand;
    float causticNoise = fbm3(radialUp * 420.0 + vec3(timeSeconds * 0.16, 11.7, timeSeconds * 0.09));
    float refractNoiseA = fbm3(radialUp * 260.0 + vec3(6.4, timeSeconds * 0.11, 19.2));
    float refractNoiseB = fbm3(radialUp * 340.0 + vec3(27.8, 3.2, timeSeconds * 0.14));
    float refractionRipple = (refractNoiseA - refractNoiseB) * riverRefractionStrength;
    vec3 refractedBed = mix(color, color.bgr * vec3(0.84, 0.98, 1.10), 0.18 + refractionRipple * 0.18);
    refractedBed *= mix(0.92, 1.12, causticNoise * riverRefractionStrength);
    vec3 riverWaterColor = mix(riverColor * 0.70, vec3(0.02, 0.58, 0.66), clamp(channelMask * 0.58 + moisture * 0.26, 0.0, 1.0));
    vec3 refractedRiverColor = mix(refractedBed, riverWaterColor, clamp(0.42 + riverMask * 0.42, 0.0, 0.88));
    color = mix(color, vec3(0.035, 0.105, 0.065), riverBank * 0.18 * riversEnabled);
    color = mix(color, refractedRiverColor, riverMask * 0.86);
    surface.riverMask = riverMask;
    surface.riverSpecular = riverMask * riverShine;
    color = mix(color, terrainBeachColor * 0.72, shallowWaterWeight * 0.20);
    color = mix(color, alpineColor, treeLineBlend * 0.48);

    float h01 = clamp(relativeHeight / 0.42, 0.0, 1.0);
    float lowBand = 1.0 - smoothstep(0.05, 0.26, h01);
    float highBand = smoothstep(0.38, 0.82, h01);
    float alpineBand = smoothstep(0.62, 0.96, h01);
    float peakBand = smoothstep(0.82, 1.0, h01);
    float cliffBand = smoothstep(0.26, 0.72, geomSlope + wearMask * 0.18 + upliftMask * 0.06);
    float valleyBand = smoothstep(0.10, 0.76, geomChannel + flowMask * 0.24);
    float shoreBand = runtimeShore * runtimeLand;

    float macroRegion = fbm3(radialUp * 3.8 + vec3(19.0, 4.0, 11.0));
    float ecoPatch = fbm3(radialUp * 9.5 + vec3(2.7, 41.0, 6.0));
    float vegetationNoise = fbm3(radialUp * 28.0 + vec3(7.0, 13.0, 29.0));
    float mineralNoise = fbm3(radialUp * 64.0 + vec3(31.0, 4.0, 18.0));
    float grainA = fbm3(radialUp * 210.0 + vec3(5.0, 17.0, 23.0));
    float grainB = fbm3(radialUp * 620.0 + vec3(43.0, 3.0, 9.0));
    float terrainCrackNoise = 1.0 - abs(fbm3(radialUp * 145.0 + vec3(12.0, 8.0, 41.0)) * 2.0 - 1.0);

    float dry = clamp((1.0 - moisture) * 0.72 + temperature * 0.34 + macroRegion * 0.34, 0.0, 1.0);
    float lush = clamp(moisture * 0.78 + (1.0 - temperature) * 0.10 + (1.0 - macroRegion) * 0.22 + ecoPatch * 0.18, 0.0, 1.0);
    float cold = clamp((1.0 - temperature) * 0.90 + highBand * 0.22, 0.0, 1.0);
    float plainMask = runtimeLand * (1.0 - cliffBand) * (1.0 - alpineBand * 0.65);
    float forestMask = smoothstep(0.34, 0.62, lush) * smoothstep(0.12, 0.52, vegetationNoise) * plainMask;
    float savannaMask = smoothstep(0.34, 0.62, dry) * smoothstep(0.18, 0.58, moisture + ecoPatch * 0.24) * plainMask;
    float aridMask = smoothstep(0.46, 0.72, dry) * (1.0 - smoothstep(0.30, 0.62, moisture + vegetationNoise * 0.16)) * plainMask;
    float tundraMask = smoothstep(0.48, 0.78, cold) * (1.0 - peakBand) * plainMask;
    float wetMask = valleyBand * runtimeLand * smoothstep(0.34, 0.82, moisture);

    vec3 lowGrass = vec3(0.20, 0.67, 0.16);
    vec3 meadow = vec3(0.64, 0.78, 0.16);
    vec3 deepForest = vec3(0.030, 0.23, 0.070);
    vec3 warmForest = vec3(0.11, 0.43, 0.09);
    vec3 savanna = vec3(0.76, 0.66, 0.20);
    vec3 drySoil = vec3(0.68, 0.36, 0.15);
    vec3 ochre = vec3(0.92, 0.60, 0.22);
    vec3 wetGreen = vec3(0.08, 0.35, 0.25);
    vec3 tundra = vec3(0.55, 0.63, 0.42);
    vec3 brownSlope = vec3(0.50, 0.30, 0.16);
    vec3 redSoil = vec3(0.62, 0.22, 0.10);
    vec3 rockWarm = vec3(0.55, 0.43, 0.32);
    vec3 rockCool = vec3(0.52, 0.56, 0.50);
    vec3 rockDark = vec3(0.18, 0.17, 0.15);
    vec3 paleStone = vec3(0.78, 0.76, 0.62);
    vec3 snow = vec3(0.93, 0.96, 0.94);
    vec3 beach = vec3(0.78, 0.66, 0.38);

    vec3 ecoColor = mix(lowGrass, meadow, smoothstep(0.08, 0.36, h01));
    ecoColor = mix(ecoColor, deepForest, forestMask * 0.94);
    ecoColor = mix(ecoColor, warmForest, forestMask * (1.0 - cold) * 0.34);
    ecoColor = mix(ecoColor, savanna, savannaMask * 0.86);
    ecoColor = mix(ecoColor, mix(drySoil, ochre, macroRegion), aridMask * 0.96);
    ecoColor = mix(ecoColor, tundra, tundraMask * 0.82);
    ecoColor = mix(ecoColor, wetGreen, wetMask * 0.78);

    float soilStripe = smoothstep(0.35, 0.78, terrainCrackNoise + wearMask * 0.30 + channelMask * 0.18);
    float mineralStripe = smoothstep(0.40, 0.82, mineralNoise + cliffBand * 0.22 + upliftMask * 0.10);
    float screeMask = clamp(cliffBand * 0.62 + alpineBand * 0.28 + wearMask * 0.38, 0.0, 1.0) * runtimeLand;
    float capRockMask = smoothstep(0.42, 0.88, h01 + geomSlope * 0.32 + upliftMask * 0.12) * runtimeLand;
    vec3 soilColor = mix(brownSlope, redSoil, dry * 0.62 + macroRegion * 0.22);
    vec3 rockColorDetail = mix(rockWarm, rockCool, cold * 0.45 + mineralNoise * 0.35);
    rockColorDetail = mix(rockColorDetail, rockDark, soilStripe * cliffBand * 0.35);
    rockColorDetail = mix(rockColorDetail, paleStone, capRockMask * mineralStripe * 0.36);

    vec3 terrainPalette = ecoColor;
    terrainPalette = mix(terrainPalette, soilColor, clamp(screeMask * 0.34 + soilStripe * highBand * 0.22, 0.0, 0.62));
    terrainPalette = mix(terrainPalette, rockColorDetail, clamp(screeMask * 0.60 + capRockMask * 0.34, 0.0, 0.86));
    terrainPalette = mix(terrainPalette, snow, peakBand * smoothstep(0.52, 0.86, cold) * 0.56);
    terrainPalette = mix(terrainPalette, beach, shoreBand * (1.0 - cliffBand) * 0.82);

    vec3 detailTint = vec3(1.0);
    detailTint *= mix(vec3(0.86), vec3(1.22), grainA);
    detailTint *= mix(vec3(1.0), vec3(0.62, 0.84, 0.52), forestMask * smoothstep(0.50, 0.88, grainB) * 0.34);
    detailTint *= mix(vec3(1.0), vec3(1.30, 1.08, 0.66), aridMask * smoothstep(0.42, 0.84, grainB) * 0.34);
    detailTint *= mix(vec3(1.0), vec3(0.54, 0.52, 0.48), screeMask * smoothstep(0.38, 0.84, terrainCrackNoise) * 0.38);
    detailTint *= mix(vec3(1.0), vec3(0.82, 0.94, 0.76), wetMask * 0.18);

    float landAuthority = clamp(runtimeLand * max(geomLand, 0.85), 0.0, 1.0);
    vec3 vividPalette = terrainPalette * detailTint;
    float luminance = dot(vividPalette, vec3(0.299, 0.587, 0.114));
    vividPalette = mix(vec3(luminance), vividPalette, 1.34);
    color = mix(color, vividPalette, landAuthority);
    color = mix(color, wetGreen * mix(0.80, 1.14, grainA), riverBank * 0.24 * riversEnabled);
    color = mix(color, terrainPalette * vec3(0.82, 0.88, 0.96), peakBand * 0.08);
    color = clamp(color * mix(0.98, 1.16, vegetationNoise), vec3(0.0), vec3(2.2));

    surface.specularStrength = mix(0.006, 0.016, capRockMask * 0.45 + wetMask * 0.20);
    surface.specularStrength = mix(surface.specularStrength, 0.035, surface.riverMask);
    surface.baseColor = clamp(color * colorVariation, vec3(0.0), vec3(2.0));
    return surface;
#endif
}
