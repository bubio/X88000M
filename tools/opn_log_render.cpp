////////////////////////////////////////////////////////////
// YM2203 register log renderer
//
// Replays CSV logs produced by X88_OPN_LOG and writes a stereo WAV.

#include "StdHeader.h"
#include "PC88Opna.h"

#include <errno.h>

struct SEvent {
	long long nFrame;
	char chEvent;
	int nAddress;
	int nData;
};

static std::vector<int16_t> g_samples;

static void OnSamples(const int16_t* pSamples, int nFrames) {
	if ((pSamples == NULL) || (nFrames <= 0)) {
		return;
	}
	g_samples.insert(g_samples.end(), pSamples, pSamples + nFrames * 2);
}

static bool ParseHexByte(const char* psz, int& nValue) {
	if ((psz == NULL) || (*psz == '\0')) {
		return false;
	}
	char* pszEnd = NULL;
	errno = 0;
	long n = strtol(psz, &pszEnd, 16);
	if ((errno != 0) || (pszEnd == psz) || (n < 0) || (n > 0xFF)) {
		return false;
	}
	nValue = (int)n;
	return true;
}

static bool LoadLog(const char* pszPath, std::vector<SEvent>& events) {
	FILE* fp = fopen(pszPath, "rb");
	if (fp == NULL) {
		fprintf(stderr, "failed to open log: %s\n", pszPath);
		return false;
	}
	char szLine[512];
	int nLine = 0;
	while (fgets(szLine, sizeof(szLine), fp) != NULL) {
		nLine++;
		char* p = szLine;
		while ((*p == ' ') || (*p == '\t')) p++;
		if ((*p == '\0') || (*p == '\n') || (*p == '#')) {
			continue;
		}
		char* pszFrame = strtok(p, ",\r\n");
		char* pszEvent = strtok(NULL, ",\r\n");
		char* pszAddr  = strtok(NULL, ",\r\n");
		char* pszData  = strtok(NULL, ",\r\n");
		if ((pszFrame == NULL) || (pszEvent == NULL) ||
			(pszAddr == NULL) || (pszData == NULL))
		{
			fprintf(stderr, "bad log line %d\n", nLine);
			fclose(fp);
			return false;
		}
		char* pszEnd = NULL;
		errno = 0;
		long long nFrame = strtoll(pszFrame, &pszEnd, 10);
		if ((errno != 0) || (pszEnd == pszFrame) || (nFrame < 0)) {
			fprintf(stderr, "bad frame on line %d\n", nLine);
			fclose(fp);
			return false;
		}
		int nAddress = 0;
		int nData = 0;
		if (!ParseHexByte(pszAddr, nAddress) || !ParseHexByte(pszData, nData)) {
			fprintf(stderr, "bad byte value on line %d\n", nLine);
			fclose(fp);
			return false;
		}
		SEvent ev;
		ev.nFrame = nFrame;
		ev.chEvent = pszEvent[0];
		ev.nAddress = nAddress;
		ev.nData = nData;
		if ((ev.chEvent != 'A') && (ev.chEvent != 'D')) {
			fprintf(stderr, "bad event type on line %d\n", nLine);
			fclose(fp);
			return false;
		}
		events.push_back(ev);
	}
	fclose(fp);
	std::sort(events.begin(), events.end(),
		[](const SEvent& a, const SEvent& b) {
			return a.nFrame < b.nFrame;
		});
	return true;
}

static void WriteLE16(FILE* fp, uint16_t v) {
	fputc(v & 0xFF, fp);
	fputc((v >> 8) & 0xFF, fp);
}

static void WriteLE32(FILE* fp, uint32_t v) {
	fputc(v & 0xFF, fp);
	fputc((v >> 8) & 0xFF, fp);
	fputc((v >> 16) & 0xFF, fp);
	fputc((v >> 24) & 0xFF, fp);
}

static bool WriteWav(const char* pszPath, const std::vector<int16_t>& samples,
	int nSampleRate)
{
	FILE* fp = fopen(pszPath, "wb");
	if (fp == NULL) {
		fprintf(stderr, "failed to open wav: %s\n", pszPath);
		return false;
	}
	uint32_t nDataBytes = (uint32_t)(samples.size() * sizeof(int16_t));
	fwrite("RIFF", 1, 4, fp);
	WriteLE32(fp, 36 + nDataBytes);
	fwrite("WAVE", 1, 4, fp);
	fwrite("fmt ", 1, 4, fp);
	WriteLE32(fp, 16);
	WriteLE16(fp, 1);                 // PCM
	WriteLE16(fp, 2);                 // stereo
	WriteLE32(fp, (uint32_t)nSampleRate);
	WriteLE32(fp, (uint32_t)nSampleRate * 2 * 2);
	WriteLE16(fp, 2 * 2);
	WriteLE16(fp, 16);
	fwrite("data", 1, 4, fp);
	WriteLE32(fp, nDataBytes);
	if (!samples.empty()) {
		fwrite(&samples[0], sizeof(int16_t), samples.size(), fp);
	}
	fclose(fp);
	return true;
}

static void PrintUsage(const char* pszArg0) {
	fprintf(stderr,
		"usage: %s <log.csv> <out.wav> [options]\n"
		"options:\n"
		"  --rate N             sample rate (default 44100)\n"
		"  --base-clock-mhz N   YM2203 master clock MHz (default 4)\n"
		"  --tail-sec N         render N seconds after last event (default 2)\n"
		"  --fm-only            mute SSG\n"
		"  --ssg-only           mute FM\n"
		"  --fm-ch N            solo FM channel 1..3\n"
		"  --ssg-ch N           solo SSG channel 1..3\n",
		pszArg0);
}

int main(int argc, char** argv) {
	if (argc < 3) {
		PrintUsage(argv[0]);
		return 2;
	}
	// This tool replays logs through CPC88Opna::WriteData(); disable the
	// runtime logger so an inherited X88_OPN_LOG does not truncate or
	// overwrite the source log while rendering.
#ifdef _WIN32
	_putenv("X88_OPN_LOG=");
#else
	unsetenv("X88_OPN_LOG");
#endif
	const char* pszLog = argv[1];
	const char* pszWav = argv[2];
	int nSampleRate = 44100;
	int nBaseClockMhz = 4;
	int nTailSec = 2;
	int nSoloFm = -1;
	int nSoloSsg = -1;
	bool bFmOnly = false;
	bool bSsgOnly = false;

	for (int n = 3; n < argc; n++) {
		if ((strcmp(argv[n], "--rate") == 0) && (n + 1 < argc)) {
			nSampleRate = atoi(argv[++n]);
		} else if ((strcmp(argv[n], "--base-clock-mhz") == 0) && (n + 1 < argc)) {
			nBaseClockMhz = atoi(argv[++n]);
		} else if ((strcmp(argv[n], "--tail-sec") == 0) && (n + 1 < argc)) {
			nTailSec = atoi(argv[++n]);
		} else if (strcmp(argv[n], "--fm-only") == 0) {
			bFmOnly = true;
		} else if (strcmp(argv[n], "--ssg-only") == 0) {
			bSsgOnly = true;
		} else if ((strcmp(argv[n], "--fm-ch") == 0) && (n + 1 < argc)) {
			nSoloFm = atoi(argv[++n]) - 1;
		} else if ((strcmp(argv[n], "--ssg-ch") == 0) && (n + 1 < argc)) {
			nSoloSsg = atoi(argv[++n]) - 1;
		} else {
			PrintUsage(argv[0]);
			return 2;
		}
	}
	if (nSampleRate <= 0) nSampleRate = 44100;
	if (nBaseClockMhz <= 0) nBaseClockMhz = 4;
	if (nTailSec < 0) nTailSec = 0;

	std::vector<SEvent> events;
	if (!LoadLog(pszLog, events)) {
		return 1;
	}

	CPC88Opna::Initialize();
	CPC88Opna::SetBaseClock(nBaseClockMhz);
	CPC88Opna::SetSampleRate(nSampleRate);
	CPC88Opna::SetSampleOutputCallback(OnSamples);
	CPC88Opna::Reset();

	if (bFmOnly) CPC88Opna::SetSsgMute(true);
	if (bSsgOnly) CPC88Opna::SetFmMute(true);
	if ((nSoloFm >= 0) && (nSoloFm < CPC88Opna::FM_CHANNEL_COUNT)) {
		for (int c = 0; c < CPC88Opna::FM_CHANNEL_COUNT; c++) {
			CPC88Opna::SetFmChMute(c, c != nSoloFm);
		}
	}
	if ((nSoloSsg >= 0) && (nSoloSsg < CPC88Opna::SSG_CHANNEL_COUNT)) {
		for (int c = 0; c < CPC88Opna::SSG_CHANNEL_COUNT; c++) {
			CPC88Opna::SetSsgChMute(c, c != nSoloSsg);
		}
	}

	long long nCurrentFrame = 0;
	for (size_t i = 0; i < events.size(); i++) {
		const SEvent& ev = events[i];
		if (ev.nFrame > nCurrentFrame) {
			long long nGap = ev.nFrame - nCurrentFrame;
			while (nGap > 0) {
				int nChunk = (nGap > 4096)? 4096: (int)nGap;
				CPC88Opna::Generate(nChunk);
				nCurrentFrame += nChunk;
				nGap -= nChunk;
			}
		}
		if (ev.chEvent == 'A') {
			CPC88Opna::WriteAddress((uint8_t)ev.nAddress);
		} else {
			CPC88Opna::WriteAddress((uint8_t)ev.nAddress);
			CPC88Opna::WriteData((uint8_t)ev.nData);
		}
	}
	long long nTailFrames = (long long)nTailSec * nSampleRate;
	while (nTailFrames > 0) {
		int nChunk = (nTailFrames > 4096)? 4096: (int)nTailFrames;
		CPC88Opna::Generate(nChunk);
		nTailFrames -= nChunk;
	}

	if (!WriteWav(pszWav, g_samples, nSampleRate)) {
		return 1;
	}
	fprintf(stderr, "wrote %s (%lld frames, %zu events)\n",
		pszWav, (long long)(g_samples.size() / 2), events.size());
	return 0;
}
