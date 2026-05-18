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
int CPC88Opna::m_anAddress[CPC88Opna::REGISTER_PAGE_COUNT];
int CPC88Opna::m_nSoundBoard2Address;
int CPC88Opna::m_nSoundBoard2AddressUpper;
bool CPC88Opna::m_bSoundBoard2InterruptMask;
int CPC88Opna::m_nWritePage;
int CPC88Opna::m_nWriteChannelBase;

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
CPC88Opna::STimerState CPC88Opna::m_timerInternalOpn;
CPC88Opna::STimerState CPC88Opna::m_timerExpansionOpna;

// interrupt vector change callback function

CPC88Opna::IntVectChangeCallback CPC88Opna::m_pIntVectChangeCallback;

// ----- Phase C synthesis state -----

// Mirror of all written register values

uint8_t CPC88Opna::m_abtRegisters[CPC88Opna::REGISTER_COUNT];
uint8_t CPC88Opna::m_abtSoundBoard2Registers[CPC88Opna::REGISTER_COUNT];
int CPC88Opna::m_nSoundBoardMode = CPC88Opna::SOUNDBOARD_OPN;
CPC88Opna::SystemFileOpenCallback CPC88Opna::m_pSystemFileOpenCallback;

// Output sample rate

int CPC88Opna::m_nSampleRate;

// Cycles per output sample (8.8 fixed point)

int CPC88Opna::m_nCyclesPerSampleX256;

// Sample accumulator (8.8 fixed point)

int CPC88Opna::m_nSampleAccumX256;

// Sample output callback

CPC88Opna::SampleOutputCallback CPC88Opna::m_pSampleOutputCallback;
long long CPC88Opna::m_nRenderedFrames;
bool CPC88Opna::m_bFmMute  = false;
bool CPC88Opna::m_bSsgMute = false;
bool CPC88Opna::m_bInternalOpnMute = false;
bool CPC88Opna::m_bExpansionOpnaMute = false;
bool CPC88Opna::m_bRhythmMute = false;
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
CPC88Opna::SSsgState CPC88Opna::m_ssgInternalOpn;
CPC88Opna::SSsgState CPC88Opna::m_ssgExpansionOpna;

// ----- FM (YM2203) state -----

CPC88Opna::SFmChannel CPC88Opna::m_aFmCh[CPC88Opna::FM_CHANNEL_COUNT];
bool                  CPC88Opna::m_bFmCh3SpecialMode;
bool                  CPC88Opna::m_bCsmKeyState;
uint8_t               CPC88Opna::m_abtFmFnumLatch[CPC88Opna::FM_CHANNEL_COUNT];
uint8_t               CPC88Opna::m_abtFmCh3FnumLatch[CPC88Opna::FM_CHANNEL_COUNT][3];
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
CPC88Opna::SRhythmSample CPC88Opna::m_aRhythmSample[CPC88Opna::RHYTHM_CHANNEL_COUNT];
CPC88Opna::SRhythmVoice  CPC88Opna::m_aRhythmVoice[CPC88Opna::RHYTHM_CHANNEL_COUNT];
bool CPC88Opna::m_bRhythmSamplesLoaded = false;
uint8_t CPC88Opna::m_btRhythmTotalLevel = 0x3F;
CPC88Opna::SAdpcmState CPC88Opna::m_adpcm;
uint8_t CPC88Opna::m_abtAdpcmRegisters[0x11];

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

static double ReadEnvDouble(const char* pszName, double dDefault,
	double dMin, double dMax)
{
	const char* psz = getenv(pszName);
	if ((psz == NULL) || (*psz == '\0')) {
		return dDefault;
	}
	double d = atof(psz);
	if ((d < dMin) || (d > dMax)) {
		return dDefault;
	}
	return d;
}

static int ScaleSignedInt(int nValue, double dScale) {
	double d = (double)nValue * dScale;
	if (d >= 0.0) {
		return (int)(d + 0.5);
	}
	return (int)(d - 0.5);
}

static double GetFmModScaleForAlgo(int nAlgo) {
	static int s_checked = 0;
	static double s_scale = 1.0;
	static double s_algoScale[8] = {
		1.0, 1.0, 0.95, 1.0, 1.0, 1.0, 1.0, 1.0
	};
	if (!s_checked) {
		s_checked = 1;
		s_scale = ReadEnvDouble("X88_FM_MOD_SCALE", 1.0, 0.05, 8.0);
		char szName[32];
		for (int n = 0; n < 8; n++) {
			snprintf(szName, sizeof(szName), "X88_FM_MOD_SCALE_ALGO%d", n);
			s_algoScale[n] = ReadEnvDouble(szName, s_algoScale[n],
				0.05, 8.0);
		}
	}
	if ((nAlgo < 0) || (nAlgo >= 8)) {
		return s_scale;
	}
	return s_scale * s_algoScale[nAlgo];
}

static int ScaleFmModInput(int nModulation, int nAlgo) {
	return ScaleSignedInt(nModulation, GetFmModScaleForAlgo(nAlgo));
}

static int ScaleFmFeedbackInput(int nFeedback) {
	static int s_checked = 0;
	static double s_scale = 1.0;
	if (!s_checked) {
		s_checked = 1;
		s_scale = ReadEnvDouble("X88_FM_FB_SCALE", 1.0, 0.05, 8.0);
	}
	return ScaleSignedInt(nFeedback, s_scale);
}

static bool ReadEnvBool(const char* pszName, bool bDefault) {
	const char* psz = getenv(pszName);
	if ((psz == NULL) || (*psz == '\0')) {
		return bDefault;
	}
	return (*psz != '0') && (*psz != 'n') && (*psz != 'N') &&
		(*psz != 'f') && (*psz != 'F');
}

static bool ShouldResetFmPhaseOnKeyOn() {
	static int s_checked = 0;
	static bool s_enabled = true;
	if (!s_checked) {
		s_checked = 1;
		s_enabled = ReadEnvBool("X88_FM_PHASE_RESET_ON_KEY", true);
	}
	return s_enabled;
}

static bool ShouldRetriggerFmOnKeyWrite() {
	static int s_checked = 0;
	static bool s_enabled = false;
	if (!s_checked) {
		s_checked = 1;
		s_enabled = ReadEnvBool("X88_FM_RETRIGGER_ON_KEY_WRITE", false);
	}
	return s_enabled;
}

static int ScaleFmEnvRateInc(int nInc, const char* pszEnvName,
	double dDefault)
{
	double dScale = ReadEnvDouble(pszEnvName, dDefault, 0.000001, 16.0);
	double d = (double)nInc * dScale;
	if (d < 1.0) return 1;
	return (int)(d + 0.5);
}

static double GetFmPercussiveDecayScale(int nAlgo, int nFb)
{
	if ((nAlgo == 4) && (nFb >= 7)) {
		return ReadEnvDouble("X88_FM_PERC_DECAY_RATE_SCALE", 1.5,
			0.000001, 16.0);
	}
	return 1.0;
}

static int ScaleAudioSample(int nSample, double dScale) {
	double d = (double)nSample * dScale;
	if (d >  2147483000.0) return  2147483000;
	if (d < -2147483000.0) return -2147483000;
	if (d >= 0.0) return (int)(d + 0.5);
	return (int)(d - 0.5);
}

static double GetSsgMixScale() {
	static int s_checked = 0;
	static double s_scale = 1.0;
	if (!s_checked) {
		s_checked = 1;
		s_scale = ReadEnvDouble("X88_SSG_MIX_SCALE", 1.0, 0.0, 4.0);
	}
	return s_scale;
}

static double GetFmMixScale() {
	static int s_checked = 0;
	static double s_scale = 1.6;
	if (!s_checked) {
		s_checked = 1;
		s_scale = ReadEnvDouble("X88_FM_MIX_SCALE", 1.6, 0.0, 4.0);
	}
	return s_scale;
}

static double GetAdpcmMixScale() {
	static int s_checked = 0;
	static double s_scale = 1.4;
	if (!s_checked) {
		s_checked = 1;
		s_scale = ReadEnvDouble("X88_ADPCM_MIX_SCALE", 1.4, 0.0, 4.0);
	}
	return s_scale;
}

static double GetRhythmMixScale() {
	static int s_checked = 0;
	static double s_scale = 1.4;
	if (!s_checked) {
		s_checked = 1;
		s_scale = ReadEnvDouble("X88_RHYTHM_MIX_SCALE", 1.4, 0.0, 4.0);
	}
	return s_scale;
}

static bool GetAdpcmInterruptEnabled() {
	static int s_checked = 0;
	static bool s_enabled = false;
	if (!s_checked) {
		s_checked = 1;
		const char* p = getenv("X88_ADPCM_IRQ");
		s_enabled = (p != NULL) && (*p != '\0') && (*p != '0');
	}
	return s_enabled;
}

static int GetAdpcmDeclickSamples() {
	static int s_checked = 0;
	static int s_samples = 32;
	if (!s_checked) {
		s_checked = 1;
		s_samples = (int)ReadEnvDouble("X88_ADPCM_DECLICK_SAMPLES", 32.0, 0.0, 512.0);
	}
	return s_samples;
}

static double RhythmTotalLevelToScale(int nLevel) {
	if (nLevel <= 0) return 0.0;
	if (nLevel >= 63) return 1.0;
	return pow(10.0, ((double)nLevel - 63.0) * 0.75 / 20.0);
}

static double RhythmInstrumentLevelToScale(int nLevel) {
	if (nLevel <= 0) return 0.0;
	if (nLevel >= 31) return 1.0;
	return pow(10.0, ((double)nLevel - 31.0) * 0.75 / 20.0);
}

////////////////////////////////////////////////////////////
// create & destroy

// default constructor

CPC88Opna::CPC88Opna() {
	m_pIntVectChangeCallback = NULL;
	m_pSampleOutputCallback = NULL;
	m_pSystemFileOpenCallback = NULL;
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
	for (int n = 0; n < REGISTER_PAGE_COUNT; n++) {
		m_anAddress[n] = 0;
	}
	m_nSoundBoard2Address = 0;
	m_nSoundBoard2AddressUpper = 0;
	m_bSoundBoard2InterruptMask = false;
	m_nWritePage = 0;
	m_nWriteChannelBase = INTERNAL_FM_CHANNEL_BASE;
	m_nCh3Mode = 0;
	m_nPreScalerFM = 6;
	m_nPreScalerPSG = 4;
	ResetTimerState(m_timerInternalOpn);
	ResetTimerState(m_timerExpansionOpna);
	LoadTimerState(m_timerInternalOpn);
	m_bOpnaInterruptRequest = false;
	// Phase C synthesis state.
	for (int n = 0; n < REGISTER_COUNT; n++) {
		m_abtRegisters[n] = 0;
		m_abtSoundBoard2Registers[n] = 0;
	}
	for (int n = 0; n < 0x11; n++) {
		m_abtAdpcmRegisters[n] = 0;
	}
	// SSG mixer reset state per YM2149 datasheet: $07 = $FF
	// (= every channel's tone and noise both disabled, I/O ports as
	// outputs). Match m_btSsgMixer below so the mirror and the
	// dedicated runtime variable agree from the very first sample.
	m_abtRegisters[0x07] = 0xFF;
	m_nSampleAccumX256 = 0;
	m_nRenderedFrames = 0;
	// Recompute cycles-per-sample because m_nBaseClock may have changed.
	if (m_nSampleRate > 0) {
		SetSampleRate(m_nSampleRate);
	}

	ResetSsgState(m_ssgInternalOpn);
	ResetSsgState(m_ssgExpansionOpna);
	LoadSsgState(m_ssgInternalOpn);
	UpdateSsgTickRate();

	// FM state reset (Phase C-FM).
	ResetFmState();
	UpdateFmTickRate();
	ApplyFmClockToChannels(INTERNAL_FM_CHANNEL_BASE, OPN_FM_CHANNEL_COUNT);
	ApplyFmClockToChannels(EXPANSION_FM_CHANNEL_BASE, OPNA_FM_CHANNEL_COUNT);
	LoadRhythmSamples();
	for (int n = 0; n < RHYTHM_CHANNEL_COUNT; n++) {
		m_aRhythmVoice[n].bActive = false;
		m_aRhythmVoice[n].nPosX16 = 0;
		m_aRhythmVoice[n].nStepX16 = 1 << 16;
		m_aRhythmVoice[n].btLevel = 0x1F;
		m_aRhythmVoice[n].btPan = 0xC0;
	}
	m_btRhythmTotalLevel = 0x3F;
	ResetAdpcmState();
}

void CPC88Opna::LoadTimerState(const STimerState& state) {
	m_btStatus = state.btStatus;
	m_bTimerAAcvive = state.bTimerAActive;
	m_bTimerASetFlag = state.bTimerASetFlag;
	m_nTimerAValue = state.nTimerAValue;
	m_nTimerACounter = state.nTimerACounter;
	m_nTimerACounterMax = state.nTimerACounterMax;
	m_bTimerBAcvive = state.bTimerBActive;
	m_bTimerBSetFlag = state.bTimerBSetFlag;
	m_nTimerBValue = state.nTimerBValue;
	m_nTimerBCounter = state.nTimerBCounter;
	m_nTimerBCounterMax = state.nTimerBCounterMax;
	m_nPreScalerFM = state.nPreScalerFM;
	m_nPreScalerPSG = state.nPreScalerPSG;
	m_nCh3Mode = state.nCh3Mode;
	m_bFmCh3SpecialMode = state.bFmCh3SpecialMode;
	m_bCsmKeyState = state.bCsmKeyState;
}

void CPC88Opna::SaveTimerState(STimerState& state) {
	state.btStatus = m_btStatus;
	state.bTimerAActive = m_bTimerAAcvive;
	state.bTimerASetFlag = m_bTimerASetFlag;
	state.nTimerAValue = m_nTimerAValue;
	state.nTimerACounter = m_nTimerACounter;
	state.nTimerACounterMax = m_nTimerACounterMax;
	state.bTimerBActive = m_bTimerBAcvive;
	state.bTimerBSetFlag = m_bTimerBSetFlag;
	state.nTimerBValue = m_nTimerBValue;
	state.nTimerBCounter = m_nTimerBCounter;
	state.nTimerBCounterMax = m_nTimerBCounterMax;
	state.nPreScalerFM = m_nPreScalerFM;
	state.nPreScalerPSG = m_nPreScalerPSG;
	state.nCh3Mode = m_nCh3Mode;
	state.bFmCh3SpecialMode = m_bFmCh3SpecialMode;
	state.bCsmKeyState = m_bCsmKeyState;
}

void CPC88Opna::ResetTimerState(STimerState& state) {
	state.btStatus = 0;
	state.bTimerAActive = false;
	state.bTimerASetFlag = false;
	state.nTimerAValue = 0;
	state.nTimerACounterMax = 12 * 1024 * m_nPreScalerFM * (m_nBaseClock / 4);
	state.nTimerACounter = state.nTimerACounterMax;
	state.bTimerBActive = false;
	state.bTimerBSetFlag = false;
	state.nTimerBValue = 0;
	state.nTimerBCounterMax = 192 * 256 * m_nPreScalerFM * (m_nBaseClock / 4);
	state.nTimerBCounter = state.nTimerBCounterMax;
	state.nPreScalerFM = m_nPreScalerFM;
	state.nPreScalerPSG = m_nPreScalerPSG;
	state.nCh3Mode = 0;
	state.bFmCh3SpecialMode = false;
	state.bCsmKeyState = false;
}

void CPC88Opna::AdvanceTimerState(STimerState& state, int nClock,
	bool bExpansion)
{
	LoadTimerState(state);
	if (m_bTimerAAcvive) {
		if ((m_nTimerACounter -= nClock) <= 0) {
			TimerAOverFlow(bExpansion);
		}
	}
	if (m_bTimerBAcvive) {
		if ((m_nTimerBCounter -= nClock) <= 0) {
			TimerBOverFlow();
		}
	}
	SaveTimerState(state);
}

void CPC88Opna::WriteTimerDeviceRegister(bool bExpansion, int nAddress,
	uint8_t btData)
{
	STimerState& state = bExpansion? m_timerExpansionOpna: m_timerInternalOpn;
	LoadTimerState(state);
	switch (nAddress) {
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
		{
			int nNewMode = (btData >> 6) & 0x03;
			if (nNewMode != 1) {
				m_bCsmKeyState = false;
			}
			m_nCh3Mode = nNewMode;
			int nSpecialCh = (bExpansion? EXPANSION_FM_CHANNEL_BASE:
				INTERNAL_FM_CHANNEL_BASE) + 2;
			if ((nSpecialCh >= 0) && (nSpecialCh < FM_CHANNEL_COUNT)) {
				m_aFmCh[nSpecialCh].nSpecialMode = nNewMode;
				RecomputeFmChannelPhaseIncs(nSpecialCh);
			}
			if (bExpansion) {
				int nUpperSpecialCh = EXPANSION_FM_CHANNEL_BASE + 5;
				if ((nUpperSpecialCh >= 0) &&
					(nUpperSpecialCh < FM_CHANNEL_COUNT))
				{
					m_aFmCh[nUpperSpecialCh].nSpecialMode = nNewMode;
					RecomputeFmChannelPhaseIncs(nUpperSpecialCh);
				}
			}
		}
		m_bFmCh3SpecialMode = (m_nCh3Mode != 0);
		break;
	}
	SaveTimerState(state);
	RefreshInterruptRequest();
}

void CPC88Opna::LoadSsgState(const SSsgState& state) {
	m_nSsgTickAccumX16 = state.nTickAccumX16;
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		m_anSsgTonePeriod[n] = state.anTonePeriod[n];
		m_anSsgToneCounter[n] = state.anToneCounter[n];
		m_anSsgToneState[n] = state.anToneState[n];
		m_anSsgVolume[n] = state.anVolume[n];
		m_abSsgUseEnv[n] = state.abUseEnv[n];
	}
	m_nSsgNoisePeriod = state.nNoisePeriod;
	m_nSsgNoiseCounter = state.nNoiseCounter;
	m_nSsgNoiseLfsr = state.nNoiseLfsr;
	m_nSsgNoiseState = state.nNoiseState;
	m_nSsgEnvPeriod = state.nEnvPeriod;
	m_nSsgEnvCounter = state.nEnvCounter;
	m_nSsgEnvLevel = state.nEnvLevel;
	m_nSsgEnvDir = state.nEnvDir;
	m_bSsgEnvHolding = state.bEnvHolding;
	m_btSsgMixer = state.btMixer;
}

void CPC88Opna::SaveSsgState(SSsgState& state) {
	state.nTickAccumX16 = m_nSsgTickAccumX16;
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		state.anTonePeriod[n] = m_anSsgTonePeriod[n];
		state.anToneCounter[n] = m_anSsgToneCounter[n];
		state.anToneState[n] = m_anSsgToneState[n];
		state.anVolume[n] = m_anSsgVolume[n];
		state.abUseEnv[n] = m_abSsgUseEnv[n];
	}
	state.nNoisePeriod = m_nSsgNoisePeriod;
	state.nNoiseCounter = m_nSsgNoiseCounter;
	state.nNoiseLfsr = m_nSsgNoiseLfsr;
	state.nNoiseState = m_nSsgNoiseState;
	state.nEnvPeriod = m_nSsgEnvPeriod;
	state.nEnvCounter = m_nSsgEnvCounter;
	state.nEnvLevel = m_nSsgEnvLevel;
	state.nEnvDir = m_nSsgEnvDir;
	state.bEnvHolding = m_bSsgEnvHolding;
	state.btMixer = m_btSsgMixer;
}

void CPC88Opna::ResetSsgState(SSsgState& state) {
	state.nTickAccumX16 = 0;
	for (int n = 0; n < SSG_CHANNEL_COUNT; n++) {
		state.anTonePeriod[n] = 1;
		state.anToneCounter[n] = 1;
		state.anToneState[n] = 0;
		state.anVolume[n] = 0;
		state.abUseEnv[n] = false;
	}
	state.nNoisePeriod = 1;
	state.nNoiseCounter = 1;
	state.nNoiseLfsr = 0x1FFFF;
	state.nNoiseState = 1;
	state.nEnvPeriod = 1;
	state.nEnvCounter = 1;
	state.nEnvLevel = 0;
	state.nEnvDir = -1;
	state.bEnvHolding = true;
	state.btMixer = 0xFF;
}

void CPC88Opna::CopyLowerRegisters(uint8_t* pDst, const uint8_t* pSrc) {
	if ((pDst == NULL) || (pSrc == NULL)) {
		return;
	}
	for (int n = 0; n < 0x10; n++) {
		pDst[n] = pSrc[n];
	}
}

void CPC88Opna::WriteSsgDeviceRegister(bool bExpansion, int nAddress,
	uint8_t btData)
{
	uint8_t abtSaved[0x10];
	CopyLowerRegisters(abtSaved, m_abtRegisters);
	uint8_t* pRegs = (bExpansion && m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA)?
		m_abtSoundBoard2Registers: m_abtRegisters;
	CopyLowerRegisters(m_abtRegisters, pRegs);
	m_abtRegisters[nAddress & 0x0F] = btData;
	LoadTimerState(bExpansion? m_timerExpansionOpna: m_timerInternalOpn);
	UpdateSsgTickRate();
	LoadSsgState(bExpansion? m_ssgExpansionOpna: m_ssgInternalOpn);
	OnSsgRegisterWrite(nAddress, btData);
	SaveSsgState(bExpansion? m_ssgExpansionOpna: m_ssgInternalOpn);
	CopyLowerRegisters(pRegs, m_abtRegisters);
	CopyLowerRegisters(m_abtRegisters, abtSaved);
}

int CPC88Opna::RenderSsgDevice(bool bExpansion) {
	uint8_t abtSaved[0x10];
	CopyLowerRegisters(abtSaved, m_abtRegisters);
	uint8_t* pRegs = (bExpansion && m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA)?
		m_abtSoundBoard2Registers: m_abtRegisters;
	CopyLowerRegisters(m_abtRegisters, pRegs);
	LoadTimerState(bExpansion? m_timerExpansionOpna: m_timerInternalOpn);
	UpdateSsgTickRate();
	LoadSsgState(bExpansion? m_ssgExpansionOpna: m_ssgInternalOpn);
	m_nSsgTickAccumX16 += m_nSsgTicksPerSampleX16;
	while (m_nSsgTickAccumX16 >= (1 << 16)) {
		m_nSsgTickAccumX16 -= (1 << 16);
		AdvanceSsgOneTick();
	}
	int nSample = RenderSsgSample();
	SaveSsgState(bExpansion? m_ssgExpansionOpna: m_ssgInternalOpn);
	CopyLowerRegisters(pRegs, m_abtRegisters);
	CopyLowerRegisters(m_abtRegisters, abtSaved);
	return nSample;
}

////////////////////////////////////////////////////////////
// operation

// timer A overflow

void CPC88Opna::TimerAOverFlow(bool bExpansion) {
	if (m_bTimerASetFlag) {
		if ((m_btStatus & 0x01) == 0) {
			UpdateInterruptRequest(true);
		}
		m_btStatus |= 0x01;
	}
	// CH3 mode 1 = CSM: every Timer A overflow forces a
	// key-on retrigger on all four operators of CH3 regardless of
	// their current $28 key state. This is what drives speech-style
	// formant synthesis (Sorcerian opening, Xanadu II demo, etc.).
	if (m_nCh3Mode == 1) {
		OnCsmKeyTrigger(bExpansion? EXPANSION_FM_CHANNEL_BASE:
			INTERNAL_FM_CHANNEL_BASE);
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
}

// read data

uint8_t CPC88Opna::ReadData() {
	if (m_nSoundBoardMode == SOUNDBOARD_NONE) {
		return 0xFF;
	}
	if ((m_nSoundBoardMode == SOUNDBOARD_OPNA) &&
		((m_nAddress & 0xFF) == 0xFF))
	{
		// YM2608 lower-port register $FF is the device ID readback.
		return 0x01;
	}
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
		// I/O ports $0E/$0F on PC-88 are wired to the joystick input
		// pins. $07 bits 6/7 select "input" or "output" mode for these
		// ports inside the YM2203, but on real PC-88 hardware the pins
		// are externally driven by the joystick connector regardless.
		// Some sound drivers leave bits 6/7 set to "output" as a side
		// effect of writing the mixer (their RMW preserves whatever was
		// in $07 at boot, which can be 1). If we honoured that and
		// returned m_abtRegisters[$0E/$0F] (a mostly-zero internal
		// value), the game would see "all directions + all buttons
		// pressed" — Hydlide 3 then auto-skips its opening, Xak 2 self-
		// navigates the title menu, etc. So always report idle ($FF)
		// here, matching what an unconnected joystick would read.
		return 0xFF;
	}
	return 0xFF;
}

uint8_t CPC88Opna::ReadStatusUpper() {
	if (m_nSoundBoardMode != SOUNDBOARD_OPNA) {
		return 0xFF;
	}
	return ReadAdpcmStatus();
}

uint8_t CPC88Opna::ReadDataUpper() {
	if (m_nSoundBoardMode != SOUNDBOARD_OPNA) {
		return 0xFF;
	}
	int nAddress = m_anAddress[1] & 0xFF;
	if ((nAddress >= 0x00) && (nAddress <= 0x10)) {
		return ReadAdpcmData();
	}
	return m_abtRegisters[0x100 + nAddress];
}

// write address

void CPC88Opna::WriteAddress(uint8_t btAddress) {
	if (m_nSoundBoardMode == SOUNDBOARD_NONE) {
		return;
	}
	m_nAddress = btAddress;
	m_anAddress[0] = btAddress;
	bool bExpansion = (m_nSoundBoardMode == SOUNDBOARD_OPNA);
	bool bPrescalerChanged = false;
	LoadTimerState(bExpansion? m_timerExpansionOpna: m_timerInternalOpn);
	switch (m_nAddress) {
	case 0x2D:
		LogRegisterEvent('A', m_nAddress, 0);
		SetPreScaler(6, 4);
		bPrescalerChanged = true;
		break;
	case 0x2E:
		LogRegisterEvent('A', m_nAddress, 0);
		SetPreScaler(3, 2);
		bPrescalerChanged = true;
		break;
	case 0x2F:
		LogRegisterEvent('A', m_nAddress, 0);
		SetPreScaler(2, 1);
		bPrescalerChanged = true;
		break;
	}
	if (bPrescalerChanged) {
		ApplyFmClockToChannels(bExpansion? EXPANSION_FM_CHANNEL_BASE:
			INTERNAL_FM_CHANNEL_BASE,
			bExpansion? OPNA_FM_CHANNEL_COUNT: OPN_FM_CHANNEL_COUNT);
	}
	SaveTimerState(bExpansion? m_timerExpansionOpna: m_timerInternalOpn);
}

void CPC88Opna::WriteAddressUpper(uint8_t btAddress) {
	if (m_nSoundBoardMode != SOUNDBOARD_OPNA) {
		return;
	}
	m_anAddress[1] = btAddress;
}

// write data

void CPC88Opna::WriteData(uint8_t btData) {
	if (m_nSoundBoardMode == SOUNDBOARD_NONE) {
		return;
	}
	g_nOpnTotalWrites++;
	LogRegisterEvent('D', m_nAddress, btData);
	WriteLowerDataForTargets(btData,
		(m_nSoundBoardMode == SOUNDBOARD_OPN ||
		 m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA),
		(m_nSoundBoardMode == SOUNDBOARD_OPNA));
}

void CPC88Opna::WriteLowerDataForTargets(uint8_t btData,
	bool bInternalOpn, bool bExpansionOpna)
{
	if ((bInternalOpn || (bExpansionOpna && m_nSoundBoardMode == SOUNDBOARD_OPNA)) &&
		(m_nAddress >= 0) && (m_nAddress < 0x100))
	{
		m_abtRegisters[m_nAddress] = btData;
	}
	if (bExpansionOpna && (m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA) &&
		(m_nAddress >= 0) && (m_nAddress < 0x100))
	{
		m_abtSoundBoard2Registers[m_nAddress] = btData;
	}
	// Phase C-SSG: SSG / PSG section is at $00–$0F.
	if ((m_nAddress >= 0x00) && (m_nAddress <= 0x0F)) {
		if (bInternalOpn) {
			WriteSsgDeviceRegister(false, m_nAddress, btData);
		}
		if (bExpansionOpna) {
			WriteSsgDeviceRegister(true, m_nAddress, btData);
		}
	}
		// Phase C-FM: $28 (key on/off), $30–$B2 (FM operator/channel
		// parameters), and OPNA $B4–$B6 pan bits. LFO/AMS/PMS values are
		// stored but not yet applied to synthesis.
	if (bInternalOpn) {
		OnFmRegisterWriteAt(0, INTERNAL_FM_CHANNEL_BASE, m_nAddress, btData);
	}
	if (bExpansionOpna) {
		OnFmRegisterWriteAt(0, EXPANSION_FM_CHANNEL_BASE, m_nAddress, btData);
		OnRhythmRegisterWrite(m_nAddress, btData);
	}
	if ((m_nAddress >= 0x24) && (m_nAddress <= 0x27)) {
		if (bInternalOpn) {
			WriteTimerDeviceRegister(false, m_nAddress, btData);
		}
		if (bExpansionOpna) {
			WriteTimerDeviceRegister(true, m_nAddress, btData);
		}
	}
}

void CPC88Opna::WriteDataUpper(uint8_t btData) {
	if (m_nSoundBoardMode != SOUNDBOARD_OPNA) {
		return;
	}
	int nAddress = m_anAddress[1] & 0xFF;
	g_nOpnTotalWrites++;
	LogRegisterEvent('E', nAddress, btData);
	m_abtRegisters[0x100 + nAddress] = btData;
	if ((nAddress >= 0x00) && (nAddress <= 0x10)) {
		WriteAdpcmRegister(nAddress, btData);
	} else {
		OnFmRegisterWriteAt(1, EXPANSION_FM_CHANNEL_BASE + 3, nAddress, btData);
	}
}

uint8_t CPC88Opna::ReadStatusSoundBoard2() {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return 0xFF;
	}
	return m_timerExpansionOpna.btStatus;
}

uint8_t CPC88Opna::ReadDataSoundBoard2() {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return 0xFF;
	}
	int nAddress = m_nSoundBoard2Address & 0xFF;
	if ((nAddress >= 0x00) && (nAddress <= 0x0D)) {
		return m_abtSoundBoard2Registers[nAddress];
	}
	if (nAddress == 0x0E || nAddress == 0x0F) {
		return 0xFF;
	}
	if (nAddress == 0xFF) {
		// Sound Board II detection probes OPNA ID via A8=$FF, read A9.
		return 0x01;
	}
	return 0x00;
}

void CPC88Opna::WriteAddressSoundBoard2(uint8_t btAddress) {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return;
	}
	m_nSoundBoard2Address = btAddress;
	LoadTimerState(m_timerExpansionOpna);
	bool bPrescalerChanged = false;
	switch (btAddress) {
	case 0x2D:
		LogRegisterEvent('A', btAddress, 0);
		SetPreScaler(6, 4);
		bPrescalerChanged = true;
		break;
	case 0x2E:
		LogRegisterEvent('A', btAddress, 0);
		SetPreScaler(3, 2);
		bPrescalerChanged = true;
		break;
	case 0x2F:
		LogRegisterEvent('A', btAddress, 0);
		SetPreScaler(2, 1);
		bPrescalerChanged = true;
		break;
	}
	if (bPrescalerChanged) {
		ApplyFmClockToChannels(EXPANSION_FM_CHANNEL_BASE, OPNA_FM_CHANNEL_COUNT);
	}
	SaveTimerState(m_timerExpansionOpna);
}

void CPC88Opna::WriteDataSoundBoard2(uint8_t btData) {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return;
	}
	int nAddressPrev = m_nAddress;
	m_nAddress = m_nSoundBoard2Address & 0xFF;
	g_nOpnTotalWrites++;
	LogRegisterEvent('S', m_nAddress, btData);
	m_abtSoundBoard2Registers[m_nAddress] = btData;
	WriteLowerDataForTargets(btData, false, true);
	m_nAddress = nAddressPrev;
}

uint8_t CPC88Opna::ReadStatusSoundBoard2Upper() {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return 0xFF;
	}
	return ReadAdpcmStatus();
}

uint8_t CPC88Opna::ReadDataSoundBoard2Upper() {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return 0xFF;
	}
	int nAddress = m_nSoundBoard2AddressUpper & 0xFF;
	if ((nAddress >= 0x00) && (nAddress <= 0x10)) {
		return ReadAdpcmData();
	}
	return m_abtSoundBoard2Registers[0x100 + nAddress];
}

void CPC88Opna::WriteAddressSoundBoard2Upper(uint8_t btAddress) {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return;
	}
	m_nSoundBoard2AddressUpper = btAddress;
}

void CPC88Opna::WriteDataSoundBoard2Upper(uint8_t btData) {
	if (m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA) {
		return;
	}
	int nAddress = m_nSoundBoard2AddressUpper & 0xFF;
	g_nOpnTotalWrites++;
	LogRegisterEvent('T', nAddress, btData);
	m_abtSoundBoard2Registers[0x100 + nAddress] = btData;
	if ((nAddress >= 0x00) && (nAddress <= 0x10)) {
		WriteAdpcmRegister(nAddress, btData);
	} else {
		OnFmRegisterWriteAt(1, EXPANSION_FM_CHANNEL_BASE + 3, nAddress, btData);
	}
}

void CPC88Opna::WriteSoundBoard2InterruptMask(uint8_t btData) {
	// AAh bit7 is S2INTM: 0 = Sound Board II interrupt enabled,
	// 1 = interrupt masked. Lower bits are not the mask bit.
	m_bSoundBoard2InterruptMask = ((btData & 0x80) != 0);
	if (m_bSoundBoard2InterruptMask &&
		(m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA))
	{
		UpdateInterruptRequest(false);
	} else if ((m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA) &&
		((m_timerExpansionOpna.btStatus & 0x03) != 0))
	{
		UpdateInterruptRequest(true);
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
	LoadTimerState(m_timerInternalOpn);
	UpdateFmTickRate();
	ApplyFmClockToChannels(INTERNAL_FM_CHANNEL_BASE, OPN_FM_CHANNEL_COUNT);
	LoadTimerState(m_timerExpansionOpna);
	UpdateFmTickRate();
	ApplyFmClockToChannels(EXPANSION_FM_CHANNEL_BASE, OPNA_FM_CHANNEL_COUNT);
	LoadTimerState((m_nSoundBoardMode == SOUNDBOARD_OPNA)?
		m_timerExpansionOpna: m_timerInternalOpn);
}

void CPC88Opna::SetSoundBoardMode(int nMode) {
	if ((nMode < SOUNDBOARD_NONE) || (nMode > SOUNDBOARD_OPN_OPNA)) {
		nMode = SOUNDBOARD_OPN;
	}
	m_nSoundBoardMode = nMode;
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
			// Always run synthesis so timers / envelopes / phase
			// accumulators stay consistent; only suppress the section's
			// contribution to the output sample if muted.
			int nSsgSample = 0;
			if (m_nSoundBoardMode == SOUNDBOARD_OPN ||
				m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA)
			{
				nSsgSample += RenderSsgDevice(false);
			}
			if (m_nSoundBoardMode == SOUNDBOARD_OPNA ||
				m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA)
			{
				nSsgSample += RenderSsgDevice(true);
			}
			if (m_nSoundBoardMode == SOUNDBOARD_OPNA) {
				LoadTimerState(m_timerExpansionOpna);
			} else {
				LoadTimerState(m_timerInternalOpn);
			}
			int nFmLeft = 0;
			int nFmRight = 0;
			RenderFmStereoSample(nFmLeft, nFmRight);
			int nRhythmLeft = 0;
			int nRhythmRight = 0;
			RenderRhythmStereoSample(nRhythmLeft, nRhythmRight);
			int nAdpcmLeft = 0;
			int nAdpcmRight = 0;
			RenderAdpcmStereoSample(nAdpcmLeft, nAdpcmRight);
			int nLeft = 0;
			int nRight = 0;
			// Mix SSG and FM with relative balance. Earlier revisions
			// halved SSG here as a workaround for thin/short FM output.
			// After the 2026-05 FM envelope/detune fixes, that makes SSG
			// sit too far back, but FM was still too quiet relative to
			// Bubilator/fmgen references. Keep SSG at unity and lift FM by
			// default; environment knobs remain for quick A/B:
			//   X88_SSG_MIX_SCALE=0.5 restores the old SSG half-level.
			//   X88_FM_MIX_SCALE=1.0 restores the previous FM level.
			if (!m_bSsgMute) {
				int n = ScaleAudioSample(nSsgSample, GetSsgMixScale());
				nLeft += n;
				nRight += n;
			}
			if (!m_bFmMute) {
				nLeft += ScaleAudioSample(nFmLeft, GetFmMixScale());
				nRight += ScaleAudioSample(nFmRight, GetFmMixScale());
			}
			if (!m_bRhythmMute) {
				nLeft += nRhythmLeft;
				nRight += nRhythmRight;
			}
			nLeft += nAdpcmLeft;
			nRight += nAdpcmRight;
			if (nLeft >  32767) nLeft =  32767;
			if (nLeft < -32768) nLeft = -32768;
			if (nRight >  32767) nRight =  32767;
			if (nRight < -32768) nRight = -32768;
			aBuf[n*2 + 0] = (int16_t)nLeft;
			aBuf[n*2 + 1] = (int16_t)nRight;
		}
		m_pSampleOutputCallback(aBuf, nThis);
		m_nRenderedFrames += nThis;
		nFrames -= nThis;
	}
}

// optional YM2203 register event logger

void CPC88Opna::LogRegisterEvent(char chEvent, int nAddress, int nData) {
	static int s_checked = 0;
	static FILE* s_file = NULL;
	if (!s_checked) {
		s_checked = 1;
		const char* pszPath = getenv("X88_OPN_LOG");
		if ((pszPath != NULL) && (*pszPath != '\0')) {
			s_file = fopen(pszPath, "wb");
			if (s_file != NULL) {
				fprintf(s_file,
					"# X88000M YM2203 register log\n"
					"# columns: frame,event,address,data\n");
			}
		}
	}
	if (s_file == NULL) {
		return;
	}
	fprintf(s_file, "%lld,%c,%02X,%02X\n",
		m_nRenderedFrames,
		chEvent,
		nAddress & 0xFF,
		nData & 0xFF);
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
	// YM2203 chip clock is a dedicated ~4 MHz crystal, independent of
	// the Z80 CPU clock (which can be 4 or 8 MHz on PC-8801).
	const long long nChipHz = 4000000LL;
	long long nSsgClockHz =
		nChipHz / m_nPreScalerPSG / 4LL;
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
	double dEnvRateScale = ReadEnvDouble("X88_FM_ENV_RATE_SCALE", 1.0, 0.000001, 16.0);
	double adEnvRateScaleByRate[FM_ENV_RATE_TABLE_SIZE];
	for (int nRate = 0; nRate < FM_ENV_RATE_TABLE_SIZE; nRate++) {
		// 2026-05 Scheme CH3 comparison:
		// The previous global shift=13 approximation still made FM
		// percussion decay too quickly. Black-box comparison against
		// Scheme CH3 and YS Opening CH1 references showed the best broad
		// fit with lower/mid effective rates at 0.20x and high rates at
		// 0.12x. Environment overrides below remain available for further
		// A/B testing; X88_FM_ENV_RATE_BANDS=0-63:1 restores the prior
		// post-v1.0.2 shift=13 behavior.
		adEnvRateScaleByRate[nRate] = (nRate <= 35)? 0.20: 0.12;
	}
	{
		const char* pszBands = getenv("X88_FM_ENV_RATE_BANDS");
		if ((pszBands != NULL) && (*pszBands != '\0')) {
			char szBands[512];
			strncpy(szBands, pszBands, sizeof(szBands) - 1);
			szBands[sizeof(szBands) - 1] = '\0';
			char* pszTok = strtok(szBands, ",");
			while (pszTok != NULL) {
				int nFirst = -1;
				int nLast = -1;
				double dScale = 1.0;
				if (sscanf(pszTok, "%d-%d:%lf", &nFirst, &nLast, &dScale) == 3 ||
					sscanf(pszTok, "%d:%lf", &nFirst, &dScale) == 2)
				{
					if (nLast < 0) nLast = nFirst;
					if (nFirst < 0) nFirst = 0;
					if (nLast >= FM_ENV_RATE_TABLE_SIZE) nLast = FM_ENV_RATE_TABLE_SIZE - 1;
					if ((nFirst <= nLast) && (dScale > 0.0) && (dScale < 16.0)) {
						for (int nRate = nFirst; nRate <= nLast; nRate++) {
							adEnvRateScaleByRate[nRate] = dScale;
						}
					}
				}
				pszTok = strtok(NULL, ",");
			}
		}
	}
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
			m_anFmEnvRateTable[nRate] =
				(int)((double)((nAdd << 16) >> nShift) *
					dEnvRateScale * adEnvRateScaleByRate[nRate] + 0.5);
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
			ch.btPan   = 0xC0;
			ch.anFeedback[0] = 0;
			ch.anFeedback[1] = 0;
			ch.nFmPhaseScaleX16 = 0;
			ch.nSpecialMode = 0;
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
	m_bCsmKeyState = false;
	for (int n = 0; n < FM_CHANNEL_COUNT; n++) m_abtFmFnumLatch[n] = 0;
	for (int nCh = 0; nCh < FM_CHANNEL_COUNT; nCh++) {
		for (int n = 0; n < 3; n++) m_abtFmCh3FnumLatch[nCh][n] = 0;
	}
}

// recompute FM ticks-per-output-sample (informational; the per-sample
// path uses operator phase increments directly)

void CPC88Opna::UpdateFmTickRate() {
	if ((m_nSampleRate <= 0) || (m_nPreScalerFM <= 0) || (m_nBaseClock <= 0)) {
		m_nFmTicksPerSampleX16 = 0;
		m_nFmPhaseScaleX16     = 0;
		return;
	}
	// FM section clock = chip / prescaler_fm / 6. The YM2203 chip clock
	// is a dedicated ~4 MHz crystal on PC-8801, independent of Z80 clock.
	const long long nChipHz = 4000000LL;
	long long nFmClockHz =
		nChipHz / m_nPreScalerFM / 6LL;
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
	long long nMaster = nChipHz;
	m_nFmPhaseScaleX16 = ComputeFmPhaseScaleX16();

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
	double dDtScale = ReadEnvDouble("X88_FM_DT_SCALE", 1.0, 0.0, 16.0);
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
				m_anFmDetunePhaseInc[b][n][f] =
					(int)((double)(nNum / nDen) * dDtScale + 0.5);
			}
		}
	}
}

int CPC88Opna::ComputeFmPhaseScaleX16() {
	if ((m_nSampleRate <= 0) || (m_nPreScalerFM <= 0)) {
		return 0;
	}
	const long long nChipHz = 4000000LL;
	long long nDiv = (long long)m_nPreScalerFM * 24LL * (long long)m_nSampleRate;
	if (nDiv <= 0) {
		return 0;
	}
	return (int)((nChipHz * (1LL << 16)) / nDiv);
}

void CPC88Opna::ApplyFmClockToChannels(int nChannelBase, int nChannelCount) {
	int nScale = ComputeFmPhaseScaleX16();
	for (int n = 0; n < nChannelCount; n++) {
		int nCh = nChannelBase + n;
		if ((nCh < 0) || (nCh >= FM_CHANNEL_COUNT)) {
			continue;
		}
		m_aFmCh[nCh].nFmPhaseScaleX16 = nScale;
		RecomputeFmChannelPhaseIncs(nCh);
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
	if (((nChannel == INTERNAL_FM_CHANNEL_BASE + 2) ||
		 (nChannel == EXPANSION_FM_CHANNEL_BASE + 2) ||
		 (nChannel == EXPANSION_FM_CHANNEL_BASE + 5)) &&
		(ch.nSpecialMode != 0) && (nOpIndex < 3))
	{
		wFnum   = ch.awFnumPerOp[nOpIndex];
		btBlock = ch.abtBlockPerOp[nOpIndex];
	} else {
		wFnum   = ch.wFnum;
		btBlock = ch.btBlock;
	}
	// Base phase increment (per output sample, scaled to the 20-bit
	// phase accumulator) using the YM2203 spec frequency formula.
	long long nBase = (long long)wFnum << btBlock;
	long long nIncX16 = nBase * (long long)ch.nFmPhaseScaleX16;
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
			if (ReadEnvBool("X88_FM_DT_APPLY_MUL", true)) {
				// 2026-05 Scheme DeathWorld CH2 comparison: ALGO 4 patches
				// with detuned high-MUL operators sounded too thin when DT
				// was applied as a fixed Hz offset after MUL. YS Opening CH1
				// then showed that the follow amount is worth exposing for
				// A/B. DeathWorld still needs the full MUL-following behavior
				// for the two-chain thickness, so that remains the default.
				// Set X88_FM_DT_APPLY_MUL=0 or X88_FM_DT_MUL_WEIGHT=0 to test
				// the older fixed-offset behaviour.
			int nDtMul = op.btMul;
			double dWeight = ReadEnvDouble("X88_FM_DT_MUL_WEIGHT", 1.0,
				0.0, 1.0);
			double dMulScale;
			if (nDtMul == 0) {
				dMulScale = 0.5;
			} else {
				dMulScale = (double)nDtMul;
			}
			double dAppliedScale = 1.0 + (dMulScale - 1.0) * dWeight;
			nDtOfs = (long long)((double)nDtOfs * dAppliedScale +
				((nDtOfs >= 0)? 0.5: -0.5));
		}
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
	if (nCh >= 3) {
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
	OnFmRegisterWriteAt(0, INTERNAL_FM_CHANNEL_BASE, nAddress, btData);
}

void CPC88Opna::OnFmRegisterWriteAt(int nPage, int nChannelBase,
	int nAddress, uint8_t btData)
{
	if ((nChannelBase < 0) || (nChannelBase >= FM_CHANNEL_COUNT)) {
		return;
	}
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
		OnFmKeyOnOffAt(nChannelBase,
			(nChannelBase == EXPANSION_FM_CHANNEL_BASE)?
				OPNA_FM_CHANNEL_COUNT: OPN_FM_CHANNEL_COUNT,
			btData);
		return;
	}

	// Operator slot register ($30..$9E)
	if ((nAddress >= 0x30) && (nAddress <= 0x9E)) {
		int nCh, nOp;
		if (!ResolveFmSlotAddress(nAddress, nCh, nOp)) {
			return;  // unused slot column
		}
		nCh += nChannelBase;
		if (nCh >= FM_CHANNEL_COUNT) return;
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
		int nCh = nChannelBase + (nAddress - 0xA0);
		if (nCh >= FM_CHANNEL_COUNT) return;
		uint8_t btLatch = m_abtFmFnumLatch[nCh];
		m_aFmCh[nCh].btBlock = (btLatch >> 3) & 0x07;
		m_aFmCh[nCh].wFnum =
			(((uint16_t)(btLatch & 0x07)) << 8) | btData;
		RecomputeFmChannelPhaseIncs(nCh);
		return;
	}
	// Channel BLOCK + F-Number high ($A4..$A6). Latch only — no commit.
	if ((nAddress >= 0xA4) && (nAddress <= 0xA6)) {
		int nCh = nChannelBase + (nAddress - 0xA4);
		if (nCh >= FM_CHANNEL_COUNT) return;
		m_abtFmFnumLatch[nCh] = btData & 0x3F;  // BLOCK[2:0] + FNUM[10:8]
		return;
	}
	// CH3/CH6 special mode F-Number low for slots S1/S2/S3.
	// Manual Table 2-2 maps these in slot order:
	//   $A9=S1(OP1), $AA=S2(OP2), $A8=S3(OP3).
	// S4(OP4) always uses the normal channel F-Number ($A2/$A6).
	if ((nAddress >= 0xA8) && (nAddress <= 0xAA)) {
		int nSpecialCh = nChannelBase + 2;
		if (nSpecialCh >= FM_CHANNEL_COUNT) return;
		static const int kSpecialAddrToOp[3] = { 2, 0, 1 };
		int nOp = kSpecialAddrToOp[nAddress - 0xA8];
		uint8_t btLatch = m_abtFmCh3FnumLatch[nSpecialCh][nOp];
		m_aFmCh[nSpecialCh].abtBlockPerOp[nOp] = (btLatch >> 3) & 0x07;
		m_aFmCh[nSpecialCh].awFnumPerOp[nOp] =
			(((uint16_t)(btLatch & 0x07)) << 8) | btData;
		if (m_aFmCh[nSpecialCh].nSpecialMode != 0) {
			RecomputeFmOperatorPhaseInc(nSpecialCh, nOp);
		}
		return;
	}
	// CH3/CH6 special mode BLOCK + F-Number high for S1/S2/S3.
	// Address-to-slot order is the same as $A8-$AA above.
	if ((nAddress >= 0xAC) && (nAddress <= 0xAE)) {
		int nSpecialCh = nChannelBase + 2;
		if (nSpecialCh >= FM_CHANNEL_COUNT) return;
		static const int kSpecialAddrToOp[3] = { 2, 0, 1 };
		int nOp = kSpecialAddrToOp[nAddress - 0xAC];
		m_abtFmCh3FnumLatch[nSpecialCh][nOp] = btData & 0x3F;
		return;
	}
	// Channel feedback / algorithm ($B0..$B2)
	if ((nAddress >= 0xB0) && (nAddress <= 0xB2)) {
		int nCh = nChannelBase + (nAddress - 0xB0);
		if (nCh >= FM_CHANNEL_COUNT) return;
		m_aFmCh[nCh].btFb   = (btData >> 3) & 0x07;
		m_aFmCh[nCh].btAlgo = btData & 0x07;
		return;
	}
	if ((nAddress >= 0xB4) && (nAddress <= 0xB6)) {
		int nCh = nChannelBase + (nAddress - 0xB4);
		if (nCh >= FM_CHANNEL_COUNT) return;
		m_aFmCh[nCh].btPan = btData & 0xC0;
		return;
	}
}

// handle key on/off ($28)

void CPC88Opna::OnFmKeyOnOff(uint8_t btData) {
	OnFmKeyOnOffAt(INTERNAL_FM_CHANNEL_BASE, OPN_FM_CHANNEL_COUNT, btData);
}

void CPC88Opna::OnFmKeyOnOffAt(int nChannelBase, int nChannelCount,
	uint8_t btData)
{
	int nCh;
	if (nChannelCount > 3) {
		nCh = (btData & 0x03) + ((btData & 0x04)? 3: 0);
	} else {
		nCh = btData & 0x07;
	}
	if (nCh >= nChannelCount) {
		return;
	}
	nCh += nChannelBase;
	if ((nCh < 0) || (nCh >= FM_CHANNEL_COUNT)) {
		return;
	}
	SFmChannel& ch = m_aFmCh[nCh];
	// Bits 4..7 = OP1..OP4 key on (1) / off (0). The op order in this
	// register is the *physical* OP1..OP4 sequence (not the slot order
	// from $30..$9E).
	for (int nOp = 0; nOp < FM_OP_PER_CHANNEL; nOp++) {
		bool bKey = ((btData >> (4 + nOp)) & 1) != 0;
		SFmOperator& op = ch.aOp[nOp];
			bool bRetrigger = bKey &&
				(!op.bKeyOn || ShouldRetriggerFmOnKeyWrite());
			if (bRetrigger) {
				// Key-on edge: transition to ATTACK. Repeated writes with the
				// same key-on mask must not retrigger by default; some drivers
				// rewrite $28 while updating F-Number/special-mode registers.
				// Treating those writes as fresh attacks makes notes sound in
				// the middle of parameter updates. The EG level is left at its
				// current attenuation because forcing it to 1023 made attacks
				// too soft in Scheme and is not how the existing tuning was
				// balanced.
			op.bKeyOn = true;
			op.nEnvState = FM_ENV_ATTACK;
			op.nEnvCounter = 0;
			op.nOutPrev = 0;
			ch.anFeedback[0] = 0;
			ch.anFeedback[1] = 0;
			if (ShouldResetFmPhaseOnKeyOn()) {
				op.nPhase = 0;
			}
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

// CSM (CH3 mode 1) auto key trigger from Timer A overflow.
//
// Per the YM2203/YM2608 spec (and verified against the reference
// write-up at mydocuments.g2.xrea.com/html/p8/csm_voice.html):
//   "Timer-A のオーバーフロー毎に ch.3 の 4 オペレータが
//    自動的に一括キーオンされる"
// CSM is **key-on only**, no automatic key-off. Formant shaping is
// the responsibility of software, which rewrites TL/F-Number on the
// Timer A interrupt; the chip just retriggers the envelope to
// ATTACK every overflow.
//
	// Implementation: emulate a key-on write on all 4 operators of the
	// chip-local CH3. The envelope is retriggered from the current level,
	// while phase and feedback history are reset for deterministic starts.
// The operator's bKeyOn flag (driven by $28) is intentionally not
// touched; a software-issued $28 key-off still latches cleanly via
// the next OnFmKeyOnOff call.

void CPC88Opna::OnCsmKeyTrigger(int nChannelBase) {
	int nChannel = nChannelBase + 2;
	if ((nChannel < 0) || (nChannel >= FM_CHANNEL_COUNT)) {
		return;
	}
	m_bCsmKeyState = true;
	SFmChannel& ch = m_aFmCh[nChannel];
	ch.anFeedback[0] = 0;
	ch.anFeedback[1] = 0;
	for (int nOp = 0; nOp < FM_OP_PER_CHANNEL; nOp++) {
		SFmOperator& op = ch.aOp[nOp];
		op.nEnvState   = FM_ENV_ATTACK;
		op.nEnvCounter = 0;
		op.nOutPrev    = 0;
		// Reset phase on every CSM trigger so all 4 sinusoids
		// re-align at the Timer A period boundary. This is what
		// produces the regularly-spaced harmonic ladder that
		// reference implementations show in the spectrogram —
		// without phase reset, the four oscillators drift freely
		// and the formant pattern smears out.
		op.nPhase = 0;
		if (op.btSsgEg & 0x08) {
			op.bSsgEgInverted = (op.btSsgEg & 0x04) != 0;
		} else {
			op.bSsgEgInverted = false;
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
	if (((nChannel == INTERNAL_FM_CHANNEL_BASE + 2) ||
		 (nChannel == EXPANSION_FM_CHANNEL_BASE + 2) ||
		 (nChannel == EXPANSION_FM_CHANNEL_BASE + 5)) &&
		(ch.nSpecialMode != 0) && (nOpIndex < 3))
	{
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

	void CPC88Opna::AdvanceFmEnvelope(SFmOperator& op, int nKsr,
		int nAlgo, int nFb) {
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
		{
			// DR=0 → envelope stops (true "no decay"). KSR alone must not
			// drive the rate.
			if (op.btDr == 0) return;
			nRate = op.btDr * 2 + nKsr;
			if (nRate > 63) nRate = 63;
			int nDecayInc = ScaleFmEnvRateInc(
				m_anFmEnvRateTable[nRate],
				"X88_FM_DECAY_RATE_SCALE",
				1.0);
			nDecayInc = ScaleSignedInt(nDecayInc,
				GetFmPercussiveDecayScale(nAlgo, nFb));
			if (nDecayInc < 1) nDecayInc = 1;
			op.nEnvCounter += nDecayInc;
		}
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
			op.nEnvCounter += ScaleFmEnvRateInc(
				m_anFmEnvRateTable[nRate],
				"X88_FM_SUSTAIN_RATE_SCALE",
				1.0);
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
			op.nEnvCounter += ScaleFmEnvRateInc(
				m_anFmEnvRateTable[nRate],
				"X88_FM_RELEASE_RATE_SCALE",
				1.0);
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

static uint16_t ReadLe16(const uint8_t* p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ReadLe32(const uint8_t* p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

enum {
	ADPCM_STATUS_EOS      = 0x04,
	ADPCM_STATUS_BRDY     = 0x08,
	ADPCM_STATUS_ZERO     = 0x10,
	ADPCM_STATUS_PCMBUSY  = 0x20
};

static int ClampInt(int n, int nMin, int nMax) {
	if (n < nMin) return nMin;
	if (n > nMax) return nMax;
	return n;
}

void CPC88Opna::ResetAdpcmState() {
	for (int n = 0; n < 0x11; n++) {
		m_abtAdpcmRegisters[n] = 0;
	}
	if (m_adpcm.vMemory.size() != ADPCM_MEMORY_SIZE) {
		m_adpcm.vMemory.assign(ADPCM_MEMORY_SIZE, 0);
	}
	m_adpcm.vCpuFifo.clear();
	m_adpcm.btControl1 = 0;
	m_adpcm.btControl2 = 0;
	m_adpcm.btFlagControl = 0x1C;
	m_adpcm.btStatus = 0;
	m_adpcm.nStartAddr = 0;
	m_adpcm.nStopAddr = 0;
	m_adpcm.nLimitAddr = ADPCM_MEMORY_SIZE - 1;
	m_adpcm.nCurrentAddr = 0;
	m_adpcm.nStepX16 = 1 << 16;
	m_adpcm.nAccumX16 = 0;
	m_adpcm.nPreviousSample = 0;
	m_adpcm.nCurrentSample = 0;
	m_adpcm.nLastOutputSample = 0;
	m_adpcm.nFadeSample = 0;
	m_adpcm.nFadeCounter = 0;
	m_adpcm.nFadeTotal = 0;
	m_adpcm.bPlaying = false;
	m_adpcm.bExternal = false;
	m_adpcm.bMemoryWrite = false;
	m_adpcm.bMemoryRead = false;
	m_adpcm.bHighNibble = true;
	m_adpcm.bCpuStream = false;
	m_adpcm.bEndAfterByte = false;
	m_adpcm.bTransferEnded = false;
	m_adpcm.bTransferStarted = false;
	m_adpcm.bHaveCurrentSample = false;
	ResetAdpcmDecoder();
}

void CPC88Opna::ResetAdpcmDecoder() {
	m_adpcm.nPredictor = 0;
	m_adpcm.nDelta = 127;
	m_adpcm.nPreviousSample = 0;
	m_adpcm.nCurrentSample = 0;
	m_adpcm.nLastOutputSample = 0;
	m_adpcm.nFadeSample = 0;
	m_adpcm.nFadeCounter = 0;
	m_adpcm.nFadeTotal = 0;
	m_adpcm.btDecodeByte = 0;
	m_adpcm.nAccumX16 = 0;
	m_adpcm.bHighNibble = true;
	m_adpcm.bHaveCurrentSample = false;
}

void CPC88Opna::UpdateAdpcmAddresses() {
	uint32_t nStart = ((uint32_t)m_abtAdpcmRegisters[0x03] << 8) |
		(uint32_t)m_abtAdpcmRegisters[0x02];
	uint32_t nStop = ((uint32_t)m_abtAdpcmRegisters[0x05] << 8) |
		(uint32_t)m_abtAdpcmRegisters[0x04];
	uint32_t nLimit = ((uint32_t)m_abtAdpcmRegisters[0x0D] << 8) |
		(uint32_t)m_abtAdpcmRegisters[0x0C];
	// YM2608 addresses have different low-bit granularity depending on
	// external memory type: DRAM x1 is 32 bits (= 4 bytes), while ROM and
	// DRAM x8 are 32 bytes. Sound Board II software commonly selects
	// DRAM x1 ($01 bit1=0), so treating everything as 32-byte units plays
	// past the written ADPCM data and turns samples into noise.
	uint32_t nUnitShift = ((m_adpcm.btControl2 & 0x03) == 0x00)? 2: 5;
	uint32_t nUnitMask = (1u << nUnitShift) - 1u;
	m_adpcm.nStartAddr = (nStart << nUnitShift) & (ADPCM_MEMORY_SIZE - 1);
	m_adpcm.nStopAddr = ((nStop << nUnitShift) | nUnitMask) & (ADPCM_MEMORY_SIZE - 1);
	m_adpcm.nLimitAddr = ((nLimit << nUnitShift) | nUnitMask) & (ADPCM_MEMORY_SIZE - 1);
	if (m_adpcm.nLimitAddr < m_adpcm.nStopAddr) {
		m_adpcm.nLimitAddr = ADPCM_MEMORY_SIZE - 1;
	}
	if ((m_adpcm.bMemoryWrite || m_adpcm.bMemoryRead) && !m_adpcm.bTransferStarted) {
		m_adpcm.nCurrentAddr = m_adpcm.nStartAddr;
		m_adpcm.bTransferEnded = false;
	}
}

void CPC88Opna::UpdateAdpcmStep() {
	uint32_t nDeltaN = ((uint32_t)m_abtAdpcmRegisters[0x0A] << 8) |
		(uint32_t)m_abtAdpcmRegisters[0x09];
	if (nDeltaN == 0) {
		nDeltaN = 0x49BA;  // about 16 kHz from the manual's 55.5 kHz base.
	}
	if (m_nSampleRate <= 0) {
		m_adpcm.nStepX16 = 1 << 16;
		return;
	}
	const uint64_t nAdpcmBaseHzX16 = (8000000ULL << 16) / 144ULL;
	uint64_t nStep = (uint64_t)nDeltaN * nAdpcmBaseHzX16 /
		((uint64_t)m_nSampleRate << 16);
	if (nStep == 0) nStep = 1;
	if (nStep > 0x1000000ULL) nStep = 0x1000000ULL;
	m_adpcm.nStepX16 = (uint32_t)nStep;
}

uint8_t CPC88Opna::ReadAdpcmStatus() {
	return (m_timerExpansionOpna.btStatus & 0x03) | (m_adpcm.btStatus & 0x3C);
}

uint8_t CPC88Opna::ReadAdpcmData() {
	int nAddress = 0;
	if (m_nSoundBoardMode == SOUNDBOARD_OPNA) {
		nAddress = m_anAddress[1] & 0xFF;
	} else {
		nAddress = m_nSoundBoard2AddressUpper & 0xFF;
	}
	if (nAddress != 0x08) {
		if (nAddress == 0x0F) {
			return 0x80;
		}
		return m_abtAdpcmRegisters[nAddress];
	}
	uint8_t btData = 0x00;
	if (m_adpcm.bMemoryRead && !m_adpcm.vMemory.empty()) {
		m_adpcm.bTransferStarted = true;
		btData = m_adpcm.vMemory[m_adpcm.nCurrentAddr % ADPCM_MEMORY_SIZE];
		SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
		if (m_adpcm.nCurrentAddr == m_adpcm.nStopAddr) {
			m_adpcm.bTransferEnded = true;
			SetAdpcmStatusFlag(ADPCM_STATUS_EOS);
		} else {
			AdvanceAdpcmAddress();
		}
	}
	return btData;
}

void CPC88Opna::SetAdpcmStatusFlag(uint8_t btFlag) {
	if ((btFlag == ADPCM_STATUS_EOS) && (m_adpcm.btFlagControl & 0x04)) return;
	if ((btFlag == ADPCM_STATUS_BRDY) && (m_adpcm.btFlagControl & 0x08)) return;
	if ((btFlag == ADPCM_STATUS_ZERO) && (m_adpcm.btFlagControl & 0x10)) return;
	m_adpcm.btStatus |= btFlag;
	RefreshInterruptRequest();
}

void CPC88Opna::ClearAdpcmStatusFlag(uint8_t btFlag) {
	m_adpcm.btStatus &= ~btFlag;
	RefreshInterruptRequest();
}

void CPC88Opna::RefreshInterruptRequest() {
	bool bRequest = false;
	if ((m_nSoundBoardMode == SOUNDBOARD_OPN ||
		 m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA) &&
		((m_timerInternalOpn.btStatus & 0x03) != 0))
	{
		bRequest = true;
	}
	if ((m_nSoundBoardMode == SOUNDBOARD_OPNA ||
		 m_nSoundBoardMode == SOUNDBOARD_OPN_OPNA) &&
		((m_timerExpansionOpna.btStatus & 0x03) != 0))
	{
		bRequest = true;
	}
	if (GetAdpcmInterruptEnabled() && ((m_adpcm.btStatus & 0x1C) != 0)) {
		bRequest = true;
	}
	UpdateInterruptRequest(bRequest);
}

void CPC88Opna::StartAdpcmPlayback(bool bExternal) {
	UpdateAdpcmAddresses();
	UpdateAdpcmStep();
	ResetAdpcmDecoder();
	m_adpcm.bPlaying = true;
	m_adpcm.bExternal = bExternal;
	m_adpcm.bCpuStream = !bExternal;
	m_adpcm.bMemoryWrite = false;
	m_adpcm.bMemoryRead = false;
	m_adpcm.nCurrentAddr = m_adpcm.nStartAddr;
	m_adpcm.bEndAfterByte = false;
	m_adpcm.bTransferEnded = false;
	m_adpcm.bTransferStarted = false;
	m_adpcm.nFadeCounter = 0;
	m_adpcm.nFadeTotal = 0;
	m_adpcm.btStatus |= ADPCM_STATUS_PCMBUSY;
	SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
	PrimeAdpcmInterpolator();
}

void CPC88Opna::StopAdpcmPlayback(bool bSetEos) {
	if (m_adpcm.bPlaying) {
		int nDeclick = GetAdpcmDeclickSamples();
		if ((nDeclick > 0) && (m_adpcm.nLastOutputSample != 0)) {
			m_adpcm.nFadeSample = m_adpcm.nLastOutputSample;
			m_adpcm.nFadeCounter = nDeclick;
			m_adpcm.nFadeTotal = nDeclick;
		}
	}
	m_adpcm.bPlaying = false;
	m_adpcm.bCpuStream = false;
	m_adpcm.bMemoryWrite = false;
	m_adpcm.bMemoryRead = false;
	m_adpcm.bEndAfterByte = false;
	m_adpcm.bTransferEnded = false;
	m_adpcm.bTransferStarted = false;
	m_adpcm.btStatus &= ~ADPCM_STATUS_PCMBUSY;
	if (bSetEos) {
		SetAdpcmStatusFlag(ADPCM_STATUS_EOS);
	} else {
		RefreshInterruptRequest();
	}
}

void CPC88Opna::WriteAdpcmRegister(int nAddress, uint8_t btData) {
	if ((nAddress < 0) || (nAddress > 0x10)) {
		return;
	}
	m_abtAdpcmRegisters[nAddress] = btData;
	switch (nAddress) {
	case 0x00:
		m_adpcm.btControl1 = btData;
		if (btData & 0x01) {
			StopAdpcmPlayback(false);
			int nFadeSample = m_adpcm.nFadeSample;
			int nFadeCounter = m_adpcm.nFadeCounter;
			int nFadeTotal = m_adpcm.nFadeTotal;
			ResetAdpcmDecoder();
			m_adpcm.nFadeSample = nFadeSample;
			m_adpcm.nFadeCounter = nFadeCounter;
			m_adpcm.nFadeTotal = nFadeTotal;
			m_adpcm.vCpuFifo.clear();
			return;
		}
		m_adpcm.bMemoryWrite = ((btData & 0x60) == 0x60);
		m_adpcm.bMemoryRead = ((btData & 0x60) == 0x20);
		if (m_adpcm.bMemoryWrite || m_adpcm.bMemoryRead) {
			UpdateAdpcmAddresses();
			m_adpcm.nCurrentAddr = m_adpcm.nStartAddr;
			m_adpcm.bTransferEnded = false;
			m_adpcm.bTransferStarted = false;
			SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
		}
		if ((btData & 0x80) && (btData & 0x20) && ((btData & 0x40) == 0)) {
			StartAdpcmPlayback(true);
		} else if ((btData & 0x80) == 0 && m_adpcm.bPlaying) {
			StopAdpcmPlayback(false);
		} else if ((btData & 0x20) == 0) {
			m_adpcm.bCpuStream = ((btData & 0x80) != 0) && ((btData & 0x40) == 0);
			if (m_adpcm.bCpuStream) {
				UpdateAdpcmStep();
				SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
			}
		}
		break;
	case 0x01:
		m_adpcm.btControl2 = btData;
		UpdateAdpcmAddresses();
		break;
	case 0x02: case 0x03: case 0x04: case 0x05:
	case 0x0C: case 0x0D:
		UpdateAdpcmAddresses();
		break;
	case 0x08:
		if (m_adpcm.bMemoryWrite && !m_adpcm.vMemory.empty()) {
			m_adpcm.bTransferStarted = true;
			if (!m_adpcm.bTransferEnded) {
				m_adpcm.vMemory[m_adpcm.nCurrentAddr % ADPCM_MEMORY_SIZE] = btData;
				if (m_adpcm.nCurrentAddr == m_adpcm.nStopAddr) {
					m_adpcm.bTransferEnded = true;
				} else {
					AdvanceAdpcmAddress();
				}
			}
			SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
			if (m_adpcm.bTransferEnded) {
				SetAdpcmStatusFlag(ADPCM_STATUS_EOS);
			}
		} else if (m_adpcm.bCpuStream) {
			m_adpcm.vCpuFifo.push_back(btData);
			if (!m_adpcm.bPlaying) {
				StartAdpcmPlayback(false);
			}
			SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
		}
		break;
	case 0x09: case 0x0A:
		UpdateAdpcmStep();
		break;
	case 0x10:
		if (btData & 0x80) {
			m_adpcm.btStatus = 0;
			if (m_adpcm.bCpuStream || m_adpcm.bMemoryWrite || m_adpcm.bMemoryRead) {
				SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
			}
			RefreshInterruptRequest();
		} else {
			m_adpcm.btFlagControl = btData & 0x1F;
			if (btData & 0x01) m_timerExpansionOpna.btStatus &= 0xFE;
			if (btData & 0x02) m_timerExpansionOpna.btStatus &= 0xFD;
			if (btData & 0x04) m_adpcm.btStatus &= ~ADPCM_STATUS_EOS;
			if (btData & 0x08) m_adpcm.btStatus &= ~ADPCM_STATUS_BRDY;
			if (btData & 0x10) m_adpcm.btStatus &= ~ADPCM_STATUS_ZERO;
			if (((btData & 0x08) == 0) &&
				(m_adpcm.bCpuStream || m_adpcm.bMemoryWrite || m_adpcm.bMemoryRead))
			{
				SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
			}
			RefreshInterruptRequest();
		}
		break;
	}
}

void CPC88Opna::AdvanceAdpcmAddress() {
	if (m_adpcm.nCurrentAddr >= m_adpcm.nLimitAddr) {
		m_adpcm.nCurrentAddr = 0;
	} else {
		m_adpcm.nCurrentAddr = (m_adpcm.nCurrentAddr + 1) % ADPCM_MEMORY_SIZE;
	}
}

bool CPC88Opna::FetchNextAdpcmByte(uint8_t& btData) {
	if (m_adpcm.bExternal) {
		if (m_adpcm.vMemory.empty()) {
			return false;
		}
		btData = m_adpcm.vMemory[m_adpcm.nCurrentAddr % ADPCM_MEMORY_SIZE];
		if (m_adpcm.nCurrentAddr == m_adpcm.nStopAddr) {
			if (m_adpcm.btControl1 & 0x10) {
				m_adpcm.nCurrentAddr = m_adpcm.nStartAddr;
			} else {
				m_adpcm.bEndAfterByte = true;
			}
		} else {
			AdvanceAdpcmAddress();
		}
		return true;
	}
	if (m_adpcm.vCpuFifo.empty()) {
		SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
		return false;
	}
	btData = m_adpcm.vCpuFifo.front();
	m_adpcm.vCpuFifo.erase(m_adpcm.vCpuFifo.begin());
	SetAdpcmStatusFlag(ADPCM_STATUS_BRDY);
	return true;
}

bool CPC88Opna::DecodeNextAdpcmSample(int& nSample) {
	if (m_adpcm.bHighNibble) {
		if (!FetchNextAdpcmByte(m_adpcm.btDecodeByte)) {
			return false;
		}
		DecodeAdpcmNibble((m_adpcm.btDecodeByte >> 4) & 0x0F);
		m_adpcm.bHighNibble = false;
	} else {
		DecodeAdpcmNibble(m_adpcm.btDecodeByte & 0x0F);
		m_adpcm.bHighNibble = true;
		if (m_adpcm.bEndAfterByte) {
			StopAdpcmPlayback(true);
		}
	}
	nSample = m_adpcm.nPredictor;
	return true;
}

bool CPC88Opna::PrimeAdpcmInterpolator() {
	if (m_adpcm.bHaveCurrentSample) {
		return true;
	}
	int nSample = 0;
	m_adpcm.nPreviousSample = 0;
	m_adpcm.nCurrentSample = 0;
	m_adpcm.nAccumX16 = 0;
	if (!DecodeNextAdpcmSample(nSample)) {
		return false;
	}
	m_adpcm.nCurrentSample = nSample;
	m_adpcm.bHaveCurrentSample = true;
	return true;
}

void CPC88Opna::DecodeAdpcmNibble(uint8_t btNibble) {
	static const int kDeltaScale[8] = { 57, 57, 57, 57, 77, 102, 128, 153 };
	int nMag = btNibble & 0x07;
	int nDiff = ((2 * nMag + 1) * m_adpcm.nDelta) / 8;
	if (btNibble & 0x08) {
		m_adpcm.nPredictor -= nDiff;
	} else {
		m_adpcm.nPredictor += nDiff;
	}
	m_adpcm.nPredictor = ClampInt(m_adpcm.nPredictor, -32768, 32767);
	m_adpcm.nDelta = (m_adpcm.nDelta * kDeltaScale[nMag]) / 64;
	m_adpcm.nDelta = ClampInt(m_adpcm.nDelta, 127, 24576);
	m_adpcm.nPreviousSample = m_adpcm.nCurrentSample;
	m_adpcm.nCurrentSample = m_adpcm.nPredictor;
}

void CPC88Opna::RenderAdpcmStereoSample(int& nLeft, int& nRight) {
	nLeft = 0;
	nRight = 0;
	if ((m_nSoundBoardMode != SOUNDBOARD_OPNA) &&
		(m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA))
	{
		return;
	}
	if (m_bExpansionOpnaMute || !m_adpcm.bPlaying) {
		if (!m_bExpansionOpnaMute && (m_adpcm.nFadeCounter > 0) &&
			(m_adpcm.nFadeTotal > 0))
		{
			int nSample = (m_adpcm.nFadeSample * m_adpcm.nFadeCounter) /
				m_adpcm.nFadeTotal;
			m_adpcm.nFadeCounter--;
			if (m_adpcm.nFadeCounter <= 0) {
				m_adpcm.nFadeSample = 0;
				m_adpcm.nFadeTotal = 0;
			}
			bool bL = (m_adpcm.btControl2 & 0x80) != 0;
			bool bR = (m_adpcm.btControl2 & 0x40) != 0;
			if (!bL && !bR) {
				bL = bR = true;
			}
			if (bL) nLeft += nSample;
			if (bR) nRight += nSample;
		}
		return;
	}
	if (!PrimeAdpcmInterpolator()) {
		return;
	}
	int nFrac = (int)(m_adpcm.nAccumX16 & 0xFFFF);
	int nInterp = m_adpcm.nPreviousSample +
		(int)(((long long)(m_adpcm.nCurrentSample - m_adpcm.nPreviousSample) *
			nFrac) >> 16);
	m_adpcm.nAccumX16 += m_adpcm.nStepX16;
	while (m_adpcm.bPlaying && (m_adpcm.nAccumX16 >= (1 << 16))) {
		m_adpcm.nAccumX16 -= (1 << 16);
		m_adpcm.nPreviousSample = m_adpcm.nCurrentSample;
		int nNextSample = 0;
		if (!DecodeNextAdpcmSample(nNextSample)) {
			if (m_adpcm.bExternal) {
				StopAdpcmPlayback(true);
			}
			break;
		}
		m_adpcm.nCurrentSample = nNextSample;
	}
	int nLevel = m_abtAdpcmRegisters[0x0B];
	int nSample = 0;
	if (nLevel != 0) {
		nSample = ScaleAudioSample(
			(nInterp * nLevel) / (255 * 2),
			GetAdpcmMixScale());
	}
	m_adpcm.nLastOutputSample = nSample;
	bool bL = (m_adpcm.btControl2 & 0x80) != 0;
	bool bR = (m_adpcm.btControl2 & 0x40) != 0;
	if (!bL && !bR) {
		bL = bR = true;
	}
	if (bL) nLeft += nSample;
	if (bR) nRight += nSample;
}

void CPC88Opna::LoadRhythmSamples() {
	if (m_bRhythmSamplesLoaded) {
		return;
	}
	static const char* kNames[RHYTHM_CHANNEL_COUNT] = {
		"2608_BD.WAV",
		"2608_SD.WAV",
		"2608_TOP.WAV",
		"2608_HH.WAV",
		"2608_TOM.WAV",
		"2608_RIM.WAV"
	};
	bool bAllLoaded = true;
	for (int n = 0; n < RHYTHM_CHANNEL_COUNT; n++) {
		if (!LoadOneRhythmSample(kNames[n], m_aRhythmSample[n])) {
			fprintf(stderr, "[OPNA] rhythm sample not available: %s\n", kNames[n]);
			bAllLoaded = false;
		}
	}
	m_bRhythmSamplesLoaded = bAllLoaded;
}

bool CPC88Opna::LoadOneRhythmSample(const char* pszName, SRhythmSample& smp) {
	smp.vSamples.clear();
	smp.vSamplesRight.clear();
	smp.nSampleRate = 0;
	if ((m_pSystemFileOpenCallback == NULL) || (pszName == NULL)) {
		return false;
	}
	FILE* fp = m_pSystemFileOpenCallback(pszName);
	if (fp == NULL) {
		return false;
	}
	fseek(fp, 0, SEEK_END);
	long nSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (nSize < 44) {
		fclose(fp);
		return false;
	}
	std::vector<uint8_t> vData((size_t)nSize);
	if (fread(&vData[0], 1, (size_t)nSize, fp) != (size_t)nSize) {
		fclose(fp);
		return false;
	}
	fclose(fp);
	if (memcmp(&vData[0], "RIFF", 4) != 0 || memcmp(&vData[8], "WAVE", 4) != 0) {
		return false;
	}
	int nChannels = 0;
	int nBits = 0;
	int nRate = 0;
	const uint8_t* pSample = NULL;
	uint32_t nSampleBytes = 0;
	size_t pos = 12;
	while (pos + 8 <= vData.size()) {
		const uint8_t* pChunk = &vData[pos];
		uint32_t nChunkSize = ReadLe32(pChunk + 4);
		pos += 8;
		if (pos + nChunkSize > vData.size()) {
			break;
		}
		if (memcmp(pChunk, "fmt ", 4) == 0 && nChunkSize >= 16) {
			const uint8_t* p = &vData[pos];
			uint16_t nFormat = ReadLe16(p + 0);
			nChannels = ReadLe16(p + 2);
			nRate = (int)ReadLe32(p + 4);
			nBits = ReadLe16(p + 14);
			if (nFormat != 1) {
				return false;
			}
		} else if (memcmp(pChunk, "data", 4) == 0) {
			pSample = &vData[pos];
			nSampleBytes = nChunkSize;
		}
		pos += nChunkSize + (nChunkSize & 1);
	}
	if ((pSample == NULL) || (nChannels <= 0) || (nChannels > 2) ||
		((nBits != 8) && (nBits != 16)) || (nRate <= 0))
	{
		return false;
	}
	int nBytesPerSample = nBits / 8;
	int nFrameBytes = nBytesPerSample * nChannels;
	int nFrames = (int)(nSampleBytes / nFrameBytes);
	if (nFrames <= 0) {
		return false;
	}
	smp.vSamples.resize(nFrames);
	smp.vSamplesRight.resize(nFrames);
	smp.nSampleRate = nRate;
	for (int i = 0; i < nFrames; i++) {
		int nLeftVal = 0;
		int nRightVal = 0;
		for (int ch = 0; ch < nChannels; ch++) {
			const uint8_t* p = pSample + i * nFrameBytes + ch * nBytesPerSample;
			int nVal;
			if (nBits == 8) {
				nVal = ((int)*p - 128) << 8;
			} else {
				nVal = (int)(int16_t)ReadLe16(p);
			}
			if (ch == 0) {
				nLeftVal = nVal;
			} else {
				nRightVal = nVal;
			}
		}
		if (nChannels == 1) {
			nRightVal = nLeftVal;
		}
		smp.vSamples[i] = (int16_t)nLeftVal;
		smp.vSamplesRight[i] = (int16_t)nRightVal;
	}
	fprintf(stderr, "[OPNA] loaded rhythm sample %s (%d Hz, %d frames)\n",
		pszName, nRate, nFrames);
	return true;
}

void CPC88Opna::OnRhythmRegisterWrite(int nAddress, uint8_t btData) {
	if (nAddress == 0x10) {
		for (int n = 0; n < RHYTHM_CHANNEL_COUNT; n++) {
			if ((btData & (1 << n)) == 0) {
				continue;
			}
			if (btData & 0x80) {
				m_aRhythmVoice[n].bActive = false;
			} else {
				m_aRhythmVoice[n].bActive =
					!m_aRhythmSample[n].vSamples.empty();
				m_aRhythmVoice[n].nPosX16 = 0;
				if ((m_aRhythmSample[n].nSampleRate > 0) && (m_nSampleRate > 0)) {
					m_aRhythmVoice[n].nStepX16 =
						(uint32_t)(((long long)m_aRhythmSample[n].nSampleRate << 16) /
							(long long)m_nSampleRate);
					if (m_aRhythmVoice[n].nStepX16 == 0) {
						m_aRhythmVoice[n].nStepX16 = 1;
					}
				}
			}
		}
		return;
	}
	if (nAddress == 0x11) {
		m_btRhythmTotalLevel = btData & 0x3F;
		return;
	}
	if ((nAddress >= 0x18) && (nAddress <= 0x1D)) {
		int n = nAddress - 0x18;
		m_aRhythmVoice[n].btPan = btData & 0xC0;
		m_aRhythmVoice[n].btLevel = btData & 0x1F;
		return;
	}
}

void CPC88Opna::RenderRhythmStereoSample(int& nLeft, int& nRight) {
	nLeft = 0;
	nRight = 0;
	if ((m_nSoundBoardMode != SOUNDBOARD_OPNA) &&
		(m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA))
	{
		return;
	}
	if (m_bExpansionOpnaMute) {
		return;
	}
	for (int n = 0; n < RHYTHM_CHANNEL_COUNT; n++) {
		SRhythmVoice& v = m_aRhythmVoice[n];
		SRhythmSample& smp = m_aRhythmSample[n];
		if (!v.bActive || smp.vSamples.empty()) {
			continue;
		}
		uint32_t nPos = v.nPosX16 >> 16;
		if (nPos >= smp.vSamples.size()) {
			v.bActive = false;
			continue;
		}
		int nSampleLeft = smp.vSamples[nPos];
		int nSampleRight = (nPos < smp.vSamplesRight.size())?
			smp.vSamplesRight[nPos]: nSampleLeft;
		v.nPosX16 += v.nStepX16;
		int nInstGain = (int)(v.btLevel & 0x1F);
		int nTotalGain = (int)(m_btRhythmTotalLevel & 0x3F);
		if (nInstGain == 0 || nTotalGain == 0) {
			continue;
		}
		double dScale = RhythmInstrumentLevelToScale(nInstGain) *
			RhythmTotalLevelToScale(nTotalGain) *
			GetRhythmMixScale() * 0.5;
		nSampleLeft = ScaleAudioSample(nSampleLeft, dScale);
		nSampleRight = ScaleAudioSample(nSampleRight, dScale);
		uint8_t btPan = v.btPan;
		bool bL = (btPan & 0x80) != 0;
		bool bR = (btPan & 0x40) != 0;
		if (!bL && !bR) {
			bL = bR = true;
		}
		if (bL) nLeft += nSampleLeft;
		if (bR) nRight += nSampleRight;
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

void CPC88Opna::RenderFmStereoSample(int& nLeft, int& nRight) {
	nLeft = 0;
	nRight = 0;
	for (int nCh = 0; nCh < FM_CHANNEL_COUNT; nCh++) {
		int nChOut = RenderFmChannel(nCh);
		if (m_abFmChMute[nCh]) {
			continue;
		}
		bool bInternal = (nCh >= INTERNAL_FM_CHANNEL_BASE) &&
			(nCh < INTERNAL_FM_CHANNEL_BASE + OPN_FM_CHANNEL_COUNT);
		bool bExpansion = (nCh >= EXPANSION_FM_CHANNEL_BASE) &&
			(nCh < EXPANSION_FM_CHANNEL_BASE + OPNA_FM_CHANNEL_COUNT);
		if (bInternal) {
			if ((m_nSoundBoardMode != SOUNDBOARD_OPN) &&
				(m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA))
			{
				continue;
			}
			if (m_bInternalOpnMute) {
				continue;
			}
			nLeft += nChOut;
			nRight += nChOut;
		} else if (bExpansion) {
			if ((m_nSoundBoardMode != SOUNDBOARD_OPNA) &&
				(m_nSoundBoardMode != SOUNDBOARD_OPN_OPNA))
			{
				continue;
			}
			if (m_bExpansionOpnaMute) {
				continue;
			}
			uint8_t btPan = m_aFmCh[nCh].btPan;
			bool bL = (btPan & 0x80) != 0;
			bool bR = (btPan & 0x40) != 0;
			if (!bL && !bR) {
				bL = bR = true;
			}
			if (bL) nLeft += nChOut;
			if (bR) nRight += nChOut;
		}
	}
	nLeft >>= 1;
	nRight >>= 1;
}

// produce one mono FM sample by mixing all channels

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
		AdvanceFmEnvelope(ch.aOp[nOp], ComputeFmKsr(nChannel, nOp),
			ch.btAlgo, ch.btFb);
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
		nFbMod = ScaleFmFeedbackInput(nFbMod);
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

	// Op→op modulation inputs are shifted right by 1 to match the
	// standard YM2203 modulation depth. Op output peak is ±8192
	// (14-bit, since the 2026-04 update). The manual specifies max
	// modulator output → 8π peak phase deviation (= 4096 phase units
	// over a 1024-unit cycle). Passing unshifted gave 16π (2× too
	// much), which produced the "明るすぎ・荒い" timbre seen in FFT
	// vs fmgen (over-strong 2nd/3rd harmonics). Feedback path is
	// scaled separately via >> (10-FB) above, so it stays correct.
	switch (ch.btAlgo) {
	case 0:
		// OP1 → OP2 → OP3 → OP4 → out  (serial 4-op)
		nOp2Out = RenderFmOperator(ch.aOp[1], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
		nOp3Out = RenderFmOperator(ch.aOp[2], ScaleFmModInput(nOp2Out >> 1, ch.btAlgo));
		nOp4Out = RenderFmOperator(ch.aOp[3], ScaleFmModInput(nOp3Out >> 1, ch.btAlgo));
		nMix = nOp4Out;
		break;
	case 1:
		// (OP1 + OP2) → OP3 → OP4 → out  (parallel mods, serial out)
		nOp2Out = RenderFmOperator(ch.aOp[1], 0);
		nOp3Out = RenderFmOperator(ch.aOp[2], ScaleFmModInput((nOp1Out + nOp2Out) >> 1, ch.btAlgo));
		nOp4Out = RenderFmOperator(ch.aOp[3], ScaleFmModInput(nOp3Out >> 1, ch.btAlgo));
		nMix = nOp4Out;
		break;
	case 2:
		// (OP1 + (OP2 → OP3)) → OP4 → out
		nOp2Out = RenderFmOperator(ch.aOp[1], 0);
		nOp3Out = RenderFmOperator(ch.aOp[2], ScaleFmModInput(nOp2Out >> 1, ch.btAlgo));
		nOp4Out = RenderFmOperator(ch.aOp[3], ScaleFmModInput((nOp1Out + nOp3Out) >> 1, ch.btAlgo));
		nMix = nOp4Out;
		break;
	case 3:
		// ((OP1 → OP2) + OP3) → OP4 → out
		nOp2Out = RenderFmOperator(ch.aOp[1], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
		nOp3Out = RenderFmOperator(ch.aOp[2], 0);
		nOp4Out = RenderFmOperator(ch.aOp[3], ScaleFmModInput((nOp2Out + nOp3Out) >> 1, ch.btAlgo));
		nMix = nOp4Out;
		break;
	case 4:
		// (OP1 → OP2☆) + (OP3 → OP4☆)  (two 2-op chains, both heard)
		nOp2Out = RenderFmOperator(ch.aOp[1], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
		nOp3Out = RenderFmOperator(ch.aOp[2], 0);
		nOp4Out = RenderFmOperator(ch.aOp[3], ScaleFmModInput(nOp3Out >> 1, ch.btAlgo));
		nMix = nOp2Out + nOp4Out;
		break;
	case 5:
		// OP1 → {OP2☆, OP3☆, OP4☆}  (1 modulator, 3 carriers)
		nOp2Out = RenderFmOperator(ch.aOp[1], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
		nOp3Out = RenderFmOperator(ch.aOp[2], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
		nOp4Out = RenderFmOperator(ch.aOp[3], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
		nMix = nOp2Out + nOp3Out + nOp4Out;
		break;
	case 6:
		// (OP1 → OP2☆) + OP3☆ + OP4☆
		nOp2Out = RenderFmOperator(ch.aOp[1], ScaleFmModInput(nOp1Out >> 1, ch.btAlgo));
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
