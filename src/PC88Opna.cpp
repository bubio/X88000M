////////////////////////////////////////////////////////////
// PC-8801 OPNA Emulator
//
// Written by Manuke

////////////////////////////////////////////////////////////
// include

#include "StdHeader.h"

#include "PC88Opna.h"

#include <math.h>

////////////////////////////////////////////////////////////
// implementation of CPC88Opna

////////////////////////////////////////////////////////////
// attribute

// base clock

int CPC88Opna::m_nBaseClock;

// status

uint8_t CPC88Opna::m_btStatus;

// address

int CPC88Opna::m_nAddress;

// CH3 mode

int CPC88Opna::m_nCh3Mode;

// FM synthesis pre-scaler

int CPC88Opna::m_nPreScalerFM;

// PSG synthesis pre-scaler

int CPC88Opna::m_nPreScalerPSG;

// timer A active

bool CPC88Opna::m_bTimerAAcvive;

// timer A set flag

bool CPC88Opna::m_bTimerASetFlag;

// timer A value

int CPC88Opna::m_nTimerAValue;

// timer A counter

int CPC88Opna::m_nTimerACounter;

// timer A counter max value

int CPC88Opna::m_nTimerACounterMax;

// timer B active

bool CPC88Opna::m_bTimerBAcvive;

// timer B set flag

bool CPC88Opna::m_bTimerBSetFlag;

// timer B value

int CPC88Opna::m_nTimerBValue;

// timer B counter

int CPC88Opna::m_nTimerBCounter;

// timer B counter max value

int CPC88Opna::m_nTimerBCounterMax;

// OPNA interrupt requested

bool CPC88Opna::m_bOpnaInterruptRequest;

// interrupt vector change callback function

CPC88Opna::IntVectChangeCallback CPC88Opna::m_pIntVectChangeCallback;

// ----- Phase C synthesis state -----

// Mirror of all written register values

uint8_t CPC88Opna::m_abtRegisters[CPC88Opna::REGISTER_COUNT];

// Output sample rate

int CPC88Opna::m_nSampleRate;

// Cycles per output sample (8.8 fixed point)

int CPC88Opna::m_nCyclesPerSampleX256;

// Sample accumulator (8.8 fixed point)

int CPC88Opna::m_nSampleAccumX256;

// Sample output callback

CPC88Opna::SampleOutputCallback CPC88Opna::m_pSampleOutputCallback;
bool CPC88Opna::m_bFmMute  = false;
bool CPC88Opna::m_bSsgMute = false;
bool CPC88Opna::m_abFmChMute[CPC88Opna::FM_CHANNEL_COUNT]   = { false, false, false };
bool CPC88Opna::m_abSsgChMute[CPC88Opna::SSG_CHANNEL_COUNT] = { false, false, false };

// ----- SSG (PSG) state -----

int      CPC88Opna::m_nSsgTicksPerSampleX16;
int      CPC88Opna::m_nSsgTickAccumX16;
int      CPC88Opna::m_anSsgTonePeriod[CPC88Opna::SSG_CHANNEL_COUNT];
int      CPC88Opna::m_anSsgToneCounter[CPC88Opna::SSG_CHANNEL_COUNT];
int      CPC88Opna::m_anSsgToneState[CPC88Opna::SSG_CHANNEL_COUNT];
int      CPC88Opna::m_nSsgNoisePeriod;
int      CPC88Opna::m_nSsgNoiseCounter;
uint32_t CPC88Opna::m_nSsgNoiseLfsr;
int      CPC88Opna::m_nSsgNoiseState;
int      CPC88Opna::m_nSsgEnvPeriod;
int      CPC88Opna::m_nSsgEnvCounter;
int      CPC88Opna::m_nSsgEnvLevel;
int      CPC88Opna::m_nSsgEnvDir;
bool     CPC88Opna::m_bSsgEnvHolding;
int      CPC88Opna::m_anSsgVolume[CPC88Opna::SSG_CHANNEL_COUNT];
bool     CPC88Opna::m_abSsgUseEnv[CPC88Opna::SSG_CHANNEL_COUNT];
uint8_t  CPC88Opna::m_btSsgMixer;
int      CPC88Opna::m_anSsgVolTable[CPC88Opna::SSG_VOL_TABLE_SIZE];
int      CPC88Opna::m_anSsgEnvTable[CPC88Opna::SSG_ENV_TABLE_SIZE];

// ----- FM (YM2203) state -----

CPC88Opna::SFmChannel CPC88Opna::m_aFmCh[CPC88Opna::FM_CHANNEL_COUNT];
bool                  CPC88Opna::m_bFmCh3SpecialMode;
uint8_t               CPC88Opna::m_abtFmFnumLatch[CPC88Opna::FM_CHANNEL_COUNT];
uint8_t               CPC88Opna::m_abtFmCh3FnumLatch[3];
int                   CPC88Opna::m_nFmTicksPerSampleX16;
int                   CPC88Opna::m_nFmPhaseScaleX16;

// Diagnostic counters (file-scope, not class members so they don't
// require header changes for a debug aid). Incremented from
// WriteData() and OnFmRegisterWrite() to verify that the I/O port
// dispatch is reaching FM-relevant addresses.
int g_nFmTotalWrites = 0;
int g_nOpnTotalWrites = 0;
int                   CPC88Opna::m_anFmSinTable[CPC88Opna::FM_SIN_TABLE_SIZE];
int                   CPC88Opna::m_anFmExpTable[CPC88Opna::FM_EXP_TABLE_SIZE];
int                   CPC88Opna::m_anFmEnvRateTable[CPC88Opna::FM_ENV_RATE_TABLE_SIZE];
int                   CPC88Opna::m_anFmDetunePhaseInc[CPC88Opna::FM_DT_BLOCKS][CPC88Opna::FM_DT_NOTES][CPC88Opna::FM_DT_FDS];

// Detune amounts in milli-Hz at master clock = 8 MHz, transcribed
// from Table 2-6 of the YM2608 application manual. The arrows ("↑")
// in the printed table mean "same value as the cell directly above";
// they are flattened here. Indexed by [BLOCK 0..7][NOTE 0..3][FD 0..3].
// FD=0 column is always zero (no detune). The DT register field's
// bit 2 (sign) is applied separately at runtime.
//
// At runtime UpdateFmTickRate() converts these milli-Hz values into
// per-output-sample phase increment offsets, scaling for the active
// master clock and sample rate. (At master clocks other than 8 MHz
// the chip's phase generator produces correspondingly different Hz
// offsets, so the manual values must be scaled by master/8e6.)
static const short s_anFmDetuneMilliHz[CPC88Opna::FM_DT_BLOCKS]
                                       [CPC88Opna::FM_DT_NOTES]
                                       [CPC88Opna::FM_DT_FDS] = {
	// BLOCK 0
	{ {0,   0,  53, 106},  {0,   0,  53, 106},
	  {0,   0,  53, 106},  {0,   0,  53, 106} },
	// BLOCK 1
	{ {0,  53, 106, 106},  {0,  53, 106, 159},
	  {0,  53, 106, 159},  {0,  53, 106, 159} },
	// BLOCK 2
	{ {0,  53, 106, 212},  {0,  53, 159, 212},
	  {0,  53, 159, 212},  {0,  53, 159, 264} },
	// BLOCK 3
	{ {0, 106, 212, 264},  {0, 106, 212, 317},
	  {0, 106, 212, 317},  {0, 106, 264, 370} },
	// BLOCK 4
	{ {0, 106, 264, 423},  {0, 159, 317, 423},
	  {0, 159, 317, 476},  {0, 159, 370, 529} },
	// BLOCK 5
	{ {0, 212, 423, 582},  {0, 212, 423, 635},
	  {0, 212, 476, 688},  {0, 264, 529, 741} },
	// BLOCK 6
	{ {0, 264, 582, 846},  {0, 317, 635, 899},
	  {0, 317, 688,1005},  {0, 370, 741,1058} },
	// BLOCK 7
	{ {0, 423, 846,1164},  {0, 423, 846,1164},
	  {0, 423, 846,1164},  {0, 423, 846,1164} },
};

////////////////////////////////////////////////////////////
// create & destroy

// default constructor

CPC88Opna::CPC88Opna() {
	m_pIntVectChangeCallback = NULL;
	m_pSampleOutputCallback = NULL;
}

// destructor

CPC88Opna::~CPC88Opna() {
}

////////////////////////////////////////////////////////////
// initialize

// initialize at first
//
// Initialize() is responsible for one-time setup that does NOT need
// to be re-done on every emulator reset:
//   - Base clock default
//   - Output amplitude tables (purely a function of constants)
//   - Sample rate default (only if the frontend hasn't set one yet)
//
// All runtime state (register mirror, channel state, accumulators,
// envelope state, mixer, etc.) is handled by Reset(). The frontend
// always calls Initialize() and then Reset() so calling Reset() from
// within Initialize() would be redundant; instead we rely on the
// caller to do it.

void CPC88Opna::Initialize() {
	m_nBaseClock = 4;
	BuildSsgTables();
	BuildFmTables();
	if (m_nSampleRate <= 0) {
		// Default to 44.1 kHz; the frontend may override via
		// SetSampleRate() before or after Initialize().
		SetSampleRate(44100);
	}
}

// reset

void CPC88Opna::Reset() {
	m_btStatus = 0;
	m_nAddress = 0;
	m_nCh3Mode = 0;
	m_nPreScalerFM = 6;
	m_nPreScalerPSG = 4;
	m_bTimerAAcvive = m_bTimerASetFlag = false;
	m_nTimerAValue = 0;
	SetTimerACounterMax();
	m_nTimerACounter = m_nTimerACounterMax;
	m_bTimerBAcvive = m_bTimerBSetFlag = false;
	m_nTimerBValue = 0;
	SetTimerBCounterMax();
	m_nTimerBCounter = m_nTimerBCounterMax;
	m_bOpnaInterruptRequest = false;
	// Phase C synthesis state.
	for (int n = 0; n < REGISTER_COUNT; n++) {
		m_abtRegisters[n] = 0;
	}
	// SSG mixer reset state per YM2149 datasheet: $07 = $FF
	// (= every channel's tone and noise both disabled, I/O ports as
	// outputs). Match m_btSsgMixer below so the mirror and the
	// dedicated runtime variable agree from the very first sample.
	m_abtRegisters[0x07] = 0xFF;
	m_nSampleAccumX256 = 0;
	// Recompute cycles-per-sample because m_nBaseClock may have changed.
	if (m_nSampleRate > 0) {
		SetSampleRate(m_nSampleRate);
	}

	// SSG state reset.
	m_nSsgTickAccumX16 = 0;
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		m_anSsgTonePeriod[n]  = 1;
		m_anSsgToneCounter[n] = 1;
		m_anSsgToneState[n]   = 0;
		m_anSsgVolume[n]      = 0;
		m_abSsgUseEnv[n]      = false;
	}
	m_nSsgNoisePeriod  = 1;
	m_nSsgNoiseCounter = 1;
	m_nSsgNoiseLfsr    = 0x1FFFF;  // 17-bit, all ones (any non-zero seed works)
	m_nSsgNoiseState   = 1;
	m_nSsgEnvPeriod    = 1;
	m_nSsgEnvCounter   = 1;
	m_nSsgEnvLevel     = 0;
	m_nSsgEnvDir       = -1;
	// Start the envelope generator in the "holding" state. Real
	// hardware doesn't run the envelope until the first $0D write
	// configures a shape, and emitting an unsolicited envelope step
	// here would briefly affect channels that already have the
	// envelope-use bit set in $08-$0A from a previous Reset().
	m_bSsgEnvHolding   = true;
	// Mixer default: tones disabled, noise disabled (all bits 1).
	m_btSsgMixer       = 0xFF;
	UpdateSsgTickRate();

	// FM state reset (Phase C-FM).
	ResetFmState();
	UpdateFmTickRate();
}

////////////////////////////////////////////////////////////
// operation

// timer A overflow

void CPC88Opna::TimerAOverFlow() {
	if (m_bTimerASetFlag) {
		if ((m_btStatus & 0x01) == 0) {
			UpdateInterruptRequest(true);
		}
		m_btStatus |= 0x01;
	}
	do {
		m_nTimerACounter += m_nTimerACounterMax;
	} while (m_nTimerACounter <= 0);
}

// timer B overflow

void CPC88Opna::TimerBOverFlow() {
	if (m_bTimerBSetFlag) {
		if ((m_btStatus & 0x02) == 0) {
			UpdateInterruptRequest(true);
		}
		m_btStatus |= 0x02;
	}
	do {
		m_nTimerBCounter += m_nTimerBCounterMax;
	} while (m_nTimerBCounter <= 0);
}

// set timer A counter max value

void CPC88Opna::SetTimerACounterMax() {
	m_nTimerACounterMax = 12*(1024-m_nTimerAValue)*m_nPreScalerFM*
		(m_nBaseClock/4);
}

// set timer B counter max value

void CPC88Opna::SetTimerBCounterMax() {
	m_nTimerBCounterMax = 192*(256-m_nTimerBValue)*m_nPreScalerFM*
		(m_nBaseClock/4);
}

// set pre-scaler

void CPC88Opna::SetPreScaler(int nPreScalerFM, int nPreScalerPSG) {
	m_nTimerACounter = (m_nTimerACounter*nPreScalerFM)/m_nPreScalerFM;
	m_nTimerBCounter = (m_nTimerBCounter*nPreScalerFM)/m_nPreScalerFM;
	m_nPreScalerFM = nPreScalerFM;
	m_nPreScalerPSG = nPreScalerPSG;
	SetTimerACounterMax();
	SetTimerBCounterMax();
	UpdateSsgTickRate();
	UpdateFmTickRate();
	// FM operator phase increments depend on the FM section's clock,
	// so a prescaler change requires recomputing all of them.
	for (int nCh = 0; nCh < FM_CHANNEL_COUNT; nCh++) {
		RecomputeFmChannelPhaseIncs(nCh);
	}
}

// read data

uint8_t CPC88Opna::ReadData() {
	// SSG registers $00–$0D are readable and return the last written
	// value. $0E–$0F are I/O ports: when configured as input (bit 6/7
	// of $07 = 0), reading returns the external port state; when
	// configured as output, reading returns the last written value.
	//
	// Previously this returned a hard-coded 0xFF, which broke any
	// read-modify-write sequence on $07 (the mixer / I/O direction
	// register). Ys's sound driver reads $07 to preserve the mixer
	// bits while changing I/O direction — getting $FF back caused
	// the mixer to be overwritten with all-disabled, silencing the
	// SSG melody channels entirely.
	if ((m_nAddress >= 0x00) && (m_nAddress <= 0x0D)) {
		return m_abtRegisters[m_nAddress];
	}
	if (m_nAddress == 0x0E || m_nAddress == 0x0F) {
		// I/O ports $0E/$0F: direction is controlled by $07 bits 6/7.
		// bit = 0 → input mode (read external hardware state)
		// bit = 1 → output mode (read back last written value)
		// On PC-88, $0E is joystick input (active low: $FF = no
		// buttons pressed). When in input mode and no external
		// hardware is connected, return $FF so the game sees "idle".
		int nDirBit = (m_nAddress == 0x0E) ? 6 : 7;
		bool bOutputMode = (m_abtRegisters[0x07] >> nDirBit) & 1;
		if (bOutputMode) {
			return m_abtRegisters[m_nAddress];
		}
		return 0xFF;  // input mode: no joystick press
	}
	return 0xFF;
}

// write address

void CPC88Opna::WriteAddress(uint8_t btAddress) {
	m_nAddress = btAddress;
	switch (m_nAddress) {
	case 0x2D:
		SetPreScaler(6, 4);
		break;
	case 0x2E:
		SetPreScaler(3, 2);
		break;
	case 0x2F:
		SetPreScaler(2, 1);
		break;
	}
}

// write data

void CPC88Opna::WriteData(uint8_t btData) {
	g_nOpnTotalWrites++;
	// Phase C-準備: mirror every register write into the FM/SSG state
	// area. The synthesis engine added in Phase C-SSG / Phase C-FM
	// reads from this mirror.
	if ((m_nAddress >= 0) && (m_nAddress < REGISTER_COUNT)) {
		m_abtRegisters[m_nAddress] = btData;
	}
	// Phase C-SSG: SSG / PSG section is at $00–$0F.
	if ((m_nAddress >= 0x00) && (m_nAddress <= 0x0F)) {
		OnSsgRegisterWrite(m_nAddress, btData);
	}
	// Phase C-FM: $28 (key on/off) and $30–$B2 (FM operator/channel
	// parameters). $22 (LFO) and $B4–$B6 (L/R/AMS/PMS) are OPNA-only
	// and intentionally not handled.
	if ((m_nAddress == 0x28) ||
		((m_nAddress >= 0x30) && (m_nAddress <= 0x9E)) ||
		((m_nAddress >= 0xA0) && (m_nAddress <= 0xAE)) ||
		((m_nAddress >= 0xB0) && (m_nAddress <= 0xB2)))
	{
		OnFmRegisterWrite(m_nAddress, btData);
	}
	switch (m_nAddress) {
	case 0x24:
		m_nTimerAValue = (m_nTimerAValue & 0x0003) | (btData << 2);
		SetTimerACounterMax();
		break;
	case 0x25:
		m_nTimerAValue = (m_nTimerAValue & 0x03FC) | (btData & 0x03);
		SetTimerACounterMax();
		break;
	case 0x26:
		m_nTimerBValue = btData;
		SetTimerBCounterMax();
		break;
	case 0x27:
		if ((btData & 0x01) != 0) {
			if (!m_bTimerAAcvive) {
				m_bTimerAAcvive = true;
				m_nTimerACounter = m_nTimerACounterMax;
			}
		} else {
			m_bTimerAAcvive = false;
		}
		if ((btData & 0x02) != 0) {
			if (!m_bTimerBAcvive) {
				m_bTimerBAcvive = true;
				m_nTimerBCounter = m_nTimerBCounterMax;
			}
		} else {
			m_bTimerBAcvive = false;
		}
		m_bTimerASetFlag = ((btData & 0x04) != 0);
		m_bTimerBSetFlag = ((btData & 0x08) != 0);
		if ((btData & 0x10) != 0) {
			m_btStatus &= 0xFE;
		}
		if ((btData & 0x20) != 0) {
			m_btStatus &= 0xFD;
		}
		m_nCh3Mode = (btData >> 6) & 0x03;
		// Phase C-FM: bit 6/7 of $27 enables CH3 special mode (any
		// non-zero value). The three sub-modes only differ in the
		// retrigger semantics on real hardware; for synthesis we just
		// need to know "per-op F-Number for CH3 yes/no".
		m_bFmCh3SpecialMode = (m_nCh3Mode != 0);
		break;
	}
}

////////////////////////////////////////////////////////////
// Phase C-準備: sample synthesis scaffolding

// set sample rate

void CPC88Opna::SetSampleRate(int nSampleRate) {
	if (nSampleRate <= 0) {
		nSampleRate = 44100;
	}
	m_nSampleRate = nSampleRate;
	// cycles_per_sample = (Z80_clock_hz / sample_rate)
	// stored in 8.8 fixed point so the accumulator can advance with
	// sub-cycle precision.
	int nClockHz = m_nBaseClock * 1000000;
	if (nClockHz <= 0) {
		nClockHz = 4 * 1000000;
	}
	m_nCyclesPerSampleX256 = (int)(((long long)nClockHz * 256) / nSampleRate);
	if (m_nCyclesPerSampleX256 <= 0) {
		m_nCyclesPerSampleX256 = 256;
	}
	UpdateSsgTickRate();
	UpdateFmTickRate();
}

// generate nFrames stereo samples and push them via the callback

void CPC88Opna::Generate(int nFrames) {
	if ((nFrames <= 0) || (m_pSampleOutputCallback == NULL)) {
		return;
	}
	const int nMaxBatch = 256;
	int16_t aBuf[nMaxBatch * 2];
	while (nFrames > 0) {
		int nThis = (nFrames < nMaxBatch)? nFrames: nMaxBatch;
		for (int n = 0; n < nThis; n++) {
			// Advance SSG state by however many internal SSG ticks
			// elapse during one output sample.
			m_nSsgTickAccumX16 += m_nSsgTicksPerSampleX16;
			while (m_nSsgTickAccumX16 >= (1 << 16)) {
				m_nSsgTickAccumX16 -= (1 << 16);
				AdvanceSsgOneTick();
			}
			// Always run synthesis so timers / envelopes / phase
			// accumulators stay consistent; only suppress the section's
			// contribution to the output sample if muted.
			int nSsgSample = RenderSsgSample();
			int nFmSample  = RenderFmSample();
			int nSample    = 0;
			// Mix SSG and FM with relative balance. Real PC-88 hardware
			// outputs SSG at a lower level than FM; halving the SSG
			// contribution here roughly matches the perceived balance.
			// (The SSG peak is ~24000 vs FM ~12288 after the 14-bit
			// op output >> 1 in RenderFmSample, so >> 1 on SSG brings
			// both sections to a similar range.)
			if (!m_bSsgMute) nSample += nSsgSample >> 1;
			if (!m_bFmMute)  nSample += nFmSample;
			if (nSample >  32767) nSample =  32767;
			if (nSample < -32768) nSample = -32768;
			aBuf[n*2 + 0] = (int16_t)nSample;
			aBuf[n*2 + 1] = (int16_t)nSample;
		}
		m_pSampleOutputCallback(aBuf, nThis);
		nFrames -= nThis;
	}
}

////////////////////////////////////////////////////////////
// Phase C-SSG: SSG (PSG / YM2149-compatible) implementation

// build amplitude tables from log-domain formulas

void CPC88Opna::BuildSsgTables() {
	// Per-channel maximum: keep 3 channels summed below int16 max with
	// some headroom for the future FM section.
	const double dMax = 8000.0;
	// 16-step volume table — YM2149 / AY-3-8910 standard is 3 dB per
	// step (adjacent voltages differ by a factor of 0.707 = 1/√2),
	// giving a 45 dB total range. PC-88 BGM relies on this curve so
	// that vol=4 tails fade out cleanly relative to vol=7 attacks.
	m_anSsgVolTable[0] = 0;
	for (int n = 1; n < SSG_VOL_TABLE_SIZE; n++) {
		// vol[n] = max * 10^((n - (size-1)) * 3 / 20)
		double dExp = ((double)n - (double)(SSG_VOL_TABLE_SIZE-1)) * 3.0 / 20.0;
		double dGain = pow(10.0, dExp);
		m_anSsgVolTable[n] = (int)(dMax * dGain + 0.5);
	}
	// 32-step envelope table — 1.5 dB per step. The envelope uses a
	// 5-bit DAC with twice the resolution of the volume register, so
	// each envelope step is half the volume step size in dB.
	m_anSsgEnvTable[0] = 0;
	for (int n = 1; n < SSG_ENV_TABLE_SIZE; n++) {
		double dExp = ((double)n - (double)(SSG_ENV_TABLE_SIZE-1)) * 1.5 / 20.0;
		double dGain = pow(10.0, dExp);
		m_anSsgEnvTable[n] = (int)(dMax * dGain + 0.5);
	}
}

// recompute the SSG ticks-per-output-sample ratio (16.16 fixed)

void CPC88Opna::UpdateSsgTickRate() {
	if ((m_nSampleRate <= 0) || (m_nPreScalerPSG <= 0) || (m_nBaseClock <= 0)) {
		m_nSsgTicksPerSampleX16 = 0;
		return;
	}
	// SSG internal counters advance at base / prescaler_psg / 4.
	//
	// The OPNA manual (page 37) gives f_tone = φM / (64 × TP) for
	// φM = 8 MHz with SSG prescaler /4. For our 4 MHz setup the
	// effective formula is f = φM / (32 × TP), which matches the
	// tick rate of master / prescaler / 4 = 250 kHz with half-period
	// toggle: f = 250 000 / (2 × TP). User-verified pitch matches
	// PC-88 hardware for Ys FEENA and ハイドライド3.
	long long nSsgClockHz =
		(long long)m_nBaseClock * 1000000LL / m_nPreScalerPSG / 4LL;
	m_nSsgTicksPerSampleX16 = (int)((nSsgClockHz * (1LL << 16)) / m_nSampleRate);
	if (m_nSsgTicksPerSampleX16 < 0) {
		m_nSsgTicksPerSampleX16 = 0;
	}
}

// advance one SSG internal tick

void CPC88Opna::AdvanceSsgOneTick() {
	// Tone counters: each channel toggles when its counter reaches 0.
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		int nPeriod = m_anSsgTonePeriod[n];
		if (nPeriod < 1) {
			// Period == 0: real YM2149 doesn't toggle the square output
			// in this case (the counter never reloads to a positive
			// value, so the divider sits at "no edge"). Some sound
			// drivers (silpheed and others) write tone period 0 as
			// part of their initialisation or as a "DC-output trick"
			// for software volume modulation. Real hardware filters
			// the resulting DC level out by analog low-pass before the
			// speaker, so it's perceptually silent.
			//
			// Our previous code clamped period to 1 here, which made
			// the divider toggle every internal SSG tick (≈250 kHz at
			// the typical PC-88 4 MHz / prescaler 4 setup). At 44.1 kHz
			// output that ultrasonic square aliased back into the
			// audible band as a piercing high-frequency tone. Setting
			// state to 0 silences the channel cleanly and matches the
			// "perceptually inaudible" behaviour of the real chip.
			m_anSsgToneState[n]   = 0;
			m_anSsgToneCounter[n] = 1;
			continue;
		}
		if (--m_anSsgToneCounter[n] <= 0) {
			m_anSsgToneCounter[n] = nPeriod;
			m_anSsgToneState[n] ^= 1;
		}
	}
	// Noise: at our fc/4 SSG tick rate, LFSR steps every 2 × NP ticks
	// to match the practical fc/(8 × NP) noise cadence. (Doubling the
	// reload here keeps the noise audible rate constant when we change
	// the tick rate elsewhere.)
	if (--m_nSsgNoiseCounter <= 0) {
		int nPeriod = m_nSsgNoisePeriod;
		if (nPeriod < 1) nPeriod = 1;
		m_nSsgNoiseCounter = nPeriod * 2;
		// 17-bit LFSR, taps at bit 0 and bit 3 (XOR), shift right.
		uint32_t nFeedback = ((m_nSsgNoiseLfsr >> 0) ^ (m_nSsgNoiseLfsr >> 3)) & 1;
		m_nSsgNoiseLfsr = (m_nSsgNoiseLfsr >> 1) | (nFeedback << 16);
		m_nSsgNoiseState = (int)(m_nSsgNoiseLfsr & 1);
	}
	// Envelope: the YM2149 spec defines f_envelope = fc/(256 × EP),
	// but this is the rate of one COMPLETE 32-step cycle (not the per-
	// step rate). One step therefore takes 8×EP/fc seconds, which at
	// our fc/4 SSG tick rate is exactly EP ticks per step.
	if (!m_bSsgEnvHolding) {
		if (--m_nSsgEnvCounter <= 0) {
			int nPeriod = m_nSsgEnvPeriod;
			if (nPeriod < 1) nPeriod = 1;
			m_nSsgEnvCounter = nPeriod;
			m_nSsgEnvLevel += m_nSsgEnvDir;
			if ((m_nSsgEnvLevel < 0) || (m_nSsgEnvLevel > 31)) {
				// End of one envelope cycle — apply the shape's
				// continue / attack / alternate / hold rules.
				int nShape = m_abtRegisters[0x0D] & 0x0F;
				bool bCont = (nShape & 0x08) != 0;
				bool bAtt  = (nShape & 0x04) != 0;
				bool bAlt  = (nShape & 0x02) != 0;
				bool bHold = (nShape & 0x01) != 0;
				if (!bCont) {
					// Shapes 0..7 collapse to a one-shot ramp ending at 0.
					m_nSsgEnvLevel = 0;
					m_bSsgEnvHolding = true;
				} else if (bHold) {
					// Hold at the appropriate end depending on ALT.
					if (bAlt) {
						m_nSsgEnvLevel = bAtt? 0: 31;
					} else {
						m_nSsgEnvLevel = bAtt? 31: 0;
					}
					m_bSsgEnvHolding = true;
				} else if (bAlt) {
					// Reverse direction and bounce one step inside.
					m_nSsgEnvDir = -m_nSsgEnvDir;
					m_nSsgEnvLevel += m_nSsgEnvDir;
				} else {
					// Repeat the same direction (sawtooth wrap).
					m_nSsgEnvLevel = (m_nSsgEnvDir > 0)? 0: 31;
				}
				if (m_nSsgEnvLevel < 0)  m_nSsgEnvLevel = 0;
				if (m_nSsgEnvLevel > 31) m_nSsgEnvLevel = 31;
			}
		}
	}
}

// produce one output sample by mixing the three SSG channels

int CPC88Opna::RenderSsgSample() {
	int anChAmp[SSG_CHANNEL_COUNT] = { 0, 0, 0 };
	int nMix = 0;
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		// Per-channel mute (Debug menu). Skip the contribution but
		// keep the loop running so any subsequent state stays consistent.
		if (m_abSsgChMute[n]) {
			continue;
		}
		// Mixer bits: 1 = disabled, 0 = enabled. When disabled the
		// corresponding signal is forced high so the AND of the two
		// lets the other signal through unchanged.
		//
		// Per the YM2149 mixer schematic the channel output is
		//   ch = (T_n OR D_T_n) AND (N_n OR D_N_n)
		// which means BOTH disabled → constant 1 → the channel just
		// drives its volume DAC at full scale. Several PC-88 sound
		// drivers (notably Ys) use this mode for their main melody:
		// the mixer is set to "everything disabled" but the driver
		// still rewrites the vol register at IRQ rate (and the vol
		// register is decoded by the chip's DAC into an analog
		// level). The audible result is the vol DAC's stair-step
		// waveform — the melody is encoded directly in the sequence
		// of vol writes, not in the tone counter.
		//
		// We previously short-circuited "both disabled" to silence
		// here under the assumption that the constant DC was
		// inaudible. That dropped the entire Ys SSG melody phase.
		// Letting the standard formula run produces nOut=1 for
		// both-disabled, the vol DAC value passes through, and the
		// driver's vol writes become audible as designed.
		int nToneDisable  = (m_btSsgMixer >> n)        & 1;
		int nNoiseDisable = (m_btSsgMixer >> (n + 3))  & 1;
		int nToneOut  = nToneDisable  | m_anSsgToneState[n];
		int nNoiseOut = nNoiseDisable | m_nSsgNoiseState;
		int nOut = nToneOut & nNoiseOut;
		if (nOut == 0) {
			continue;
		}
		// Pick amplitude: envelope vs static volume.
		int nAmp;
		if (m_abSsgUseEnv[n]) {
			nAmp = m_anSsgEnvTable[m_nSsgEnvLevel & 0x1F];
		} else {
			nAmp = m_anSsgVolTable[m_anSsgVolume[n] & 0x0F];
		}
		anChAmp[n] = nAmp;
		nMix += nAmp;
	}
	// DEBUG: SSG-only status dump emitted to stderr every 0.25 sec when
	// X88_SSG_DEBUG=1. Independent of the FM debug log so the two can
	// be enabled separately. Includes the full $00-$0F register mirror,
	// runtime envelope / tone / noise state, and per-channel
	// contribution stats.
	{
		static int s_check = -1;
		static bool s_enabled = false;
		if (s_check < 0) {
			s_check = 1;
			const char* p = getenv("X88_SSG_DEBUG");
			s_enabled = (p != NULL) && (*p != '\0') && (*p != '0');
		}
		if (s_enabled) {
			static int s_sampleCount = 0;
			static int s_lastReport = 0;
			static long long s_sumAbs = 0;
			static int s_minSample = 0;
			static int s_maxSample = 0;
			static long long s_sumAbsCh[SSG_CHANNEL_COUNT] = { 0, 0, 0 };
			static int s_maxCh[SSG_CHANNEL_COUNT] = { 0, 0, 0 };
			s_sampleCount++;
			s_sumAbs += (nMix < 0)? -nMix: nMix;
			if (nMix < s_minSample) s_minSample = nMix;
			if (nMix > s_maxSample) s_maxSample = nMix;
			for (int c = 0; c < SSG_CHANNEL_COUNT; c++) {
				int v = anChAmp[c];
				s_sumAbsCh[c] += v;  // anChAmp is already non-negative
				if (v > s_maxCh[c]) s_maxCh[c] = v;
			}
			if (s_sampleCount - s_lastReport >= 11025) {
				int nReportFrames = s_sampleCount - s_lastReport;
				int nMeanAbs = (int)(s_sumAbs / nReportFrames);
				s_lastReport = s_sampleCount;
				fprintf(stderr,
					"\n========== [SSG dump @ s=%d (~%.2fs)] ==========\n"
					"section mute: SSG=%c    chMute=[A:%c B:%c C:%c]\n"
					"mix: min=%d max=%d meanAbs=%d\n"
					"regs: ",
					s_sampleCount, (double)s_sampleCount / 44100.0,
					m_bSsgMute? 'M': '.',
					m_abSsgChMute[0]? 'M': '.',
					m_abSsgChMute[1]? 'M': '.',
					m_abSsgChMute[2]? 'M': '.',
					s_minSample, s_maxSample, nMeanAbs);
				for (int r = 0; r < 16; r++) {
					fprintf(stderr, "%02X%s",
						m_abtRegisters[r],
						(r == 7)? "  " : " ");
				}
				fprintf(stderr, "\n");
				// Mixer decoded
				int nMix07 = m_btSsgMixer;
				fprintf(stderr,
					"mixer $07=$%02X  toneDis:[A:%d B:%d C:%d]  "
					"noiseDis:[A:%d B:%d C:%d]  ioDir:[A:%d B:%d]\n",
					nMix07,
					(nMix07     ) & 1, (nMix07 >> 1) & 1, (nMix07 >> 2) & 1,
					(nMix07 >> 3) & 1, (nMix07 >> 4) & 1, (nMix07 >> 5) & 1,
					(nMix07 >> 6) & 1, (nMix07 >> 7) & 1);
				// Per-channel detail
				for (int c = 0; c < SSG_CHANNEL_COUNT; c++) {
					int nMeanCh = (int)(s_sumAbsCh[c] / nReportFrames);
					fprintf(stderr,
						"ch%c %s tone_period=%4d  tone_cnt=%4d state=%d  "
						"vol=%2d %s  contrib: max=%d meanAbs=%d\n",
						'A' + c,
						m_abSsgChMute[c]? "[MUTE]": "      ",
						m_anSsgTonePeriod[c],
						m_anSsgToneCounter[c],
						m_anSsgToneState[c],
						m_anSsgVolume[c],
						m_abSsgUseEnv[c]? "ENV": "fix",
						s_maxCh[c], nMeanCh);
				}
				// Noise + envelope
				fprintf(stderr,
					"noise: period=%d cnt=%d state=%d lfsr=%05X\n"
					"env: period=%d cnt=%d level=%d dir=%+d hold=%c shape=$%X (cont=%d att=%d alt=%d hold=%d)\n",
					m_nSsgNoisePeriod, m_nSsgNoiseCounter,
					m_nSsgNoiseState, m_nSsgNoiseLfsr & 0x1FFFF,
					m_nSsgEnvPeriod, m_nSsgEnvCounter,
					m_nSsgEnvLevel, m_nSsgEnvDir,
					m_bSsgEnvHolding? 'Y': 'N',
					m_abtRegisters[0x0D] & 0x0F,
					(m_abtRegisters[0x0D] >> 3) & 1,
					(m_abtRegisters[0x0D] >> 2) & 1,
					(m_abtRegisters[0x0D] >> 1) & 1,
					(m_abtRegisters[0x0D]     ) & 1);
				fflush(stderr);
				s_sumAbs = 0;
				s_minSample = 0;
				s_maxSample = 0;
				for (int c = 0; c < SSG_CHANNEL_COUNT; c++) {
					s_sumAbsCh[c] = 0;
					s_maxCh[c] = 0;
				}
			}
		}
	}
	// Note: no static DC offset is subtracted. The signal swings
	// between 0 and (sum of active channel amplitudes), so it
	// naturally has a DC component that depends on which channels
	// are active. The downstream Sdl3AudioOutput mixer is responsible
	// for any final filtering; in practice the perceptual DC is
	// inaudible because the silence baseline is exactly 0.
	return nMix;
}

// handle a write to one of the SSG registers ($00–$0F)

void CPC88Opna::OnSsgRegisterWrite(int nAddress, uint8_t btData) {
	// Per-write log gated by X88_SSG_DEBUG=1. Logs every $00-$0F write
	// in the order they arrive so we can see exactly what the sound
	// driver does (envelope period sequences for melody-via-envelope
	// tricks, mixer toggles, vol register modulation, etc.).
	{
		static int s_check = -1;
		static bool s_enabled = false;
		if (s_check < 0) {
			s_check = 1;
			const char* p = getenv("X88_SSG_DEBUG");
			s_enabled = (p != NULL) && (*p != '\0') && (*p != '0');
		}
		if (s_enabled) {
			static const char* kRegName[16] = {
				"At.lo", "At.hi", "Bt.lo", "Bt.hi",
				"Ct.lo", "Ct.hi", "noise", "mixer",
				"Avol ", "Bvol ", "Cvol ", "ep.lo",
				"ep.hi", "shape", "ioA  ", "ioB  "
			};
			fprintf(stderr, "[SSG] $%02X=%02X (%s)\n",
				nAddress, btData,
				(nAddress < 16)? kRegName[nAddress]: "?    ");
			fflush(stderr);
		}
	}
	switch (nAddress) {
	case 0x00: // CH A tone period low
	case 0x02: // CH B tone period low
	case 0x04: { // CH C tone period low
		int nCh = nAddress >> 1;
		int nHi = m_abtRegisters[nAddress + 1] & 0x0F;
		// Allow period == 0 — AdvanceSsgOneTick() handles it as
		// "no toggle / silent DC", matching real chip behaviour.
		m_anSsgTonePeriod[nCh] = (nHi << 8) | btData;
		break;
	}
	case 0x01: // CH A tone period high (4 bits)
	case 0x03: // CH B tone period high
	case 0x05: { // CH C tone period high
		int nCh = nAddress >> 1;
		int nLo = m_abtRegisters[nAddress - 1];
		m_anSsgTonePeriod[nCh] = ((btData & 0x0F) << 8) | nLo;
		break;
	}
	case 0x06: // noise period (5 bits)
		m_nSsgNoisePeriod = btData & 0x1F;
		if (m_nSsgNoisePeriod < 1) m_nSsgNoisePeriod = 1;
		break;
	case 0x07: // mixer / I/O direction
		m_btSsgMixer = btData;
		break;
	case 0x08: // CH A volume (low 4 bits) + envelope flag (bit 4)
	case 0x09: // CH B volume
	case 0x0A: { // CH C volume
		int nCh = nAddress - 0x08;
		m_anSsgVolume[nCh] = btData & 0x0F;
		m_abSsgUseEnv[nCh] = (btData & 0x10) != 0;
		break;
	}
	case 0x0B: // envelope period low
		m_nSsgEnvPeriod = (m_nSsgEnvPeriod & 0xFF00) | btData;
		if (m_nSsgEnvPeriod < 1) m_nSsgEnvPeriod = 1;
		break;
	case 0x0C: // envelope period high
		m_nSsgEnvPeriod = (m_nSsgEnvPeriod & 0x00FF) | ((int)btData << 8);
		if (m_nSsgEnvPeriod < 1) m_nSsgEnvPeriod = 1;
		break;
	case 0x0D: { // envelope shape (resets envelope on every write)
		int nShape = btData & 0x0F;
		bool bAtt = (nShape & 0x04) != 0;
		m_nSsgEnvDir   = bAtt? +1: -1;
		m_nSsgEnvLevel = bAtt? 0: 31;
		// One env step every EP ticks at fc/4 (see AdvanceSsgOneTick).
		int nP = m_nSsgEnvPeriod;
		if (nP < 1) nP = 1;
		m_nSsgEnvCounter = nP;
		m_bSsgEnvHolding = false;
		break;
	}
	case 0x0E: // I/O port A — not used for sound output
	case 0x0F: // I/O port B
		break;
	}
}

// advance sample accumulator and emit samples as they accumulate

void CPC88Opna::AdvanceSampleAccumulator(int nClock) {
	if (m_nCyclesPerSampleX256 <= 0) {
		return;
	}
	m_nSampleAccumX256 += nClock * 256;
	int nReadyFrames = 0;
	while (m_nSampleAccumX256 >= m_nCyclesPerSampleX256) {
		m_nSampleAccumX256 -= m_nCyclesPerSampleX256;
		nReadyFrames++;
		// Cap per-call to avoid pathological catch-up bursts.
		if (nReadyFrames >= 1024) {
			break;
		}
	}
	if (nReadyFrames > 0) {
		Generate(nReadyFrames);
	}
}

////////////////////////////////////////////////////////////
// Phase C-FM: YM2203 FM synthesis (Phase C-FM-1: scaffolding only)
//
// This stage adds the data structures, lookup tables, and register
// dispatch needed by the FM section. NO audio is produced yet — the
// state is updated correctly from incoming register writes, but
// Generate() does not yet mix the FM channels into its output.
// Phase C-FM-2 will add the actual phase generator + envelope output.

// build sin / exp / envelope rate / detune tables from formulas

void CPC88Opna::BuildFmTables() {
	const double dPI = 3.14159265358979323846;

	// Sin table: 1/4 period, log2 domain.
	// Each entry holds -log2(sin(angle)) scaled to fixed point so that
	// adding TL and envelope attenuation in the same domain becomes a
	// pure integer addition. The "+ 0.5" in the angle samples the
	// midpoint of each step (matches the standard YM2151/YM2203
	// pipeline; equivalent to a half-step offset in time).
	for (int n = 0; n < FM_SIN_TABLE_SIZE; n++) {
		double dAngle = ((double)n + 0.5) / (double)FM_SIN_TABLE_SIZE * dPI / 2.0;
		double dSin = sin(dAngle);
		// -log2(sin) is always >= 0 because sin in (0, 1].
		double dLog = -log(dSin) / log(2.0);
		// Scale: 8 fractional bits. Larger value = quieter.
		m_anFmSinTable[n] = (int)(dLog * 256.0 + 0.5);
	}

	// Exp table: 2^(-x/256) for fractional log-domain attenuation.
	// Used to convert log-domain values back to linear amplitude.
	//
	// Per-operator peak amplitude is 8192 (~14-bit signed), matching
	// the standard YM2151/YM2203 op output range. Pre-2026 the peak
	// was 4096 (~13-bit), but that gave half the modulation depth of
	// the chip and made FM patches sound thin / under-modulated. To
	// keep the final mix in int16 range despite the larger per-op
	// output, RenderFmSample() right-shifts the mixed FM signal by 1
	// before returning. The shift happens AFTER the modulation feed-
	// forward path, so modulators still drive carriers at the full
	// ±8192 amplitude (= correct modulation index for typical patches).
	for (int n = 0; n < FM_EXP_TABLE_SIZE; n++) {
		double dFrac = (double)n / (double)FM_EXP_TABLE_SIZE;
		double dExp = pow(2.0, -dFrac);
		m_anFmExpTable[n] = (int)(dExp * 8192.0 + 0.5);
	}

	// Envelope rate table: rate 0..63 → average counter increment per
	// FM section clock, in 16.16 fixed point. The standard YM2151/
	// YM2203 envelope counter formula uses two parameters derived from
	// the 6-bit effective rate:
	//
	//   shift = 11 - (rate >> 2)     // larger rate = smaller shift = faster
	//   add   = 4 + (rate & 3)       // 4..7
	//
	// On real hardware the counter is decremented by `add` whenever the
	// global tick number is divisible by `1 << shift`. The average
	// per-tick increment is therefore add / (1 << shift). We store the
	// average in 16.16 fixed point so that the per-sample envelope
	// update in Phase C-FM-2 just becomes a saturating addition.
	for (int nRate = 0; nRate < FM_ENV_RATE_TABLE_SIZE; nRate++) {
		if (nRate < 2) {
			// Rates 0 and 1 are effectively zero (envelope frozen).
			m_anFmEnvRateTable[nRate] = 0;
		} else {
			// 2026-04-17: shift base increased from 11 → 13 (envelope
			// ~4× slower across the board) after A/B comparing Ys piano
			// tone against fmgen. Our prior formula made piano carrier
			// decay ~93 ms at DR=7+KSR=6 (rate=20), vs ~340 ms on the
			// reference. A crude global slowdown brings the piano into
			// the right ballpark without needing the exact decap-derived
			// rate-selector LUT. Other patches (sustained pads, rapid
			// release) are more tolerant of this because R=0 still
			// freezes the envelope regardless of shift.
			int nShift = 13 - (nRate >> 2);
			int nAdd   = 4 + (nRate & 3);
			if (nShift < 0) nShift = 0;
			// (add << 16) >> shift = (add * 65536) / (1 << shift)
			m_anFmEnvRateTable[nRate] = (nAdd << 16) >> nShift;
		}
	}

	// Detune table values are derived from s_anFmDetuneMilliHz[][][]
	// in UpdateFmTickRate() because they depend on the current master
	// clock and sample rate. Just clear the runtime table here.
	for (int b = 0; b < FM_DT_BLOCKS; b++) {
		for (int n = 0; n < FM_DT_NOTES; n++) {
			for (int f = 0; f < FM_DT_FDS; f++) {
				m_anFmDetunePhaseInc[b][n][f] = 0;
			}
		}
	}
}

// reset all FM operator/channel state to a quiet baseline

void CPC88Opna::ResetFmState() {
	for (int nCh = 0; nCh < FM_CHANNEL_COUNT; nCh++) {
		SFmChannel& ch = m_aFmCh[nCh];
		ch.wFnum   = 0;
		ch.btBlock = 0;
		ch.btAlgo  = 0;
		ch.btFb    = 0;
		ch.anFeedback[0] = 0;
		ch.anFeedback[1] = 0;
		for (int n = 0; n < 3; n++) {
			ch.awFnumPerOp[n]  = 0;
			ch.abtBlockPerOp[n] = 0;
		}
		for (int nOp = 0; nOp < FM_OP_PER_CHANNEL; nOp++) {
			SFmOperator& op = ch.aOp[nOp];
			op.nPhase     = 0;
			op.nPhaseInc  = 0;
			op.nEnvLevel  = 1023;       // fully attenuated (silent)
			op.nEnvState  = FM_ENV_OFF;
			op.nEnvCounter = 0;
			op.btTl       = 127;        // 0 dB output = max attenuation
			op.btAr       = 0;
			op.btDr       = 0;
			op.btSr       = 0;
			op.btRr       = 0;
			op.btSl       = 0;
			op.btKs       = 0;
			op.btMul      = 0;
			op.btDt       = 0;
			op.bKeyOn     = false;
			op.btSsgEg    = 0;
			op.bSsgEgInverted = false;
			op.nOutPrev   = 0;
		}
	}
	m_bFmCh3SpecialMode = false;
	for (int n = 0; n < FM_CHANNEL_COUNT; n++) m_abtFmFnumLatch[n] = 0;
	for (int n = 0; n < 3; n++) m_abtFmCh3FnumLatch[n] = 0;
}

// recompute FM ticks-per-output-sample (informational; the per-sample
// path uses operator phase increments directly)

void CPC88Opna::UpdateFmTickRate() {
	if ((m_nSampleRate <= 0) || (m_nPreScalerFM <= 0) || (m_nBaseClock <= 0)) {
		m_nFmTicksPerSampleX16 = 0;
		m_nFmPhaseScaleX16     = 0;
		return;
	}
	// FM section clock = base / prescaler_fm / 6.
	long long nFmClockHz =
		(long long)m_nBaseClock * 1000000LL / m_nPreScalerFM / 6LL;
	m_nFmTicksPerSampleX16 = (int)((nFmClockHz * (1LL << 16)) / m_nSampleRate);
	if (m_nFmTicksPerSampleX16 < 0) {
		m_nFmTicksPerSampleX16 = 0;
	}

	// Phase increment scale factor (16.16 fixed point):
	//   scale = master * 65536 / (prescaler_fm * 24 * sample_rate)
	// For 4 MHz / 6 / 44100 this is ≈ 41 280 (≈ 0.6299), matching the
	// YM2203 spec frequency formula. RecomputeFmOperatorPhaseInc()
	// multiplies (FNUM << BLOCK) by this scale to derive the per-
	// output-sample phase increment.
	long long nMaster = (long long)m_nBaseClock * 1000000LL;
	long long nDiv    = (long long)m_nPreScalerFM * 24LL * (long long)m_nSampleRate;
	if (nDiv > 0) {
		m_nFmPhaseScaleX16 = (int)((nMaster * (1LL << 16)) / nDiv);
	} else {
		m_nFmPhaseScaleX16 = 0;
	}

	// Re-derive the detune phase increment table. The manual's Hz
	// values were measured at master clock = 8 MHz; at any other clock
	// the chip's phase generator produces proportionally different Hz
	// offsets. Convert each entry to the same 16.16 nIncX16 units used
	// by RecomputeFmOperatorPhaseInc.
	//
	//   freq_Hz                  = milliHz * (master / 8e6) / 1000
	//   phase_inc_per_sample     = freq_Hz * (1 << 20) / sample_rate
	//   phase_inc_per_sample_x16 = phase_inc_per_sample * (1 << 16)
	//
	// To stay inside int64 we factor master/8MHz as a 16.16 ratio:
	long long nMasterRatioX16 = (nMaster << 16) / 8000000LL;  // 65536 @ 8 MHz
	for (int b = 0; b < FM_DT_BLOCKS; b++) {
		for (int n = 0; n < FM_DT_NOTES; n++) {
			for (int f = 0; f < FM_DT_FDS; f++) {
				long long nMHz = (long long)s_anFmDetuneMilliHz[b][n][f];
				if (nMHz == 0) {
					m_anFmDetunePhaseInc[b][n][f] = 0;
					continue;
				}
				// nResult = milliHz * master_ratio_x16 * 2^20
				//             / (1000 * sample_rate)
				// bits: 11 + 17 + 20 = 48 → safe in int64.
				long long nNum = nMHz * nMasterRatioX16;
				nNum <<= 20;
				long long nDen = 1000LL * (long long)m_nSampleRate;
				m_anFmDetunePhaseInc[b][n][f] = (int)(nNum / nDen);
			}
		}
	}
}

// recompute one operator's phase increment from its channel's F-Number,
// block, MUL, and DT (Phase C-FM-1: stub; the formula will be filled in
// in Phase C-FM-2 once we wire up the actual phase generator)

void CPC88Opna::RecomputeFmOperatorPhaseInc(int nChannel, int nOpIndex) {
	if ((nChannel < 0) || (nChannel >= FM_CHANNEL_COUNT)) return;
	if ((nOpIndex < 0) || (nOpIndex >= FM_OP_PER_CHANNEL)) return;
	SFmChannel& ch = m_aFmCh[nChannel];
	SFmOperator& op = ch.aOp[nOpIndex];
	// Pick the F-Number / Block source: CH3 special mode uses per-op
	// values for op0/op1/op2, op3 always uses the channel-wide value.
	uint16_t wFnum;
	uint8_t btBlock;
	if ((nChannel == 2) && m_bFmCh3SpecialMode && (nOpIndex < 3)) {
		wFnum   = ch.awFnumPerOp[nOpIndex];
		btBlock = ch.abtBlockPerOp[nOpIndex];
	} else {
		wFnum   = ch.wFnum;
		btBlock = ch.btBlock;
	}
	// Base phase increment (per output sample, scaled to the 20-bit
	// phase accumulator) using the YM2203 spec frequency formula.
	long long nBase = (long long)wFnum << btBlock;
	long long nIncX16 = nBase * (long long)m_nFmPhaseScaleX16;
	// Apply MUL (multiple): MUL=0 means 0.5×, MUL=1..15 means 1×..15×.
	int nMul = op.btMul;
	if (nMul == 0) {
		nIncX16 >>= 1;
	} else {
		nIncX16 *= (long long)nMul;
	}
	// Apply DT (detune). The 3-bit DT field is bit 2 = sign and bits
	// 1..0 = magnitude (FD 0..3). FD=0 (and DT=4 = sign-only) mean
	// "no detune". The detune amount is looked up from
	// m_anFmDetunePhaseInc, which UpdateFmTickRate() pre-computes
	// from the YM2608 application manual's Hz table (Table 2-6).
	// Index by (BLOCK, NOTE) using the same N4/N3 derivation as the
	// KSR computation.
	int nFd      = op.btDt & 0x03;
	bool bDtNeg  = (op.btDt & 0x04) != 0;
	if (nFd != 0) {
		int nF11 = (wFnum >> 10) & 1;
		int nF10 = (wFnum >>  9) & 1;
		int nF9  = (wFnum >>  8) & 1;
		int nF8  = (wFnum >>  7) & 1;
		int nN4  = nF11;
		int nN3  = nF11? (nF10 | nF9 | nF8): (nF10 & nF9 & nF8);
		int nNote = (nN4 << 1) | nN3;
		int nBlk  = (int)btBlock;
		if (nBlk < 0) nBlk = 0;
		if (nBlk > 7) nBlk = 7;
		// Stored value is already in the 16.16 nIncX16 unit system.
		long long nDtOfs = (long long)m_anFmDetunePhaseInc[nBlk][nNote][nFd];
		if (bDtNeg) nDtOfs = -nDtOfs;
		nIncX16 += nDtOfs;
	}
	uint32_t nInc;
	if (nIncX16 < 0) {
		nInc = 0;
	} else {
		nInc = (uint32_t)(nIncX16 >> 16);
	}
	// Clamp to a reasonable maximum so a runaway FNUM/BLOCK/MUL/DT
	// combination doesn't produce wraparound noise.
	if (nInc >= (1u << FM_PHASE_BITS)) {
		nInc = (1u << FM_PHASE_BITS) - 1u;
	}
	op.nPhaseInc = nInc;
}

// recompute phase increments for every operator of a channel

void CPC88Opna::RecomputeFmChannelPhaseIncs(int nChannel) {
	for (int nOp = 0; nOp < FM_OP_PER_CHANNEL; nOp++) {
		RecomputeFmOperatorPhaseInc(nChannel, nOp);
	}
}

// map a register address in $30..$9E to (channel, op index 0..3)
// using the YM family's 1-3-2-4 slot order

bool CPC88Opna::ResolveFmSlotAddress(int nAddress, int& nChannel, int& nOpIndex) {
	int nLow = nAddress & 0x0F;
	int nCh   = nLow & 0x03;       // 0..3
	int nSlot = (nLow >> 2) & 0x03; // 0..3
	if (nCh >= FM_CHANNEL_COUNT) {
		// Slot column 0x_3 / 0x_7 / 0x_B / 0x_F targets the (absent)
		// 4th channel — YM2203 only has 3 channels.
		return false;
	}
	// Slot index → physical operator number (0-based):
	// slot 0 → OP1 (= 0), slot 1 → OP3 (= 2),
	// slot 2 → OP2 (= 1), slot 3 → OP4 (= 3).
	static const int kSlotToOp[4] = { 0, 2, 1, 3 };
	nChannel = nCh;
	nOpIndex = kSlotToOp[nSlot];
	return true;
}

// handle a write to any FM register ($28, $30..$B2)

void CPC88Opna::OnFmRegisterWrite(int nAddress, uint8_t btData) {
	// Always count writes, even when X88_FM_DEBUG is off — the
	// counter is reported by the per-second [FM-mix] status line.
	extern int g_nFmTotalWrites;
	g_nFmTotalWrites++;
	// Optional debug logging — enable with X88_FM_DEBUG=1.
	{
		static int s_check = -1;
		static bool s_enabled = false;
		if (s_check < 0) {
			s_check = 1;
			const char* p = getenv("X88_FM_DEBUG");
			s_enabled = (p != NULL) && (*p != '\0') && (*p != '0');
		}
		if (s_enabled) {
			// Log only "interesting" register writes (algorithm /
			// feedback / key on/off) so we can see how the music
			// is configuring channels without drowning out the
			// [FM-mix] status lines.
			if ((nAddress >= 0xB0 && nAddress <= 0xB2) ||
			    nAddress == 0x28)
			{
				fprintf(stderr,
					"[FM]  $%02X=%02X\n", nAddress, btData);
				fflush(stderr);
			}
		}
	}

	// Key on/off
	if (nAddress == 0x28) {
		OnFmKeyOnOff(btData);
		return;
	}

	// Operator slot register ($30..$9E)
	if ((nAddress >= 0x30) && (nAddress <= 0x9E)) {
		int nCh, nOp;
		if (!ResolveFmSlotAddress(nAddress, nCh, nOp)) {
			return;  // unused slot column
		}
		SFmOperator& op = m_aFmCh[nCh].aOp[nOp];
		int nGroup = nAddress & 0xF0;
		switch (nGroup) {
		case 0x30:  // DT (3 bit) + MUL (4 bit)
			op.btDt  = (btData >> 4) & 0x07;
			op.btMul = btData & 0x0F;
			RecomputeFmOperatorPhaseInc(nCh, nOp);
			break;
		case 0x40:  // TL (7 bit) — high bit ignored
			op.btTl = btData & 0x7F;
			break;
		case 0x50:  // KS (2 bit, bits 7-6) + AR (5 bit, bits 4-0)
			op.btKs = (btData >> 6) & 0x03;
			op.btAr = btData & 0x1F;
			break;
		case 0x60:  // AM (bit 7, ignored on YM2203) + DR (5 bit)
			op.btDr = btData & 0x1F;
			break;
		case 0x70:  // SR (5 bit)
			op.btSr = btData & 0x1F;
			break;
		case 0x80:  // SL (4 bit, bits 7-4) + RR (4 bit, bits 3-0)
			op.btSl = (btData >> 4) & 0x0F;
			op.btRr = btData & 0x0F;
			break;
		case 0x90:  // SSG-type Envelope Control ($90-$9E)
			// Manual section 2-5-2: bit 3 = enable, bits 2..0 =
			// Att/Alt/Hold shape selectors (same semantics as the
			// SSG envelope generator's $0D shape register).
			op.btSsgEg = btData & 0x0F;
			break;
		}
		return;
	}

	// Channel F-Number low byte ($A0..$A2). Real hardware: this is the
	// COMMIT — the chip applies the previously latched BLOCK+FNUM[10:8]
	// (from $A4..$A6) together with this low byte atomically. Writing
	// $A0 without first writing $A4 is a software bug; we apply the
	// last-known latch value, which is what real hardware would do too.
	if ((nAddress >= 0xA0) && (nAddress <= 0xA2)) {
		int nCh = nAddress - 0xA0;
		uint8_t btLatch = m_abtFmFnumLatch[nCh];
		m_aFmCh[nCh].btBlock = (btLatch >> 3) & 0x07;
		m_aFmCh[nCh].wFnum =
			(((uint16_t)(btLatch & 0x07)) << 8) | btData;
		RecomputeFmChannelPhaseIncs(nCh);
		return;
	}
	// Channel BLOCK + F-Number high ($A4..$A6). Latch only — no commit.
	if ((nAddress >= 0xA4) && (nAddress <= 0xA6)) {
		int nCh = nAddress - 0xA4;
		m_abtFmFnumLatch[nCh] = btData & 0x3F;  // BLOCK[2:0] + FNUM[10:8]
		return;
	}
	// CH3 special mode F-Number low for OP1/OP2/OP3 ($A8..$AA) — commit.
	if ((nAddress >= 0xA8) && (nAddress <= 0xAA)) {
		int nOp = nAddress - 0xA8;  // 0..2
		uint8_t btLatch = m_abtFmCh3FnumLatch[nOp];
		m_aFmCh[2].abtBlockPerOp[nOp] = (btLatch >> 3) & 0x07;
		m_aFmCh[2].awFnumPerOp[nOp] =
			(((uint16_t)(btLatch & 0x07)) << 8) | btData;
		if (m_bFmCh3SpecialMode) {
			RecomputeFmOperatorPhaseInc(2, nOp);
		}
		return;
	}
	// CH3 special mode BLOCK + F-Number high for OP1/OP2/OP3 ($AC..$AE)
	// — latch only.
	if ((nAddress >= 0xAC) && (nAddress <= 0xAE)) {
		int nOp = nAddress - 0xAC;  // 0..2
		m_abtFmCh3FnumLatch[nOp] = btData & 0x3F;
		return;
	}
	// Channel feedback / algorithm ($B0..$B2)
	if ((nAddress >= 0xB0) && (nAddress <= 0xB2)) {
		int nCh = nAddress - 0xB0;
		m_aFmCh[nCh].btFb   = (btData >> 3) & 0x07;
		m_aFmCh[nCh].btAlgo = btData & 0x07;
		return;
	}
}

// handle key on/off ($28)

void CPC88Opna::OnFmKeyOnOff(uint8_t btData) {
	int nCh = btData & 0x07;
	if (nCh >= FM_CHANNEL_COUNT) {
		return;
	}
	SFmChannel& ch = m_aFmCh[nCh];
	// Bits 4..7 = OP1..OP4 key on (1) / off (0). The op order in this
	// register is the *physical* OP1..OP4 sequence (not the slot order
	// from $30..$9E).
	for (int nOp = 0; nOp < FM_OP_PER_CHANNEL; nOp++) {
		bool bKey = ((btData >> (4 + nOp)) & 1) != 0;
		SFmOperator& op = ch.aOp[nOp];
		if (bKey && !op.bKeyOn) {
			// Key-on edge: transition to ATTACK and reset phase.
			// (YM2203 actually does NOT reset phase on key-on, but
			// for our simple implementation a reset gives a cleaner
			// attack and is closer to what most game music expects.)
			op.bKeyOn = true;
			op.nEnvState = FM_ENV_ATTACK;
			op.nEnvCounter = 0;
			// Don't reset env_level — letting it ramp from its
			// current attenuation produces less "click" than jumping
			// straight to 1023.
			// SSG-EG: initial inversion comes from the Att (bit 2)
			// of the shape, but only when SSG-EG is enabled (bit 3).
			if (op.btSsgEg & 0x08) {
				op.bSsgEgInverted = (op.btSsgEg & 0x04) != 0;
			} else {
				op.bSsgEgInverted = false;
			}
		} else if (!bKey && op.bKeyOn) {
			op.bKeyOn = false;
			// If SSG-EG was running with the envelope inverted,
			// "freeze" the apparent level into the real env_level
			// so that the standard RELEASE rate then attenuates it
			// from where the listener last heard it (rather than
			// from env_level=1023, which under inversion would mean
			// "still loud" and a release that does nothing audible).
			if ((op.btSsgEg & 0x08) && op.bSsgEgInverted) {
				op.nEnvLevel = 1023 - op.nEnvLevel;
				if (op.nEnvLevel < 0)    op.nEnvLevel = 0;
				if (op.nEnvLevel > 1023) op.nEnvLevel = 1023;
				op.bSsgEgInverted = false;
			}
			op.nEnvState = FM_ENV_RELEASE;
		}
	}
}

////////////////////////////////////////////////////////////
// Phase C-FM-2: per-sample synthesis (ALGO 7 only)

// compute key-scale rate offset (KSR) for a given operator

int CPC88Opna::ComputeFmKsr(int nChannel, int nOpIndex) {
	if ((nChannel < 0) || (nChannel >= FM_CHANNEL_COUNT)) return 0;
	if ((nOpIndex < 0) || (nOpIndex >= FM_OP_PER_CHANNEL)) return 0;
	const SFmChannel& ch = m_aFmCh[nChannel];
	const SFmOperator& op = ch.aOp[nOpIndex];
	uint16_t wFnum;
	uint8_t btBlock;
	if ((nChannel == 2) && m_bFmCh3SpecialMode && (nOpIndex < 3)) {
		wFnum   = ch.awFnumPerOp[nOpIndex];
		btBlock = ch.abtBlockPerOp[nOpIndex];
	} else {
		wFnum   = ch.wFnum;
		btBlock = ch.btBlock;
	}
	// Per the YM2608 application manual (Table 2-8):
	//   KC[4:0]   = (BLOCK << 2) | (N4 << 1) | N3
	//   N4        = F11
	//   N3        = F11·(F10+F9+F8) + !F11·F10·F9·F8
	//   Rks       = KC >> (3 - KS)
	// (Earlier we used the incorrect "(BLOCK<<1) | top bit" form which
	// produced half the spec rate scaling — high notes decayed too slowly
	// and the KS=3 maximum range was 0..15 instead of 0..31.)
	int nF11 = (wFnum >> 10) & 1;
	int nF10 = (wFnum >>  9) & 1;
	int nF9  = (wFnum >>  8) & 1;
	int nF8  = (wFnum >>  7) & 1;
	int nN4  = nF11;
	int nN3;
	if (nF11) {
		nN3 = nF10 | nF9 | nF8;
	} else {
		nN3 = nF10 & nF9 & nF8;
	}
	int nNote = (nN4 << 1) | nN3;
	int nKc   = ((int)btBlock << 2) | nNote;  // 0..31
	int nShift = 3 - op.btKs;
	if (nShift < 0) nShift = 0;
	return nKc >> nShift;
}

// advance an operator's envelope state machine by one output sample

void CPC88Opna::AdvanceFmEnvelope(SFmOperator& op, int nKsr) {
	int nRate = 0;
	switch (op.nEnvState) {
	case FM_ENV_ATTACK:
		// YM2151/YM2203 rule: a 5-bit AR/DR/SR field of 0 forces the
		// rate to 0 (envelope stopped) regardless of KSR. Without this
		// check, an AR=0 op would still attack slowly via KSR alone.
		if (op.btAr == 0) return;
		nRate = op.btAr * 2 + nKsr;
		if (nRate > 63) nRate = 63;
		op.nEnvCounter += m_anFmEnvRateTable[nRate];
		while (op.nEnvCounter >= (1 << 16)) {
			op.nEnvCounter -= (1 << 16);
			// Standard YM2151 attack curve: exponential approach to
			// zero attenuation (= full volume). At each step the
			// remaining attenuation is reduced by ~1/16 of itself,
			// plus a constant 1 to guarantee progress at very low
			// attenuation values.
			int nDelta = (op.nEnvLevel >> 4) + 1;
			op.nEnvLevel -= nDelta;
			if (op.nEnvLevel <= 0) {
				op.nEnvLevel = 0;
				op.nEnvState = FM_ENV_DECAY;
				op.nEnvCounter = 0;
				return;
			}
		}
		break;
	case FM_ENV_DECAY:
		// DR=0 → envelope stops (true "no decay"). KSR alone must not
		// drive the rate.
		if (op.btDr == 0) return;
		nRate = op.btDr * 2 + nKsr;
		if (nRate > 63) nRate = 63;
		op.nEnvCounter += m_anFmEnvRateTable[nRate];
		while (op.nEnvCounter >= (1 << 16)) {
			op.nEnvCounter -= (1 << 16);
			op.nEnvLevel++;
			// Sustain level: per the YM2608 application manual
			// (Table 2-7), SL bits D7..D4 carry weights 24/12/6/3 dB,
			// i.e. 1 SL step = 3 dB. Our envelope unit is 96 dB / 1024
			// ≈ 0.094 dB, so 1 SL step = 32 env units. SL=15 (all
			// bits set) is a hardware special case that jumps from
			// 42 dB to 93 dB (≈ 992 env units, ~"fully attenuated").
			// (Earlier this code used SL*64 — wrongly assuming a 6 dB
			// step — which made decay overshoot to half the intended
			// sustain level.)
			int nSlThreshold = (op.btSl == 0x0F)? 992: (op.btSl * 32);
			if (op.nEnvLevel >= nSlThreshold) {
				op.nEnvState = FM_ENV_SUSTAIN;
				return;
			}
		}
		break;
	case FM_ENV_SUSTAIN:
		// SR (= D2R) = 0 → "true sustain", envelope holds indefinitely
		// at the sustain level. This is the most common piano patch
		// idiom (long held note). Without this check, KSR keeps the
		// envelope crawling toward silence and high notes die out
		// audibly faster than low notes.
		if (op.btSr == 0) return;
		nRate = op.btSr * 2 + nKsr;
		if (nRate > 63) nRate = 63;
		op.nEnvCounter += m_anFmEnvRateTable[nRate];
		while (op.nEnvCounter >= (1 << 16)) {
			op.nEnvCounter -= (1 << 16);
			op.nEnvLevel++;
			if (op.nEnvLevel >= 1023) {
				op.nEnvLevel = 1023;
				// SSG-EG: at the silent endpoint of the natural
				// envelope flow, the operator may loop / alternate /
				// hold per the shape register. If SSG-EG is not
				// enabled the operator falls through to OFF as
				// usual.
				if (ApplySsgEgEndpoint(op)) return;
				op.nEnvState = FM_ENV_OFF;
				return;
			}
		}
		break;
	case FM_ENV_RELEASE:
		// Per the YM2608 application manual (page 30):
		//   Rate = 2R + Rks
		//   where R = 2*RR + 1 (RR is the 4-bit register field)
		//   ⇒ Rate = 4*RR + 2 + Rks
		// The "+2" guarantees RR=0 still produces a (very slow) release
		// rather than a true hold. Earlier code used (RR<<1)+1+Rks
		// which gave half the spec rate, making piano releases ~2×
		// longer than the target hardware.
		nRate = (op.btRr << 2) + 2 + nKsr;
		if (nRate > 63) nRate = 63;
		if (nRate < 2) return;
		op.nEnvCounter += m_anFmEnvRateTable[nRate];
		while (op.nEnvCounter >= (1 << 16)) {
			op.nEnvCounter -= (1 << 16);
			op.nEnvLevel++;
			if (op.nEnvLevel >= 1023) {
				op.nEnvLevel = 1023;
				op.nEnvState = FM_ENV_OFF;
				return;
			}
		}
		break;
	case FM_ENV_OFF:
	default:
		op.nEnvLevel = 1023;
		break;
	}
}

// SSG-EG endpoint handler — see header for full description.

bool CPC88Opna::ApplySsgEgEndpoint(SFmOperator& op) {
	if ((op.btSsgEg & 0x08) == 0) {
		return false;  // SSG-EG not enabled — caller falls through
	}
	bool bAlt  = (op.btSsgEg & 0x02) != 0;
	bool bHold = (op.btSsgEg & 0x01) != 0;
	if (bHold) {
		// Hold at this end. With Alt set the inversion is flipped
		// once before the freeze, so shapes 3 / 5 finish at "loud"
		// (env_level = 1023 read inverted = 0 effective) and shapes
		// 1 / 7 finish at "silent" (env_level = 1023 read straight).
		if (bAlt) {
			op.bSsgEgInverted = !op.bSsgEgInverted;
		}
		op.nEnvLevel = 1023;
		op.nEnvState = FM_ENV_OFF;
	} else {
		// Loop: optionally toggle inversion (triangle), then restart
		// the envelope at env_level = 0. Sawtooth shapes (Alt=0) just
		// reset; triangle shapes (Alt=1) toggle inversion so the next
		// pass appears to swing in the opposite direction.
		if (bAlt) {
			op.bSsgEgInverted = !op.bSsgEgInverted;
		}
		op.nEnvLevel   = 0;
		op.nEnvCounter = 0;
		// Skip ATTACK on restart (real chip has AR fixed at 1F under
		// SSG-EG = instant attack). Going straight to DECAY lets DR
		// drive the next "decay" segment immediately.
		op.nEnvState = FM_ENV_DECAY;
	}
	return true;
}

// compute one operator's output sample (with optional modulation)

int CPC88Opna::RenderFmOperator(SFmOperator& op, int nModulation) {
	// Advance phase accumulator.
	op.nPhase = (op.nPhase + op.nPhaseInc) & ((1u << FM_PHASE_BITS) - 1u);

	// 10-bit phase index over a full sin period.
	int nPhaseIdx = (int)(op.nPhase >> (FM_PHASE_BITS - 10));
	nPhaseIdx = (nPhaseIdx + nModulation) & 0x3FF;

	bool bNeg = (nPhaseIdx & 0x200) != 0;
	int nQuad = nPhaseIdx & 0xFF;
	if (nPhaseIdx & 0x100) {
		// Descending half — mirror the index.
		nQuad ^= 0xFF;
	}

	int nSinLog = m_anFmSinTable[nQuad];

	// SSG-EG: when the shape is being read in inverted polarity, the
	// effective envelope value is mirrored around the silent endpoint
	// (1023 - env). This makes the same env state machine drive both
	// "down" and "up" segments of triangle/sawtooth-up shapes.
	int nEffEnv = (op.btSsgEg & 0x08) && op.bSsgEgInverted
	            ? (1023 - op.nEnvLevel)
	            : op.nEnvLevel;
	if (nEffEnv < 0)    nEffEnv = 0;
	if (nEffEnv > 1023) nEffEnv = 1023;

	// Combined attenuation in 1/256-octave units (1 unit ≈ 0.0234 dB):
	//   sin_log    : 0..~2138 (function of phase, log domain)
	//   env << 2   : 0..4092  (function of envelope, 0.094 dB/step)
	//   tl  << 5   : 0..4064  (function of total level, 0.75 dB/step)
	// Peak op output = exp[0] = 8192 (when sin/env/tl all 0).
	int nTotalAtten = nSinLog + (nEffEnv << 2) + ((int)op.btTl << 5);
	if (nTotalAtten >= (13 << 8)) {
		// > 13 octaves down (~78 dB) — effectively silent. The exp
		// table peak of 8192 supports useful values down to ~13
		// octaves of right-shift; beyond that nLinear becomes 0
		// anyway, so cutting off here is just a perf optimization.
		return 0;
	}

	// Convert log → linear via exp lookup + integer octave shift.
	int nFracIdx = nTotalAtten & 0xFF;
	int nOctave  = nTotalAtten >> 8;
	int nLinear = m_anFmExpTable[nFracIdx] >> nOctave;

	int nOut = bNeg? -nLinear: nLinear;
	op.nOutPrev = nOut;
	return nOut;
}

// produce one mono FM sample by mixing all 3 channels

int CPC88Opna::RenderFmSample() {
	int anChOut[FM_CHANNEL_COUNT] = { 0, 0, 0 };
	int nMix = 0;
	for (int nCh = 0; nCh < FM_CHANNEL_COUNT; nCh++) {
		// Always run RenderFmChannel so envelopes / phase / state stay
		// coherent across mute toggles. Suppress only the contribution
		// to the mix when this channel is muted.
		// RenderFmChannel returns the channel's per-sample mono output
		// at full operator amplitude (peak ≈ ±8192 per op since the
		// 14-bit op output extension). Modulation feed-forward inside
		// RenderFmChannel uses the full amplitude — only the FINAL
		// mix value is right-shifted below to keep the section's int16
		// output in range without halving the per-op modulation index.
		anChOut[nCh] = RenderFmChannel(nCh);
		if (!m_abFmChMute[nCh]) {
			nMix += anChOut[nCh];
		}
	}
	// Halve the FM section's contribution to compensate for the doubled
	// per-op output amplitude (4096 → 8192). The shift here, not inside
	// the channel render path, preserves the original modulation index
	// for the modulator chain while keeping the final mix at the same
	// dynamic range as before the 14-bit extension.
	nMix >>= 1;
	// DEBUG: comprehensive status dump emitted to stderr every 0.25 sec
	// when X88_FM_DEBUG=1. Output covers every parameter that the FM
	// renderer reads — algorithm, feedback, F-Number/Block, and per-op
	// (KeyOn, AR/DR/SR/RR/SL/KS/MUL/DT/TL, SSG-EG, env state/level,
	// inversion flag) — plus per-channel mute and contribution to the
	// final mix. The intent is "dump everything you'd need to reproduce
	// the same audio in another emulator".
	{
		static int s_check = -1;
		static bool s_enabled = false;
		if (s_check < 0) {
			s_check = 1;
			const char* p = getenv("X88_FM_DEBUG");
			s_enabled = (p != NULL) && (*p != '\0') && (*p != '0');
		}
		if (s_enabled) {
			static int s_sampleCount = 0;
			static int s_nonZeroCount = 0;
			static int s_lastReport = 0;
			static int s_minSample = 0;
			static int s_maxSample = 0;
			static long long s_sumAbs = 0;
			static long long s_sumAbsCh[FM_CHANNEL_COUNT] = { 0, 0, 0 };
			static int s_minCh[FM_CHANNEL_COUNT] = { 0, 0, 0 };
			static int s_maxCh[FM_CHANNEL_COUNT] = { 0, 0, 0 };
			s_sampleCount++;
			if (nMix != 0) s_nonZeroCount++;
			if (nMix < s_minSample) s_minSample = nMix;
			if (nMix > s_maxSample) s_maxSample = nMix;
			s_sumAbs += (nMix < 0)? -nMix: nMix;
			for (int c = 0; c < FM_CHANNEL_COUNT; c++) {
				int v = anChOut[c];
				s_sumAbsCh[c] += (v < 0)? -v: v;
				if (v < s_minCh[c]) s_minCh[c] = v;
				if (v > s_maxCh[c]) s_maxCh[c] = v;
			}
			if (s_sampleCount - s_lastReport >= 11025) {
				int nReportFrames = s_sampleCount - s_lastReport;
				int nMeanAbs = (int)(s_sumAbs / nReportFrames);
				s_lastReport = s_sampleCount;
				extern int g_nFmTotalWrites;
				extern int g_nOpnTotalWrites;
				fprintf(stderr,
					"\n========== [FM dump @ s=%d (~%.2fs)] ==========\n"
					"writes: opn_total=%d fm_total=%d  mix: nz=%d min=%d max=%d meanAbs=%d\n"
					"section mute: FM=%c SSG=%c\n",
					s_sampleCount, (double)s_sampleCount / 44100.0,
					g_nOpnTotalWrites, g_nFmTotalWrites,
					s_nonZeroCount, s_minSample, s_maxSample, nMeanAbs,
					m_bFmMute? 'M': '.', m_bSsgMute? 'M': '.');
				static const char* kStateStr[5] = {
					"OFF", "ATK", "DEC", "SUS", "REL"
				};
				for (int c = 0; c < FM_CHANNEL_COUNT; c++) {
					const SFmChannel& ch = m_aFmCh[c];
					int nMeanCh = (int)(s_sumAbsCh[c] / nReportFrames);
					fprintf(stderr,
						"FM ch%d %s ALGO=%d FB=%d FNum=%4d Blk=%d "
						"contrib: min=%d max=%d meanAbs=%d\n",
						c + 1,
						m_abFmChMute[c]? "[MUTE]": "      ",
						ch.btAlgo, ch.btFb,
						ch.wFnum, ch.btBlock,
						s_minCh[c], s_maxCh[c], nMeanCh);
					for (int o = 0; o < FM_OP_PER_CHANNEL; o++) {
						const SFmOperator& op = ch.aOp[o];
						int nState = op.nEnvState;
						if (nState < 0 || nState > 4) nState = 0;
						fprintf(stderr,
							"  OP%d %s TL=%3d AR=%2d DR=%2d SR=%2d RR=%2d SL=%2d "
							"KS=%d MUL=%2d DT=%d  ssgEG=%X%s  env=%4d %s\n",
							o + 1,
							op.bKeyOn? "ON ": "off",
							op.btTl, op.btAr, op.btDr, op.btSr, op.btRr, op.btSl,
							op.btKs, op.btMul, op.btDt,
							op.btSsgEg,
							op.bSsgEgInverted? "i": " ",
							op.nEnvLevel, kStateStr[nState]);
					}
				}
				fflush(stderr);
				s_nonZeroCount = 0;
				s_minSample = 0;
				s_maxSample = 0;
				s_sumAbs = 0;
				for (int c = 0; c < FM_CHANNEL_COUNT; c++) {
					s_sumAbsCh[c] = 0;
					s_minCh[c] = 0;
					s_maxCh[c] = 0;
				}
			}
		}
	}
	return nMix;
}

// produce one channel's mono sample using its current algorithm

int CPC88Opna::RenderFmChannel(int nChannel) {
	if ((nChannel < 0) || (nChannel >= FM_CHANNEL_COUNT)) return 0;
	SFmChannel& ch = m_aFmCh[nChannel];

	// Advance envelopes for all 4 operators of this channel.
	for (int nOp = 0; nOp < FM_OP_PER_CHANNEL; nOp++) {
		AdvanceFmEnvelope(ch.aOp[nOp], ComputeFmKsr(nChannel, nOp));
	}

	// OP1 self-feedback: sum of the two most recent OP1 outputs,
	// shifted to convert into phase-index units. FB=0 disables
	// feedback; FB=1..7 maps to a right-shift of (10..4).
	//
	// The shift amount must compensate for the operator output peak
	// so that FB=7 produces the manual-specified 4π modulation level
	// (= 2048 phase units) when the modulator is at maximum:
	//
	//   peak op output     = 8192  (14-bit, post-2026 update)
	//   peak nSum (=2 ops) = 16384
	//   FB=7 target        = 2048 phase units
	//   shift = 16384 / 2048 = 8 = >> 3 = >> (10-7)
	//
	// (Pre-2026 the peak was 4096 and the shift was `>> (9-FB)`.)
	int nFbMod = 0;
	if (ch.btFb > 0) {
		int nSum = ch.anFeedback[0] + ch.anFeedback[1];
		nFbMod = nSum >> (10 - ch.btFb);
	}

	// Operator outputs in physical OP1..OP4 order.
	int nOp1Out, nOp2Out, nOp3Out, nOp4Out;
	int nMix = 0;

	// OP1 always uses feedback as its modulation input. We compute it
	// first because most algorithms feed its output into other ops.
	nOp1Out = RenderFmOperator(ch.aOp[0], nFbMod);
	// Update feedback history (FIFO of length 2).
	ch.anFeedback[1] = ch.anFeedback[0];
	ch.anFeedback[0] = nOp1Out;

	// The remaining operators are routed per-algorithm. Modulation
	// inputs are passed through unshifted: the operator output is
	// already in the same units as the 10-bit phase index, so the
	// addition (and subsequent &0x3FF mask in RenderFmOperator) does
	// the right thing on its own. (Previously we shifted modulation
	// by >> 1 here, which combined with our 13-bit op output gave
	// half the modulation depth of the standard YM2203 — leading to
	// a notably duller / less FM-like timbre with weak harmonics.)
	switch (ch.btAlgo) {
	case 0:
		// OP1 → OP2 → OP3 → OP4 → out  (serial 4-op)
		nOp2Out = RenderFmOperator(ch.aOp[1], nOp1Out);
		nOp3Out = RenderFmOperator(ch.aOp[2], nOp2Out);
		nOp4Out = RenderFmOperator(ch.aOp[3], nOp3Out);
		nMix = nOp4Out;
		break;
	case 1:
		// (OP1 + OP2) → OP3 → OP4 → out  (parallel mods, serial out)
		nOp2Out = RenderFmOperator(ch.aOp[1], 0);
		nOp3Out = RenderFmOperator(ch.aOp[2], nOp1Out + nOp2Out);
		nOp4Out = RenderFmOperator(ch.aOp[3], nOp3Out);
		nMix = nOp4Out;
		break;
	case 2:
		// (OP1 + (OP2 → OP3)) → OP4 → out
		nOp2Out = RenderFmOperator(ch.aOp[1], 0);
		nOp3Out = RenderFmOperator(ch.aOp[2], nOp2Out);
		nOp4Out = RenderFmOperator(ch.aOp[3], nOp1Out + nOp3Out);
		nMix = nOp4Out;
		break;
	case 3:
		// ((OP1 → OP2) + OP3) → OP4 → out
		nOp2Out = RenderFmOperator(ch.aOp[1], nOp1Out);
		nOp3Out = RenderFmOperator(ch.aOp[2], 0);
		nOp4Out = RenderFmOperator(ch.aOp[3], nOp2Out + nOp3Out);
		nMix = nOp4Out;
		break;
	case 4:
		// (OP1 → OP2☆) + (OP3 → OP4☆)  (two 2-op chains, both heard)
		nOp2Out = RenderFmOperator(ch.aOp[1], nOp1Out);
		nOp3Out = RenderFmOperator(ch.aOp[2], 0);
		nOp4Out = RenderFmOperator(ch.aOp[3], nOp3Out);
		nMix = nOp2Out + nOp4Out;
		break;
	case 5:
		// OP1 → {OP2☆, OP3☆, OP4☆}  (1 modulator, 3 carriers)
		nOp2Out = RenderFmOperator(ch.aOp[1], nOp1Out);
		nOp3Out = RenderFmOperator(ch.aOp[2], nOp1Out);
		nOp4Out = RenderFmOperator(ch.aOp[3], nOp1Out);
		nMix = nOp2Out + nOp3Out + nOp4Out;
		break;
	case 6:
		// (OP1 → OP2☆) + OP3☆ + OP4☆
		nOp2Out = RenderFmOperator(ch.aOp[1], nOp1Out);
		nOp3Out = RenderFmOperator(ch.aOp[2], 0);
		nOp4Out = RenderFmOperator(ch.aOp[3], 0);
		nMix = nOp2Out + nOp3Out + nOp4Out;
		break;
	case 7:
	default:
		// OP1☆ + OP2☆ + OP3☆ + OP4☆  (4 carriers, additive)
		nOp2Out = RenderFmOperator(ch.aOp[1], 0);
		nOp3Out = RenderFmOperator(ch.aOp[2], 0);
		nOp4Out = RenderFmOperator(ch.aOp[3], 0);
		nMix = nOp1Out + nOp2Out + nOp3Out + nOp4Out;
		break;
	}

	return nMix;
}
