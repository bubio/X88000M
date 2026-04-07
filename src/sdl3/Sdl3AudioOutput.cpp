////////////////////////////////////////////////////////////
// SDL3 audio output — implementation

#include "Sdl3AudioOutput.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

int ClampInt(int n, int nMin, int nMax)
{
	if (n < nMin) return nMin;
	if (n > nMax) return nMax;
	return n;
}

// Convert an integer 0..100 volume to a 0.0..1.0 linear gain.
float VolumeToGain(int nVolume)
{
	if (nVolume <= 0) return 0.0f;
	if (nVolume >= 100) return 1.0f;
	return (float)nVolume / 100.0f;
}

} // namespace

CSdl3AudioOutput::CSdl3AudioOutput() :
	m_bInitialized(false),
	m_nSampleRate(0),
	m_pStream(NULL),
	m_nDevice(0),
	m_anVolume(80),
	m_anBeepVolume(50),
	m_anPcgVolume(50),
	m_abBeepMute(false),
	m_abPcgMute(false),
	m_abBeepMain(false),
	m_abBeepExt(false)
{
	for (int n = 0; n < PCG_CHANNEL_COUNT; n++) {
		m_anPcgCounter[n] = -1;
	}
}

CSdl3AudioOutput::~CSdl3AudioOutput()
{
	Shutdown();
}

bool CSdl3AudioOutput::Initialize(int nSampleRate)
{
	if (m_bInitialized) {
		return true;
	}
	if (nSampleRate <= 0) {
		nSampleRate = 44100;
	}

	SDL_AudioSpec spec;
	SDL_zero(spec);
	spec.format   = SDL_AUDIO_S16LE;
	spec.channels = 2;
	spec.freq     = nSampleRate;

	m_pStream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
		&spec,
		&CSdl3AudioOutput::StreamCallback,
		this);
	if (m_pStream == NULL) {
		fprintf(stderr,
			"Sdl3AudioOutput: SDL_OpenAudioDeviceStream failed: %s\n",
			SDL_GetError());
		return false;
	}

	m_nDevice = SDL_GetAudioStreamDevice(m_pStream);
	m_nSampleRate = nSampleRate;
	m_bInitialized = true;

	// Audio devices opened with SDL_OpenAudioDeviceStream start paused;
	// resume so the get-callback fires.
	SDL_ResumeAudioStreamDevice(m_pStream);
	return true;
}

void CSdl3AudioOutput::Shutdown()
{
	if (m_pStream != NULL) {
		SDL_DestroyAudioStream(m_pStream);
		m_pStream = NULL;
	}
	m_nDevice = 0;
	m_bInitialized = false;
}

void CSdl3AudioOutput::SetBeepEnabled(bool bEnabled, bool bExtended)
{
	m_abBeepMain.store(bEnabled, std::memory_order_relaxed);
	m_abBeepExt.store(bExtended, std::memory_order_relaxed);
}

void CSdl3AudioOutput::SetPcgChannel(int nChannel, int nCounter)
{
	if ((nChannel < 0) || (nChannel >= PCG_CHANNEL_COUNT)) {
		return;
	}
	m_anPcgCounter[nChannel].store(nCounter, std::memory_order_relaxed);
}

void CSdl3AudioOutput::SetMasterVolume(int nVolume)
{
	m_anVolume.store(ClampInt(nVolume, 0, 100), std::memory_order_relaxed);
}

void CSdl3AudioOutput::SetBeepVolume(int nVolume)
{
	m_anBeepVolume.store(ClampInt(nVolume, 0, 100), std::memory_order_relaxed);
}

void CSdl3AudioOutput::SetPcgVolume(int nVolume)
{
	m_anPcgVolume.store(ClampInt(nVolume, 0, 100), std::memory_order_relaxed);
}

void CSdl3AudioOutput::SetBeepMute(bool bMute)
{
	m_abBeepMute.store(bMute, std::memory_order_relaxed);
}

void CSdl3AudioOutput::SetPcgMute(bool bMute)
{
	m_abPcgMute.store(bMute, std::memory_order_relaxed);
}

void CSdl3AudioOutput::PushOpnSamples(const int16_t* pbBuf, int nFrames)
{
	// Phase C will fill this in. For now we just drop the data.
	(void)pbBuf;
	(void)nFrames;
}

void CSdl3AudioOutput::RefreshSourcesFromAtomics()
{
	// Beep main: 2400 Hz square wave when gate is on.
	bool bBeepMain = m_abBeepMain.load(std::memory_order_relaxed);
	bool bBeepExt  = m_abBeepExt.load(std::memory_order_relaxed);
	bool bBeepMute = m_abBeepMute.load(std::memory_order_relaxed);
	if (bBeepMute) {
		bBeepMain = false;
		bBeepExt  = false;
	}
	m_beepMain.bEnabled    = bBeepMain;
	m_beepMain.dPhaseInc   = (double)BEEP_FREQUENCY_HZ / (double)m_nSampleRate;
	m_beepExtended.bEnabled  = bBeepExt;
	m_beepExtended.dPhaseInc = (double)BEEP_FREQUENCY_HZ / (double)m_nSampleRate;

	// PCG: each channel has its own 8253 counter; freq = clk / counter.
	bool bPcgMute = m_abPcgMute.load(std::memory_order_relaxed);
	for (int n = 0; n < PCG_CHANNEL_COUNT; n++) {
		int nCounter = m_anPcgCounter[n].load(std::memory_order_relaxed);
		if (bPcgMute || (nCounter <= 0)) {
			m_pcg[n].bEnabled = false;
			m_pcg[n].dPhaseInc = 0.0;
		} else {
			double dFreq = (double)PCG_TIMER_HZ / (double)nCounter;
			// 8253 actually halves the input clock for square-wave mode
			// (mode 3). The audible pitch is therefore clk / (2*counter).
			dFreq *= 0.5;
			// Clamp to a sane audible range to avoid aliasing nightmares.
			if (dFreq > (double)(m_nSampleRate/2)) {
				dFreq = (double)(m_nSampleRate/2);
			}
			m_pcg[n].bEnabled = true;
			m_pcg[n].dPhaseInc = dFreq / (double)m_nSampleRate;
		}
	}
}

void CSdl3AudioOutput::Synthesize(int16_t* pBuf, int nFrames)
{
	RefreshSourcesFromAtomics();

	float fMasterGain = VolumeToGain(m_anVolume.load(std::memory_order_relaxed));
	float fBeepGain   = VolumeToGain(m_anBeepVolume.load(std::memory_order_relaxed));
	float fPcgGain    = VolumeToGain(m_anPcgVolume.load(std::memory_order_relaxed));

	// Per-source amplitude (signed int16). Pre-divide so the sum stays
	// inside int16 range without clipping for the typical worst case
	// (Beep + 3 PCG channels = 4 simultaneous square waves).
	const float fMaxAmp = 32767.0f * 0.20f;
	float fBeepAmp = fMaxAmp * fBeepGain * fMasterGain;
	float fPcgAmp  = fMaxAmp * fPcgGain  * fMasterGain;

	for (int nFrame = 0; nFrame < nFrames; nFrame++) {
		float fMix = 0.0f;

		// Beep main + extended (extended is rarely used; sum it in too).
		if (m_beepMain.bEnabled) {
			fMix += (m_beepMain.dPhase < 0.5)? +fBeepAmp: -fBeepAmp;
			m_beepMain.dPhase += m_beepMain.dPhaseInc;
			if (m_beepMain.dPhase >= 1.0) m_beepMain.dPhase -= 1.0;
		}
		if (m_beepExtended.bEnabled) {
			fMix += (m_beepExtended.dPhase < 0.5)? +fBeepAmp: -fBeepAmp;
			m_beepExtended.dPhase += m_beepExtended.dPhaseInc;
			if (m_beepExtended.dPhase >= 1.0) m_beepExtended.dPhase -= 1.0;
		}

		// PCG square waves.
		for (int n = 0; n < PCG_CHANNEL_COUNT; n++) {
			SSquareSource& src = m_pcg[n];
			if (!src.bEnabled) {
				continue;
			}
			fMix += (src.dPhase < 0.5)? +fPcgAmp: -fPcgAmp;
			src.dPhase += src.dPhaseInc;
			if (src.dPhase >= 1.0) src.dPhase -= 1.0;
		}

		// Soft clip into int16.
		if (fMix >  32767.0f) fMix =  32767.0f;
		if (fMix < -32768.0f) fMix = -32768.0f;
		int16_t nSample = (int16_t)fMix;
		// Mono → stereo: same value on both channels.
		pBuf[nFrame*2+0] = nSample;
		pBuf[nFrame*2+1] = nSample;
	}
}

void SDLCALL CSdl3AudioOutput::StreamCallback(
	void* userdata,
	SDL_AudioStream* stream,
	int additional_amount,
	int /*total_amount*/)
{
	CSdl3AudioOutput* pSelf = (CSdl3AudioOutput*)userdata;
	if ((pSelf == NULL) || (additional_amount <= 0)) {
		return;
	}
	// 2 channels * 2 bytes (int16) = 4 bytes per frame.
	const int nBytesPerFrame = 2 * (int)sizeof(int16_t);
	int nFrames = additional_amount / nBytesPerFrame;
	if (nFrames <= 0) {
		return;
	}

	// Synthesize in chunks to keep stack usage bounded.
	const int nChunkFrames = 512;
	int16_t aBuf[nChunkFrames * 2];
	while (nFrames > 0) {
		int nThis = (nFrames < nChunkFrames)? nFrames: nChunkFrames;
		pSelf->Synthesize(aBuf, nThis);
		SDL_PutAudioStreamData(stream, aBuf, nThis * nBytesPerFrame);
		nFrames -= nThis;
	}
}
