#if !defined(VULKFM_H_)
#define VULKFM_H_

#include <cstdint>

#define OP_COUNT 6

#define ACONST 	1.059463094359f


struct Algorithm;

extern Algorithm defaultAlgorithm;

enum EWaveForm
{
	Sine,
	Square,
	ClampSine,
	AbsSine,
};



struct Env
{
	Env( float _attack = 0.05f, float _attackLevel = 1.0f, float _decay = 0.3f, float _sustain = 0.7f, float _release = 1.7f );

	void trigger();
	void release();

	bool update(float dt);
	float evaluate();

	float attackLevel_;	// attack level
	float attack_;		// attack time, 0 -> attack level
	float decay_;		// decay time, atack level -> sustain
	float sustain_;		// sustain level
	float release_;		// release time, sustain -> 0

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



struct OperatorData
{
	Env env;

	float oscFreq;
	float oscAmp;
	EWaveForm oscWaveform;

	float output;

	float oscPhase;
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

	OperatorData data_;
};



struct Algorithm
{
	int8_t operatorCount;     //
	int8_t order[OP_COUNT];  //		{  0,  1,  2,  3 }; 	// order to run operators
	int8_t mods[OP_COUNT];  //		{  0,  -1,  -1,  -1 }; 	// modulation input to operators
	int8_t outs[OP_COUNT]; //		{  1,  0,  0,  0 }; 	// modulation input to operators
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
	const Algorithm&  algo_;
	float freqscale[OP_COUNT]	{  1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
	float feedBack_ = 0.3f;

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
