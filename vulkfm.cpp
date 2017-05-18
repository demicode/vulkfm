
#include "vulkfm.h"

#include <cmath>
#include <cstdio>
#include <cassert>

#define TAU (2*M_PI)

static inline float clamp01f(float v) { return (v<0?0:(v>1.f?1.0f:v)); }

Algorithm defaultAlgorithm { 
		.operatorCount = 4, 
		.mods  = { 0b00000001,  0b00000010,  0b00001000, 0b00000000 },
		.outs  = { 1,  0,  1,  0 },
};


Osc::Osc() { }

void Osc::trigger(float _freq, const OperatorConf *opConf)
{
	opConf_ = opConf;
	freq_ = _freq;
	phase_ = 0;
}


void Osc::update(float time)
{
	phase_ += time * freq_ * TAU;
	while(phase_ > TAU)
		phase_ -= TAU;
}

float Osc::evaluate(float fmodulation) const
{
	float a = phase_ + fmodulation;
	float sample = 0.f; 

	switch(opConf_->oscWaveform) {
	case Sine: 		sample = sinf(a); break;
	case AbsSine: 	sample = fabs(sinf(a)); break;
	case ClampSine: sample = clamp01f(sinf(a)); break;
	case Square: 	sample = (a > M_PI ? -1.f : 1.f); break;
	default: 		sample = 0.f; break;
	}
	return sample * opConf_->oscAmp;
}


Env::Env() : state_(4) { }

void Env::trigger( const EnvConf* _envConf) { envConf_ = _envConf; state_ = 0; }
void Env::retrigger() { state_ = 0; }

void Env::release() { state_ = 3; }

bool Env::update(float dt)
{
	switch(state_) {
		case 0: // attack
			if(envConf_->attack > 0) {
				level_ += dt*(1.f/envConf_->attack);
				if(level_ >= envConf_->attackLevel) {
					state_ = 1;
				}
			} else { state_ = 1; }
			break;

		case 1:
			if(envConf_->decay > 0) {
				level_ -= dt*(1.f/envConf_->decay);
				if(level_ <= envConf_->sustain) {
					level_ = envConf_->sustain;
					state_ = 2;
				}
			} else {
				state_ = 2;
			}
			break;

		case 2:
			/* sustain at sustain :) */
			level_ = envConf_->sustain;
			break;
		case 3:
			if(envConf_->release > 0) {
				level_ -= dt*(1.f/envConf_->release);
				if(level_ <= 0) {
					state_ = 4;
					level_ = 0.f;
				}
			} else {
				level_ = 0.f;
				state_ = 4;
			}
			break;
	}
	return ( state_ < 4);
}

float Env::evaluate() const { return level_; }

Operator::Operator() { }

void Operator::trigger(float freq, const OperatorConf *conf) { osc_.trigger(freq*conf->freqScale, conf); env_.trigger(&conf->env); }

void Operator::retrigger() { env_.retrigger(); }

void Operator::release() { env_.release(); }

bool Operator::update(float deltaTime)
{
	bool done = env_.update(deltaTime);
	osc_.update(deltaTime);
	return done;
}

float Operator::evaluate(float modulation) const
{
	float sample = osc_.evaluate(modulation);
	float e = env_.evaluate();
	return e*sample;
}

//

Instrument::Instrument() : algo_(defaultAlgorithm) { }


Voice::Voice() : algo_(defaultAlgorithm) { }

void Voice::trigger(int _note, const Instrument* _inst)
{
	this->instrument = _inst;
	int freqDiff = _note-57; // Midi note 57 is A4 (440Hz)

	float baseFreq = 440.f * powf( ACONST, freqDiff);

	for(int i = 0; i < algo_.operatorCount; ++i) {
		ops_[i].trigger(baseFreq, &_inst->opConf_[i]);
	}
	active_ = true;
}

void Voice::retrigger()
{
	for(int i = 0; i < algo_.operatorCount; ++i) {		
		ops_[i].retrigger();
	}	
}

void Voice::release()
{
	for(int i = 0; i < algo_.operatorCount; ++i)
		ops_[i].release();
}

float Voice::evaluate()
{ 
	float output = 0;
	if(active_) {
		int count = 0;
		for(int i = algo_.operatorCount -1; i >= 0; --i) {
			float modulation = 0;
			uint8_t modFlags = algo_.mods[i];
			if( modFlags!= 0) {
				for(int m = 0; m < algo_.operatorCount;++m) {
					if(modFlags&1) {
						modulation += outs_[algo_.mods[m]];
					}
					modFlags >>= 1;
				}
			}
			outs_[i] = ops_[i].evaluate(modulation);

			if(algo_.outs[i]) { output += outs_[i]; count++; }
		}
		output = output * 1.f/count;
	}
	return output;
}

bool Voice::update(float dt)
{
	bool playing = false;

	if( active_ ) {
		for(int i = 0; i < algo_.operatorCount; ++i) {
			playing |= ops_[i].update(dt) && (algo_.outs[i] != 0);
		}
		active_ = playing;
	}
	return playing;
}

//---------------------------------------------------

VulkFM::VulkFM()
{
	outBufferIdx_ = 0;
	voices_ = 32;
	voicePool_ = new Voice*[voices_];
	activeVoices_ = new ActiveVoice[voices_];

	for(int i = 0; i < voices_; ++i)
		voicePool_[i] = new Voice();
	poolCount_ = voices_;
	activeCount_ = 0;

	activeInstrument_ = new Instrument();
}

VulkFM::~VulkFM()
{
	for(int i = 0; i <poolCount_; ++i) {
		delete voicePool_[i];
	}
	delete voicePool_;
	voicePool_ = nullptr;

	for(int i = 0; i <activeCount_; ++i) {
		delete activeVoices_[i].voice_;
	}
	delete activeVoices_;
	activeVoices_ = nullptr;
}


void VulkFM::trigger(int8_t note, int8_t channel, int8_t /*velocity*/)
{
	Voice* voice = nullptr;

	for(int i = 0; i < activeCount_; ++i) {
		if(activeVoices_[i].note_ == note ) {
			voice = activeVoices_[i].voice_;
			break;
		}
	}

	if( voice == nullptr ) {
		voice = getFromPool();
	}

	if( voice != nullptr) {
		if( !voice->isActive() ) {

			auto inst = getInstrumentByChannel(channel);
			voice->trigger(note, inst);

			auto &voiceNotePair = activeVoices_[activeCount_++];
			voiceNotePair.note_ = note;
			voiceNotePair.voice_ = voice;
		} else {
			voice->retrigger();
		}
	}
}

void VulkFM::release(int8_t note, int8_t /*channel*/, int8_t /*velocity*/)
{
	for(int i = 0; i < activeCount_; ++i) {
		if(activeVoices_[i].note_ == note ) {
			activeVoices_[i].voice_->release();
			break;
		}
	}
}

void VulkFM::update(float dt)
{
	for(int i = 0; i < activeCount_; ++i) {
		bool playing = activeVoices_[i].voice_->update(dt);
		if(!playing) {
			returnToPool(activeVoices_[i].voice_);
			auto &tmp = activeVoices_[--activeCount_];
			activeVoices_[i] = tmp;
		}
	}
}

float VulkFM::evaluate()
{
	float sample = 0;
	for(int i = 0; i < activeCount_; ++i) {
		sample += activeVoices_[i].voice_->evaluate() * 0.7f;
	}

	outBuffer_[outBufferIdx_++] = sample;
	outBufferIdx_ =  outBufferIdx_ % 1024;


	fflush(stdout);

	return sample*0.3f;
}

Voice* VulkFM::getFromPool()
{
	Voice* inst = nullptr;
	if(poolCount_ > 0) {
		inst = voicePool_[--poolCount_];
	}
	return inst;
}

void VulkFM::returnToPool(Voice* inst)
{
	voicePool_[poolCount_++] = inst;
}


