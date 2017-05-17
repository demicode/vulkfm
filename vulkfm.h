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

struct EnvConf
{
	float attackLevel 	= 1.0f;			// attack level
	float attack		= 0.05f;		// attack time, 0 -> attack level
	float decay			= 0.2f;			// decay time, atack level -> sustain
	float sustain		= 0.8f;			// sustain level
	float release		= 0.4f; 		// release time, sustain -> 0
};


struct OperatorData
{
	EnvConf env;

	float oscFreq;
	float oscAmp;
	EWaveForm oscWaveform;

	float output;
	float oscPhase;
};



class Env
{
public:
	Env();
	void trigger(const EnvConf* _envConf);
	void release();
	bool update(float dt);
	float evaluate() const;

protected:
	const EnvConf* envConf_;

	float level_;
	int8_t state_;
};


class Osc
{
public:
	Osc();
	void trigger(float _freq, bool retrigger);
	void update(float time);
	float evaluate(float fmodulation) const;

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
	float evaluate(float modulation) const;

	OperatorData data_;
};



struct Algorithm
{
	int8_t operatorCount;     //
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

protected:
	// This should never change by evaluation
	const Algorithm&  algo_;
	float freqscale[OP_COUNT]	{  2.f, 1.f, 0.25f, 3.f, 1.f, 1.f };
	float feedBack_ = 0.3f;

	int note_;

	bool active_;
	Operator ops[OP_COUNT];
	float outputs[OP_COUNT];
};


class Voice
{
public:
	Voice();
	void trigger(int note, Instrument* instrument);
	void retrigger();
	void release();
	float evaluate();
	bool update(float dt);
	bool isActive() { return active_; }
	Instrument* getInstrument(int idx) { return instrument; }


	int currentNote() { return note_; }

protected:
	int note_;
	bool active_;
	Instrument *instrument;
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

	struct ActiveVoice
	{
		int note_;
		Instrument* instrument_;
	};

	Instrument** instrumentPool_;
	int poolCount_;

	ActiveVoice* activeInstruments_;
	int activeCount_;

	int voices_;

	float outBuffer_[1024]; // Used for visualization, nothing else
	int outBufferIdx_;
};



#endif
