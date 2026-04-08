////////////////////////////////////////////////////////////
// PC-8801 OPNA Emulator
//
// Written by Manuke

#ifndef PC88Opna_DEFINED
#define PC88Opna_DEFINED

////////////////////////////////////////////////////////////
// declare

class CPC88Opna;

////////////////////////////////////////////////////////////
// declaration of CPC88Opna

class CPC88Opna {
// typedef
public:
	// type of interrupt vector change callback function
	typedef void (*IntVectChangeCallback)();
	// type of sample output callback function
	//   pSamples : interleaved stereo int16 samples (L, R, L, R, ...)
	//   nFrames  : number of stereo frames in pSamples
	typedef void (*SampleOutputCallback)(
		const int16_t* pSamples, int nFrames);

// enum
public:
	// interrupt bit
	enum {
		INTERRUPT_BIT = 0x10
	};
	// register space size (YM2203 register addresses are 8-bit)
	enum {
		REGISTER_COUNT = 0x100
	};
	// SSG (PSG) constants
	enum {
		SSG_CHANNEL_COUNT = 3,
		SSG_VOL_TABLE_SIZE = 16,
		SSG_ENV_TABLE_SIZE = 32
	};

// attribute
protected:
	// base clock
	static int m_nBaseClock;
	// status
	static uint8_t m_btStatus;
	// address
	static int m_nAddress;
	// CH3 mode
	static int m_nCh3Mode;
	// FM synthesis pre-scaler
	static int m_nPreScalerFM;
	// PSG synthesis pre-scaler
	static int m_nPreScalerPSG;
	// timer A active
	static bool m_bTimerAAcvive;
	// timer A set flag
	static bool m_bTimerASetFlag;
	// timer A value
	static int m_nTimerAValue;
	// timer A counter
	static int m_nTimerACounter;
	// timer A counter max value
	static int m_nTimerACounterMax;
	// timer B active
	static bool m_bTimerBAcvive;
	// timer B set flag
	static bool m_bTimerBSetFlag;
	// timer B value
	static int m_nTimerBValue;
	// timer B counter
	static int m_nTimerBCounter;
	// timer B counter max value
	static int m_nTimerBCounterMax;
	// OPNA interrupt requested
	static bool m_bOpnaInterruptRequest;

	// interrupt vector change callback function
	static IntVectChangeCallback m_pIntVectChangeCallback;

	// ----- Phase C synthesis state (Phase C-準備で導入) -----
	// Mirror of all written FM/SSG register values. The synthesis engine
	// (added incrementally in Phase C-SSG and Phase C-FM) reads from
	// here. Timer and prescaler registers are still handled by the
	// dedicated members above; the mirror keeps a copy too so debugging
	// and full-state dumps work uniformly.
	static uint8_t m_abtRegisters[REGISTER_COUNT];

	// Output sample rate in Hz (set by the frontend before Reset()).
	static int m_nSampleRate;
	// Pre-computed cycles-per-output-sample, in 8.8 fixed point. The
	// emulator thread accumulates Z80 cycles passed to PassClock() and
	// emits one sample whenever the accumulator crosses one full sample
	// worth of cycles.
	static int m_nCyclesPerSampleX256;
	// Fractional cycle accumulator (8.8 fixed point).
	static int m_nSampleAccumX256;

	// Sample output callback (set by the frontend; may be NULL).
	static SampleOutputCallback m_pSampleOutputCallback;

	// ----- SSG (PSG) synthesis state (Phase C-SSG) -----
	// SSG section is clocked at base_clock / prescaler_psg / 16, which
	// is about 62.5 kHz for the typical PC-8801 configuration. The
	// fractional accumulator tracks how many internal SSG ticks should
	// fire per output sample, in 16.16 fixed point.
	static int m_nSsgTicksPerSampleX16;
	static int m_nSsgTickAccumX16;
	// Per-channel tone state.
	static int  m_anSsgTonePeriod[SSG_CHANNEL_COUNT];   // 12-bit
	static int  m_anSsgToneCounter[SSG_CHANNEL_COUNT];  // current countdown
	static int  m_anSsgToneState[SSG_CHANNEL_COUNT];    // 0 or 1 (square wave high)
	// Noise generator (single, shared by all channels via the mixer).
	static int      m_nSsgNoisePeriod;     // 5-bit
	static int      m_nSsgNoiseCounter;
	static uint32_t m_nSsgNoiseLfsr;       // 17-bit, taps 0 ^ 3
	static int      m_nSsgNoiseState;      // 0 or 1
	// Envelope generator.
	static int  m_nSsgEnvPeriod;          // 16-bit
	static int  m_nSsgEnvCounter;
	static int  m_nSsgEnvLevel;           // 0..31
	static int  m_nSsgEnvDir;             // +1 or -1
	static bool m_bSsgEnvHolding;
	// Per-channel volume / envelope-use flag (from registers $08..$0A).
	static int  m_anSsgVolume[SSG_CHANNEL_COUNT];   // 0..15
	static bool m_abSsgUseEnv[SSG_CHANNEL_COUNT];
	// Mixer (from register $07): tone disable bits 0..2, noise disable bits 3..5
	static uint8_t m_btSsgMixer;
	// Pre-computed amplitude tables (Initialize() generates from formulas).
	static int m_anSsgVolTable[SSG_VOL_TABLE_SIZE];   // 16 levels, ~1.5 dB/step
	static int m_anSsgEnvTable[SSG_ENV_TABLE_SIZE];   // 32 levels, ~0.75 dB/step

public:
	// get base clock
	static int GetBaseClock() {
		return m_nBaseClock;
	}
	// set base clock
	static void SetBaseClock(int nBaseClock) {
		m_nBaseClock = nBaseClock;
	}
	// is OPNA interrupt requested
	static bool IsOpnaInterruptRequest() {
		return m_bOpnaInterruptRequest;
	}
	// set OPNA interrupt requested
	static void SetOpnaInterruptRequest(bool bOpnaInterruptRequest) {
		m_bOpnaInterruptRequest = bOpnaInterruptRequest;
	}

	// set interrupt vector change callback function
	static void SetIntVectChangeCallback(
		IntVectChangeCallback pIntVectChangeCallback)
	{
		m_pIntVectChangeCallback = pIntVectChangeCallback;
	}

	// set sample output callback function (Phase C-準備)
	static void SetSampleOutputCallback(
		SampleOutputCallback pSampleOutputCallback)
	{
		m_pSampleOutputCallback = pSampleOutputCallback;
	}

	// set output sample rate (Phase C-準備). Must be called before
	// PassClock() if changed from the default 44100 Hz.
	static void SetSampleRate(int nSampleRate);

	// read a mirrored register value (for debug / state inspection)
	static uint8_t GetRegister(int nAddress) {
		if ((nAddress < 0) || (nAddress >= REGISTER_COUNT)) {
			return 0;
		}
		return m_abtRegisters[nAddress];
	}

// create & destroy
public:
	// default constructor
	CPC88Opna();
	// destructor
	~CPC88Opna();

// initialize
public:
	// initialize at first
	static void Initialize();
	// reset
	static void Reset();

// operation
protected:
	// update interrupt request
	static void UpdateInterruptRequest(bool bOpnaInterruptRequest) {
		if (bOpnaInterruptRequest != m_bOpnaInterruptRequest) {
			m_bOpnaInterruptRequest = bOpnaInterruptRequest;
			m_pIntVectChangeCallback();
		}
	}

public:
	// pass clock
	static void PassClock(int nClock) {
		if (m_bTimerAAcvive) {
			if ((m_nTimerACounter -= nClock) <= 0) {
				TimerAOverFlow();
			}
		}
		if (m_bTimerBAcvive) {
			if ((m_nTimerBCounter -= nClock) <= 0) {
				TimerBOverFlow();
			}
		}
		// Phase C-準備: sample-rate conversion accumulator. Each Z80
		// cycle nudges the fractional sample position; once we have
		// accumulated at least one sample worth of cycles, we emit
		// samples in a small batch via Generate().
		AdvanceSampleAccumulator(nClock);
	}

	// Generate nFrames stereo int16 samples and push them to the
	// frontend through the sample output callback. In Phase C-準備
	// this just emits silence; Phase C-SSG / Phase C-FM will fill in
	// the actual synthesis.
	static void Generate(int nFrames);

protected:
	// Internal: advance the sample accumulator by nClock Z80 cycles
	// and emit complete samples as they accumulate.
	static void AdvanceSampleAccumulator(int nClock);
	// Internal: build SSG amplitude tables from log-domain formulas.
	static void BuildSsgTables();
	// Internal: recompute the SSG ticks-per-sample ratio whenever
	// base clock, prescaler, or sample rate changes.
	static void UpdateSsgTickRate();
	// Internal: advance one SSG tick (tone, noise, envelope state).
	static void AdvanceSsgOneTick();
	// Internal: produce one stereo sample for the SSG section, mixing
	// the three SSG channels into a single int16 value (mono → both
	// channels written equally).
	static int RenderSsgSample();
	// Internal: handle a register write that targets the SSG section
	// ($00–$0F). Called from WriteData().
	static void OnSsgRegisterWrite(int nAddress, uint8_t btData);

public:
	// timer A overflow
	static void TimerAOverFlow();
	// timer B overflow
	static void TimerBOverFlow();
	// set timer A counter max value
	static void SetTimerACounterMax();
	// set timer B counter max value
	static void SetTimerBCounterMax();
	// set pre-scaler
	static void SetPreScaler(int nPreScalerFM, int nPreScalerPSG);
	// read status
	static uint8_t ReadStatus() {
		return m_btStatus;
	}
	// read data
	static uint8_t ReadData();
	// write address
	static void WriteAddress(uint8_t btAddress);
	// write data
	static void WriteData(uint8_t btData);
};

#endif // PC88Opna_DEFINED
