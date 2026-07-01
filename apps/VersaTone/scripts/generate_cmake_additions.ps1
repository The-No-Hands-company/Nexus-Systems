# Generate CMakeLists.txt additions for all new DSP effects
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

Write-Host "# New DSP effect sources to add to CMakeLists.txt:"
Write-Host ""

foreach ($category in $effects.Keys) {
    Write-Host "    # $category effects"
    foreach ($effect in $effects[$category]) {
        Write-Host "    src/dsp/$category/$effect.cpp"
    }
}

Write-Host ""
Write-Host "Total new effects: $((($effects.Values | ForEach-Object { $_.Count }) | Measure-Object -Sum).Sum)"
