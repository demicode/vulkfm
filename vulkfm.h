#if !defined(VULKFM_H_)
#define VULKFM_H_

#include <cstdint>

#define OP_COUNT 4

#define ACONST 	1.059463094359f


enum EWaveForm
{
	Sine,
	Square,
	ClampSine,
	AbsSine,
};



class Env
{
public:
	Env( float _attack = 0.15f, float _attackLevel = 1.0f, float _decay = 0.3f, float _sustain = 0.3f, float _release = 0.3f );
	void trigger();

	void release();

	bool update(float dt);
	float evaluate();


public:
	float attackLevel_;	// attack level
	float attack_;		// attack time, 0 -> attack level
	float decay_;		// decay time, atack level -> sustain
	float sustain_;		// sustain level
	float release_;		// release time, sustain -> 0


protected:
	bool held_ = true;
	float releaseTime_;
	float time_;
};



class Osc
{
public:
	Osc(float _freq = 440, float _amp = 1.0f, EWaveForm _waveform = Sine);
	void trigger(float _freq);
	void update(float time);
	float evaluate(float fmodulation);

public:
	float freq;
	float amp;
	EWaveForm waveform;

protected:
	float phase_;
};


class Operator
{
public:
	Osc osc_;
	Env env_;

	float time_;
	float release_;
	bool held_;

	void trigger(float freq);
	void release();

	bool update(float deltaTime);
	float evaluate(float modulation);
};



class Instrument
{
public:
	Instrument();
	void trigger(int note);
	void release();
	float evaluate();
	bool update(float dt);
	bool isActive() { return active_; }
	Operator& getOperator(int idx) { return ops[idx]; }

protected:

	int8_t order[OP_COUNT] 		{  0, 1, -1, -1 }; 	// order to run operators
	int8_t mods[OP_COUNT] 		{  -1, -1, -1, -1 }; 	// modulation input to operators
	int8_t outs[OP_COUNT] 		{  0, 1,  0,  0 }; 	// modulation input to operators
	float  freqscale[OP_COUNT]	{  3.f/2, 1, 0, 0 };

	bool active_;
	Operator ops[OP_COUNT];
	float outputs[OP_COUNT];
};




class VulkFM
{
public:
	VulkFM()
	{
		channels_ = 16;
		instrumentPool_ = new Instrument*[channels_];
		for(int i = 0; i < channels_; ++i)
			instrumentPool_[i] = new Instrument();
		poolCount_ = channels_;
	}

protected:
	Instrument** instrumentPool_;
	int poolCount_;
	int channels_;
};



#endif
