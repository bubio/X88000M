////////////////////////////////////////////////////////////
// SDL3 audio output for the X88000M frontend
//
// Owns a single SDL_AudioStream and synthesizes the PC-8801 sound
// sources directly:
//   - Beep (Port 40h bit5/bit7. bit5 gates the hardware 2400 Hz
//           square wave; bit7 "SING" is the CPU-driven speaker line
//           used for sampled voice. The emulator runs Z80 in a
//           per-frame burst, so individual writes cannot be replayed
//           pulse-accurately in wall-clock time. Instead we count
//           bit5/bit7 transitions over a ~10 ms sliding window and
//           synthesize a clean square wave at the derived frequency
//           (same strategy as the original X88000 core).)
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

	// Record a Port 40h write. bBeep is bit5 (gate for the hardware
	// 2400 Hz square wave); bSing is bit7 ("SING") which drives the
	// speaker output directly. We only maintain transition counters
	// and the current state; the audio thread periodically samples
	// these and derives a square-wave frequency over a sliding
	// window (see Synthesize).
	void SetBeepEnabled(bool bBeep, bool bSing);

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

	// Diagnostic snapshot of Port 40h activity (populated from the
	// emulator thread by SetBeepEnabled). All counters are monotonic
	// since Initialize().
	struct SBeepStats {
		uint64_t nWriteCount;      // total SetBeepEnabled calls
		uint64_t nBit5Transitions; // bit5 edges (both rising and falling)
		uint64_t nBit7Transitions; // bit7 edges
		bool     bCurBit5;         // last observed bit5 state
		bool     bCurBit7;         // last observed bit7 state
	};
	SBeepStats GetBeepStats() const;

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
	SSquareSource m_pcg[PCG_CHANNEL_COUNT];

	// Audio-thread BEEP state. The emulator runs Z80 in a per-frame
	// burst (2-3 ms of wall clock for 16.7 ms of emulated time), so
	// individual Port 40h writes are NOT evenly distributed in real
	// time. We therefore cannot treat each write as a pulse with its
	// real-time timestamp; instead we borrow the original X88000.cpp
	// strategy of counting bit5/bit7 transitions over a sliding
	// ~10 ms window and synthesizing a clean square wave at the
	// derived frequency. Priority: bit7 toggles > bit5 toggles > bit5
	// held HIGH (default 2400 Hz) > silence.
	int      m_nBeepWindowSamples; // samples per frequency-update window
	int      m_nBeepWindowCounter; // samples elapsed in current window
	uint64_t m_nBeepPrevBit5Trans; // transition count at window start
	uint64_t m_nBeepPrevBit7Trans;
	double   m_dBeepOutPhase;      // 0..1, square wave phase
	double   m_dBeepOutInc;        // phase increment per sample
	bool     m_bBeepOutActive;     // is output currently emitting sound

	// Cross-thread state. Volumes and gates are atomics so the audio
	// thread does not need to lock.
	std::atomic<int>  m_anVolume;     // master, beep, pcg (0..100)
	std::atomic<int>  m_anBeepVolume;
	std::atomic<int>  m_anPcgVolume;
	std::atomic<bool> m_abBeepMute;
	std::atomic<bool> m_abPcgMute;
	// Each PCG channel stores the current 8253 counter (-1 == off).
	std::atomic<int>  m_anPcgCounter[PCG_CHANNEL_COUNT];

	// Transition counters and current-state atomics maintained by
	// the emulator thread inside SetBeepEnabled. The audio thread
	// samples these periodically to infer the BEEP frequency; the
	// BEEP Stats diagnostic window also reads them.
	std::atomic<uint64_t>   m_nStatWriteCount;
	std::atomic<uint64_t>   m_nStatBit5Trans;
	std::atomic<uint64_t>   m_nStatBit7Trans;
	std::atomic<bool>       m_bStatLastBit5;
	std::atomic<bool>       m_bStatLastBit7;
	// Previous-value tracker for edge detection. Written only from
	// the emulator thread. Initial value false gives the correct
	// "no edge" result on the first call.
	bool                    m_bStatPrevBit5;
	bool                    m_bStatPrevBit7;

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
