uniform float seaLevelOffset;
uniform float planetRadius;
uniform vec3 terrainLowlandColor;
uniform vec3 terrainForestColor;
uniform vec3 terrainDesertColor;
uniform vec3 terrainRockColor;
uniform vec3 terrainBeachColor;
uniform vec3 terrainSnowColor;
uniform vec3 terrainPaletteLowGrass;
uniform vec3 terrainPaletteMeadow;
uniform vec3 terrainPaletteForestDark;
uniform vec3 terrainPaletteForestWarm;
uniform vec3 terrainPaletteSavanna;
uniform vec3 terrainPaletteDrySoil;
uniform vec3 terrainPaletteOchre;
uniform vec3 terrainPaletteWetGreen;
uniform vec3 terrainPaletteTundra;
uniform vec3 terrainPaletteBrownSlope;
uniform vec3 terrainPaletteRedSoil;
uniform vec3 terrainPaletteRockWarm;
uniform vec3 terrainPaletteRockCool;
uniform vec3 terrainPaletteRockDark;
uniform vec3 terrainPalettePaleStone;
uniform vec3 terrainPaletteSnow;
uniform vec3 terrainPaletteSnowShadow;
uniform vec3 terrainPaletteBeach;
uniform vec3 terrainPaletteRiverBed;
uniform vec3 terrainPaletteShallowSeabed;
uniform vec3 terrainPaletteDeepSeabed;
uniform float terrainBeachWidth;
uniform float terrainRockSlopeStart;
uniform float terrainRockSlopeEnd;
uniform float terrainSnowStart;
uniform float terrainSnowEnd;
uniform float terrainMaterialNoiseScale;
uniform float terrainMaterialNoiseStrength;
uniform float timeSeconds;
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

float ridgedDetail(float value)
{
    return 1.0 - abs(value * 2.0 - 1.0);
}

float materialMicroHeight(vec3 dir, float grassWeight, float soilWeight, float rockWeight, float snowWeight, float wetWeight)
{
    float grassFiber = fbm3(dir * 95.0 + vec3(4.0, 19.0, 8.0)) * 0.012;
    grassFiber += ridgedDetail(fbm3(dir * 180.0 + vec3(31.0, 7.0, 12.0))) * 0.004;

    float soilGrain = fbm3(dir * 82.0 + vec3(11.0, 3.0, 27.0)) * 0.024;
    soilGrain += fbm3(dir * 190.0 + vec3(2.0, 41.0, 5.0)) * 0.008;

    float rockLayer = ridgedDetail(fbm3(dir * 36.0 + vec3(17.0, 23.0, 6.0))) * 0.050;
    rockLayer += ridgedDetail(fbm3(dir * 95.0 + vec3(5.0, 13.0, 31.0))) * 0.018;
    rockLayer += fbm3(dir * 210.0 + vec3(29.0, 2.0, 16.0)) * 0.006;

    float snowPowder = fbm3(dir * 90.0 + vec3(6.0, 11.0, 43.0)) * 0.006;
    float wetSilt = fbm3(dir * 90.0 + vec3(47.0, 9.0, 3.0)) * 0.010;

    float totalWeight = max(grassWeight + soilWeight + rockWeight + snowWeight + wetWeight, 0.001);
    return (grassFiber * grassWeight
          + soilGrain * soilWeight
          + rockLayer * rockWeight
          + snowPowder * snowWeight
          + wetSilt * wetWeight) / totalWeight;
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
    surface.roughness = 0.82;
    surface.materialDebugColor = vec3(0.0);
    surface.detailNormal = normalize(shadingNormal);
    PlanetSample planet = samplePlanet(sphereDir, height);

    // slope 由真实法线和径向方向计算，后面用于裸岩/雪线/植被剔除。
    vec3 radialUp = normalize(worldPos);
    vec3 materialDir = normalize(sphereDir);
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
    float mAltitude = clamp(mHeight + upliftMask * 0.10 + geomSlope * 0.08 + wearMask * 0.04 - mShore * 0.10, 0.0, 1.0);
    float mLowlandBand = (1.0 - smoothstep(0.18, 0.34, mAltitude)) * runtimeLand;
    float mHillBand = smoothstep(0.12, 0.32, mAltitude) * (1.0 - smoothstep(0.46, 0.66, mAltitude)) * runtimeLand;
    float mMountainBand = smoothstep(0.34, 0.58, mAltitude) * (1.0 - smoothstep(0.74, 0.92, mAltitude)) * runtimeLand;
    float mAlpineBand = smoothstep(0.56, 0.78, mAltitude) * runtimeLand;
    float mPeakBand = smoothstep(0.72, 0.94, mAltitude) * runtimeLand;

    float mMacro = fbm3(materialDir * 3.6 + vec3(19.0, 4.0, 11.0));
    float mPatch = fbm3(materialDir * 9.5 + vec3(2.7, 41.0, 6.0));
    float mVegNoise = fbm3(materialDir * 18.0 + vec3(7.0, 13.0, 29.0));
    float mMineral = fbm3(materialDir * 34.0 + vec3(31.0, 4.0, 18.0));
    float mGrainA = fbm3(materialDir * 38.0 + vec3(5.0, 17.0, 23.0));
    float mGrainB = fbm3(materialDir * 76.0 + vec3(43.0, 3.0, 9.0));
    float mCrack = 1.0 - abs(fbm3(materialDir * 42.0 + vec3(12.0, 8.0, 41.0)) * 2.0 - 1.0);
    float mColorPatch = fbm3(materialDir * 13.0 + vec3(8.0, 25.0, 3.0));
    float mMaterialNoise = fbm3(materialDir * (terrainMaterialNoiseScale * 200.0));
    float mColorVariation = mix(1.0 - terrainMaterialNoiseStrength, 1.0 + terrainMaterialNoiseStrength, mMaterialNoise);

    float mDry = clamp((1.0 - moisture) * 0.74 + temperature * 0.34 + mMacro * 0.34, 0.0, 1.0);
    float mLush = clamp(moisture * 0.78 + (1.0 - temperature) * 0.10 + (1.0 - mMacro) * 0.24 + mPatch * 0.18, 0.0, 1.0);
    float mCold = clamp((1.0 - temperature) * 0.90 + mHigh * 0.22, 0.0, 1.0);
    float mVegetationCull = smoothstep(0.36, 0.72, mAltitude + mCliff * 0.18 + wearMask * 0.12);
    float mPlain = runtimeLand * (1.0 - mCliff * 0.70) * (1.0 - mVegetationCull * 0.92);
    float mForest = smoothstep(0.36, 0.66, mLush) * smoothstep(0.20, 0.62, mVegNoise) * mPlain;
    float mSavanna = smoothstep(0.54, 0.78, mDry) * smoothstep(0.18, 0.50, moisture + mPatch * 0.16) * mPlain;
    float mArid = smoothstep(0.66, 0.88, mDry) * (1.0 - smoothstep(0.20, 0.48, moisture + mVegNoise * 0.10)) * mPlain;
    float mTundra = smoothstep(0.44, 0.76, mCold + mAlpineBand * 0.12) * (1.0 - mPeakBand) * runtimeLand * (mHillBand * 0.35 + mMountainBand * 0.65 + mAlpineBand * 0.40);
    float mWet = mValley * runtimeLand * smoothstep(0.34, 0.82, moisture);
    float mRiverBed = clamp(max(channelMask, flowMask * 0.74) * runtimeLand, 0.0, 1.0);

    mForest = pow(clamp(mForest, 0.0, 1.0), 0.82);
    mSavanna = pow(clamp(mSavanna, 0.0, 1.0), 1.10);
    mArid = pow(clamp(mArid, 0.0, 1.0), 1.18);
    mTundra = pow(clamp(mTundra, 0.0, 1.0), 1.12);
    mWet = pow(clamp(mWet, 0.0, 1.0), 0.92);

    vec3 mLowGrass = terrainPaletteLowGrass;
    vec3 mMeadow = terrainPaletteMeadow;
    vec3 mForestDark = terrainPaletteForestDark;
    vec3 mForestWarm = terrainPaletteForestWarm;
    vec3 mSavannaColor = terrainPaletteSavanna;
    vec3 mDrySoil = terrainPaletteDrySoil;
    vec3 mOchre = terrainPaletteOchre;
    vec3 mWetGreen = terrainPaletteWetGreen;
    vec3 mTundraColor = terrainPaletteTundra;
    vec3 mBrownSlope = terrainPaletteBrownSlope;
    vec3 mRedSoil = terrainPaletteRedSoil;
    vec3 mRockWarm = terrainPaletteRockWarm;
    vec3 mRockCool = terrainPaletteRockCool;
    vec3 mRockDark = terrainPaletteRockDark;
    vec3 mPaleStone = terrainPalettePaleStone;
    vec3 mSnow = terrainPaletteSnow;
    vec3 mSnowShadow = terrainPaletteSnowShadow;
    vec3 mBeach = terrainPaletteBeach;
    vec3 mRiverBedColor = terrainPaletteRiverBed;
    vec3 mShallowSeabed = terrainPaletteShallowSeabed;
    vec3 mDeepSeabed = terrainPaletteDeepSeabed;

    vec3 mEco = mix(mLowGrass, mMeadow, smoothstep(0.08, 0.36, mHeight));
    mEco = mix(mEco, mLowGrass, mLowlandBand * 0.42);
    mEco = mix(mEco, mMeadow, mHillBand * 0.26);
    mEco = mix(mEco, mForestDark, mForest * 0.94);
    mEco = mix(mEco, mForestWarm, mForest * (1.0 - mCold) * 0.34);
    mEco = mix(mEco, mSavannaColor, mSavanna * 0.48);
    mEco = mix(mEco, mix(mDrySoil, mOchre, mMacro), mArid * 0.58);
    mEco = mix(mEco, mTundraColor, mTundra * 0.82);
    mEco = mix(mEco, mWetGreen, mWet * 0.78);

    float mSoilStripe = smoothstep(0.42, 0.78, mCrack + wearMask * 0.26 + channelMask * 0.16 + flowMask * 0.10);
    float mMineralStripe = smoothstep(0.48, 0.86, mMineral + mCliff * 0.16 + upliftMask * 0.08);
    float mFineErosion = smoothstep(0.42, 0.84, mCrack + wearMask * 0.34 + channelMask * 0.18 + flowMask * 0.12);
    float mErosionFocus = clamp(mValley * 0.44 + mCliff * 0.34 + mMountainBand * 0.22 + mAlpineBand * 0.28, 0.0, 1.0);
    float mErodedSoil = smoothstep(0.34, 0.78, wearMask * 0.46 + channelMask * 0.30 + flowMask * 0.18 + geomSlope * 0.14) * mErosionFocus * mFineErosion * runtimeLand;
    float mScree = clamp(mCliff * 0.56 + mMountainBand * 0.24 + mAlpineBand * 0.42 + wearMask * 0.34, 0.0, 1.0) * runtimeLand;
    float mCapRock = smoothstep(0.40, 0.82, mAltitude + geomSlope * 0.28 + upliftMask * 0.12) * runtimeLand;
    mCapRock *= clamp(0.18 + mMountainBand * 0.52 + mAlpineBand * 0.72 + mPeakBand * 0.86, 0.0, 1.0);
    float mSteepRock = smoothstep(0.40, 0.72, geomSlope + wearMask * 0.14 + upliftMask * 0.08)
                     * smoothstep(0.22, 0.52, mAltitude)
                     * (1.0 - mShore * 0.60)
                     * runtimeLand;
    float mRockExposure = clamp(mSteepRock * 0.62 + mScree * 0.36 + mCapRock * 0.30 + mAlpineBand * 0.36 + mPeakBand * 0.54, 0.0, 1.0);
    float mSnowMask = smoothstep(0.62, 0.88, mAltitude + mCold * 0.20 + mPeakBand * 0.16 - mDry * 0.08) * runtimeLand;
    mSnowMask *= smoothstep(0.26, 0.70, 1.0 - temperature + mPeakBand * 0.24);
    mSnowMask *= 1.0 - clamp(mCliff * 0.28 + wearMask * 0.18, 0.0, 0.42);
    vec3 mSoil = mix(mBrownSlope, mRedSoil, mDry * 0.62 + mMacro * 0.22);
    vec3 mRock = mix(mRockWarm, mRockCool, mCold * 0.45 + mMineral * 0.35);
    mRock = mix(mRock, mRockDark, mSoilStripe * mCliff * 0.15);
    mRock = mix(mRock, mPaleStone, mCapRock * mMineralStripe * 0.28);
    vec3 mSnowColor = mix(mSnowShadow, mSnow, smoothstep(0.34, 0.82, mGrainB + mPeakBand * 0.24 + mCold * 0.12));

    vec3 mLandColor = mEco;
    mLandColor = mix(mLandColor, mSoil, clamp(mErodedSoil * 0.58 + mScree * 0.10 + mSoilStripe * mMountainBand * 0.12, 0.0, 0.44));
    mLandColor = mix(mLandColor, mRock, clamp(mRockExposure * 1.14, 0.0, 0.86));
    mLandColor = mix(mLandColor, mRiverBedColor, mRiverBed * 0.62);
    mLandColor = mix(mLandColor, mSnowColor, clamp(mSnowMask * 0.96, 0.0, 0.96));
    mLandColor = mix(mLandColor, mBeach, mShore * (1.0 - mCliff) * 0.82);

    vec3 mDetail = vec3(1.0);
    mDetail *= mix(vec3(0.97), vec3(1.05), mGrainA);
    mDetail *= mix(vec3(1.0), vec3(0.76, 1.06, 0.62), mForest * smoothstep(0.52, 0.88, mGrainB) * 0.08);
    mDetail *= mix(vec3(1.0), vec3(1.10, 0.99, 0.76), mArid * smoothstep(0.42, 0.84, mGrainB) * 0.08);
    mDetail *= mix(vec3(1.0), vec3(1.04, 0.84, 0.60), mErodedSoil * smoothstep(0.36, 0.86, mCrack) * 0.08);
    mDetail *= mix(vec3(1.0), vec3(0.90, 0.96, 1.04), mRockExposure * smoothstep(0.42, 0.86, mCrack) * 0.14);
    mDetail *= mix(vec3(1.0), vec3(0.78, 1.08, 0.82), mWet * 0.07);

    vec3 mStylizedPatch = mix(vec3(0.88, 1.05, 0.72), vec3(1.12, 0.99, 0.78), mColorPatch);
    vec3 mVivid = mLandColor * mix(vec3(1.0), mStylizedPatch, 0.42) * mDetail;
    float mLum = dot(mVivid, vec3(0.299, 0.587, 0.114));
    mVivid = mix(vec3(mLum), mVivid, 2.05);
    float mRockTintGuard = clamp(mRockExposure * 0.78 + mCapRock * 0.22 + mScree * 0.18 + mAlpineBand * 0.16, 0.0, 1.0);
    mVivid *= mix(vec3(1.08, 1.04, 0.86), vec3(0.94, 0.99, 1.06), mRockTintGuard);
    vec3 mSeabed = mix(mShallowSeabed, mDeepSeabed, smoothstep(0.2, 5.0, waterDepth));
    mSeabed = mix(mSeabed, mRiverBedColor, depositionMask * 0.34 + wearMask * 0.18);
    float mLand = clamp(runtimeLand * max(geomLand, 0.85), 0.0, 1.0);
    vec3 mColor = mix(mSeabed, mVivid, mLand);
    mColor = mix(mColor, mWetGreen * mix(0.80, 1.14, mGrainA), mRiverBed * 0.28);
    mColor = mix(mColor, mLandColor * vec3(0.88, 0.91, 0.90), mPeakBand * 0.05);
    float mReliefShade = mix(0.92, 1.12, smoothstep(0.02, 0.70, mHeight));
    mReliefShade *= mix(1.06, 0.94, mCliff * (0.26 + wearMask * 0.12));
    mColor = clamp(mColor * mix(0.99, 1.05, mVegNoise) * mReliefShade, vec3(0.0), vec3(2.2));
    float mRockGrade = clamp(mRockExposure * 0.70 + mScree * 0.24 + mCapRock * 0.22 + mAlpineBand * 0.18, 0.0, 1.0);
    mRockGrade *= 1.0 - clamp(mSnowMask * 0.90, 0.0, 0.90);
    float mRockLum = dot(mColor, vec3(0.299, 0.587, 0.114));
    vec3 mCoolNeutralRock = mix(vec3(mRockLum) * vec3(0.92, 0.98, 1.04), mRock, 0.34);
    mColor = mix(mColor, mCoolNeutralRock, mRockGrade * 0.42);
    float mSnowCap = pow(clamp(mSnowMask, 0.0, 1.0), 0.76);
    vec3 mSnowHighlight = mix(mSnowColor * 0.94, vec3(1.18, 1.20, 1.24), smoothstep(0.42, 0.88, mGrainA + mPeakBand * 0.20));
    mColor = mix(mColor, mSnowHighlight, mSnowCap * 0.32);

    float mRockWeight = clamp(mRockExposure, 0.0, 1.0);
    float mSnowWeight = clamp(mSnowMask, 0.0, 1.0);
    float mWetWeight = clamp(mWet * 0.75 + mRiverBed * 0.55, 0.0, 1.0);
    float mSoilWeight = clamp(mArid * 0.46 + mErodedSoil * 0.68 + mScree * 0.18 + mSoilStripe * mMountainBand * 0.22, 0.0, 1.0);
    float mGrassWeight = clamp(mPlain * (1.0 - mForest * 0.18) * (1.0 - mRockWeight * 0.82) * (1.0 - mArid * 0.45), 0.0, 1.0);

    vec3 mNormalBase = normalize(shadingNormal);
    vec3 mTangentSeed = abs(materialDir.y) < 0.92 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 mTangent = normalize(cross(mTangentSeed, materialDir));
    vec3 mBitangent = normalize(cross(materialDir, mTangent));
    float mNormalStep = 0.0045;
    float mMicroCenter = materialMicroHeight(materialDir, mGrassWeight, mSoilWeight, mRockWeight, mSnowWeight, mWetWeight);
    float mMicroT = materialMicroHeight(normalize(materialDir + mTangent * mNormalStep), mGrassWeight, mSoilWeight, mRockWeight, mSnowWeight, mWetWeight);
    float mMicroB = materialMicroHeight(normalize(materialDir + mBitangent * mNormalStep), mGrassWeight, mSoilWeight, mRockWeight, mSnowWeight, mWetWeight);
    float mNormalStrength = clamp(0.06 + mRockWeight * 0.22 + mSoilWeight * 0.08 - mSnowWeight * 0.08, 0.02, 0.32);
    vec3 mDetailNormal = normalize(mNormalBase
        + mTangent * ((mMicroCenter - mMicroT) / mNormalStep) * mNormalStrength
        + mBitangent * ((mMicroCenter - mMicroB) / mNormalStep) * mNormalStrength);

    surface.riverMask = 0.0;
    surface.riverSpecular = 0.0;
    surface.specularStrength = mix(0.006, 0.016, mCapRock * 0.45 + mWet * 0.20);
    surface.specularStrength = mix(surface.specularStrength, 0.014, mRiverBed * 0.30);
    surface.roughness = clamp(0.86 - mWetWeight * 0.20 - mCapRock * 0.12 + mGrassWeight * 0.06 + mSnowWeight * 0.10, 0.48, 0.96);
    surface.materialDebugColor = vec3(0.14, 0.82, 0.16);
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(0.02, 0.36, 0.06), clamp(mForest, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(1.00, 0.78, 0.05), clamp(mSavanna, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(1.00, 0.34, 0.06), clamp(mArid, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(0.58, 0.24, 0.08), clamp(mErodedSoil, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(0.72, 0.64, 0.48), clamp(mRockExposure, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(0.92, 0.96, 1.00), clamp(mSnowWeight, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(0.00, 0.55, 0.42), clamp(mWet, 0.0, 1.0));
    surface.materialDebugColor = mix(surface.materialDebugColor, vec3(1.00, 0.84, 0.30), clamp(mShore, 0.0, 1.0));
    surface.detailNormal = mDetailNormal;
    surface.baseColor = clamp(mColor * mColorVariation, vec3(0.0), vec3(2.0));
    return surface;
}
