$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Utf8NoBom = New-Object System.Text.UTF8Encoding -ArgumentList $false

function Write-Utf8File {
    param([string]$RelativePath, [string]$Content)
    $full = Join-Path $Root $RelativePath
    $dir = Split-Path -Parent $full
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    [System.IO.File]::WriteAllText($full, $Content, $Utf8NoBom)
}

function Escape-Xml {
    param([string]$Text)
    if ($null -eq $Text) { return "" }
    return [System.Security.SecurityElement]::Escape($Text)
}

function Wrap-Text {
    param([string]$Text, [int]$MaxChars)
    $result = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrWhiteSpace($Text)) { return $result }
    if ($Text -match "\s") {
        $line = ""
        foreach ($word in ($Text -split "\s+")) {
            if ($line.Length -eq 0) {
                $line = $word
            } elseif (($line.Length + 1 + $word.Length) -le $MaxChars) {
                $line = "$line $word"
            } else {
                $result.Add($line)
                $line = $word
            }
        }
        if ($line.Length -gt 0) { $result.Add($line) }
    } else {
        for ($i = 0; $i -lt $Text.Length; $i += $MaxChars) {
            $len = [Math]::Min($MaxChars, $Text.Length - $i)
            $result.Add($Text.Substring($i, $len))
        }
    }
    return $result
}

function New-TextBlock {
    param(
        [string]$Text,
        [int]$X,
        [int]$Y,
        [string]$Class,
        [int]$MaxChars,
        [int]$LineHeight,
        [string]$Anchor = "start"
    )
    $lines = Wrap-Text $Text $MaxChars
    $out = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt [Math]::Min(4, $lines.Count); $i++) {
        $yy = $Y + $i * $LineHeight
        $escaped = Escape-Xml $lines[$i]
        $out.Add("<text class=`"$Class`" x=`"$X`" y=`"$yy`" text-anchor=`"$Anchor`">$escaped</text>")
    }
    return ($out -join "`n")
}

function New-Arrow {
    param([int]$X1, [int]$Y1, [int]$X2, [int]$Y2)
    $cx1 = [int](($X1 + $X2) / 2)
    $cx2 = $cx1
    return "<path class=`"arrow`" d=`"M$X1 $Y1 C$cx1 $Y1, $cx2 $Y2, $X2 $Y2`"/>"
}

function New-Box {
    param(
        [int]$X,
        [int]$Y,
        [int]$W,
        [int]$H,
        [string]$Title,
        [string]$Body = "",
        [string]$Palette = "blue",
        [int]$MaxTitle = 14,
        [int]$MaxBody = 18
    )
    $tx = $X + 22
    $titleY = $Y + 36
    $bodyY = $Y + 72
    $titleBlock = New-TextBlock $Title $tx $titleY "label" $MaxTitle 24
    $bodyBlock = ""
    if ($Body.Length -gt 0) {
        $bodyBlock = New-TextBlock $Body $tx $bodyY "small" $MaxBody 21
    }
    return @"
<rect class="box $Palette" x="$X" y="$Y" width="$W" height="$H" rx="12"/>
$titleBlock
$bodyBlock
"@
}

function New-FlowSvg {
    param([hashtable]$Module, [string]$Lang)
    $font = if ($Lang -eq "cn") { "'Microsoft YaHei','Noto Sans CJK SC',Arial,sans-serif" } else { "Inter,Segoe UI,Arial,sans-serif" }
    $steps = $Module.Steps
    $height = if ($steps.Count -gt 8) { 880 } else { 720 }
    $boxW = 255
    $boxH = if ($Lang -eq "cn") { 116 } else { 126 }
    $startX = 60
    $gapX = 35
    $rowY = @(155, 380, 605)
    $palettes = @("blue", "green", "amber", "violet", "blue", "green", "amber", "violet", "blue", "green", "amber", "violet")
    $body = New-Object System.Collections.Generic.List[string]
    $prevX = $null
    $prevY = $null
    for ($i = 0; $i -lt $steps.Count; $i++) {
        $row = [int][Math]::Floor($i / 4)
        $colInRow = $i % 4
        $actualCol = if (($row % 2) -eq 0) { $colInRow } else { 3 - $colInRow }
        $x = $startX + $actualCol * ($boxW + $gapX)
        $y = $rowY[$row]
        $num = "{0:D2}" -f ($i + 1)
        $body.Add("<circle class=`"numCircle`" cx=`"$($x + 28)`" cy=`"$($y - 14)`" r=`"22`"/>")
        $body.Add("<text class=`"num`" x=`"$($x + 28)`" y=`"$($y - 7)`" text-anchor=`"middle`">$num</text>")
        $maxTitle = if ($Lang -eq "cn") { 14 } else { 20 }
        $body.Add((New-Box $x $y $boxW $boxH $steps[$i] "" $palettes[$i % $palettes.Count] $maxTitle 20))
        if ($null -ne $prevX) {
            $prevRow = [int][Math]::Floor(($i - 1) / 4)
            if ($prevRow -eq $row) {
                if (($row % 2) -eq 0) {
                    $body.Add((New-Arrow ($prevX + $boxW) ($prevY + [int]($boxH / 2)) $x ($y + [int]($boxH / 2))))
                } else {
                    $body.Add((New-Arrow $prevX ($prevY + [int]($boxH / 2)) ($x + $boxW) ($y + [int]($boxH / 2))))
                }
            } else {
                $px = if (($row % 2) -eq 1) { $prevX + $boxW } else { $prevX }
                $curInX = if (($row % 2) -eq 0) { $x } else { $x + $boxW }
                $midY = $y - 42
                $body.Add("<path class=`"arrow`" d=`"M$px $($prevY + $boxH) L$px $midY L$curInX $midY L$curInX $($y + [int]($boxH / 2))`"/>")
            }
        }
        $prevX = $x
        $prevY = $y
    }
    $title = Escape-Xml $Module.Title
    $subtitle = Escape-Xml $Module.Subtitle
    $bodyText = $body -join "`n"
    return @"
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="$height" viewBox="0 0 1280 $height" role="img" aria-labelledby="title desc">
  <title id="title">$title</title>
  <desc id="desc">$subtitle</desc>
  <defs>
    <marker id="arrow" markerWidth="12" markerHeight="12" refX="10" refY="6" orient="auto">
      <path d="M2,2 L10,6 L2,10 Z" fill="#315b7c"/>
    </marker>
    <style>
      .bg{fill:#f7fbff}.title{font:700 34px $font;fill:#17324d}.sub{font:18px $font;fill:#5d6b78}
      .box{fill:#fff;stroke-width:2.2}.blue{stroke:#7daed5;fill:#ffffff}.green{stroke:#76b98d;fill:#f0faf3}.amber{stroke:#dca44a;fill:#fff8e8}.violet{stroke:#a986d7;fill:#f8f2ff}
      .label{font:700 20px $font;fill:#17324d}.small{font:16px $font;fill:#435464}.arrow{stroke:#315b7c;stroke-width:3;fill:none;marker-end:url(#arrow)}
      .numCircle{fill:#315b7c}.num{font:700 16px $font;fill:white}
    </style>
  </defs>
  <rect class="bg" width="1280" height="$height"/>
  <text class="title" x="58" y="62">$title</text>
  <text class="sub" x="60" y="94">$subtitle</text>
  $bodyText
</svg>
"@
}

function New-ConceptSvg {
    param([hashtable]$Diagram)
    $font = "Inter,Segoe UI,Arial,sans-serif"
    $colors = @("blue", "green", "amber", "violet")
    $body = New-Object System.Collections.Generic.List[string]
    $colW = 350
    $gap = 45
    for ($idx = 0; $idx -lt $Diagram.Columns.Count; $idx++) {
        $col = $Diagram.Columns[$idx]
        $x = 70 + $idx * ($colW + $gap)
        $lane = Escape-Xml $col[0]
        $body.Add("<text class=`"lane`" x=`"$x`" y=`"154`">$lane</text>")
        for ($j = 1; $j -lt $col.Count; $j++) {
            $y = 188 + ($j - 1) * 118
            $body.Add((New-Box $x $y $colW 86 $col[$j] "" $colors[($idx + $j) % $colors.Count] 24 26))
            if ($j -gt 1) {
                $body.Add("<path class=`"arrow`" d=`"M$($x + [int]($colW / 2)) $($y - 32) L$($x + [int]($colW / 2)) $y`"/>")
            }
        }
        if ($idx -gt 0) {
            $px = 70 + ($idx - 1) * ($colW + $gap) + $colW
            $body.Add((New-Arrow $px 318 $x 318))
        }
    }
    $title = Escape-Xml $Diagram.Title
    $subtitle = Escape-Xml $Diagram.Subtitle
    $bodyText = $body -join "`n"
    return @"
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720" viewBox="0 0 1280 720" role="img" aria-labelledby="title desc">
  <title id="title">$title</title>
  <desc id="desc">$subtitle</desc>
  <defs>
    <marker id="arrow" markerWidth="12" markerHeight="12" refX="10" refY="6" orient="auto">
      <path d="M2,2 L10,6 L2,10 Z" fill="#315b7c"/>
    </marker>
    <style>
      .bg{fill:#f7fbff}.title{font:700 34px $font;fill:#17324d}.sub{font:18px $font;fill:#5d6b78}
      .lane{font:700 22px $font;fill:#36566f}.box{fill:#fff;stroke-width:2.2}.blue{stroke:#7daed5;fill:#ffffff}.green{stroke:#76b98d;fill:#f0faf3}.amber{stroke:#dca44a;fill:#fff8e8}.violet{stroke:#a986d7;fill:#f8f2ff}
      .label{font:700 21px $font;fill:#17324d}.small{font:16px $font;fill:#435464}.arrow{stroke:#315b7c;stroke-width:3;fill:none;marker-end:url(#arrow)}
    </style>
  </defs>
  <rect class="bg" width="1280" height="720"/>
  <text class="title" x="58" y="62">$title</text>
  <text class="sub" x="60" y="94">$subtitle</text>
  $bodyText
</svg>
"@
}

function New-ScriptMarkdown {
    param([hashtable]$Module, [string]$Lang)
    $imgDir = if ($Lang -eq "cn") { "../images" } else { "../images_en" }
    $images = ($Module.Images | ForEach-Object { "- $imgDir/$_" }) -join "`n"
    $code = ($Module.Code | ForEach-Object { "- $_" }) -join "`n"
    $steps = for ($i = 0; $i -lt $Module.Steps.Count; $i++) { "$($i + 1). $($Module.Steps[$i])" }
    $stepsText = $steps -join "`n"
    $talk = ($Module.Talk | ForEach-Object { "> $_" }) -join "`n`n"
    if ($Lang -eq "cn") {
        return @"
# $($Module.Id). $($Module.Title)

## 对应图片
$images

## 讲解目标
$($Module.Subtitle)

## 关键代码位置
$code

## 讲解流程
$stepsText

## 口播稿
$talk

## 老师可能追问时的回答
$($Module.Ask)
"@
    }
    return @"
# $($Module.Id). $($Module.Title)

## Matching Images
$images

## Goal
$($Module.Subtitle)

## Key Code Locations
$code

## Explanation Flow
$stepsText

## Speaking Script
$talk

## Possible Follow-up Answer
$($Module.Ask)
"@
}

$ModulesCn = @(
    @{
        Id="01"; Slug="project_overview"; Title="项目总体流程"; Subtitle="从应用启动、CPU 程序化生成，到 GPU 实时渲染的完整链路。";
        Code=@("src/main.cpp","src/PlanetRenderer.cpp","src/PlanetProceduralData.cpp","xmake.lua");
        Images=@("01_overall_pipeline.svg","13_flow_01_project_overview.svg");
        Steps=@("启动 main() 创建窗口和 OpenGL 上下文","初始化 ImGui 与 PlanetRenderer","加载 shader、网格、海洋与大气资源","尝试读取 session/cache","无缓存时进入参数设置并生成星球","CPU 生成 DEM、气候、水文和材质数据","主线程上传 texture array 与 baked mesh","每帧绘制地形、海洋、大气、体积云和 UI");
        Talk=@("这一页先讲项目的总目标：它不是静态模型，而是一个实时程序化星球渲染系统。","代码把工作拆成两层。CPU 端负责生成地形高度、水文、侵蚀、温度、湿度和材质 mask；GPU 端负责把这些数据变成实时画面。","主程序先初始化窗口、OpenGL、ImGui 和渲染器，然后优先尝试读取本地缓存。缓存存在时可以直接进入渲染；没有缓存时，用户在参数界面点击 Generate Planet 后启动后台生成。","生成完成后，真正的 OpenGL 上传一定回到主线程完成，因为纹理和缓冲对象依赖当前 OpenGL context。","最后进入 render loop，每帧根据相机位置做可见性判断，再依次绘制地形、海洋、大气、体积云和调试界面。");
        Ask="老师如果问项目核心创新点，可以回答：重点是把程序化 DEM 星球、分块 LOD、FFT 海洋、大气散射和可调试 UI 组合成了一个完整的实时渲染管线。"
    },
    @{
        Id="02"; Slug="app_state_generation_thread"; Title="应用状态与生成线程"; Subtitle="用三个阶段隔离参数设置、后台生成和实时渲染。";
        Code=@("src/main.cpp"); Images=@("02_application_workflow.svg","14_flow_02_app_state_generation_thread.svg");
        Steps=@("ProceduralSetup 显示参数设置界面","点击 Generate Planet","startPlanetGeneration() 复制当前参数","std::async 在后台生成 PlanetProceduralData","progressCallback 更新原子进度值","主循环继续刷新进度条","future ready 后调用 finishPlanetGeneration()","setProceduralData() 上传 GPU 并进入 Render");
        Talk=@("这个模块体现了项目的工程组织方式。生成星球可能比较耗时，所以主线程不直接卡住等待，而是通过 std::async 放到后台执行。","后台线程只做 CPU 数据计算，比如高度场、水文和材质权重；它不碰 OpenGL 对象。","主循环通过 future 和 atomic 进度值更新 UI，因此生成时仍然可以显示进度条。","当 future 完成后，finishPlanetGeneration() 把结果交给 PlanetRenderer，主线程再创建纹理、VBO、IBO 和 VAO。","这样既避免 OpenGL 线程问题，也让应用状态从参数设置、生成中、渲染中变得很清楚。");
        Ask="可以强调：线程拆分不是为了炫技，而是为了避免 UI 假死，并保证 OpenGL 资源只在主线程创建。"
    },
    @{
        Id="03"; Slug="cube_sphere_mapping"; Title="六面体星球与接缝处理"; Subtitle="用 cube face 组织数据，再映射到球面，降低球面采样复杂度。";
        Code=@("src/PlanetProceduralData.cpp","include/PlanetProceduralData.h"); Images=@("03_cube_sphere_mapping.svg","15_flow_03_cube_sphere_mapping.svg");
        Steps=@("把星球拆成 6 个 cube face","每个 face 使用二维 texel 网格","face UV 转换为 cube 坐标","cubeSphereDirection() 归一化到球面方向","用方向向量采样噪声和气候函数","跨 face 查找 neighborCell()","fixCubeFaceSeams() 多轮边界融合","输出连续的球面高度场");
        Talk=@("直接在球面上做规则网格比较麻烦，所以项目采用 cube-sphere 思路：先把星球拆成六张方形贴图，每张贴图都是普通二维数组。","生成时，每个 texel 先得到它在 cube face 上的位置，再归一化成球面方向。之后所有噪声、温度、湿度和水文计算都基于这个球面方向。","难点在接缝。如果六个面分别生成，边界很容易出现高度断裂。","代码里用 neighborCell() 找到跨面的相邻 texel，再用 fixCubeFaceSeams() 对边界做多轮融合。","讲解时可以说：cube-sphere 是数据组织方式，接缝处理保证它最终看起来像一个连续星球。");
        Ask="如果被问为什么不用 UV 球，可以回答：cube-sphere 的采样更均匀，适合 texture array 和 chunk 化处理。"
    },
    @{
        Id="04"; Slug="dem_generation"; Title="DEM 程序化地形生成"; Subtitle="当前实际运行路径是 generateDemPrototype()，用于生成高度、水文和地表属性。";
        Code=@("src/PlanetProceduralData.cpp"); Images=@("04_dem_generation.svg","16_flow_04_dem_generation.svg");
        Steps=@("PlanetProceduralData::generate()","clamp faceResolution","进入 generateDemPrototype()","为 6 个 face 分配高度、水深、侵蚀等数组","遍历每个 texel 并计算 sphereDir","fBM / ridgedFbm 生成大陆和山脉","记录 height、uplift、landMask、preErosionHeight","接缝融合、水文侵蚀、气候计算","buildTerrainChunks() 烘焙地形块");
        Talk=@("这一页要特别讲清楚当前真实代码路径。PlanetProceduralData::generate() 进入 generateDemPrototype() 后直接 return，因此现在运行的是 DEM 原型路径。","DEM 可以理解为数字高程模型。代码给六个 cube face 分配多张数组，包括高度、水深、侵蚀、温度、湿度、河道和材质权重。","每个 texel 通过球面方向采样多层噪声。低频噪声决定大陆轮廓，中频和高频噪声塑造山脉、山脊和局部起伏。","得到基础地形后，代码再进行接缝修正、水文侵蚀和气候字段计算。","最后 buildTerrainChunks() 把 DEM 数据烘焙成可渲染的地形块，这一步把生成阶段和渲染阶段连接起来。");
        Ask="不要把后面保留的旧版分模块生成代码说成当前主路径；它现在是保留代码，当前入口已经提前返回。"
    },
    @{
        Id="05"; Slug="hydrology_erosion_masks"; Title="水文、侵蚀与河道 Mask"; Subtitle="用下游汇流、stream power 和坡面扩散生成河道与侵蚀痕迹。";
        Code=@("src/PlanetProceduralData.cpp"); Images=@("05_hydrology_erosion_masks.svg","17_flow_05_hydrology_erosion_masks.svg");
        Steps=@("根据高度对 texel 排序","为每个 texel 寻找下游 receiver","累加 drainage / flow accumulation","根据 slope 和 drainage 计算 streamPower","切割河道并写入 channel mask","记录 wear / deposition / flow maps","thermal diffusion 平滑陡坡","计算水深、岸线和湿度影响");
        Talk=@("这个模块让地形不只是噪声堆出来的山，而是有水流塑造过的痕迹。","代码先按高度排序，让高处的水往低处汇流。每个格子会寻找下游 receiver，并累计 drainage。","当汇流量和坡度都足够高时，stream power 会变大，表示水流有能力切割河道。","切割结果会写入 channel、flow、wear 和 deposition 等调试层，后续可以影响材质显示和调试视图。","最后 thermal diffusion 会让过陡坡面变得更自然，水深和岸线数据则服务于海岸、湿度和海洋混合。");
        Ask="可以把这个模块讲成：先决定水往哪里走，再决定水对地形切多少。"
    },
    @{
        Id="06"; Slug="climate_materials"; Title="温湿度、材质与地表分类"; Subtitle="把高度、纬度、水体和侵蚀信息转成可渲染的地表材质。";
        Code=@("src/PlanetProceduralData.cpp","shaders/terrain.frag"); Images=@("18_flow_06_climate_materials.svg");
        Steps=@("根据 sphereDir 估算纬度","高度影响温度递减","水体、河道和岸线影响湿度","地形坡度影响岩石裸露程度","侵蚀和沉积影响颜色变化","计算 materialWeights","上传到 GPU texture array","terrain shader 混合草地、岩石、雪线和海岸");
        Talk=@("地形高度只决定形状，材质模块负责决定表面看起来是什么。","温度主要和纬度、高度有关；湿度和水体、河道、岸线有关。","坡度较大的地方更容易显示岩石，海拔高且温度低的地方更容易出现雪线，靠近水体的区域会偏湿润。","这些结果最终被打包成材质权重，上传到 GPU。","terrain shader 根据权重混合不同颜色和法线效果，所以同一套 DEM 数据能呈现山地、平原、海岸、雪线等差异。");
        Ask="这个模块适合展示参数如何影响最终视觉：不是只有高度，还包括温度、湿度、坡度和水文。"
    },
    @{
        Id="07"; Slug="terrain_chunks_lod"; Title="地形 Chunk 烘焙与 Baked LOD"; Subtitle="把 DEM 预烘焙为多个块，运行时只绘制可见块。";
        Code=@("src/PlanetProceduralData.cpp","src/PlanetRenderer.cpp","include/PlanetRenderer.h"); Images=@("07_baked_chunk_lod.svg","19_flow_07_terrain_chunks_lod.svg");
        Steps=@("buildTerrainChunks() 划分每个 face","每个 chunk 生成顶点和索引","记录 chunk 中心、半径和误差范围","buildVisibleBakedChunks() 按相机筛选","视锥裁剪剔除不可见块","根据距离选择 LOD","绑定对应 VAO / IBO","drawBakedTerrainPass() 绘制");
        Talk=@("如果把整个星球一次性画完，顶点数量会很大。项目把地形烘焙成 chunk，运行时只处理相机附近或视野内的块。","每个 chunk 有自己的顶点、索引、中心点和包围范围。","渲染前，buildVisibleBakedChunks() 根据相机、视锥和距离挑出当前需要绘制的块。","LOD 的作用是近处保留更多细节，远处减少几何开销。","这部分可以强调为性能优化模块：视觉上仍然是完整星球，但 GPU 每帧只画必要部分。");
        Ask="如果老师问为什么叫 baked，可以解释为：生成阶段已经把网格准备好，渲染阶段主要选择和提交。"
    },
    @{
        Id="08"; Slug="gpu_upload"; Title="CPU 数据到 GPU 的上传"; Subtitle="把生成好的多层数据转成 texture array 和 GPU mesh。";
        Code=@("src/PlanetRenderer.cpp","include/PlanetRenderer.h"); Images=@("06_gpu_data_upload.svg","20_flow_08_gpu_upload.svg");
        Steps=@("setProceduralData() 接收 CPU 数据","检查 face count 和 resolution","创建 height texture array","创建 water / climate / mask texture array","创建材质权重 texture array","上传 baked terrain vertex/index buffers","初始化或刷新海洋、大气相关资源","渲染器标记数据 ready");
        Talk=@("这个模块是 CPU 生成和 GPU 渲染之间的桥。","CPU 数据本质上是多个数组，而 shader 更适合通过纹理采样。因此代码把六个 cube face 打包成 texture array。","高度、水体、温度、湿度、侵蚀、河道和材质权重可以作为不同纹理或不同通道上传。","地形 chunk 的顶点和索引则上传到 VBO、IBO、VAO。","上传完成后，渲染器每帧就不再重新计算地形，只需要采样纹理和绘制 mesh。");
        Ask="重点解释 texture array：六个面作为 layer，shader 根据 face/layer 取对应数据。"
    },
    @{
        Id="09"; Slug="ocean_patch_lod"; Title="海洋 Patch LOD 与球面海面"; Subtitle="在球面上动态选择海洋 patch，并用 tessellation 提升近处细节。";
        Code=@("src/PlanetRenderer.cpp","shaders/ocean.tesc","shaders/ocean.tese","shaders/ocean.frag"); Images=@("21_flow_09_ocean_patch_lod.svg");
        Steps=@("从 6 个 cube face 创建 ocean root patch","collectVisibleOceanPatches() 遍历四叉树","视锥裁剪排除不可见 patch","analyzePatchWaterCoverage() 判断水域比例","shouldSplitNode() 根据距离和误差细分","生成可见 ocean patch 列表","tessellation shader 球面细分","fragment shader 计算水面颜色");
        Talk=@("海洋不是一张固定平面，而是围绕星球的球面 patch。","每个 cube face 有海洋根节点，运行时通过四叉树选择可见区域。","如果 patch 不在视野里，或者水域覆盖率太低，就可以跳过。","近处 patch 会继续细分，远处 patch 保持粗略，从而节省绘制成本。","tessellation shader 负责把 patch 细分并贴合球面，fragment shader 再根据 FFT 波浪、反射、折射和水深计算颜色。");
        Ask="可以把海洋 LOD 和地形 chunk LOD 对比：地形是 baked chunk，海洋是运行时 quadtree patch。"
    },
    @{
        Id="10"; Slug="fft_ocean"; Title="FFT 海浪"; Subtitle="用频域海浪谱生成高度、法线、位移和 folding 纹理。";
        Code=@("src/FFTOcean.cpp","include/FFTOcean.h"); Images=@("08_ocean_fft.svg","22_flow_10_fft_ocean.svg");
        Steps=@("initialize() 配置 cascades 和分辨率","buildInitialSpectrum() 生成 Phillips spectrum","根据 wind、amplitude、gravity 初始化频域","update(time) 推进频域相位","IFFT 转回空间域","计算 height / normal / displacement / folding","uploadTextures() 上传 GPU","ocean shader 采样形成动态波浪");
        Talk=@("FFT 海洋模块负责让水面真正动起来。","它先在频域生成初始海浪谱，参数包括风向、风速、振幅和重力等。","每一帧根据时间更新频域相位，再通过 IFFT 得到空间域波形。","输出不只有高度，还有法线、水平位移和 folding 信息。","这些纹理上传到 GPU 后，海洋 shader 就可以在不同距离采样不同 cascade，形成从近景波浪到远景起伏的连续效果。");
        Ask="讲 FFT 时不用展开数学细节，重点说：频域生成复杂波形，IFFT 转成水面纹理。"
    },
    @{
        Id="11"; Slug="ocean_material"; Title="海洋材质、反射、折射与水深混合"; Subtitle="用反射/折射 FBO、Fresnel 和深浅水颜色混合增强真实感。";
        Code=@("src/PlanetRenderer.cpp","shaders/ocean.frag"); Images=@("09_ocean_reflection_refraction.svg","23_flow_11_ocean_material.svg");
        Steps=@("drawReflectionRefractionPasses() 创建离屏画面","反射 pass 渲染天空和地形反射","折射 pass 渲染水下/透过水面颜色","主海洋 pass 采样 FFT 波浪纹理","根据视角计算 Fresnel","根据水深混合浅水和深水颜色","加入高光、泡沫、SSS 和大气远景","输出最终水面颜色");
        Talk=@("这个模块解释为什么海水看起来不只是蓝色平面。","渲染器先做反射和折射两个离屏 pass，得到水面应该反射什么、透过水面能看到什么。","主海洋 pass 再结合 FFT 波浪法线，计算 Fresnel。视角越贴近水面，反射越强；俯视时折射和水体颜色更明显。","水深也会影响颜色，浅水更亮更偏透明，深水更暗更饱和。","最后再叠加高光、泡沫、次表面散射和大气远景，让海洋融入整个星球画面。");
        Ask="可以强调它是多 pass 渲染：不是一次 shader 直接凭空画海，而是先准备反射/折射纹理。"
    },
    @{
        Id="12"; Slug="atmosphere_lut"; Title="大气散射 LUT 与天空"; Subtitle="预计算大气散射查找表，运行时快速渲染天空和远景。";
        Code=@("src/PlanetRenderer.cpp","shaders/atmosphere*.frag","shaders/atmosphere*.vert"); Images=@("24_flow_12_atmosphere_lut.svg");
        Steps=@("根据大气参数计算 LUT signature","参数变化时触发 precomputeAtmosphereLuts()","计算 transmittance LUT","计算 irradiance LUT","计算 scattering LUT","运行时 atmosphere.frag 采样 LUT","根据太阳方向、视线和高度计算天空颜色","与地形、海洋和云层合成");
        Talk=@("大气散射如果每个像素都完整积分会很贵，所以项目采用 LUT 预计算。","当大气参数变化时，渲染器生成一个 signature，判断 LUT 是否需要重算。","预计算阶段会得到透射率、辐照度和散射相关纹理。","运行时 shader 通过查表快速得到天空颜色、地平线雾化和远景衰减。","这个模块的价值是把复杂物理近似前置，保证实时帧率。");
        Ask="讲解时可以把 LUT 说成提前算好的表：用一点预处理换每帧渲染速度。"
    },
    @{
        Id="13"; Slug="procedural_clouds"; Title="程序化体积云"; Subtitle="在大气 shader 中用噪声和 raymarch 生成可调体积云。";
        Code=@("shaders/atmosphere.frag","src/main.cpp"); Images=@("10_atmosphere_clouds.svg","25_flow_13_procedural_clouds.svg");
        Steps=@("UI 控制 clouds 开关和参数","传入 coverage、density、height、thickness","在 atmosphere.frag 中构造云层区域","沿视线进行 raymarch","多层噪声计算云密度","light march 估算光照和阴影","与天空散射颜色混合","输出带体积感的云层");
        Talk=@("云层属于视觉增强模块，但它和大气放在一起很合理，因为云需要天空颜色和太阳方向。","UI 提供覆盖率、锐度、缩放、高度、厚度、密度、步数等参数。","shader 在大气层中定义一段云层高度范围，然后沿相机视线采样多个点。","每个点通过噪声得到云密度，再沿太阳方向做简化 light march，估算云内部受光程度。","最后把云颜色和原本天空散射结果混合，得到可调的程序化体积云。");
        Ask="可以补一句：步数越高越细腻，但开销也越大，所以 UI 里保留了质量参数。"
    },
    @{
        Id="14"; Slug="debug_visualization"; Title="调试、可视化与性能面板"; Subtitle="用 ImGui 暴露参数和调试视图，帮助解释生成结果。";
        Code=@("src/main.cpp","src/PlanetRenderer.cpp"); Images=@("12_debug_and_presentation.svg","26_flow_14_debug_visualization.svg");
        Steps=@("Render panel 展示地形、海洋、大气、云参数","用户切换 render mode / debug overlay","renderer 更新 shader uniform","显示 hydrology、erosion、material 等调试层","Performance panel 统计帧时间和可见块","Ocean / cloud / baked chunk 参数实时反馈","Feature overlay 代码存在","当前初次 DEM 路径和主 render 还未完整接通 overlay");
        Talk=@("这个模块适合答辩现场展示，因为它能证明项目不是只跑一次结果，而是可以调参数、看中间层。","Render panel 里可以控制地形表面、水文调试、海洋、大气、云和相机质量参数。","调试视图能显示河道、侵蚀、材质、LOD 等信息，用来解释为什么某个区域会形成山脉、河流或海岸。","Performance panel 则显示帧时间、可见 chunk、海洋 patch 和云质量等数据。","需要诚实说明：特征线 overlay 的构建代码存在，但当前初次 DEM 生成路径会跳过，主 render 也没有完整调用；cache 加载路径可以重建部分数据。");
        Ask="这一页可以主动展示工程诚实性：哪些功能已实装，哪些是代码支持但当前路径未完全启用。"
    },
    @{
        Id="15"; Slug="input_camera"; Title="输入与相机控制"; Subtitle="用键鼠控制轨道、移动和视角，使星球可交互观察。";
        Code=@("src/main.cpp","include/PlanetRenderer.h"); Images=@("27_flow_15_input_camera.svg");
        Steps=@("GLFW 捕获键盘和鼠标事件","main.cpp 更新 camera state","鼠标控制视角或轨道","键盘控制移动、缩放和模式","根据 delta time 平滑更新位置","计算 view / projection matrix","传入 PlanetRenderer","各 shader 使用相机位置和矩阵渲染");
        Talk=@("实时渲染项目必须能交互观察，否则很难展示星球尺度和细节。","输入模块主要由 GLFW 回调和 main loop 中的状态更新组成。","键盘和鼠标改变相机位置、方向或轨道参数，代码再根据 delta time 做平滑更新。","最终得到 view 和 projection matrix，传给 PlanetRenderer 和 shader。","相机位置还会影响地形 LOD、海洋 patch LOD、大气视角和 Fresnel 效果，所以它不仅是观察工具，也参与渲染决策。");
        Ask="可以把相机讲成渲染系统的输入之一：它决定看哪里，也决定哪些资源需要画。"
    },
    @{
        Id="16"; Slug="shader_resources"; Title="Shader 编译与资源管理"; Subtitle="集中加载 shader、材质资源和 GPU 对象，失败时给出日志。";
        Code=@("src/PlanetRenderer.cpp","shaders/","xmake.lua"); Images=@("28_flow_16_shader_resources.svg");
        Steps=@("xmake 构建时复制 shaders 和 assets","PlanetRenderer::initialize() 加载 shader 文件","编译 vertex / fragment / tessellation shader","链接 program 并检查错误","创建 VAO / VBO / texture / FBO","运行时绑定对应 program","设置 uniform 和 texture slot","程序退出时释放 GPU 资源");
        Talk=@("这个模块偏工程基础，但很重要，因为所有视觉效果都依赖 shader 和 GPU 资源正确加载。","xmake.lua 会把 shaders 和 assets 复制到输出目录，避免运行时找不到资源。","PlanetRenderer 初始化时编译并链接多个 shader program，包括地形、海洋、大气和调试 pass。","创建纹理、缓冲、FBO 等资源后，每个渲染 pass 会绑定对应 program 和 uniform。","如果 shader 编译失败，日志可以帮助定位是哪一行 GLSL 出问题。");
        Ask="这页不用讲太久，但能说明项目不是只写算法，也处理了实际图形程序的资源生命周期。"
    },
    @{
        Id="17"; Slug="height_diagnostics"; Title="高度诊断工具"; Subtitle="独立命令行工具用于检查高度范围和生成质量。";
        Code=@("tools/TerrainHeightDiagnostics.cpp","xmake.lua"); Images=@("29_flow_17_height_diagnostics.svg");
        Steps=@("运行 TerrainHeightDiagnostics","构造 PlanetProceduralSettings","调用 PlanetProceduralData::generate()","遍历 6 个 face 的 height 数据","统计 min / max / average","检查水体和陆地比例","输出诊断结果","辅助调整生成参数");
        Talk=@("高度诊断工具是辅助开发用的，不需要打开完整渲染窗口。","它直接生成一份程序化数据，然后统计高度范围、平均值、水陆比例等信息。","这样可以快速发现生成参数是否异常，比如高度过平、海洋过多或陆地过少。","对答辩来说，这说明项目有验证工具，不完全依赖肉眼看画面。","如果后续继续扩展，也可以把更多指标加入这个工具，比如河道数量、侵蚀强度分布和材质覆盖率。");
        Ask="这一页可以作为工程完整性的补充：有主程序，也有独立诊断工具。"
    },
    @{
        Id="18"; Slug="frame_render_sequence"; Title="每帧渲染顺序"; Subtitle="把一次 frame 拆成反射折射、地形、海洋、大气和 UI。";
        Code=@("src/PlanetRenderer.cpp","src/main.cpp"); Images=@("11_frame_render_sequence.svg","30_flow_18_frame_render_sequence.svg");
        Steps=@("主循环处理输入和时间","更新相机矩阵和 renderer 参数","构建可见地形 chunk","构建可见 ocean patch","绘制 reflection/refraction FBO","绘制主场景地形","绘制海洋和大气云层","绘制 ImGui UI 并交换缓冲");
        Talk=@("这页可以作为前面所有模块的汇总，说明它们在一帧里如何协作。","每帧开始先处理输入和时间，更新相机。","渲染器根据相机选择可见地形 chunk 和海洋 patch。","海洋需要反射和折射，所以主画面前会先做离屏 pass。","之后绘制地形、海洋、大气和云层，最后绘制 ImGui。","这样讲可以让老师看到：生成模块负责准备数据，渲染模块负责按顺序把数据组织成最终画面。");
        Ask="这页适合放在答辩中后段，用来把前面的模块重新串起来。"
    },
    @{
        Id="19"; Slug="presentation_summary"; Title="答辩总结与边界说明"; Subtitle="总结项目亮点，同时避免把未完全接通的功能说过头。";
        Code=@("PROJECT_MODULE_FLOWCHARTS_CN.md","PROJECT_GRADING_GUIDE_CN.md"); Images=@("31_flow_19_presentation_summary.svg");
        Steps=@("先讲项目目标：实时程序化星球","再讲 CPU DEM 生成","说明 cube-sphere 和接缝处理","讲水文、侵蚀、气候和材质","讲 chunk LOD 与 GPU 上传","讲海洋、FFT、大气和云","展示调试 UI 和性能数据","最后说明已完成内容和当前限制");
        Talk=@("最后总结时，可以把项目概括为一个完整的实时程序化星球渲染系统。","它的主线是 CPU 生成 DEM 和多种环境数据，GPU 通过 texture array、chunk LOD、FFT 海洋、大气 LUT 和体积云实时渲染。","答辩时建议先讲数据怎么生成，再讲数据怎么上传，最后讲每帧怎么渲染。","同时要注意边界：当前真实生成路径是 generateDemPrototype；特征线 overlay 代码存在，但初次 DEM 路径和主 render 还没有完全接通。","这样的讲法既能展示工作量，也比较可信。");
        Ask="一分钟版：CPU 程序化生成星球数据，GPU 用分块 LOD、海洋、大气和云实时渲染，并通过 ImGui 提供调试和参数控制。"
    }
)

$ModulesEn = @(
    @{
        Id="01"; Slug="project_overview"; Title="Project Overview"; Subtitle="The complete path from application startup and CPU procedural generation to real-time GPU rendering.";
        Code=@("src/main.cpp","src/PlanetRenderer.cpp","src/PlanetProceduralData.cpp","xmake.lua"); Images=@("01_overall_pipeline.svg","13_flow_01_project_overview.svg");
        Steps=@("Start main() and create the OpenGL window","Initialize ImGui and PlanetRenderer","Load shaders, meshes, ocean and atmosphere resources","Try to restore session/cache data","Without cache, enter the procedural setup screen","CPU generates DEM, climate, hydrology and material data","Main thread uploads texture arrays and baked meshes","Render terrain, ocean, atmosphere, clouds and UI every frame");
        Talk=@("This slide introduces the main goal: the project is not a static model, but a real-time procedural planet renderer.","The work is separated into two layers. The CPU generates terrain height, hydrology, erosion, temperature, moisture and material masks. The GPU turns those datasets into the final real-time image.","The program initializes the window, OpenGL, ImGui and the renderer, then tries to restore local cached data. If no cache is available, the user edits parameters and starts planet generation.","Once generation is finished, OpenGL resource upload happens on the main thread, because textures and buffers depend on the active OpenGL context.","The render loop then culls visible regions and draws terrain, ocean, atmosphere, volumetric clouds and debug UI.");
        Ask="If asked about the core contribution, explain that the project integrates procedural DEM terrain, chunk LOD, FFT ocean, atmosphere scattering and an interactive debug UI into one renderer."
    },
    @{
        Id="02"; Slug="app_state_generation_thread"; Title="Application States and Generation Thread"; Subtitle="Three stages separate parameter editing, background generation and real-time rendering.";
        Code=@("src/main.cpp"); Images=@("02_application_workflow.svg","14_flow_02_app_state_generation_thread.svg");
        Steps=@("ProceduralSetup shows the parameter UI","User clicks Generate Planet","startPlanetGeneration() copies current settings","std::async generates PlanetProceduralData in the background","progressCallback updates atomic progress values","The main loop keeps refreshing the progress bar","When the future is ready, finishPlanetGeneration() runs","setProceduralData() uploads GPU data and enters Render");
        Talk=@("This module shows the engineering structure of the application. Planet generation can take time, so the main thread does not block directly.","The background thread only computes CPU-side data such as height fields, hydrology and material weights. It does not create OpenGL objects.","The main loop reads the future status and atomic progress values to keep the UI responsive.","When generation is done, finishPlanetGeneration() hands the result to PlanetRenderer, and the main thread creates textures, buffers and vertex arrays.","This design avoids OpenGL threading problems and keeps the program states easy to explain.");
        Ask="The key point is that threading is used to keep the UI responsive and to keep all OpenGL resource creation on the main thread."
    },
    @{
        Id="03"; Slug="cube_sphere_mapping"; Title="Cube-Sphere Mapping and Seam Handling"; Subtitle="The planet is stored as six square faces and mapped onto a sphere.";
        Code=@("src/PlanetProceduralData.cpp","include/PlanetProceduralData.h"); Images=@("03_cube_sphere_mapping.svg","15_flow_03_cube_sphere_mapping.svg");
        Steps=@("Split the planet into six cube faces","Each face uses a regular 2D texel grid","Convert face UV to cube coordinates","Normalize through cubeSphereDirection()","Sample noise and climate functions by direction","Find cross-face neighbors with neighborCell()","Blend borders with fixCubeFaceSeams()","Output a continuous spherical height field");
        Talk=@("Generating a regular grid directly on a sphere is difficult, so this project uses a cube-sphere representation.","Each face is a normal 2D array. A texel is first positioned on a cube face, then normalized into a direction on the sphere.","Noise, climate and hydrology calculations use that spherical direction.","The main challenge is seam handling. Independent faces can produce visible cracks or height discontinuities along borders.","neighborCell() and fixCubeFaceSeams() blend the borders so the final planet behaves like one continuous surface.");
        Ask="If asked why not use a UV sphere, answer that cube-sphere sampling is more uniform and works naturally with texture arrays and chunks."
    },
    @{
        Id="04"; Slug="dem_generation"; Title="DEM Terrain Generation"; Subtitle="The current runtime path enters generateDemPrototype() to produce height, hydrology and surface attributes.";
        Code=@("src/PlanetProceduralData.cpp"); Images=@("04_dem_generation.svg","16_flow_04_dem_generation.svg");
        Steps=@("PlanetProceduralData::generate()","Clamp faceResolution","Enter generateDemPrototype()","Allocate height, water, erosion and climate arrays for six faces","Iterate over every texel and compute sphereDir","Use fBM / ridgedFbm to form continents and mountains","Store height, uplift, landMask and preErosionHeight","Run seam blending, hydrology erosion and climate passes","Bake terrain chunks with buildTerrainChunks()");
        Talk=@("This slide should make the real code path clear. PlanetProceduralData::generate() enters generateDemPrototype() and returns, so this is the active generation path.","DEM means digital elevation model. The code allocates several arrays for each of the six cube faces, including height, water depth, erosion, temperature, moisture, channels and material weights.","Each texel samples layered noise through its spherical direction. Low-frequency noise forms continents, while mid and high frequencies shape mountains and ridges.","After the base terrain is generated, the code performs seam correction, hydrology erosion and climate computation.","Finally buildTerrainChunks() converts the DEM into renderable terrain chunks.");
        Ask="Do not overstate the retained legacy generation code after the early return; the active path is the DEM prototype path."
    },
    @{
        Id="05"; Slug="hydrology_erosion_masks"; Title="Hydrology, Erosion and River Masks"; Subtitle="Downhill flow, stream power and thermal diffusion create channels and erosion traces.";
        Code=@("src/PlanetProceduralData.cpp"); Images=@("05_hydrology_erosion_masks.svg","17_flow_05_hydrology_erosion_masks.svg");
        Steps=@("Sort texels by height","Find a downstream receiver for each texel","Accumulate drainage / flow","Compute streamPower from slope and drainage","Carve channels into the terrain","Write channel, wear, deposition and flow maps","Smooth steep slopes with thermal diffusion","Compute water depth, shorelines and moisture effects");
        Talk=@("This module makes the terrain look shaped by water instead of being pure noise.","The code sorts texels by height and sends water from higher cells to lower receivers.","Drainage accumulates downstream. When both drainage and slope are high, stream power becomes strong enough to cut channels.","The result is written into channel, flow, wear and deposition maps, which can be inspected in debug views.","Thermal diffusion smooths overly steep slopes, while water depth and shoreline data support coastal and moisture effects.");
        Ask="A simple way to explain it is: first decide where water flows, then decide how strongly water cuts the terrain."
    },
    @{
        Id="06"; Slug="climate_materials"; Title="Climate, Materials and Surface Classification"; Subtitle="Height, latitude, water and erosion data become renderable surface materials.";
        Code=@("src/PlanetProceduralData.cpp","shaders/terrain.frag"); Images=@("18_flow_06_climate_materials.svg");
        Steps=@("Estimate latitude from sphereDir","Reduce temperature by elevation","Increase moisture near water, rivers and coasts","Use slope to expose rock","Use erosion and deposition for color variation","Compute materialWeights","Upload data as GPU texture arrays","Blend grass, rock, snow and coast in terrain shader");
        Talk=@("The height field defines shape, while this module decides what the surface looks like.","Temperature depends mainly on latitude and elevation. Moisture depends on water bodies, rivers and coastlines.","Steep slopes expose more rock. High and cold areas can become snowy. Wet areas near water can look different from dry terrain.","These values are packed into material weights and uploaded to the GPU.","The terrain shader blends the final surface colors based on those weights.");
        Ask="This is a good place to explain that the final look is controlled by more than height: climate, slope and hydrology all matter."
    },
    @{
        Id="07"; Slug="terrain_chunks_lod"; Title="Terrain Chunk Baking and Baked LOD"; Subtitle="The DEM is baked into chunks so the renderer only draws visible pieces.";
        Code=@("src/PlanetProceduralData.cpp","src/PlanetRenderer.cpp","include/PlanetRenderer.h"); Images=@("07_baked_chunk_lod.svg","19_flow_07_terrain_chunks_lod.svg");
        Steps=@("buildTerrainChunks() divides every face","Generate vertices and indices for each chunk","Store chunk center, radius and error range","buildVisibleBakedChunks() filters by camera","Frustum culling removes invisible chunks","Distance selects the LOD level","Bind the matching VAO / IBO","drawBakedTerrainPass() renders terrain");
        Talk=@("Drawing the whole planet at full detail would be expensive, so the terrain is baked into chunks.","Each chunk stores its own vertices, indices, center and bounding information.","Before rendering, buildVisibleBakedChunks() selects chunks based on the camera, frustum and distance.","LOD keeps more geometry close to the camera and less geometry far away.","This is the key performance module for terrain rendering.");
        Ask="Baked means the mesh is prepared during generation; the render stage mainly selects and submits it."
    },
    @{
        Id="08"; Slug="gpu_upload"; Title="CPU-to-GPU Data Upload"; Subtitle="Generated arrays become texture arrays and GPU meshes.";
        Code=@("src/PlanetRenderer.cpp","include/PlanetRenderer.h"); Images=@("06_gpu_data_upload.svg","20_flow_08_gpu_upload.svg");
        Steps=@("setProceduralData() receives CPU data","Validate face count and resolution","Create height texture array","Create water / climate / mask texture arrays","Create material weight texture array","Upload baked terrain vertex/index buffers","Refresh ocean and atmosphere resources if needed","Mark renderer data as ready");
        Talk=@("This module connects CPU generation to GPU rendering.","The generated data is mostly arrays, while shaders prefer texture sampling. The six cube faces are therefore uploaded as layers of texture arrays.","Height, water, climate, erosion, river and material data can be stored in separate textures or channels.","Terrain vertices and indices are uploaded into VBOs, IBOs and VAOs.","After this upload, rendering no longer recalculates the terrain; it samples GPU data and draws meshes.");
        Ask="Emphasize texture arrays: each cube face is a layer, and shaders sample the correct layer."
    },
    @{
        Id="09"; Slug="ocean_patch_lod"; Title="Ocean Patch LOD on a Sphere"; Subtitle="Visible ocean patches are selected dynamically and refined with tessellation.";
        Code=@("src/PlanetRenderer.cpp","shaders/ocean.tesc","shaders/ocean.tese","shaders/ocean.frag"); Images=@("21_flow_09_ocean_patch_lod.svg");
        Steps=@("Create ocean root patches for six cube faces","Traverse quadtree with collectVisibleOceanPatches()","Cull patches outside the frustum","Analyze water coverage per patch","Split nodes by distance and error","Build the visible ocean patch list","Tessellation shader refines patches on the sphere","Fragment shader computes water color");
        Talk=@("The ocean is not a fixed flat plane. It is a set of spherical patches around the planet.","Each cube face owns root ocean patches, and a quadtree selects visible regions at runtime.","Patches outside the view or with too little water coverage can be skipped.","Near patches are subdivided, while distant ones stay coarse.","The tessellation shader refines the geometry on the sphere, and the fragment shader handles the final water appearance.");
        Ask="Compare it with terrain LOD: terrain uses baked chunks, while the ocean uses runtime quadtree patches."
    },
    @{
        Id="10"; Slug="fft_ocean"; Title="FFT Ocean Waves"; Subtitle="Frequency-domain wave spectra generate height, normal, displacement and folding textures.";
        Code=@("src/FFTOcean.cpp","include/FFTOcean.h"); Images=@("08_ocean_fft.svg","22_flow_10_fft_ocean.svg");
        Steps=@("initialize() sets cascades and resolution","buildInitialSpectrum() creates a Phillips spectrum","Initialize frequency data from wind, amplitude and gravity","update(time) advances spectral phases","IFFT converts data to spatial wave fields","Compute height, normal, displacement and folding","uploadTextures() sends results to GPU","Ocean shader samples textures for animated waves");
        Talk=@("The FFT ocean module makes the water surface move.","It first builds a wave spectrum in the frequency domain using wind, amplitude and gravity parameters.","Every frame, the frequency phases advance with time. An inverse FFT converts the result back into spatial wave textures.","The output includes height, normals, horizontal displacement and folding information.","The ocean shader samples these textures across multiple cascades to produce continuous waves at different scales.");
        Ask="You do not need to derive the math; explain that FFT lets complex wave patterns be generated efficiently."
    },
    @{
        Id="11"; Slug="ocean_material"; Title="Ocean Material, Reflection and Refraction"; Subtitle="FBO passes, Fresnel and depth blending produce richer water shading.";
        Code=@("src/PlanetRenderer.cpp","shaders/ocean.frag"); Images=@("09_ocean_reflection_refraction.svg","23_flow_11_ocean_material.svg");
        Steps=@("drawReflectionRefractionPasses() prepares offscreen images","Reflection pass renders reflected sky and terrain","Refraction pass renders what is seen through water","Main ocean pass samples FFT wave textures","Fresnel changes reflection by view angle","Water depth blends shallow and deep colors","Add highlights, foam, SSS and aerial perspective","Output the final ocean color");
        Talk=@("This module explains why the ocean looks richer than a blue surface.","The renderer first creates reflection and refraction images in offscreen framebuffers.","The main ocean pass combines those images with FFT wave normals and Fresnel.","At grazing angles, reflection becomes stronger. Looking down into the water makes refraction and water color more visible.","Water depth controls the shallow-to-deep color blend, and the shader adds highlights, foam, subsurface scattering and atmospheric distance effects.");
        Ask="The important point is that ocean shading is multi-pass, not a single flat color."
    },
    @{
        Id="12"; Slug="atmosphere_lut"; Title="Atmosphere Scattering LUTs"; Subtitle="Precomputed lookup tables make sky and aerial perspective fast at runtime.";
        Code=@("src/PlanetRenderer.cpp","shaders/atmosphere*.frag","shaders/atmosphere*.vert"); Images=@("24_flow_12_atmosphere_lut.svg");
        Steps=@("Build a LUT signature from atmosphere parameters","Recompute LUTs when parameters change","Compute transmittance LUT","Compute irradiance LUT","Compute scattering LUT","atmosphere.frag samples LUTs at runtime","Use sun direction, view ray and altitude","Composite sky with terrain, ocean and clouds");
        Talk=@("Atmospheric scattering is expensive if fully integrated per pixel, so the project uses precomputed lookup tables.","When atmosphere parameters change, the renderer checks a signature and recomputes the LUTs if needed.","The LUTs store transmittance, irradiance and scattering information.","At runtime, atmosphere.frag samples these tables to produce sky color, horizon haze and distance fading.","This trades some preprocessing for much faster frame rendering.");
        Ask="Explain LUTs as precomputed tables: calculate once, sample many times."
    },
    @{
        Id="13"; Slug="procedural_clouds"; Title="Procedural Volumetric Clouds"; Subtitle="Noise and raymarching inside the atmosphere shader create adjustable cloud layers.";
        Code=@("shaders/atmosphere.frag","src/main.cpp"); Images=@("10_atmosphere_clouds.svg","25_flow_13_procedural_clouds.svg");
        Steps=@("UI controls cloud parameters","Pass coverage, density, height and thickness","Define a cloud layer in atmosphere.frag","Raymarch along the view ray","Sample layered noise for density","Light march approximates illumination and shadowing","Blend cloud color with sky scattering","Output volumetric-looking clouds");
        Talk=@("Clouds are a visual module, but they belong naturally with atmosphere because they use sky color and sun direction.","The UI exposes coverage, sharpness, scale, height, thickness, density and step counts.","The shader defines a cloud layer in the atmosphere and samples points along the camera ray.","Noise determines cloud density, and a simplified light march estimates how much sunlight reaches each point.","The final cloud color is blended with the sky scattering result.");
        Ask="Mention the quality tradeoff: more raymarch steps give better clouds but cost more performance."
    },
    @{
        Id="14"; Slug="debug_visualization"; Title="Debug Views and Performance UI"; Subtitle="ImGui exposes parameters, visualization modes and performance counters.";
        Code=@("src/main.cpp","src/PlanetRenderer.cpp"); Images=@("12_debug_and_presentation.svg","26_flow_14_debug_visualization.svg");
        Steps=@("Render panel exposes terrain, ocean, atmosphere and cloud parameters","User switches render modes or debug overlays","Renderer updates shader uniforms","Hydrology, erosion and material layers can be visualized","Performance panel shows frame timing and visible regions","Ocean, cloud and baked chunk stats give feedback","Feature overlay code exists","Initial DEM path and main render do not fully connect overlay yet");
        Talk=@("This module is useful during a live presentation because it exposes intermediate data, not just the final image.","The render panel controls terrain, hydrology debug, ocean, atmosphere, clouds and camera quality settings.","Debug views can show rivers, erosion, material information and LOD behavior.","The performance panel reports frame time, visible chunks, ocean patches and cloud settings.","Be accurate about feature overlays: the code exists, but the initial DEM generation path and current main render path do not fully enable it.");
        Ask="This is a good slide to show engineering honesty: implemented systems, debug support and current limitations."
    },
    @{
        Id="15"; Slug="input_camera"; Title="Input and Camera Control"; Subtitle="Keyboard and mouse input make the planet interactively inspectable.";
        Code=@("src/main.cpp","include/PlanetRenderer.h"); Images=@("27_flow_15_input_camera.svg");
        Steps=@("GLFW captures keyboard and mouse events","main.cpp updates camera state","Mouse controls view or orbit","Keyboard controls movement, zoom and modes","Delta time smooths motion","Compute view and projection matrices","Pass camera data to PlanetRenderer","Shaders use camera position and matrices");
        Talk=@("A real-time renderer needs interactive inspection so the viewer can understand both planetary scale and local detail.","GLFW callbacks and the main loop update the camera state.","Keyboard and mouse input change position, direction, orbit and zoom values. Delta time keeps motion consistent.","The final view and projection matrices are passed into PlanetRenderer and then to shaders.","Camera position also affects terrain LOD, ocean patch LOD, atmospheric view angle and Fresnel.");
        Ask="The camera is both a viewing tool and an input to rendering decisions."
    },
    @{
        Id="16"; Slug="shader_resources"; Title="Shader Compilation and Resource Management"; Subtitle="Shaders, GPU resources and render passes are initialized and managed centrally.";
        Code=@("src/PlanetRenderer.cpp","shaders/","xmake.lua"); Images=@("28_flow_16_shader_resources.svg");
        Steps=@("xmake copies shaders and assets","PlanetRenderer::initialize() loads shader files","Compile vertex, fragment and tessellation shaders","Link programs and check errors","Create VAO, VBO, textures and FBOs","Bind the required program for each pass","Set uniforms and texture slots","Release GPU resources on shutdown");
        Talk=@("This module is basic engineering, but it is necessary for every visual system in the project.","xmake copies shader and asset folders into the build output so runtime paths work.","PlanetRenderer compiles and links shader programs for terrain, ocean, atmosphere and debug passes.","It also creates textures, buffers, vertex arrays and framebuffers.","Each render pass binds its own program, textures and uniform values. Compilation logs help locate GLSL errors.");
        Ask="This slide shows that the project handles real graphics-program resource lifecycle, not just algorithms."
    },
    @{
        Id="17"; Slug="height_diagnostics"; Title="Height Diagnostics Tool"; Subtitle="A standalone tool checks generated height ranges and planet statistics.";
        Code=@("tools/TerrainHeightDiagnostics.cpp","xmake.lua"); Images=@("29_flow_17_height_diagnostics.svg");
        Steps=@("Run TerrainHeightDiagnostics","Create PlanetProceduralSettings","Call PlanetProceduralData::generate()","Iterate over height data for six faces","Compute min, max and average height","Check water and land ratios","Print diagnostic results","Use results to tune generation parameters");
        Talk=@("The diagnostics tool supports development without opening the full render window.","It generates procedural data and measures height ranges, average values and water-land ratios.","This helps detect abnormal settings, such as terrain that is too flat or oceans that cover too much of the planet.","For presentation, it shows that the project has a validation path beyond visual inspection.","Future metrics could include river counts, erosion distribution and material coverage.");
        Ask="Use this as an engineering completeness point: there is a main renderer and a separate diagnostic tool."
    },
    @{
        Id="18"; Slug="frame_render_sequence"; Title="Per-Frame Render Sequence"; Subtitle="A frame is assembled from culling, offscreen passes, terrain, ocean, atmosphere and UI.";
        Code=@("src/PlanetRenderer.cpp","src/main.cpp"); Images=@("11_frame_render_sequence.svg","30_flow_18_frame_render_sequence.svg");
        Steps=@("Main loop processes input and time","Update camera matrices and renderer settings","Build visible terrain chunks","Build visible ocean patches","Render reflection/refraction FBOs","Draw main terrain scene","Draw ocean, atmosphere and clouds","Draw ImGui and swap buffers");
        Talk=@("This slide ties all modules together by showing what happens in one frame.","The frame begins with input and time updates, then the camera matrices are refreshed.","The renderer selects visible terrain chunks and ocean patches based on the camera.","Ocean reflection and refraction are prepared with offscreen passes before the main scene.","Then terrain, ocean, atmosphere and clouds are drawn, followed by the ImGui interface.","This makes the distinction clear: generation prepares data, rendering organizes that data into the final image every frame.");
        Ask="Use this slide in the middle or near the end to reconnect the individual modules."
    },
    @{
        Id="19"; Slug="presentation_summary"; Title="Presentation Summary and Boundaries"; Subtitle="Summarize the strengths while clearly stating the current limitations.";
        Code=@("PROJECT_MODULE_FLOWCHARTS_CN.md","PROJECT_GRADING_GUIDE_CN.md"); Images=@("31_flow_19_presentation_summary.svg");
        Steps=@("Start with the goal: real-time procedural planet","Explain CPU DEM generation","Explain cube-sphere mapping and seam handling","Explain hydrology, erosion, climate and materials","Explain chunk LOD and GPU upload","Explain ocean, FFT, atmosphere and clouds","Show debug UI and performance data","End with completed work and current limits");
        Talk=@("The final summary can describe the project as a complete real-time procedural planet rendering system.","The main path is CPU generation of DEM and environmental data, followed by GPU rendering with texture arrays, chunk LOD, FFT ocean, atmosphere LUTs and volumetric clouds.","A good presentation order is: how data is generated, how it is uploaded, and how each frame is rendered.","Also be honest about limits: the active generation path is generateDemPrototype, and the feature overlay code is not fully connected in the initial DEM render path.","This framing shows both the amount of work and the credibility of the explanation.");
        Ask="One-minute version: the CPU generates planet data, the GPU renders it with LOD terrain, ocean, atmosphere and clouds, and ImGui provides debug and parameter control."
    }
)

$ConceptEn = @(
    @{ Name="01_overall_pipeline.svg"; Title="ProceduralWorld Overall Architecture"; Subtitle="CPU-side procedural data generation feeds GPU-side real-time rendering."; Columns=@(@("CPU Generation","Parameters / App State","DEM Planet Data"),@("GPU Resources","Texture Arrays","Baked Terrain Mesh"),@("Frame Rendering","Terrain","Ocean","Atmosphere + Clouds","Debug UI")) },
    @{ Name="02_application_workflow.svg"; Title="Application Workflow"; Subtitle="The app moves from setup to background generation, then into real-time render."; Columns=@(@("Setup","Edit procedural parameters","Click Generate Planet"),@("Generation","std::async background task","Progress callback updates UI"),@("Render","Main thread uploads GPU data","Render loop starts")) },
    @{ Name="03_cube_sphere_mapping.svg"; Title="Cube-Sphere Mapping"; Subtitle="Six square faces become one continuous sphere."; Columns=@(@("Face Data","2D texel grid","Texture-array layer"),@("Mapping","Face UV to cube position","Normalize to sphere direction"),@("Continuity","Cross-face neighbors","Seam blending")) },
    @{ Name="04_dem_generation.svg"; Title="DEM Generation"; Subtitle="Layered noise and post-processing create the digital elevation model."; Columns=@(@("Base Terrain","Continents","Mountains","Ridges"),@("Post Passes","Seam fix","Hydrology","Erosion"),@("Outputs","Height","Water","Climate","Masks")) },
    @{ Name="05_hydrology_erosion_masks.svg"; Title="Hydrology and Erosion Masks"; Subtitle="Flow accumulation and stream power shape rivers and erosion layers."; Columns=@(@("Flow","Downstream receiver","Drainage accumulation"),@("Carving","Slope","Stream power","Channel mask"),@("Debug Data","Wear","Deposition","Flow map")) },
    @{ Name="06_gpu_data_upload.svg"; Title="CPU-to-GPU Upload"; Subtitle="Generated arrays are packed into texture arrays and mesh buffers."; Columns=@(@("CPU Data","Six faces","Height / masks / weights"),@("GPU Packing","Texture arrays","VBO / IBO / VAO"),@("Shader Access","Layer sampling","Material blending")) },
    @{ Name="07_baked_chunk_lod.svg"; Title="Baked Terrain Chunk LOD"; Subtitle="Prebuilt chunks are culled and selected by distance each frame."; Columns=@(@("Bake","Split faces into chunks","Store bounds and indices"),@("Cull","Camera frustum","Distance range"),@("Draw","Visible chunks","LOD level")) },
    @{ Name="08_ocean_fft.svg"; Title="FFT Ocean"; Subtitle="Frequency-domain waves become animated ocean textures."; Columns=@(@("Spectrum","Wind","Amplitude","Phillips model"),@("Simulation","Phase update","Inverse FFT"),@("Textures","Height","Normal","Displacement")) },
    @{ Name="09_ocean_reflection_refraction.svg"; Title="Ocean Reflection and Refraction"; Subtitle="Offscreen passes and Fresnel mixing give the water richer shading."; Columns=@(@("FBO Passes","Reflection","Refraction"),@("Main Water","FFT normals","Depth color","Fresnel"),@("Final","Highlights","Foam","Atmospheric fade")) },
    @{ Name="10_atmosphere_clouds.svg"; Title="Atmosphere and Volumetric Clouds"; Subtitle="Scattering LUTs combine with cloud raymarching."; Columns=@(@("Atmosphere","Transmittance LUT","Scattering LUT"),@("Clouds","Coverage","Density","Raymarch steps"),@("Composite","Sky","Horizon haze","Cloud lighting")) },
    @{ Name="11_frame_render_sequence.svg"; Title="Frame Render Sequence"; Subtitle="One frame combines culling, offscreen passes and final compositing."; Columns=@(@("Prepare","Input","Camera","Visibility"),@("Scene","Reflection / refraction","Terrain","Ocean"),@("Finish","Atmosphere","Clouds","ImGui")) },
    @{ Name="12_debug_and_presentation.svg"; Title="Debug and Presentation Support"; Subtitle="UI panels expose parameters, visual layers and performance counters."; Columns=@(@("Controls","Terrain","Ocean","Atmosphere","Clouds"),@("Debug Views","Hydrology","Erosion","Materials","LOD"),@("Presentation","Show cause and effect","Explain current limits")) }
)

New-Item -ItemType Directory -Force -Path (Join-Path $Root "images") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Root "images_en") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Root "pre") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Root "pre_en") | Out-Null

for ($i = 0; $i -lt $ModulesCn.Count; $i++) {
    $m = $ModulesCn[$i]
    $num = "{0:D2}" -f (13 + $i)
    Write-Utf8File "images\$num`_flow_$($m.Id)_$($m.Slug).svg" (New-FlowSvg $m "cn")
    Write-Utf8File "pre\$($m.Id)_$($m.Slug).md" (New-ScriptMarkdown $m "cn")
}

foreach ($diagram in $ConceptEn) {
    Write-Utf8File "images_en\$($diagram.Name)" (New-ConceptSvg $diagram)
}

for ($i = 0; $i -lt $ModulesEn.Count; $i++) {
    $m = $ModulesEn[$i]
    $num = "{0:D2}" -f (13 + $i)
    Write-Utf8File "images_en\$num`_flow_$($m.Id)_$($m.Slug).svg" (New-FlowSvg $m "en")
    Write-Utf8File "pre_en\$($m.Id)_$($m.Slug).md" (New-ScriptMarkdown $m "en")
}

$cnConcept = @(
    "- 01_overall_pipeline.svg：项目总架构",
    "- 02_application_workflow.svg：应用状态与生成线程",
    "- 03_cube_sphere_mapping.svg：六面体星球映射",
    "- 04_dem_generation.svg：DEM 地形生成",
    "- 05_hydrology_erosion_masks.svg：水文侵蚀示意",
    "- 06_gpu_data_upload.svg：CPU 到 GPU 上传",
    "- 07_baked_chunk_lod.svg：地形块 LOD",
    "- 08_ocean_fft.svg：FFT 海洋",
    "- 09_ocean_reflection_refraction.svg：海洋反射折射",
    "- 10_atmosphere_clouds.svg：大气与体积云",
    "- 11_frame_render_sequence.svg：每帧渲染顺序",
    "- 12_debug_and_presentation.svg：调试与讲解支持"
)
$cnFlows = for ($i = 0; $i -lt $ModulesCn.Count; $i++) {
    $num = "{0:D2}" -f (13 + $i)
    "- $num`_flow_$($ModulesCn[$i].Id)_$($ModulesCn[$i].Slug).svg：$($ModulesCn[$i].Title)"
}
Write-Utf8File "images\00_README_CN.md" (@"
# images 目录说明（中文）

这里按答辩使用顺序放置示意图和流程图。01-12 是概念示意图，13-31 是功能模块流程图。

## 概念示意图
$($cnConcept -join "`n")

## 功能模块流程图
$($cnFlows -join "`n")
"@)

$enConcept = $ConceptEn | ForEach-Object { "- $($_.Name): $($_.Title)" }
$enFlows = for ($i = 0; $i -lt $ModulesEn.Count; $i++) {
    $num = "{0:D2}" -f (13 + $i)
    "- $num`_flow_$($ModulesEn[$i].Id)_$($ModulesEn[$i].Slug).svg: $($ModulesEn[$i].Title)"
}
Write-Utf8File "images_en\00_README_EN.md" (@"
# images_en Directory Guide

This folder stores presentation diagrams in recommended speaking order. 01-12 are concept diagrams, and 13-31 are functional flowcharts.

## Concept Diagrams
$($enConcept -join "`n")

## Functional Flowcharts
$($enFlows -join "`n")
"@)

$preCnRows = $ModulesCn | ForEach-Object { "| $($_.Id) | $($_.Title) | $($_.Id)_$($_.Slug).md |" }
Write-Utf8File "pre\00_README_CN.md" (@"
# pre 目录说明（中文讲稿）

这里按功能模块拆分讲稿。建议答辩时打开对应编号的图片，再按同编号讲稿讲解。

| 编号 | 模块 | 讲稿文件 |
| --- | --- | --- |
$($preCnRows -join "`n")
"@)

$preEnRows = $ModulesEn | ForEach-Object { "| $($_.Id) | $($_.Title) | $($_.Id)_$($_.Slug).md |" }
Write-Utf8File "pre_en\00_README_EN.md" (@"
# pre_en Directory Guide

These are English speaking scripts split by functional module. Open the matching numbered image and use the script with the same module number.

| ID | Module | Script File |
| --- | --- | --- |
$($preEnRows -join "`n")
"@)

Write-Host "Generated $($ModulesCn.Count + 12 + $ModulesEn.Count) SVG diagrams and $($ModulesCn.Count + $ModulesEn.Count) module scripts."

