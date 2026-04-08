////////////////////////////////////////////////////////////
// SDL3 audio output for the X88000M frontend
//
// Owns a single SDL_AudioStream and synthesizes the PC-8801 sound
// sources directly:
//   - Beep (fixed 2400 Hz square wave, gated on/off by I/O port)
//   - PCG  (three channels, frequency derived from each 8253 counter)
//   - OPN  (Phase C — square waves replaced by YM2203 sample stream)
//
// State is updated from the emulator thread (via Set* setters) and
// consumed from the SDL audio thread (via the stream get-callback).
// Communication is done with std::atomic so no locking is required.

#ifndef Sdl3AudioOutput_DEFINED
#define Sdl3AudioOutput_DEFINED

#include <SDL3/SDL.h>

#include <atomic>
#include <stdint.h>
#include <vector>

class CSdl3AudioOutput {
public:
	enum {
		PCG_CHANNEL_COUNT = 3,
		// Frequency of the 8253 PIT used by the PCG-8800. PC-8801 derives
		// this from its base clock (≈3.9936 MHz / 2).
		PCG_TIMER_HZ      = 1996800,
		// Beep frequency on PC-8801 (fixed in hardware).
		BEEP_FREQUENCY_HZ = 2400
	};

	CSdl3AudioOutput();
	~CSdl3AudioOutput();

	// Open the SDL audio device. Safe to call once at startup. Returns
	// false if SDL_OpenAudioDeviceStream fails (audio remains silent).
	bool Initialize(int nSampleRate = 44100);
	// Close the SDL audio device. Safe to call multiple times.
	void Shutdown();

	bool IsInitialized() const { return m_bInitialized; }
	int  GetSampleRate() const { return m_nSampleRate; }

	// --- Source state setters (called from the emulator thread) ---

	// Beep gate. The beep is a fixed 2400 Hz square wave that is on
	// when bEnabled is true. The "extended" flag is the second beep
	// status bit reported by the I/O port; we treat it as an
	// independent gate too (most software only uses the main bit).
	void SetBeepEnabled(bool bEnabled, bool bExtended);

	// PCG channel state. nCounter is the 8253 counter value as
	// reported by CPC88Pcg::GetITimerCounterValue:
	//   -1     : channel muted (sound port off)
	//   >  0   : square wave at PCG_TIMER_HZ / nCounter Hz
	//   == 0   : treated as silence (avoid divide-by-zero)
	void SetPcgChannel(int nChannel, int nCounter);

	// Volume / mute controls (range: 0..100 for volume).
	void SetMasterVolume(int nVolume);
	void SetBeepVolume  (int nVolume);
	void SetPcgVolume   (int nVolume);
	void SetBeepMute    (bool bMute);
	void SetPcgMute     (bool bMute);

	// Push pre-mixed YM2203 samples produced by the emulator thread.
	// The data is interleaved stereo int16 at the same sample rate as
	// the audio device. Frames that don't fit in the ring buffer are
	// silently dropped (rather than blocking the emulator thread).
	void PushOpnSamples(const int16_t* pbBuf, int nFrames);

private:
	// Per-source square-wave state used inside the audio callback.
	struct SSquareSource {
		double dPhase;       // 0..1
		double dPhaseInc;    // increment per output sample
		bool   bEnabled;
		SSquareSource() : dPhase(0.0), dPhaseInc(0.0), bEnabled(false) {}
	};

	bool m_bInitialized;
	int  m_nSampleRate;

	SDL_AudioStream* m_pStream;
	SDL_AudioDeviceID m_nDevice;

	// Audio-thread state (refreshed from atomics each fill).
	SSquareSource m_beepMain;
	SSquareSource m_beepExtended;
	SSquareSource m_pcg[PCG_CHANNEL_COUNT];

	// Cross-thread state. Volumes and gates are atomics so the audio
	// thread does not need to lock.
	std::atomic<int>  m_anVolume;     // master, beep, pcg (0..100)
	std::atomic<int>  m_anBeepVolume;
	std::atomic<int>  m_anPcgVolume;
	std::atomic<bool> m_abBeepMute;
	std::atomic<bool> m_abPcgMute;
	std::atomic<bool> m_abBeepMain;
	std::atomic<bool> m_abBeepExt;
	// Each PCG channel stores the current 8253 counter (-1 == off).
	std::atomic<int>  m_anPcgCounter[PCG_CHANNEL_COUNT];

	// Single-producer / single-consumer ring buffer of stereo int16
	// frames coming from the YM2203 (PC88Opna). Producer is the
	// emulator thread (PushOpnSamples); consumer is the audio
	// callback inside Synthesize(). The capacity is fixed and
	// allocated in Initialize() to a power of two so masking works.
	std::vector<int16_t>  m_vOpnRing;       // size = capacity * 2 (stereo)
	int                   m_nOpnRingFrames; // capacity in stereo frames (power of 2)
	std::atomic<uint32_t> m_nOpnWriteIdx;   // producer index (monotonic, frames)
	std::atomic<uint32_t> m_nOpnReadIdx;    // consumer index (monotonic, frames)

	// SDL3 audio stream callback. additional_amount is the number of
	// bytes the stream is requesting; we synthesize that many samples
	// and feed them via SDL_PutAudioStreamData.
	static void SDLCALL StreamCallback(
		void* userdata,
		SDL_AudioStream* stream,
		int additional_amount,
		int total_amount);

	// Internal: synthesize nFrames samples (interleaved stereo int16)
	// into pBuf based on the current state.
	void Synthesize(int16_t* pBuf, int nFrames);

	// Internal: refresh phase increments / gates from atomic state.
	void RefreshSourcesFromAtomics();
};

#endif // Sdl3AudioOutput_DEFINED
