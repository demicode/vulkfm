
#include "vulkfm.h"

#include <cmath>
#include <cstdio>
#include <cassert>

#define TAU (2*M_PI)

static inline float clamp01f(float v) { return (v<0?0:(v>1.f?1.0f:v)); }

Algorithm defaultAlgorithm { 
		.operatorCount = 4, 
		.mods  = { 1, -1,  3, -1 },
		.outs  = { 1,  0,  1,  0 },
};


Osc::Osc()
: freq(0)
, amp(1.0f)
, waveform(Sine)
{}


void Osc::trigger(float _freq, bool retrigger = false)
{
	fflush(stdout);
	freq = _freq;
	if(!retrigger)
		phase_ = 0;
}

void Osc::update(float time)
{
	phase_ += time * freq * TAU;
	if(phase_ > TAU)
		phase_ -= TAU;
}

float Osc::evaluate(float fmodulation) const
{
	float a = phase_ + fmodulation;
	float sample = 0.f; 

	switch(waveform) {
	case Sine: 		sample = sinf(a); break;
	case AbsSine: 	sample = fabs(sinf(a)); break;
	case ClampSine: sample = clamp01f(sinf(a)); break;
	case Square: 	sample = (phase_ + a > M_PI ? -1.f : 1.f); break;
	default: 		sample = 0.1234f; break;
	}
	return sample * amp;
}


Env::Env( )
: state_(4)
{
}

void Env::trigger( const EnvConf* _envConf) { envConf_ = _envConf; state_ = 0; }

void Env::release() { state_ = 3; }

bool Env::update(float dt)
{
	switch(state_) {
		case 0: // attack
			level_ += dt*(1.f/envConf_->attack);
			if(level_ >= envConf_->attackLevel) {
				state_ = 1;
			}
			break;

		case 1:
			level_ -= dt*(1.f/envConf_->decay);
			if(level_ <= envConf_->sustain) {
				state_ = 2;
			}
			break;

		case 2:
			/* sustain at sustain :) */
			level_ = envConf_->sustain;
			break;
		case 3:
			level_ -= dt*(1.f/envConf_->release);
			if(level_ <= 0) {
				state_ = 4;
				level_ = 0.f;
			}
			break;
	}
	return ( state_ < 4);
}

float Env::evaluate() const { return level_; }


Operator::Operator() { }

void Operator::trigger(float freq, bool retrigger) { osc_.trigger(freq, retrigger); env_.trigger(&data_.env); }

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



Instrument::Instrument()
: algo_(defaultAlgorithm)
{
	active_ = false;
}

void Instrument::trigger(int note, bool retrigger = false)
{
	int freqDiff = note-57; // Midi note 57 is A4 (440Hz)

	float baseFreq = 440.f * powf( ACONST, freqDiff);

	for(int i = 0; i < algo_.operatorCount; ++i)
	{
		float fScale = freqscale[i];
		if( fScale != 0)
			ops[i].trigger(baseFreq * fScale, retrigger);
	}
	active_ = true;
}

void Instrument::release()
{
	for(int i = 0; i < algo_.operatorCount; ++i)
		ops[i].release();
}

float Instrument::evaluate()
{
	float output = 0;
	if(active_) {
		for(int i = algo_.operatorCount -1; i >= 0; --i) {
			float modulation = 0;
			if(algo_.mods[i] >= 0) {
				modulation = outputs[algo_.mods[i]];
			}
			outputs[i] = ops[i].evaluate(modulation);
		}
		int count = 0;
		for(int i = 0; i < OP_COUNT; ++i) {
			if(algo_.outs[i])	{ output += outputs[i]; count++; }
		}
		output = output * 1.f/count;
	}
	return output;
}

bool Instrument::update(float dt)
{
	bool playing = false;

	if( active_ ) {
		for(int i = 0; i < OP_COUNT; ++i) {
			playing |= ops[i].update(dt) && (algo_.outs[i] != 0);
		}
		active_ = playing;
	}
	return playing;
}



VulkFM::VulkFM()
{
	outBufferIdx_ = 0;
	voices_ = 32;
	instrumentPool_ = new Instrument*[voices_];
	activeInstruments_ = new ActiveVoice[voices_];

	for(int i = 0; i < voices_; ++i)
		instrumentPool_[i] = new Instrument();
	poolCount_ = voices_;
	activeCount_ = 0;
}

VulkFM::~VulkFM()
{
	for(int i = 0; i <poolCount_; ++i) {
		delete instrumentPool_[i];
	}
	delete instrumentPool_;
	instrumentPool_ = nullptr;

	for(int i = 0; i <activeCount_; ++i) {
		delete activeInstruments_[i].instrument_;
	}
	delete activeInstruments_;
	activeInstruments_ = nullptr;
}


void VulkFM::trigger(int8_t note, int8_t channel, int8_t velocity)
{
	Instrument* inst = nullptr;

	for(int i = 0; i < activeCount_; ++i) {
		if(activeInstruments_[i].note_ == note ) {
			inst = activeInstruments_[i].instrument_;
			break;
		}
	}

	if( inst == nullptr ) {
		inst = getFromPool();
		if(inst != nullptr) { // activate 
			auto &voice = activeInstruments_[activeCount_++];
			voice.note_ = note;
			voice.instrument_ = inst;
		}
	}

	if( inst != nullptr) {
		bool retrig = inst->isActive();
		inst->trigger(note, retrig);
	}
}

void VulkFM::release(int8_t note, int8_t channel, int8_t velocity)
{
	for(int i = 0; i < activeCount_; ++i) {
		if(activeInstruments_[i].note_ == note ) {
			activeInstruments_[i].instrument_->release();
			break;
		}
	}
}

void VulkFM::update(float dt)
{
	for(int i = 0; i < activeCount_; ++i) {
		bool playing =  activeInstruments_[i].instrument_->update(dt);
		if(!playing) {
			returnToPool(activeInstruments_[i].instrument_);
			auto &tmp = activeInstruments_[--activeCount_];
			activeInstruments_[i] = tmp;
		}
	}
}

float VulkFM::evaluate()
{
	float sample = 0;
	for(int i = 0; i < activeCount_; ++i) {
		sample += activeInstruments_[i].instrument_->evaluate()*0.7f;
	}

	outBuffer_[outBufferIdx_++] = sample;
	outBufferIdx_ =  outBufferIdx_ % 1024;

	return sample*0.3f;
}

Instrument* VulkFM::getFromPool()
{
	Instrument* inst = nullptr;
	if(poolCount_ > 0) {
		inst = instrumentPool_[--poolCount_];
	}
	return inst;
}

void VulkFM::returnToPool(Instrument* inst)
{
	instrumentPool_[poolCount_++] = inst;
}


