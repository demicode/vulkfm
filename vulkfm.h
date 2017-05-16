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
	Env( float _attack = 0.05f, float _attackLevel = 1.0f, float _decay = 0.3f, float _sustain = 0.7f, float _release = 0.7f );
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
	float level_;
	int8_t state_;
};



class Osc
{
public:
	Osc();
	void trigger(float _freq, bool retrigger);
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
	Operator();
	Osc osc_;
	Env env_;

	void trigger(float freq, bool retrigger);
	void release();

	bool update(float deltaTime);
	float evaluate(float modulation);
};



class Instrument
{
public:
	Instrument();
	void trigger(int note, bool retrigger);
	void release();
	float evaluate();
	bool update(float dt);
	bool isActive() { return active_; }
	Operator& getOperator(int idx) { return ops[idx]; }

	int currentNote() { return note_; }

protected:

	int8_t order[OP_COUNT] 		{  0,  1,  2,  3 }; 	// order to run operators
	int8_t mods[OP_COUNT] 		{  0,  0,  1,  2 }; 	// modulation input to operators
	int8_t outs[OP_COUNT] 		{  0,  0,  0,  1 }; 	// modulation input to operators
	float  freqscale[OP_COUNT]	{  2, 1.4f, 1.0f, 1.f/3 };

	int note_;

	bool active_;
	Operator ops[OP_COUNT];
	float outputs[OP_COUNT];
};




class VulkFM
{
public:
	VulkFM();

	virtual ~VulkFM();

	void update(float dt);
	float evaluate();

	void trigger(int8_t note, int8_t channel, int8_t velocity);
	void release(int8_t note, int8_t channel, int8_t velocity);

	int activeVoices() { return activeCount_; }

	float* getOutBuffer() { return outBuffer_; }

protected:
	Instrument* getFromPool();
	void returnToPool(Instrument*);

protected:
	Instrument** instrumentPool_;
	int poolCount_;
	Instrument** activeInstruments_;

	int activeCount_;
	int voices_;

	float outBuffer_[1024]; // Used for visualization, nothing else
	int outBufferIdx_;
};



#endif
