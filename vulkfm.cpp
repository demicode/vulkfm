
#include "vulkfm.h"

#include <cmath>
#include <cstdio>
#include <cassert>

#define TAU (2*M_PI)

static inline float clamp01f(float v) { return (v<0?0:(v>1.f?1.0f:v)); }


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

float Osc::evaluate(float fmodulation)
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


Env::Env( float _attack, float _attackLevel, float _decay, float _sustain, float _release )
: attackLevel_( _attackLevel )
, attack_( _attack )
, decay_(_decay)
, sustain_(_sustain)
, release_( _release )
{

}

void Env::trigger()
{
	held_ = true;
	time_ = 0.f;
}

void Env::release() { held_ = false; releaseTime_ = time_; }

bool Env::update(float dt)
{
	time_ += dt;
	if( !held_ ) {
		float sustainTime = (time_-releaseTime_);
		if( sustainTime > 0 )
			return sustainTime < release_;
	}
	return true;
}

float Env::evaluate()
{
	float t = time_;
	if( held_ )
	{
		if( t < attack_ ) 	return (t/attack_) * attackLevel_;
		t -= attack_;
		if( t < decay_ ) 	return attackLevel_ - ((t/decay_) * (attackLevel_ - sustain_));
		else return sustain_;
	} else {
		float now = ( time_- releaseTime_);
		return fmaxf(0,(1.0f - (now/release_)) * sustain_);
	}
}


Operator::Operator()
{
}


void Operator::trigger(float freq, bool retrigger) { time_ = 0.f; held_ = true; osc_.trigger(freq, retrigger); env_.trigger(); }

void Operator::release() { held_ = false; env_.release(); }

bool Operator::update(float deltaTime)
{
	bool done = env_.update(deltaTime);
	osc_.update(deltaTime);
	time_ += deltaTime;
	return done;
}


float Operator::evaluate(float modulation)
{
	float sample = osc_.evaluate(modulation);
	float e = env_.evaluate();
	return e*sample;
}

Instrument::Instrument()
{
	active_ = false;
}

void Instrument::trigger(int note, bool retrigger = false)
{
	int freqDiff = note-57; // 57 is note for A4 (440Hz)
	note_ = note;

	float baseFreq = 440.f * powf( ACONST, freqDiff);


	for(int i = 0; i < OP_COUNT; ++i)
	{
		float fScale = freqscale[i];
		if( fScale != 0)
			ops[i].trigger(baseFreq * fScale, retrigger);
	}
	active_ = true;
}

void Instrument::release()
{
	for(int i = 0; i < OP_COUNT; ++i)
		ops[i].release();
}

float Instrument::evaluate()
{
	float output = 0;
	if(active_) {
		for(int i = 0; i < OP_COUNT; ++i) {
			auto opIdx = order[i];
			if(i < 0 )
				continue;
			float modulation = 0;
			if(mods[opIdx] >= 0) {
				modulation = outputs[mods[opIdx]];
			}
			outputs[opIdx] = ops[opIdx].evaluate(modulation);
		}

		for(int i = 0; i < OP_COUNT; ++i) {
			if(outs[i])	output += outputs[i];
		}
	}
	return output;
}

bool Instrument::update(float dt)
{
	bool playing = false;

	if( active_ )
	{
		for(int i = 0; i < OP_COUNT; ++i)
		{
			playing |= ops[i].update(dt) && (outs[i] != 0);
		}
		active_ = playing;
	}
	return playing;
}



VulkFM::VulkFM()
{
	voices_ = 32;
	instrumentPool_ = new Instrument*[voices_];
	activeInstruments_ = new Instrument*[voices_];

	for(int i = 0; i < voices_; ++i)
		instrumentPool_[i] = new Instrument();
	poolCount_ = voices_;
	activeCount_ = 0;
}

VulkFM::~VulkFM()
{
	for(int i = 0; i <poolCount_; ++i)
	{
		delete instrumentPool_[i];
	}
	delete instrumentPool_;
	instrumentPool_ = nullptr;

	for(int i = 0; i <activeCount_; ++i)
	{
		delete activeInstruments_[i];
	}
	delete activeInstruments_;
	activeInstruments_ = nullptr;
}


void VulkFM::trigger(int8_t note, int8_t channel, int8_t velocity)
{
	Instrument* inst = nullptr;

	for(int i = 0; i < activeCount_; ++i)
	{
		if(activeInstruments_[i]->currentNote() == note )
		{
			inst = activeInstruments_[i];
			break;
		}
	}

	if( inst == nullptr )
	{
		inst = getFromPool();
		if(inst != nullptr) // activate
			activeInstruments_[activeCount_++] = inst;
	}

	if( inst != nullptr)
	{
		bool retrig = inst->isActive();
		inst->trigger(note, retrig);
	}
}

void VulkFM::release(int8_t note, int8_t channel, int8_t velocity)
{
	for(int i = 0; i < activeCount_; ++i)
	{
		if(activeInstruments_[i]->currentNote() == note )
		{
			activeInstruments_[i]->release();
			break;
		}
	}
}

void VulkFM::update(float dt)
{
	for(int i = 0; i < activeCount_; ++i)
	{
		bool playing =  activeInstruments_[i]->update(dt);
		if(!playing)
		{
			returnToPool(activeInstruments_[i]);
			auto tmp = activeInstruments_[--activeCount_];
			activeInstruments_[activeCount_] = nullptr;
			activeInstruments_[i] = tmp;
		}
	}
}

float VulkFM::evaluate()
{
	float sample = 0;
	for(int i = 0; i < activeCount_; ++i)
	{
		sample += activeInstruments_[i]->evaluate()*0.7f;
	}
	return sample*0.5f;
}

Instrument* VulkFM::getFromPool()
{
	Instrument* inst = nullptr;
	if(poolCount_ > 0)
	{
		inst = instrumentPool_[--poolCount_];
	}
	return inst;
}

void VulkFM::returnToPool(Instrument* inst)
{
	instrumentPool_[poolCount_++] = inst;
}


