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

function TextSvg {
    param([int]$X, [int]$Y, [string]$Text, [string]$Class = "label", [string]$Anchor = "start")
    $escaped = Escape-Xml $Text
    return "<text class=`"$Class`" x=`"$X`" y=`"$Y`" text-anchor=`"$Anchor`">$escaped</text>"
}

function Panel {
    param([int]$X, [int]$Y, [int]$W, [int]$H, [string]$Class = "panel")
    return "<rect class=`"$Class`" x=`"$X`" y=`"$Y`" width=`"$W`" height=`"$H`" rx=`"10`"/>"
}

function Arrow {
    param([int]$X1, [int]$Y1, [int]$X2, [int]$Y2, [string]$Class = "arrow")
    return "<path class=`"$Class`" d=`"M$X1 $Y1 L$X2 $Y2`"/>"
}

function SvgShell {
    param([string]$Title, [string]$Sub, [string]$Body)
    $t = Escape-Xml $Title
    $s = Escape-Xml $Sub
    return @"
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="720" viewBox="0 0 1280 720" role="img">
  <title>$t</title>
  <desc>$s</desc>
  <defs>
    <marker id="arrowHead" markerWidth="14" markerHeight="14" refX="12" refY="7" orient="auto">
      <path d="M2,2 L12,7 L2,12 Z" fill="#244a70"/>
    </marker>
    <style>
      .bg{fill:#f4f9ff}.title{font:700 30px 'Microsoft YaHei','Segoe UI',Arial,sans-serif;fill:#17324d}
      .sub{font:16px 'Microsoft YaHei','Segoe UI',Arial,sans-serif;fill:#41627f}.label{font:700 18px 'Microsoft YaHei','Segoe UI',Arial,sans-serif;fill:#17324d}
      .small{font:14px 'Microsoft YaHei','Segoe UI',Arial,sans-serif;fill:#36536d}.tiny{font:12px Consolas,'Segoe UI',Arial,sans-serif;fill:#36536d}
      .panel{fill:#ffffff;stroke:#9fc5e8;stroke-width:2}.soft{fill:#eaf4ff;stroke:#6fa8dc;stroke-width:2}.green{fill:#eef9f1;stroke:#58a66f;stroke-width:2}
      .amber{fill:#fff7e8;stroke:#d89a2b;stroke-width:2}.violet{fill:#f5efff;stroke:#9b7bd8;stroke-width:2}.water{fill:#dff2ff;stroke:#4e9bd8;stroke-width:2}
      .arrow{stroke:#244a70;stroke-width:3;marker-end:url(#arrowHead)}.thin{stroke:#6e9ec5;stroke-width:2;fill:none}.grid{stroke:#79aee0;stroke-width:2;fill:#e7f2ff}
      .formula{fill:#ffffff;stroke:#a8c6df;stroke-width:1.5}.sun{fill:#ffd66b;stroke:#d89a2b;stroke-width:2}.cloud{fill:#ffffff;stroke:#91b9d8;stroke-width:2}
    </style>
  </defs>
  <rect class="bg" width="1280" height="720"/>
  <text class="title" x="36" y="58">$t</text>
  <text class="sub" x="38" y="88">$s</text>
  $Body
</svg>
"@
}

function New-SystemOverview {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="总体结构：CPU 生成，GPU 绘制"
        $sub="这张图用来开场：先说明项目不是静态模型，而是一条实时渲染管线。"
        $cpu="CPU 程序化生成"; $gpu="GPU 资源上传"; $screen="实时画面"; $planet="最终星球"
        $a="height / water / climate / masks"; $b="texture array + baked mesh"; $c="terrain + ocean + sky + clouds"
        $formula="main.cpp 管状态，PlanetProceduralData 生成数据，PlanetRenderer 负责上传和绘制。"
    } else {
        $title="System overview: CPU makes data, GPU draws"
        $sub="Use this image first: the project is a real-time render pipeline, not a static model."
        $cpu="CPU makes planet data"; $gpu="GPU uploads resources"; $screen="Real-time image"; $planet="Final planet"
        $a="height / water / climate / masks"; $b="texture array + baked mesh"; $c="terrain + ocean + sky + clouds"
        $formula="main.cpp controls state, PlanetProceduralData makes data, PlanetRenderer uploads and draws."
    }
    $body = @"
  $(Panel 60 160 280 250 "soft")
  $(TextSvg 92 205 $cpu)
  $(TextSvg 92 245 $a "small")
  <circle cx="200" cy="330" r="70" fill="#d9edff" stroke="#6fa8dc" stroke-width="2"/>
  <path d="M150 320 C180 280, 230 280, 250 325 C220 350, 185 355, 150 320 Z" fill="#7fc97f" opacity="0.85"/>
  <path d="M165 360 C205 335, 240 360, 260 380 C225 395, 180 392, 165 360 Z" fill="#c2a670" opacity="0.75"/>
  $(Arrow 360 285 480 285)
  $(Panel 500 160 280 250 "amber")
  $(TextSvg 532 205 $gpu)
  $(TextSvg 532 245 $b "small")
  <rect x="595" y="285" width="90" height="70" fill="#e7f2ff" stroke="#6fa8dc" stroke-width="2"/>
  <rect x="615" y="265" width="90" height="70" fill="#f0f8ff" stroke="#6fa8dc" stroke-width="2"/>
  <rect x="635" y="245" width="90" height="70" fill="#ffffff" stroke="#6fa8dc" stroke-width="2"/>
  $(Arrow 800 285 920 285)
  $(Panel 940 160 280 250 "green")
  $(TextSvg 972 205 $screen)
  $(TextSvg 972 245 $c "small")
  <circle cx="1080" cy="330" r="72" fill="#cfefff" stroke="#58a66f" stroke-width="2"/>
  <path d="M1025 330 C1050 285, 1115 286, 1140 330 C1110 360, 1060 365, 1025 330 Z" fill="#6fbd78"/>
  <path d="M1045 370 C1080 345, 1120 365, 1140 390 C1105 405, 1060 400, 1045 370 Z" fill="#d5c17c"/>
  $(TextSvg 1080 438 $planet "small" "middle")
  <rect class="formula" x="85" y="545" width="1110" height="90" rx="8"/>
  $(TextSvg 112 582 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

function New-CubeSphere {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="cube-sphere：六个面映射到球面"; $sub="优点：比经纬球更适合分块、LOD 和贴图数组。"
        $formula1="cubePoint = faceNormal + uv.x * axisU + uv.y * axisV"
        $formula2="sphereDir = normalize(cubePoint),  finalPos = sphereDir * (planetRadius + height * heightScale)"
        $right="normalize 后得到 sphereDir"
    } else {
        $title="cube-sphere: six faces become one sphere"; $sub="Good for chunks, LOD, and texture arrays."
        $formula1="cubePoint = faceNormal + uv.x * axisU + uv.y * axisV"
        $formula2="sphereDir = normalize(cubePoint),  finalPos = sphereDir * (planetRadius + height * heightScale)"
        $right="normalize gives sphereDir"
    }
    $body = @"
  <g transform="translate(75 185)">
    <rect class="grid" x="170" y="0" width="170" height="170"/>
    <rect class="grid" x="0" y="170" width="170" height="170"/>
    <rect class="grid" x="170" y="170" width="170" height="170"/>
    <rect class="grid" x="340" y="170" width="170" height="170"/>
    <rect class="grid" x="510" y="170" width="170" height="170"/>
    <rect class="grid" x="170" y="340" width="170" height="170"/>
  </g>
  $(Arrow 780 355 915 355)
  <circle cx="1045" cy="355" r="85" fill="#eaf9ee" stroke="#58a66f" stroke-width="2"/>
  <ellipse cx="1045" cy="355" rx="85" ry="32" fill="none" stroke="#58a66f" stroke-width="1.6"/>
  <ellipse cx="1045" cy="355" rx="34" ry="85" fill="none" stroke="#58a66f" stroke-width="1.6"/>
  <path d="M990 385 C1015 330, 1070 305, 1105 288" class="thin"/>
  <path d="M1045 355 L1113 305" class="arrow"/>
  $(TextSvg 1045 485 $right "label" "middle")
  <rect class="formula" x="75" y="585" width="1130" height="85" rx="8"/>
  $(TextSvg 110 620 $formula1 "tiny")
  $(TextSvg 110 650 $formula2 "tiny")
"@
    return SvgShell $title $sub $body
}

function New-DemTerrain {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="DEM 地形：噪声 + 高度场 + 材质"; $sub="用这张图讲地形怎么从数据变成山脉、海岸和雪线。"
        $n1="低频噪声：大陆"; $n2="中频噪声：山脉"; $n3="高频噪声：细节"
        $formula="height = continent + mountains + ridges; material = f(height, slope, temperature, moisture)"
    } else {
        $title="DEM terrain: noise + height map + material"; $sub="Use this image to explain how data becomes mountains, coasts, and snow."
        $n1="low noise: continents"; $n2="mid noise: mountains"; $n3="high noise: detail"
        $formula="height = continent + mountains + ridges; material = f(height, slope, temperature, moisture)"
    }
    $body = @"
  $(Panel 70 160 290 260 "soft")
  $(TextSvg 100 205 $n1)
  <path d="M105 330 C145 270, 190 365, 230 305 C265 260, 305 315, 325 285" fill="none" stroke="#58a66f" stroke-width="5"/>
  $(TextSvg 100 255 $n2 "small")
  <path d="M105 360 L145 275 L185 365 L230 250 L270 360 L325 285" fill="none" stroke="#d89a2b" stroke-width="4"/>
  $(TextSvg 100 405 $n3 "small")
  $(Arrow 390 290 510 290)
  $(Panel 530 160 300 260 "amber")
  $(TextSvg 560 205 "DEM height map" "label")
  <rect x="585" y="245" width="190" height="130" fill="#dceefa" stroke="#6fa8dc" stroke-width="2"/>
  <path d="M585 340 C630 300, 675 350, 720 285 C745 250, 760 270, 775 245 L775 375 L585 375 Z" fill="#9fcf8a"/>
  <path d="M585 360 C640 350, 705 365, 775 345 L775 375 L585 375 Z" fill="#6fa8dc" opacity="0.5"/>
  $(Arrow 860 290 980 290)
  $(Panel 1000 160 220 260 "green")
  $(TextSvg 1030 205 "rendered terrain" "label")
  <path d="M1025 360 C1060 250, 1090 360, 1120 260 C1150 360, 1185 330, 1200 370 L1025 370 Z" fill="#7fc97f" stroke="#4e8d5a" stroke-width="2"/>
  <path d="M1070 288 L1090 360 L1120 260 L1136 316 C1110 310, 1095 305, 1070 288 Z" fill="#eeeeee"/>
  <rect x="1025" y="370" width="175" height="30" fill="#6fa8dc" opacity="0.55"/>
  <rect class="formula" x="75" y="555" width="1130" height="85" rx="8"/>
  $(TextSvg 110 602 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

function New-Hydrology {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="水文与侵蚀：水往低处走"; $sub="这张图用来解释河道、汇流量和侵蚀 mask 从哪里来。"
        $formula="receiver = lowest neighbor; drainage += upstream water; channel = slope * drainage"
        $a="高处降水"; $b="汇流"; $c="切割河道"; $d="输出 channel / wear / deposition mask"
    } else {
        $title="Hydrology and erosion: water goes down"; $sub="Use this image to explain rivers, flow, and erosion masks."
        $formula="receiver = lowest neighbor; drainage += upstream water; channel = slope * drainage"
        $a="rain on high land"; $b="flow gathers"; $c="river cuts ground"; $d="output channel / wear / deposition masks"
    }
    $body = @"
  $(Panel 70 150 520 340 "soft")
  <path d="M110 420 C160 300, 210 390, 260 235 C325 410, 380 275, 540 430 L110 430 Z" fill="#d6c18a" stroke="#9d8652" stroke-width="2"/>
  <path d="M140 245 L168 295 M205 210 L232 270 M315 205 L335 265" stroke="#4e9bd8" stroke-width="3"/>
  <path d="M170 305 C210 330, 250 350, 300 360 C350 372, 420 390, 520 425" fill="none" stroke="#1f80c0" stroke-width="7"/>
  <path d="M260 235 C285 270, 295 320, 300 360" fill="none" stroke="#1f80c0" stroke-width="4"/>
  $(TextSvg 130 180 $a)
  $(TextSvg 310 375 $b)
  $(TextSvg 405 460 $c)
  $(Arrow 620 320 760 320)
  $(Panel 790 150 360 340 "water")
  $(TextSvg 825 200 $d)
  <rect x="840" y="245" width="250" height="150" fill="#f7fcff" stroke="#6fa8dc" stroke-width="2"/>
  <path d="M865 360 C910 310, 975 335, 1065 270" fill="none" stroke="#1f80c0" stroke-width="8"/>
  <path d="M865 390 C920 355, 970 380, 1080 310" fill="none" stroke="#d89a2b" stroke-width="4" opacity="0.8"/>
  <circle cx="930" cy="332" r="12" fill="#58a66f"/>
  <circle cx="1020" cy="295" r="12" fill="#d89a2b"/>
  <rect class="formula" x="75" y="565" width="1130" height="80" rx="8"/>
  $(TextSvg 110 612 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

function New-GpuLod {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="GPU 上传与 LOD：近处细，远处粗"; $sub="这张图把 texture array、地形 chunk 和相机距离放在一起讲。"
        $formula="6 cube faces -> texture array layers; camera distance -> choose visible chunks and LOD"
        $a="6 层 texture array"; $b="baked terrain chunks"; $c="camera"; $d="near: high LOD"; $e="far: low LOD"
    } else {
        $title="GPU upload and LOD: near is detailed, far is simple"; $sub="This image links texture arrays, terrain chunks, and camera distance."
        $formula="6 cube faces -> texture array layers; camera distance -> choose visible chunks and LOD"
        $a="6 texture layers"; $b="baked terrain chunks"; $c="camera"; $d="near: high LOD"; $e="far: low LOD"
    }
    $body = @"
  $(Panel 70 150 310 340 "amber")
  $(TextSvg 105 200 $a)
  <rect x="155" y="315" width="140" height="90" fill="#e7f2ff" stroke="#6fa8dc" stroke-width="2"/>
  <rect x="175" y="295" width="140" height="90" fill="#f0f8ff" stroke="#6fa8dc" stroke-width="2"/>
  <rect x="195" y="275" width="140" height="90" fill="#ffffff" stroke="#6fa8dc" stroke-width="2"/>
  $(Arrow 410 320 530 320)
  $(Panel 555 150 360 340 "soft")
  $(TextSvg 590 200 $b)
  <g stroke="#6fa8dc" stroke-width="2" fill="#eaf4ff">
    <rect x="615" y="255" width="70" height="70"/><rect x="685" y="255" width="70" height="70"/><rect x="755" y="255" width="70" height="70"/>
    <rect x="615" y="325" width="70" height="70"/><rect x="685" y="325" width="70" height="70"/><rect x="755" y="325" width="70" height="70"/>
  </g>
  <path d="M605 420 C700 350, 780 380, 850 310" class="thin"/>
  $(Arrow 945 320 1030 320)
  $(Panel 1025 150 190 340 "green")
  <path d="M1065 430 L1110 245 L1190 430 Z" fill="#fff" stroke="#244a70" stroke-width="2"/>
  <circle cx="1110" cy="245" r="14" fill="#244a70"/>
  $(TextSvg 1110 220 $c "label" "middle")
  $(TextSvg 1120 320 $d "small")
  $(TextSvg 1120 390 $e "small")
  <rect class="formula" x="75" y="565" width="1130" height="80" rx="8"/>
  $(TextSvg 110 612 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

function New-Ocean {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="海洋：FFT 波浪 + 反射折射"; $sub="这张图用来讲水面为什么会动，也为什么看起来有反射和深浅变化。"
        $formula="spectrum(t) -> IFFT -> height/normal/displacement textures; Fresnel mixes reflection and refraction"
        $a="频域波谱"; $b="IFFT 后的波面"; $c="反射 / 折射"; $d="最终水面"
    } else {
        $title="Ocean: FFT waves + reflection/refraction"; $sub="Use this image to explain moving water and water color."
        $formula="spectrum(t) -> IFFT -> height/normal/displacement textures; Fresnel mixes reflection and refraction"
        $a="wave spectrum"; $b="wave surface"; $c="reflection / refraction"; $d="final water"
    }
    $body = @"
  $(Panel 65 155 265 330 "soft")
  $(TextSvg 95 205 $a)
  <g stroke="#6fa8dc" stroke-width="1.5">
    <line x1="110" y1="245" x2="285" y2="245"/><line x1="110" y1="285" x2="285" y2="285"/><line x1="110" y1="325" x2="285" y2="325"/><line x1="110" y1="365" x2="285" y2="365"/>
    <line x1="130" y1="225" x2="130" y2="390"/><line x1="170" y1="225" x2="170" y2="390"/><line x1="210" y1="225" x2="210" y2="390"/><line x1="250" y1="225" x2="250" y2="390"/>
  </g>
  <circle cx="170" cy="285" r="16" fill="#244a70"/><circle cx="250" cy="245" r="10" fill="#4e9bd8"/><circle cx="210" cy="365" r="12" fill="#4e9bd8"/>
  $(Arrow 360 320 485 320)
  $(Panel 505 155 285 330 "water")
  $(TextSvg 535 205 $b)
  <path d="M535 335 C570 300, 610 370, 650 330 C690 285, 725 360, 760 320" fill="none" stroke="#1f80c0" stroke-width="6"/>
  <path d="M535 355 C570 320, 610 390, 650 350 C690 305, 725 380, 760 340" fill="none" stroke="#79bee8" stroke-width="4"/>
  $(Arrow 820 320 940 320)
  $(Panel 955 155 270 330 "green")
  $(TextSvg 985 205 $c)
  <line x1="1000" y1="330" x2="1190" y2="330" stroke="#1f80c0" stroke-width="5"/>
  <path d="M1030 300 C1060 250, 1120 250, 1160 300" fill="none" stroke="#244a70" stroke-width="4"/>
  <path d="M1030 360 C1070 390, 1130 390, 1170 360" fill="none" stroke="#58a66f" stroke-width="4"/>
  $(TextSvg 1095 440 $d "label" "middle")
  <rect class="formula" x="75" y="565" width="1130" height="80" rx="8"/>
  $(TextSvg 110 612 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

function New-AtmosphereClouds {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="大气与云：LUT 天空 + raymarch 云层"; $sub="这张图用来解释天空颜色、地平线雾化和体积云。"
        $formula="atmosphere LUT gives sky color; cloud raymarch samples density along the view ray"
        $a="太阳光"; $b="大气壳"; $c="云层采样点"; $d="LUT 查表"
    } else {
        $title="Atmosphere and clouds: LUT sky + raymarch clouds"; $sub="Use this image to explain sky color, haze, and clouds."
        $formula="atmosphere LUT gives sky color; cloud raymarch samples density along the view ray"
        $a="sun light"; $b="air shell"; $c="cloud samples"; $d="LUT table"
    }
    $body = @"
  <circle class="sun" cx="150" cy="175" r="46"/>
  $(TextSvg 150 245 $a "label" "middle")
  <path d="M210 205 L420 300 M205 155 L430 250 M205 245 L430 350" class="arrow"/>
  <circle cx="640" cy="405" r="150" fill="#d8f0ff" stroke="#58a66f" stroke-width="2"/>
  <circle cx="640" cy="405" r="190" fill="none" stroke="#6fa8dc" stroke-width="4" opacity="0.7"/>
  $(TextSvg 640 185 $b "label" "middle")
  <path class="cloud" d="M555 275 C570 240, 615 245, 625 270 C650 240, 705 255, 700 292 C730 292, 745 330, 710 345 L565 345 C520 340, 515 300, 555 275 Z"/>
  <line x1="470" y1="240" x2="735" y2="330" stroke="#244a70" stroke-width="3"/>
  <circle cx="530" cy="260" r="7" fill="#244a70"/><circle cx="585" cy="278" r="7" fill="#244a70"/><circle cx="640" cy="296" r="7" fill="#244a70"/><circle cx="695" cy="314" r="7" fill="#244a70"/>
  $(TextSvg 620 245 $c "label" "middle")
  $(Panel 920 205 240 250 "amber")
  $(TextSvg 950 250 $d)
  <rect x="975" y="290" width="130" height="100" fill="#fff" stroke="#d89a2b" stroke-width="2"/>
  <line x1="975" y1="323" x2="1105" y2="323" stroke="#d89a2b"/><line x1="975" y1="356" x2="1105" y2="356" stroke="#d89a2b"/>
  <line x1="1018" y1="290" x2="1018" y2="390" stroke="#d89a2b"/><line x1="1061" y1="290" x2="1061" y2="390" stroke="#d89a2b"/>
  <rect class="formula" x="75" y="565" width="1130" height="80" rx="8"/>
  $(TextSvg 110 612 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

function New-FrameDebug {
    param([string]$Lang)
    if ($Lang -eq "cn") {
        $title="每帧画面：相机决定看什么，UI 帮助解释"; $sub="这张图适合收尾：把输入、渲染结果和调试面板放在一起。"
        $formula="camera -> visible chunks/patches -> draw terrain/ocean/sky -> ImGui debug panels"
        $a="相机"; $b="可见区域"; $c="主画面"; $d="调试面板"
    } else {
        $title="One frame: camera chooses view, UI explains it"; $sub="Use this image near the end: input, final image, and debug panels."
        $formula="camera -> visible chunks/patches -> draw terrain/ocean/sky -> ImGui debug panels"
        $a="camera"; $b="visible area"; $c="main image"; $d="debug UI"
    }
    $body = @"
  $(Panel 70 170 250 300 "green")
  <path d="M120 390 L165 240 L245 390 Z" fill="#fff" stroke="#244a70" stroke-width="2"/>
  <circle cx="165" cy="240" r="15" fill="#244a70"/>
  $(TextSvg 165 215 $a "label" "middle")
  $(TextSvg 185 330 $b "small")
  $(Arrow 350 320 470 320)
  $(Panel 490 130 420 380 "soft")
  $(TextSvg 700 175 $c "label" "middle")
  <rect x="555" y="210" width="290" height="205" fill="#dff2ff" stroke="#6fa8dc" stroke-width="2"/>
  <circle cx="700" cy="315" r="75" fill="#cfefff" stroke="#58a66f" stroke-width="2"/>
  <path d="M650 315 C675 280, 725 282, 750 315 C725 345, 675 350, 650 315 Z" fill="#6fbd78"/>
  <path d="M645 365 C690 345, 735 360, 760 385 L645 385 Z" fill="#6fa8dc" opacity="0.6"/>
  $(Arrow 935 320 1035 320)
  $(Panel 1025 170 210 300 "amber")
  $(TextSvg 1130 215 $d "label" "middle")
  <rect x="1060" y="250" width="135" height="26" fill="#fff" stroke="#d89a2b"/><rect x="1060" y="290" width="135" height="26" fill="#fff" stroke="#d89a2b"/>
  <rect x="1060" y="330" width="135" height="26" fill="#fff" stroke="#d89a2b"/><rect x="1060" y="370" width="135" height="26" fill="#fff" stroke="#d89a2b"/>
  <rect class="formula" x="75" y="565" width="1130" height="80" rx="8"/>
  $(TextSvg 110 612 $formula "tiny")
"@
    return SvgShell $title $sub $body
}

$Diagrams = @(
    @{Name="01_system_overview.svg"; Cn={ New-SystemOverview "cn" }; En={ New-SystemOverview "en" }; CnTitle="总体结构：CPU 生成，GPU 绘制"; EnTitle="System overview"},
    @{Name="02_cube_sphere_mapping.svg"; Cn={ New-CubeSphere "cn" }; En={ New-CubeSphere "en" }; CnTitle="cube-sphere 六面体映射"; EnTitle="cube-sphere mapping"},
    @{Name="03_dem_terrain.svg"; Cn={ New-DemTerrain "cn" }; En={ New-DemTerrain "en" }; CnTitle="DEM 地形生成"; EnTitle="DEM terrain"},
    @{Name="04_hydrology_erosion.svg"; Cn={ New-Hydrology "cn" }; En={ New-Hydrology "en" }; CnTitle="水文与侵蚀"; EnTitle="hydrology and erosion"},
    @{Name="05_gpu_lod.svg"; Cn={ New-GpuLod "cn" }; En={ New-GpuLod "en" }; CnTitle="GPU 上传与 LOD"; EnTitle="GPU upload and LOD"},
    @{Name="06_ocean_fft.svg"; Cn={ New-Ocean "cn" }; En={ New-Ocean "en" }; CnTitle="海洋 FFT 与反射折射"; EnTitle="ocean FFT and shading"},
    @{Name="07_atmosphere_clouds.svg"; Cn={ New-AtmosphereClouds "cn" }; En={ New-AtmosphereClouds "en" }; CnTitle="大气与体积云"; EnTitle="atmosphere and clouds"},
    @{Name="08_frame_debug.svg"; Cn={ New-FrameDebug "cn" }; En={ New-FrameDebug "en" }; CnTitle="每帧渲染与调试"; EnTitle="frame and debug UI"}
)

function Clear-OldSvgs {
    param([string]$RelativeDir)
    $dir = Join-Path $Root $RelativeDir
    $resolved = Resolve-Path $dir
    if (-not $resolved.Path.StartsWith($Root.Path)) {
        throw "Refusing to clean outside workspace: $resolved"
    }
    Get-ChildItem -LiteralPath $resolved.Path -Filter "*.svg" -File | Remove-Item -Force
}

Clear-OldSvgs "images"
Clear-OldSvgs "images_en"

foreach ($d in $Diagrams) {
    Write-Utf8File "images\$($d.Name)" (& $d.Cn)
    Write-Utf8File "images_en\$($d.Name)" (& $d.En)
}

$cnLines = $Diagrams | ForEach-Object { "- $($_.Name)：$($_.CnTitle)" }
$enLines = $Diagrams | ForEach-Object { "- $($_.Name): $($_.EnTitle)" }

Write-Utf8File "images\00_README_CN.md" (@"
# images 目录说明（中文）

这里现在只保留 8 张关键示意图。它们不是逐步流程图，而是答辩时更容易讲清楚思路的概念图。

$($cnLines -join "`n")
"@)

Write-Utf8File "images_en\00_README_EN.md" (@"
# images_en Directory Guide

This folder now keeps only 8 key diagrams. They are concept diagrams, not dense flowcharts.

$($enLines -join "`n")
"@)

$imageMap = @{
    "01_project_overview.md"="01_system_overview.svg";
    "02_app_state_generation_thread.md"="01_system_overview.svg";
    "03_cube_sphere_mapping.md"="02_cube_sphere_mapping.svg";
    "04_dem_generation.md"="03_dem_terrain.svg";
    "05_hydrology_erosion_masks.md"="04_hydrology_erosion.svg";
    "06_climate_materials.md"="03_dem_terrain.svg";
    "07_terrain_chunks_lod.md"="05_gpu_lod.svg";
    "08_gpu_upload.md"="05_gpu_lod.svg";
    "09_ocean_patch_lod.md"="06_ocean_fft.svg";
    "10_fft_ocean.md"="06_ocean_fft.svg";
    "11_ocean_material.md"="06_ocean_fft.svg";
    "12_atmosphere_lut.md"="07_atmosphere_clouds.svg";
    "13_procedural_clouds.md"="07_atmosphere_clouds.svg";
    "14_debug_visualization.md"="08_frame_debug.svg";
    "15_input_camera.md"="08_frame_debug.svg";
    "16_shader_resources.md"="05_gpu_lod.svg";
    "17_height_diagnostics.md"="08_frame_debug.svg";
    "18_frame_render_sequence.md"="08_frame_debug.svg";
    "19_presentation_summary.md"="01_system_overview.svg"
}

function Update-ScriptImageRefs {
    param([string]$Dir, [string]$Heading, [string]$NextHeading, [string]$ImagePrefix)
    foreach ($entry in $imageMap.GetEnumerator()) {
        $file = Join-Path (Join-Path $Root $Dir) $entry.Key
        if (-not (Test-Path -LiteralPath $file)) { continue }
        $text = Get-Content -Raw -Encoding UTF8 -LiteralPath $file
        $replacement = "$Heading`n- $ImagePrefix/$($entry.Value)`n`n$NextHeading"
        $pattern = [regex]::Escape($Heading) + ".*?" + [regex]::Escape($NextHeading)
        $text = [regex]::Replace($text, $pattern, $replacement, [System.Text.RegularExpressions.RegexOptions]::Singleline)
        [System.IO.File]::WriteAllText($file, $text, $Utf8NoBom)
    }
}

Update-ScriptImageRefs "pre" "## 对应图片" "## 讲解目标" "../images"
Update-ScriptImageRefs "pre_en" "## Matching Images" "## Goal" "../images_en"

Write-Host "Generated 8 Chinese diagrams and 8 English diagrams. Updated script image references."

