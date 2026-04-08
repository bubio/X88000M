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

void CPC88Opna::Initialize() {
	m_nBaseClock = 4;
	BuildSsgTables();
	if (m_nSampleRate <= 0) {
		// Default to 44.1 kHz; the frontend may override via
		// SetSampleRate() before the first PassClock() call.
		SetSampleRate(44100);
	}
	for (int n = 0; n < REGISTER_COUNT; n++) {
		m_abtRegisters[n] = 0;
	}
	m_nSampleAccumX256 = 0;
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
	m_bSsgEnvHolding   = false;
	// Mixer default: tones disabled, noise disabled (all bits 1).
	m_btSsgMixer       = 0xFF;
	UpdateSsgTickRate();
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
			int nSample = RenderSsgSample();
			// FM section will be added in Phase C-FM (mixed in here).
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
	// our fc/8 SSG tick rate is exactly EP ticks per step.
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
		// IMPORTANT: when BOTH tone and noise are disabled, the
		// "force high" rule above would emit constant nAmp on this
		// channel — i.e. a DC level. Real hardware strips that DC via
		// AC coupling caps; in software we have to do it ourselves by
		// outputting 0 in this case. Without this check, games that
		// silence a channel by disabling its mixer (a very common
		// idiom) leave behind a click/pop when the volume changes.
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
		// One env step every EP ticks at fc/8 (see AdvanceSsgOneTick).
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
