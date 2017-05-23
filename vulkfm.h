#if !defined(VULKFM_H_)
#define VULKFM_H_

#include <cstdint>

#define OP_COUNT 6
#define MAX_EVENTS 16
#define ACONST 	1.059463094359f


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


struct OperatorConf
{
	EnvConf env;
	float oscFreq = 0;
	float oscAmp = 1.0f;
	float freqScale = 1.0f;
	EWaveForm oscWaveform = EWaveForm::Sine;
	uint8_t modulators = 0; 		// Bit mask for what operator out to use for modulation of this one.
};

struct Algorithm
{
	int8_t operatorCount;    //
	uint8_t mods[OP_COUNT];  // bit flags for what input modules the operator
	uint8_t outs;			// bit flags for what operators should be mixed to out
};


struct Instrument
{
	void setAlgorithm(const Algorithm* _algo) { algo_ = _algo; }
	const Algorithm*  algo_;
	OperatorConf opConf_[OP_COUNT];
	int serialize(uint8_t* buffer, int maxSize) const;
};

extern Algorithm defaultAlgorithm;



class Env
{
public:
	Env();
	void trigger(const EnvConf* _envConf);
	void retrigger();
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
	void trigger(float _freq, const OperatorConf *opCont);
	void update(float time);
	float evaluate(float fmodulation) const;

protected:
	const OperatorConf* opConf_;
	float freq_;
	float phase_;
};


class Operator
{
public:
	Operator();

	void trigger(float freq, const OperatorConf *opConf);
	void retrigger();
	void release();

	bool update(float deltaTime);
	float evaluate(float modulation) const;

protected:
	Osc osc_;
	Env env_;
};


class Voice
{
public:
	Voice();
	void trigger(int note, const Instrument* instrument);
	void retrigger();
	void release();
	float evaluate();
	bool update(float dt);
	bool isActive()		{ return active_; }
	int currentNote()	{ return note_; }

	const Instrument *inst_;

protected:
	int opCount_;
	int note_;
	bool active_;

	Operator ops_[OP_COUNT];

	float outs_[OP_COUNT];
};


class VulkFM
{
protected:
	struct ActiveVoice {
		int note_;
		Voice* voice_;
	};

	enum EEvent {
		None,
		Trigger,
		Release,
	};

	struct NoteEvent {
		int8_t note_ = 0;
		int8_t ch_ = 0;
		int8_t vel_ = 0;
		EEvent event_ = EEvent::None;
	};


public:
	VulkFM();

	virtual ~VulkFM();

	void update(float dt);
	float evaluate();

	void trigger(int8_t note, int8_t channel, int8_t velocity);
	void release(int8_t note, int8_t channel, int8_t velocity);

	int activeVoices() { return activeCount_; }

	float* getOutBuffer() { return outBuffer_; }

	Instrument* getInstrument(int /*idx*/) { return activeInstrument_; }

protected:
	Voice* getFromPool();
	void returnToPool(Voice*);

	void handleEvent(const struct NoteEvent&);
	Instrument* getInstrumentByChannel(int /*channel*/) { return activeInstrument_; }

protected:
	Instrument* activeInstrument_;

	struct NoteEvent eventList_[MAX_EVENTS];
	uint16_t eventHead_ = 0;
	uint16_t eventTail_ = 0;

	Voice** voicePool_;
	int poolCount_;

	ActiveVoice* activeVoices_;
	int activeCount_;

	int voices_;

	float outBuffer_[1024]; // Used for visualization, nothing else
	int outBufferIdx_;
};

#endif
