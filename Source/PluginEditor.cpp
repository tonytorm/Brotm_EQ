/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Brotm_EQAudioProcessorEditor::Brotm_EQAudioProcessorEditor (Brotm_EQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), peakFreqSliderAttachment(audioProcessor.APVTS, "Peak Freq", peakFreqSlider),
                                                     peakGainSliderAttachment(audioProcessor.APVTS, "Peak Gain", peakGainSlider),
                                                     peakQualitySliderAttachment(audioProcessor.APVTS, "Peak Quality", peakQualitySlider),
                                                     lowCutFreqSliderAttachment(audioProcessor.APVTS, "LowCut Freq", lowCutFreqSlider),
                                                     lowCutSlopeSliderAttachment(audioProcessor.APVTS, "LowCut Slope", lowCutSlopeSlider),
                                                     highCutFreqSliderAttachment(audioProcessor.APVTS, "HighCut Freq", highCutFreqSlider),
                                                     highCutSlopeSliderAttachment(audioProcessor.APVTS, "HighCut Slope", highCutSlopeSlider)
{
    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
    }
    
    setSize (600, 400);
}

Brotm_EQAudioProcessorEditor::~Brotm_EQAudioProcessorEditor()
{
}

//==============================================================================
void Brotm_EQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void Brotm_EQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);
    
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);
                             
}

std::vector<juce::Component*> Brotm_EQAudioProcessorEditor::getComps()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutSlopeSlider
    };
}
