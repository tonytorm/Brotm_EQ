/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


void LookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider)
{
    using namespace juce;
    
    auto bounds = Rectangle<float>(x, y, width, height);
    
    g.setColour(Colour(97u, 18u, 167u));
    g.fillEllipse(bounds);
    
    g.setColour(Colour(255u, 154u, 1u));
    g.drawEllipse(bounds, 1.f);
    
    if (auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto centre = bounds.getCentre();
        Path p;
        
        Rectangle<float> r;
        r.setLeft(centre.getX()-2);
        r.setRight(centre.getX()+2);
        r.setTop(bounds.getY());
        r.setBottom(centre.getY() - rswl->getTextHeight() * 1.5);
        
        p.addRoundedRectangle(r, 2.f);
        
        jassert(rotaryStartAngle < rotaryEndAngle);
          
        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
          
        p.applyTransform(AffineTransform().rotated(sliderAngRad, centre.getX(), centre.getY()));
          
        g.fillPath(p);
        
        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);
        
        r.setSize(strWidth + 4, rswl->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());
        
        g.setColour(Colours::black);
        g.fillRect(r);
        
        g.setColour(Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

//===================================================================================================================================
void RotarySliderWithLabels::paint(juce::Graphics &g)
{
    using namespace juce;
    
    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;
    
    auto range = getRange();
    
    auto sliderBounds = getSliderBounds();
    
//    g.setColour(Colours::red);
//    g.drawRect(getLocalBounds());
//    
//    g.setColour(Colours::yellow);
//    g.drawRect(sliderBounds);
    
    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(),
                                      sliderBounds.getY(),
                                      sliderBounds.getWidth(),
                                      sliderBounds.getHeight(),
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
                                      startAng, endAng, *this);
    
    auto centre = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;
    
    g.setColour(Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());
    
    auto numChoices = labels.size();
    for (int i = 0; i < numChoices; i++)
    {
        auto pos = labels[i].pos;
        jassert(0.f <= pos);
        jassert(pos <= 1.f);
        
        auto ang = jmap(pos, 0.f, 1.f, startAng, endAng);
        
        auto c = centre.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1.f, ang);
        
        Rectangle<float> r;
        auto str = labels[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        r.setY(r.getY() + getTextHeight());
        
        g.drawFittedText(str, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    
    size -= getTextHeight() * 2;
    
    juce::Rectangle<int> r;
    
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);
    
    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();
    juce::String str;
    bool addK = false;
    
    if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();
        if (val >= 1000.f)
        {
            val /= 1000.f;
            addK = true;
        }
        
        str = juce::String(val, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse;
    }
    
    if (suffix.isNotEmpty())
    {
        str << " ";
        if (addK)
            str << "k";
        str << suffix;
    }
    return str;
}
//===================================================================================================================================
ResponseCurveComponent::ResponseCurveComponent(Brotm_EQAudioProcessor& p) :
audioProcessor(p),
leftChannelFifo(&audioProcessor.leftChannelFifo)
{
    const auto& params = audioProcessor.getParameters();
    for ( auto param : params )
    {
        param->addListener(this);
    }
    
    leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
    monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    
    updateChain();
    startTimerHz(60);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    
    for ( auto param : params )
    {
        param->removeListener(this);
    }
}

void ResponseCurveComponent::paint(juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);
    
    g.drawImage(background, getLocalBounds().toFloat());
    auto responseArea = getAnalysisArea();
    auto w = responseArea.getWidth();
    
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    
    auto sampleRate = audioProcessor.getSampleRate();
    
    std::vector<double> mags;
    
    mags.resize(w);
    for (int i = 0; i < w; i++)
    {
        double mag = 1.f;
        auto freq = mapToLog10(double(i)/ double(w), 20.0, 20000.0);
        
        if (! monoChain.isBypassed<ChainPositions::Peak>() )
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        if (! lowCut.isBypassed<0>() )
            mag *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCut.isBypassed<1>() )
            mag *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCut.isBypassed<2>() )
            mag *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCut.isBypassed<3>() )
            mag *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        if (! highCut.isBypassed<0>() )
            mag *= highCut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCut.isBypassed<1>() )
            mag *= highCut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highCut.isBypassed<2>() )
            mag *= highCut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highCut.isBypassed<3>() )
            mag *= highCut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        mags[i] = Decibels::gainToDecibels(mag);
    }
    
    Path responseCurve;
    
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    
    auto map = [outputMin, outputMax](double input)
    {
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
    
    for (int i = 0; i < mags.size(); i++)
    {
        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
    }
    
    g.setColour(Colours::blue);
    g.strokePath(leftChannelFFTPath, PathStrokeType(1));
    
    
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 4.0f, 1.f);
    
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.0f));
}

void ResponseCurveComponent::resized()
{
    using namespace juce;
    background = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    
    Graphics g(background);
    
    
    auto renderArea = getAnalysisArea();
    auto left = renderArea.getX();
    auto right = renderArea.getRight();
    auto top = renderArea.getY();
    auto bottom = renderArea.getBottom();
    auto width = renderArea.getWidth();
    
    Array<float> xs;
    
    Array<float> freqs
       {
           20, 50, 100,
           200, 500, 1000,
           2000, 5000, 10000,
           20000
       };
    
    g.setColour(Colours::dimgrey);
    
    for (auto f : freqs)
       {
           auto normX = mapFromLog10(f, 20.f, 20000.f);
           xs.add(left + width * normX);
       }
    
    for (auto x : xs)
    {
        g.drawVerticalLine(x, top, bottom);
    }
    
    Array<float> gain
    {
        -24, -12, 0, 12, 24
    };
    
    for (auto gDb : gain)
    {
        auto y = jmap(gDb, -24.f, 24.f, (float)bottom, (float)top);

        g.setColour(gDb == 0 ? Colour(0u, 172u, 1u) : Colours::darkgrey);
        g.drawHorizontalLine(y, left, right);
    }

    
    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);
    
    for (int i = 0; i < freqs.size(); i++)
    {
        auto f = freqs[i];
        auto x = xs[i];
        String str;
        
        bool addK = false;
        if (f > 999.9f)
        {
            addK = true;
            f /= 1000.f;
        }
        
        str << f;
        if (addK)
            str << "k";
        str << "Hz";
        
        auto textWidth = g.getCurrentFont().getStringWidth(str);
        
        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(1);
        
        g.drawFittedText(str, r, Justification::centred, 1);
        
    }
    
    for (auto gDb : gain)
       {
           auto y = jmap(gDb, -24.f, 24.f, (float)bottom, (float)top);
           String str;
           if (gDb > 0)
               str << "+";
           
           str << gDb;
            
           auto textWidth = g.getCurrentFont().getStringWidth(str);
           
           Rectangle<int> r;
           r.setSize(textWidth, fontHeight);
           r.setX(getWidth() - textWidth);
           r.setCentre(r.getCentreX(), y);
           
           g.setColour(gDb == 0 ? Colour(0u, 172u, 1u) : Colours::lightgrey);
           
           g.drawFittedText(str, r, Justification::centred, 1);
           
           str.clear();
           str << (gDb - 24.f);
           
           r.setX(1);
           textWidth = g.getCurrentFont().getStringWidth(str);
           r.setSize(textWidth, fontHeight);
           g.setColour(Colours::lightgrey);
           g.drawFittedText(str, r, Justification::centred, 1);
           
       }
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();
    
    bounds.removeFromTop(12);
    bounds.removeFromBottom(2);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    
    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
    
    return bounds;
}

void ResponseCurveComponent::parameterValueChanged (int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    juce::AudioBuffer<float> tempIncomingBuffer;
    
    while (leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer))
        {
            auto size = tempIncomingBuffer.getNumSamples();
            
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                                              monoBuffer.getReadPointer(0, size),
                                              monoBuffer.getNumSamples() - size);
            
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                                              tempIncomingBuffer.getReadPointer(0, 0),
                                              size);
            
            leftChannelFFTDataGenerator.produceFFTDataRendering(monoBuffer, -48.f);
        }
    }
    
    const auto fftBounds = getAnalysisArea().toFloat();
    const auto fftSize =leftChannelFFTDataGenerator.getFFTSize();
    const auto binWidth = audioProcessor.getSampleRate() / (double)fftSize;
    
    while (leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0)
    {
        std::vector<float> fftData;
        if(leftChannelFFTDataGenerator.getFFTData(fftData))
        {
            pathProducer.generatePath(fftData, fftBounds, fftSize, binWidth, -48.f);
        }
    }
    
    while (pathProducer.getNumPathsAvailable() )
    {
        pathProducer.getPath(leftChannelFFTPath);
    }
    
    
    
    if ( parametersChanged.compareAndSetBool(false, true) )
    {
        updateChain();
        //repaint();
    }
    repaint();
}

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.APVTS);
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
}

//==================================================================================================================================
Brotm_EQAudioProcessorEditor::Brotm_EQAudioProcessorEditor (Brotm_EQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
peakFreqSlider(*audioProcessor.APVTS.getParameter("Peak Freq"), "Hz"),
peakGainSlider(*audioProcessor.APVTS.getParameter("Peak Gain"), "dB"),
peakQualitySlider(*audioProcessor.APVTS.getParameter("Peak Quality"), "Q"),
lowCutFreqSlider(*audioProcessor.APVTS.getParameter("LowCut Freq"), "Hz"),
highCutFreqSlider(*audioProcessor.APVTS.getParameter("HighCut Freq"), "Hz"),
lowCutSlopeSlider(*audioProcessor.APVTS.getParameter("LowCut Slope"), "dB/Oct"),
highCutSlopeSlider(*audioProcessor.APVTS.getParameter("HighCut Slope"), "dB/Oct"),
responseCurveComponent(audioProcessor),
peakFreqSliderAttachment(audioProcessor.APVTS, "Peak Freq", peakFreqSlider),
peakGainSliderAttachment(audioProcessor.APVTS, "Peak Gain", peakGainSlider),
peakQualitySliderAttachment(audioProcessor.APVTS, "Peak Quality", peakQualitySlider),
lowCutFreqSliderAttachment(audioProcessor.APVTS, "LowCut Freq", lowCutFreqSlider),
highCutFreqSliderAttachment(audioProcessor.APVTS, "HighCut Freq", highCutFreqSlider),
lowCutSlopeSliderAttachment(audioProcessor.APVTS, "LowCut Slope", lowCutSlopeSlider),
highCutSlopeSliderAttachment(audioProcessor.APVTS, "HighCut Slope", highCutSlopeSlider)
{
    peakFreqSlider.labels.add({0.f, "20Hz"});
    peakFreqSlider.labels.add({1.f, "20kHz"});
    peakQualitySlider.labels.add({0.f, "0.1"});
    peakQualitySlider.labels.add({1.f, "10"});
    peakGainSlider.labels.add({0.f, "-24dB"});
    peakGainSlider.labels.add({1.f, "24dB"});
    
    lowCutFreqSlider.labels.add({0.f, "20Hz"});
    lowCutFreqSlider.labels.add({1.f, "20kHz"});
    lowCutSlopeSlider.labels.add({0.f, "12"});
    lowCutSlopeSlider.labels.add({1.f, "48"});
    
    highCutFreqSlider.labels.add({0.f, "20Hz"});
    highCutFreqSlider.labels.add({1.f, "20kHz"});
    highCutSlopeSlider.labels.add({0.f, "12"});
    highCutSlopeSlider.labels.add({1.f, "48"});
    
    
    for (auto* comp : getComps())
    {
        addAndMakeVisible(comp);
       
    }
    
    
    setSize (600, 480);
}

Brotm_EQAudioProcessorEditor::~Brotm_EQAudioProcessorEditor()
{
}

//==============================================================================
void Brotm_EQAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll(Colours::black);
}

void Brotm_EQAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.25);
    
    bounds.removeFromTop(5);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    
    responseCurveComponent.setBounds(responseArea);
    
    
    
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
        &highCutSlopeSlider,
        &responseCurveComponent
    };
}

