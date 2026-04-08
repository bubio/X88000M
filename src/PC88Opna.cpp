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
int                   CPC88Opna::m_anFmDetuneTable[CPC88Opna::FM_DT_KC_RANGES][CPC88Opna::FM_DT_KC_VALUES];

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
			if (!m_bSsgMute) nSample += nSsgSample;
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
	// SSG internal counters advance at base / prescaler_psg / 4. The
	// literal YM2149 datasheet formula f = fc/(16*TP) is equivalent
	// to "advance at fc/4 with TP/2 toggle interval" — practical PC-88
	// SSG implementations use the fc/4 cadence so that tone counters
	// stay integer with the TP values that PC-88 BGM actually writes.
	// For 4 MHz / prescaler 4 this is 4_000_000 / 4 / 4 = 250 000 Hz.
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
		if (--m_anSsgToneCounter[n] <= 0) {
			int nPeriod = m_anSsgTonePeriod[n];
			if (nPeriod < 1) nPeriod = 1;
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
	int nMix = 0;
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		// Mixer bits: 1 = disabled, 0 = enabled. When disabled the
		// corresponding signal is forced high so the AND of the two
		// would let the OTHER signal through.
		int nToneDisable  = (m_btSsgMixer >> n)        & 1;
		int nNoiseDisable = (m_btSsgMixer >> (n + 3))  & 1;
		// When BOTH tone and noise are disabled the channel emits a
		// constant DC level (vol DAC pass-through). We silence it here
		// to avoid click/pop on scene transitions; some games rely on
		// this being silent.
		if (nToneDisable && nNoiseDisable) {
			continue;
		}
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
		nMix += nAmp;
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
	switch (nAddress) {
	case 0x00: // CH A tone period low
	case 0x02: // CH B tone period low
	case 0x04: { // CH C tone period low
		int nCh = nAddress >> 1;
		int nHi = m_abtRegisters[nAddress + 1] & 0x0F;
		m_anSsgTonePeriod[nCh] = (nHi << 8) | btData;
		if (m_anSsgTonePeriod[nCh] < 1) m_anSsgTonePeriod[nCh] = 1;
		break;
	}
	case 0x01: // CH A tone period high (4 bits)
	case 0x03: // CH B tone period high
	case 0x05: { // CH C tone period high
		int nCh = nAddress >> 1;
		int nLo = m_abtRegisters[nAddress - 1];
		m_anSsgTonePeriod[nCh] = ((btData & 0x0F) << 8) | nLo;
		if (m_anSsgTonePeriod[nCh] < 1) m_anSsgTonePeriod[nCh] = 1;
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
	// Per-operator peak amplitude is 4096 — chosen so that the worst
	// case (3 channels × 2 carriers in ALGO 4 = 6 ops at max) gives
	// 6 × 4096 = 24576, fitting int16 alongside SSG output.
	for (int n = 0; n < FM_EXP_TABLE_SIZE; n++) {
		double dFrac = (double)n / (double)FM_EXP_TABLE_SIZE;
		double dExp = pow(2.0, -dFrac);
		m_anFmExpTable[n] = (int)(dExp * 4096.0 + 0.5);
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
			int nShift = 11 - (nRate >> 2);
			int nAdd   = 4 + (nRate & 3);
			if (nShift < 0) nShift = 0;
			// (add << 16) >> shift = (add * 65536) / (1 << shift)
			m_anFmEnvRateTable[nRate] = (nAdd << 16) >> nShift;
		}
	}

	// Detune table: simple monotonic approximation of the YM2203 DT
	// (DeTune) feature. Real hardware uses a small lookup table that
	// gives ±0..16 phase increments per key-code. We approximate it
	// as DT_strength × (1 + KC/8), which produces a similar gentle
	// "chorus" effect without copying the chip's literal table values.
	// Phase C-FM-3 uses this; Phase C-FM-4 may refine if needed.
	for (int r = 0; r < FM_DT_KC_RANGES; r++) {
		for (int v = 0; v < FM_DT_KC_VALUES; v++) {
			// r = DT strength index 0..3 (0 = no detune)
			// v = key code 0..31
			m_anFmDetuneTable[r][v] = r * (1 + (v >> 3));
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
	// Apply DT (detune). The 3-bit DT field is split into a strength
	// index (low 2 bits) and a sign (bit 2). DT 0/4 means "no detune".
	// We add (rather than multiply) the detune offset so that the
	// effect is largest for high notes (KC range 3) and smallest for
	// low notes (KC range 0).
	int nDtStrength = op.btDt & 0x03;
	bool bDtNeg     = (op.btDt & 0x04) != 0;
	if (nDtStrength != 0) {
		int nKc = ((int)btBlock << 2) | ((int)wFnum >> 9);  // 0..31 approx
		if (nKc < 0)  nKc = 0;
		if (nKc > 31) nKc = 31;
		long long nDtOfs = (long long)m_anFmDetuneTable[nDtStrength][nKc]
		                  * (long long)m_nFmPhaseScaleX16;
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
		case 0x90:  // SSG-EG (5 bit) — not implemented in this phase
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
		} else if (!bKey && op.bKeyOn) {
			op.bKeyOn = false;
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
	// Standard YM family KSR: ((BLOCK << 1) | top bit of FNUM) >> (3-KS)
	int nNote = (btBlock << 1) | ((wFnum >> 10) & 1);
	int nShift = 3 - op.btKs;
	if (nShift < 0) nShift = 0;
	return nNote >> nShift;
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
			// Sustain level: SL is 4-bit and each step is 6 dB of
			// attenuation per the YM2203 spec. Our envelope unit is
			// 96 dB / 1024 ≈ 0.094 dB, so 1 SL step = 64 env units.
			// SL=15 is the special case "fully attenuated" (= 1023).
			// (Earlier this used SL*32 which was half the proper
			// attenuation, causing decay → sustain to fire too early
			// and the sustained note to sit too loud.)
			int nSlThreshold = (op.btSl == 0x0F)? 1023: (op.btSl * 64);
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
				op.nEnvState = FM_ENV_OFF;
				return;
			}
		}
		break;
	case FM_ENV_RELEASE:
		// YM2151 release rate: (2*RR + 1) + KSR.
		// The "+1" lets RR=0 still produce a (very slow) release.
		// Earlier versions of this code had an extra "* 2" multiplier
		// here, which made every release 4× too fast and turned
		// piano/bass patches into short percussive "ton/don" sounds.
		nRate = (op.btRr << 1) + 1 + nKsr;
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

	// Combined attenuation in 1/256-octave units:
	//   sin_log    : 0..~1900 (function of phase)
	//   env << 2   : 0..4092  (function of envelope)
	//   tl  << 5   : 0..4064  (function of total level register)
	int nTotalAtten = nSinLog + (op.nEnvLevel << 2) + ((int)op.btTl << 5);
	if (nTotalAtten >= (12 << 8)) {
		// > 12 octaves down (~72 dB) — effectively silent.
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
	int nMix = 0;
	for (int nCh = 0; nCh < FM_CHANNEL_COUNT; nCh++) {
		nMix += RenderFmChannel(nCh);
	}
	// DEBUG: emit a status line every ~1 second to stderr if
	// X88_FM_DEBUG=1 is set. Reports per-channel envelope state and
	// level so we can see whether FM ops are progressing past silence.
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
			s_sampleCount++;
			if (nMix != 0) s_nonZeroCount++;
			if (nMix < s_minSample) s_minSample = nMix;
			if (nMix > s_maxSample) s_maxSample = nMix;
			s_sumAbs += (nMix < 0)? -nMix: nMix;
			// Report every 0.25 sec so a short capture catches the
			// state right after FM register init.
			if (s_sampleCount - s_lastReport >= 11025) {
				int nReportFrames = s_sampleCount - s_lastReport;
				int nMeanAbs = (int)(s_sumAbs / nReportFrames);
				s_lastReport = s_sampleCount;
				extern int g_nFmTotalWrites;
				extern int g_nOpnTotalWrites;
				const SFmChannel& ch0 = m_aFmCh[0];
				const SFmChannel& ch1 = m_aFmCh[1];
				const SFmChannel& ch2 = m_aFmCh[2];
				fprintf(stderr,
					"[FM-mix] s=%d nz=%d min=%d max=%d meanAbs=%d  opn=%d fm=%d\n"
					"  SSG mix=%02X vol=[%d%c %d%c %d%c] tone=[%d %d %d] np=%d ep=%d sh=%X envL=%d\n"
					"  ch0={A=%d FB=%d tl=[%d %d %d %d] L=[%d %d %d %d] s=[%d %d %d %d]}\n"
					"  ch1={A=%d FB=%d tl=[%d %d %d %d] L=[%d %d %d %d] s=[%d %d %d %d]}\n"
					"  ch2={A=%d FB=%d tl=[%d %d %d %d] L=[%d %d %d %d] s=[%d %d %d %d]}\n",
					s_sampleCount, s_nonZeroCount,
					s_minSample, s_maxSample, nMeanAbs,
					g_nOpnTotalWrites, g_nFmTotalWrites,
					m_btSsgMixer,
					m_anSsgVolume[0], m_abSsgUseEnv[0]? 'E': '.',
					m_anSsgVolume[1], m_abSsgUseEnv[1]? 'E': '.',
					m_anSsgVolume[2], m_abSsgUseEnv[2]? 'E': '.',
					m_anSsgTonePeriod[0], m_anSsgTonePeriod[1], m_anSsgTonePeriod[2],
					m_nSsgNoisePeriod, m_nSsgEnvPeriod,
					m_abtRegisters[0x0D] & 0x0F,
					m_nSsgEnvLevel,
					ch0.btAlgo, ch0.btFb,
					ch0.aOp[0].btTl, ch0.aOp[1].btTl,
					ch0.aOp[2].btTl, ch0.aOp[3].btTl,
					ch0.aOp[0].nEnvLevel, ch0.aOp[1].nEnvLevel,
					ch0.aOp[2].nEnvLevel, ch0.aOp[3].nEnvLevel,
					ch0.aOp[0].nEnvState, ch0.aOp[1].nEnvState,
					ch0.aOp[2].nEnvState, ch0.aOp[3].nEnvState,
					ch1.btAlgo, ch1.btFb,
					ch1.aOp[0].btTl, ch1.aOp[1].btTl,
					ch1.aOp[2].btTl, ch1.aOp[3].btTl,
					ch1.aOp[0].nEnvLevel, ch1.aOp[1].nEnvLevel,
					ch1.aOp[2].nEnvLevel, ch1.aOp[3].nEnvLevel,
					ch1.aOp[0].nEnvState, ch1.aOp[1].nEnvState,
					ch1.aOp[2].nEnvState, ch1.aOp[3].nEnvState,
					ch2.btAlgo, ch2.btFb,
					ch2.aOp[0].btTl, ch2.aOp[1].btTl,
					ch2.aOp[2].btTl, ch2.aOp[3].btTl,
					ch2.aOp[0].nEnvLevel, ch2.aOp[1].nEnvLevel,
					ch2.aOp[2].nEnvLevel, ch2.aOp[3].nEnvLevel,
					ch2.aOp[0].nEnvState, ch2.aOp[1].nEnvState,
					ch2.aOp[2].nEnvState, ch2.aOp[3].nEnvState);
				s_nonZeroCount = 0;
				s_minSample = 0;
				s_maxSample = 0;
				s_sumAbs = 0;
				fflush(stderr);
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
	// feedback; FB=1..7 maps to a right-shift of (9..3) so higher FB
	// values produce stronger phase modulation. This matches the
	// standard YM2151/YM2203 formula `(out_prev0 + out_prev1) >> (9-FB)`.
	// (Earlier code used `>> (10-FB)` which gave half the standard
	// feedback depth and contributed to a "thin" timbre.)
	int nFbMod = 0;
	if (ch.btFb > 0) {
		int nSum = ch.anFeedback[0] + ch.anFeedback[1];
		nFbMod = nSum >> (9 - ch.btFb);
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
