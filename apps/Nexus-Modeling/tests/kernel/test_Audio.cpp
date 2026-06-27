#include <gtest/gtest.h>
#include "nexus/audio/AudioMixer.h"
using namespace nexus::audio;
TEST(AudioMixer,GenerateSine){auto buf=AudioMixer::generateSine(440,1,44100);EXPECT_TRUE(buf.valid());EXPECT_GT(buf.duration(),0.5f);}
TEST(AudioMixer,MixSamples){AudioMixer mixer;AudioClip clip;clip.buffer=AudioMixer::generateSine(440,0.5f,44100);clip.playing=true;mixer.addClip(clip);mixer.play(0);auto samples=mixer.mixSamples(256);EXPECT_GT(samples.size(),0u);}
TEST(AudioMixer,GenerateNoise){auto buf=AudioMixer::generateNoise(0.5f,44100);EXPECT_TRUE(buf.valid());}
TEST(AudioMixer,GenerateSquare){auto buf=AudioMixer::generateSquare(220,0.5f,44100);EXPECT_TRUE(buf.valid());}
