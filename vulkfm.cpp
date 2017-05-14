
#include "vulkfm.h"

#include <cmath>
#include <cstdio>
#include <cassert>

#define TAU (2*M_PI)

static inline float clamp01f(float v) { return (v<0?0:(v>1.f?1.0f:v)); }


Osc::Osc(float _freq, float _amp, EWaveForm _waveform)
: freq(_freq)
, amp(_amp)
, waveform(_waveform)
{}


void Osc::trigger(float _freq)
{
	//printf("trigger osc @%fHz, amp %f, waveform %d\n", freq, amp, waveform );
	fflush(stdout);
	freq = _freq;
	//7phase_ = 0;
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

	switch(waveform)
	{
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

// Set correct release time if release was called before time > attak+decay
void Env::release() { held_ = false; releaseTime_ = fmaxf(time_, (attack_+decay_)); }

bool Env::update(float dt)
{
	time_ += dt;
	if( !held_ )
	{
		float sustainTime = (time_-releaseTime_);
		if( sustainTime > 0)
			return (time_-releaseTime_) < release_;
	}
	return true;
}

float Env::evaluate()
{
	float t = time_;
	if( t < attack_ ) return (t/attack_) * attackLevel_;
	t -= attack_;
	if( t < decay_ ) return attackLevel_ - ((t/decay_) * (attackLevel_ - release_));
	t -= decay_;
	if( held_ ) { 
		return sustain_;
	} else {
		float timeFromRelease = ( time_- releaseTime_);
		return (1.0f - (timeFromRelease/releaseTime_)) * sustain_;
	}
}



void Operator::trigger(float freq = 440) { time_ = 0.f; held_ = true; osc_.trigger(freq); env_.trigger(); }
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
	// printf("%0.3f\n",e);
	return e*sample;
}

Instrument::Instrument()
{
	active_ = false;
}

void Instrument::trigger(int note)
{
	int freqDiff = note-57; // 57 is note for A4 (440Hz)

	float baseFreq = 440.f * powf( ACONST, freqDiff);


	for(int i = 0; i < OP_COUNT; ++i)
	{
		float fScale = freqscale[i];
		if( fScale != 0)
			ops[i].trigger(baseFreq * fScale);
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
	bool playing = true;
	if( active_ )
	{
		for(int i = 0; i < OP_COUNT; ++i)
		{
			if(freqscale[i] != 0)
				playing |= ops[i].update(dt);
		}
		active_ = playing;
	}
	return playing;
}

