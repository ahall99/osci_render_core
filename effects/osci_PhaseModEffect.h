#pragma once
#include "../effect/osci_SimpleEffect.h"

#include <cmath>

class PhaseModEffect : public osci::EffectApplication {
public:
	std::shared_ptr<osci::EffectApplication> clone() const override {
		return std::make_shared<PhaseModEffect>();
	}

	void prepareToPlay(float sampleRate) override {
		buffer.resize((int)std::ceil(sampleRate));
		bufferIndex = 0;
		samplesSinceFrameStart = 0;
	}

	void onFrameStart() override {
		samplesSinceFrameStart = 0;
	}

	osci::Point apply(int index, osci::Point input, osci::Point externalInput, const std::vector<std::atomic<float>>& values, float sampleRate, float frequency) override {
		if (buffer.empty()) return input;

        
        // If I don't lock coarse freq to a natty number multiple of base freq,
        // there is no way (?) to guarantee initial phase offset on note trigger

        double pmAmp = values[0];
        double pmCoarseFrequency = juce::jmax(1.0f, values[1].load()); // TODO min?
        double pmPhaseOffset = values[2];

		const double safeFrequency = juce::jmax(1.0, (double)frequency);
		const int cycleSamples = juce::jlimit(1, (int)buffer.size(), (int)std::ceil(sampleRate / safeFrequency));
        const double trueCycleSamples = sampleRate / frequency;

		
        // TODO ********** Try Bend parameter or something


        
		const double framePhase = (double)samplesSinceFrameStart / trueCycleSamples; // [0, 1)
        double pmPhase = (framePhase * pmCoarseFrequency - pmPhaseOffset) * juce::MathConstants<double>::twoPi;
        double pmSignal = pmAmp * std::sin(pmPhase) / pmCoarseFrequency / juce::MathConstants<double>::twoPi;

        double outFramePhase = framePhase + pmSignal;
        outFramePhase = outFramePhase - std::floor(outFramePhase); // Wrap in [0, 1)

		buffer[bufferIndex] = input;

		// Linear interpolation works much better than nearest for this
        // TODO try int cycle samples instead of true cycle samples
        // TODO wrap buffer by actual buffer size instead of cycle size?
        // TODO Explain the algorithm here
        double samplePos = bufferIndex - samplesSinceFrameStart + (outFramePhase - 1) * trueCycleSamples - 1;
		samplePos = samplePos - buffer.size() * std::floor(samplePos / (double)buffer.size()); // Wrap to [0, size)
		int lowIndex = (int)std::floor(samplePos) % buffer.size();
		int highIndex = (lowIndex + 1) % buffer.size();
		double mixFactor = samplePos - std::floor(samplePos); // Fractional part
		osci::Point output = (1 - mixFactor) * buffer[lowIndex] + mixFactor * buffer[highIndex];

		bufferIndex++;
		if (bufferIndex >= buffer.size()) {
			bufferIndex = 0;
		}
		samplesSinceFrameStart++;
        // Fallback for contexts where we don't get explicit frame sync pulses
        if (samplesSinceFrameStart >= cycleSamples) {
            samplesSinceFrameStart = 0;
        }

		return output;
	}

	std::shared_ptr<osci::Effect> build() const override {
		auto eff = std::make_shared<osci::SimpleEffect>(
			std::make_shared<PhaseModEffect>(),
			std::vector<osci::EffectParameter*>{
				new osci::EffectParameter("Phase Modulation", "Controls the number of dashed lines in the drawing.", "pmAmp", VERSION_HINT, 0.0, 0.0, 30.0),
				new osci::EffectParameter("Coarse Frequency", "Offsets the location of the dashed lines.", "pmCoarseFrequency", VERSION_HINT, 1.0, 1.0, 48.0, 1.0),
				new osci::EffectParameter("Phase Offset", "Controls how much each dash unit is drawn.", "pmPhaseOffset", VERSION_HINT, 0.0, 0.0, 1.0, 0.0001f, osci::LfoType::Sawtooth, 0.5f),
			}
		);
		eff->setName("Phase Modulation");
		return configureBuiltEffect(eff);
	}

private:
	std::vector<osci::Point> buffer;
	int bufferIndex = 0;
	int samplesSinceFrameStart = 0;
};
