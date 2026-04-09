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
	// FM (YM2203) constants
	enum {
		FM_CHANNEL_COUNT  = 3,
		FM_OP_PER_CHANNEL = 4,
		FM_OPERATOR_COUNT = FM_CHANNEL_COUNT * FM_OP_PER_CHANNEL,  // 12
		// Phase accumulator: 20 fractional bits over a full sin period.
		// Top bits index the sin table after taking the upper portion.
		FM_PHASE_BITS     = 20,
		// Sin / Exp table sizes (1/4 sin period, 256-step log domain).
		FM_SIN_TABLE_SIZE = 256,
		FM_EXP_TABLE_SIZE = 256,
		// Envelope rate table covers all valid effective rate values.
		FM_ENV_RATE_TABLE_SIZE = 64,
		// Detune table dimensions, matching the YM2608 application
		// manual Table 2-6: indexed by (BLOCK, NOTE, FD) where NOTE
		// is the 2-bit (N4,N3) derived from F-Number top bits and FD
		// is the 2-bit detune magnitude (sign handled separately).
		FM_DT_BLOCKS = 8,
		FM_DT_NOTES  = 4,
		FM_DT_FDS    = 4
	};
	// FM envelope generator state values
	enum {
		FM_ENV_OFF     = 0,
		FM_ENV_ATTACK  = 1,
		FM_ENV_DECAY   = 2,
		FM_ENV_SUSTAIN = 3,
		FM_ENV_RELEASE = 4
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

	// Per-section mute (Debug menu — Generate() still updates state).
	static bool m_bFmMute;
	static bool m_bSsgMute;
	// Per-channel mute (Debug menu). Indexed by 0..2.
	// State is still advanced — only the contribution to the output
	// sample is suppressed, so timers / envelopes / phase accumulators
	// stay coherent across mute toggles.
	static bool m_abFmChMute[FM_CHANNEL_COUNT];
	static bool m_abSsgChMute[SSG_CHANNEL_COUNT];

	// ----- SSG (PSG) synthesis state (Phase C-SSG) -----
	// SSG internal counters advance at base_clock / prescaler_psg / 4,
	// which is about 250 kHz for the typical PC-8801 configuration.
	// (See docs/YM2203.md for the rationale — the literal "fc/16" in
	// the YM2149 datasheet refers to the full waveform period
	// multiplier, not the per-tick divider.) The fractional
	// accumulator tracks how many internal SSG ticks should fire per
	// output sample, in 16.16 fixed point.
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
	// YM2149 datasheet: vol register is 4-bit log with 0.707 voltage
	// ratio between adjacent levels (= 3 dB/step). The 32-step
	// envelope generator is 5-bit (= 1.5 dB/step, half the volume
	// register's resolution).
	static int m_anSsgVolTable[SSG_VOL_TABLE_SIZE];   // 16 levels, 3 dB/step
	static int m_anSsgEnvTable[SSG_ENV_TABLE_SIZE];   // 32 levels, 1.5 dB/step

	// ----- FM (YM2203) synthesis state (Phase C-FM) -----
	//
	// One SFmOperator per slot (4 ops × 3 channels = 12 active ops). All
	// values are derived from the register mirror via OnFmRegisterWrite()
	// at write time, so the per-sample render path can read them directly
	// without bit twiddling on every sample.
	struct SFmOperator {
		// Phase generator (FM_PHASE_BITS-bit fixed point).
		uint32_t nPhase;
		uint32_t nPhaseInc;
		// Envelope generator.
		int      nEnvLevel;        // 0..1023, attenuation (large = quiet)
		int      nEnvState;        // FM_ENV_*
		int      nEnvCounter;      // rate timing accumulator
		// Slot parameters (decoded from registers).
		uint8_t  btTl;             // 0..127 (Total Level, 0 = loudest)
		uint8_t  btAr, btDr, btSr, btRr;
		uint8_t  btSl;             // 0..15
		uint8_t  btKs;             // 0..3
		uint8_t  btMul;            // 0..15
		uint8_t  btDt;             // 0..7 (3-bit signed-magnitude detune)
		bool     bKeyOn;
		// SSG-type Envelope Control ($90-$9E, manual section 2-5-2).
		// btSsgEg holds the low nibble of $90:
		//   bit 3 = enable (1 = SSG-EG mode active)
		//   bit 2 = Attack (Att) — start direction
		//   bit 1 = Alternate (Alt)
		//   bit 0 = Hold
		// bSsgEgInverted is the runtime "envelope is being read in
		// inverted polarity" flag, toggled at envelope boundaries
		// when the shape demands a triangle/up-saw / hold-at-peak.
		uint8_t  btSsgEg;
		bool     bSsgEgInverted;
		// Most recent linear sample output, used by ALGO 0..6 for the
		// "previous operator output" modulation chain and by OP1 self-
		// feedback in all algorithms.
		int      nOutPrev;
	};

	struct SFmChannel {
		// Operator array, indexed by physical operator number 0..3
		// (= OP1..OP4 in 1-based notation). The 1-3-2-4 slot layout in
		// the register space is unwound by the dispatch in
		// OnFmRegisterWrite(), so this array is in the natural order.
		SFmOperator aOp[FM_OP_PER_CHANNEL];
		// Channel-wide parameters.
		uint16_t wFnum;            // 11-bit
		uint8_t  btBlock;          // 0..7
		uint8_t  btAlgo;           // 0..7
		uint8_t  btFb;             // 0..7 (OP1 self-feedback amount)
		// OP1 self-feedback history. Two samples averaged into the OP1
		// phase modulation input on the next sample.
		int      anFeedback[2];
		// CH3 special mode storage (only used by ch[2]).
		// op0/op1/op2 use these per-operator F-Number/Block values when
		// special mode is active. op3 always uses wFnum/btBlock above.
		uint16_t awFnumPerOp[3];
		uint8_t  abtBlockPerOp[3];
	};

	static SFmChannel m_aFmCh[FM_CHANNEL_COUNT];
	// CH3 special mode flag, mirrored from $27 bits 6-7 (any non-zero
	// value enables it; we don't distinguish between the three sub-modes
	// because they only differ in trigger semantics on real hardware).
	static bool m_bFmCh3SpecialMode;

	// F-Number high latch. The YM2203 implements F-Num/Block as two
	// register groups: $A4-$A6 (BLOCK + FNUM[10:8]) and $A0-$A2
	// (FNUM[7:0]). Real hardware does NOT update the channel until the
	// LOW byte is written; the HIGH byte just goes into a per-write
	// temp latch. Software must therefore write HIGH first, then LOW
	// to commit. We mirror this so that interleaved writes don't
	// produce a momentarily wrong phase increment.
	static uint8_t m_abtFmFnumLatch[FM_CHANNEL_COUNT];   // BLOCK<<3 | FNUM[10:8]
	static uint8_t m_abtFmCh3FnumLatch[3];               // CH3 special, OP1..OP3

	// FM section internal clock conversion. The FM section advances at
	// master_clock / (prescaler_fm * 6) Hz; we accumulate Z80-cycle
	// equivalents per output sample using a 16.16 fixed-point counter.
	// (Per-sample we step the FM operator phase by nPhaseInc directly,
	// so the cycle accumulator is currently informational only.)
	static int m_nFmTicksPerSampleX16;

	// 16.16 fixed-point scale factor used by RecomputeFmOperatorPhaseInc
	// to convert (FNUM << BLOCK) × MUL into a per-output-sample phase
	// increment. Derived from:
	//
	//   phase_inc = FNUM × 2^BLOCK × master_clock /
	//               (prescaler_fm × 24 × sample_rate)
	//
	// which is the YM2203 spec frequency formula
	//   f_out = FNUM × master / (144 × 2^(20-BLOCK))
	// rearranged into "phase advance per output sample" units, where
	// the 20-bit phase accumulator covers one full sin period.
	static int m_nFmPhaseScaleX16;

	// Pre-computed FM tables (BuildFmTables() generates them at startup).
	// All entries derived from formulas — no copied constant tables.
	static int m_anFmSinTable[FM_SIN_TABLE_SIZE];      // 1/4 period, log domain
	static int m_anFmExpTable[FM_EXP_TABLE_SIZE];      // log → linear conversion
	static int m_anFmEnvRateTable[FM_ENV_RATE_TABLE_SIZE]; // rate → counter inc per sample
	// Detune phase increment lookup, populated by UpdateFmTickRate
	// from a milli-Hz constant table (Table 2-6 of the YM2608
	// application manual). Indexed by [BLOCK][NOTE][FD] and stored as
	// a per-output-sample phase increment offset (positive; sign is
	// applied at runtime from the DT field's bit 2).
	static int m_anFmDetunePhaseInc[FM_DT_BLOCKS][FM_DT_NOTES][FM_DT_FDS];

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

	// per-section mute (Debug menu). The synthesis still runs so state
	// stays consistent — only the contribution to the output sample is
	// suppressed.
	static void SetFmMute(bool bMute)  { m_bFmMute  = bMute; }
	static void SetSsgMute(bool bMute) { m_bSsgMute = bMute; }
	static bool GetFmMute()  { return m_bFmMute; }
	static bool GetSsgMute() { return m_bSsgMute; }
	// Per-channel mute (Debug menu). Index 0..2 for FM ch 1..3 / SSG ch A..C.
	static void SetFmChMute(int nCh, bool bMute) {
		if ((nCh >= 0) && (nCh < FM_CHANNEL_COUNT)) m_abFmChMute[nCh] = bMute;
	}
	static bool GetFmChMute(int nCh) {
		if ((nCh < 0) || (nCh >= FM_CHANNEL_COUNT)) return false;
		return m_abFmChMute[nCh];
	}
	static void SetSsgChMute(int nCh, bool bMute) {
		if ((nCh >= 0) && (nCh < SSG_CHANNEL_COUNT)) m_abSsgChMute[nCh] = bMute;
	}
	static bool GetSsgChMute(int nCh) {
		if ((nCh < 0) || (nCh >= SSG_CHANNEL_COUNT)) return false;
		return m_abSsgChMute[nCh];
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

	// ----- Phase C-FM internal helpers -----
	// Build sin/exp/envelope-rate/detune tables from formulas. Called
	// once from Initialize().
	static void BuildFmTables();
	// Reset all FM operator and channel state. Called from Reset()
	// after the SSG state has been cleared.
	static void ResetFmState();
	// Recompute the FM ticks-per-output-sample ratio whenever base
	// clock, prescaler_fm, or sample rate changes.
	static void UpdateFmTickRate();
	// Recompute one operator's nPhaseInc from the channel's F-number,
	// block, MUL, and DT values. Called whenever any of those change.
	static void RecomputeFmOperatorPhaseInc(int nChannel, int nOpIndex);
	// Recompute phase increments for all operators of a given channel
	// (used after F-number / block writes that affect every op).
	static void RecomputeFmChannelPhaseIncs(int nChannel);
	// Handle a register write to any FM-related address ($28, $30–$9E,
	// $A0–$AE, $B0–$B2). Called from WriteData() after the register
	// mirror has been updated.
	static void OnFmRegisterWrite(int nAddress, uint8_t btData);
	// Handle a write to register $28 (key on/off mask + channel index).
	static void OnFmKeyOnOff(uint8_t btData);
	// Map an address in $30–$9E to (channel, op_index_0_based) using
	// the YM-family 1-3-2-4 slot order. Returns false if the address
	// targets the unused slot column (channel 3 / op4 of an absent
	// fourth channel).
	static bool ResolveFmSlotAddress(int nAddress, int& nChannel, int& nOpIndex);
	// Compute one operator output sample. nModulation is added to the
	// 10-bit phase index in the same units the YM family uses (= the
	// previous operator's linear output, suitably shifted).
	// Returns a signed linear sample roughly in [-FM_OP_MAX_LINEAR,
	// +FM_OP_MAX_LINEAR].
	static int RenderFmOperator(SFmOperator& op, int nModulation);
	// Advance one operator's envelope state machine by one sample.
	// nKsr is the precomputed key-scaling rate offset (0..3) for this
	// operator's current channel pitch.
	static void AdvanceFmEnvelope(SFmOperator& op, int nKsr);
	// SSG-EG endpoint handler. Called by AdvanceFmEnvelope when an
	// operator's env_level reaches 1023 (silent end) while running
	// the SUSTAIN phase. If SSG-EG is enabled on this operator, the
	// shape's loop / alternate / hold rules are applied and `true`
	// is returned (caller must skip the normal "transition to OFF"
	// path). Returns `false` if SSG-EG is not enabled.
	static bool ApplySsgEgEndpoint(SFmOperator& op);
	// Compute the key-scale rate offset for a channel.
	static int ComputeFmKsr(int nChannel, int nOpIndex);
	// Render one channel's mono sample, including all 8 algorithms
	// and OP1 self-feedback. Returns a signed int (caller mixes).
	static int RenderFmChannel(int nChannel);
	// Render one mono FM sample by mixing all 3 channels.
	static int RenderFmSample();

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
