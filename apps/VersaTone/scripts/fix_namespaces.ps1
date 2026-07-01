# Fix namespace declarations for C++23 in all DSP effect files
$effects = @{
    "dynamics" = @("Gate", "DeEsser", "TransientShaper", "MultibandCompressor")
    "eq" = @("GraphicEQ", "LinearPhaseEQ", "HighPassFilter", "LowPassFilter", "BandPassFilter", "NotchFilter", "ShelvingEQ")
    "modulation" = @("Tremolo", "Vibrato", "AutoPan")
    "time" = @("PingPongDelay", "MultiTapDelay", "SlapbackEcho")
    "pitch" = @("PitchShifter", "Harmonizer", "Octaver", "AutoTune", "Vocoder", "TalkBox")
    "distortion" = @("Overdrive", "Fuzz", "BitCrusher", "SampleRateReducer", "TapeSaturation", "TubeSaturation")
    "spatial" = @("StereoWidener", "MidSideProcessor", "Crossover")
    "analysis" = @("SpectrumAnalyzer", "Oscilloscope", "Tuner")
    "creative" = @("GranularDelay", "ReverseReverb", "GatedReverb", "FilterSweep", "StutterEdit", "Glitch", "FormantFilter")
}

foreach ($category in $effects.Keys) {
    foreach ($effect in $effects[$category]) {
        $headerPath = "d:\dev\The-No-hands-Company\projects\dawg\engine\include\dawg\dsp\$category\$effect.h"
        $cppPath = "d:\dev\The-No-hands-Company\projects\dawg\engine\src\dsp\$category\$effect.cpp"
        
        if (Test-Path $headerPath) {
            # Fix header file namespace
            $content = Get-Content $headerPath -Raw
            $oldNamespace = "namespace dawg {`r`nnamespace dsp {`r`nnamespace $category {"
            $newNamespace = "namespace dawg::dsp::$category {"
            $content = $content -replace [regex]::Escape($oldNamespace), $newNamespace
            
            # Fix closing namespace comment
            $oldClosing = "} // namespace dawg::dsp::$category"
            $content = $content -replace [regex]::Escape($oldClosing), "} // namespace dawg::dsp::$category"
            
            $content | Out-File -FilePath $headerPath -Encoding UTF8 -NoNewline
            Write-Host "Fixed namespace in $headerPath"
        }
        
        if (Test-Path $cppPath) {
            # Fix cpp file namespace
            $content = Get-Content $cppPath -Raw
            $oldNamespace = "namespace dawg {`r`nnamespace dsp {`r`nnamespace $category {"
            $newNamespace = "namespace dawg::dsp::$category {"
            $content = $content -replace [regex]::Escape($oldNamespace), $newNamespace
            
            # Fix closing namespace comment
            $oldClosing = "} // namespace dawg::dsp::$category"
            $content = $content -replace [regex]::Escape($oldClosing), "} // namespace dawg::dsp::$category"
            
            $content | Out-File -FilePath $cppPath -Encoding UTF8 -NoNewline
            Write-Host "Fixed namespace in $cppPath"
        }
    }
}

Write-Host "All namespace declarations updated to C++23 style!"
