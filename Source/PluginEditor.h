/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class Brotm_EQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    Brotm_EQAudioProcessorEditor (Brotm_EQAudioProcessor&);
    ~Brotm_EQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Brotm_EQAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Brotm_EQAudioProcessorEditor)
};
