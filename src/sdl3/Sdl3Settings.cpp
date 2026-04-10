////////////////////////////////////////////////////////////
// SDL3 frontend persistent settings — implementation

#include "StdHeader.h"
#include "Sdl3Settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

const char CSdl3Settings::SECTION_SDL3[] = "sdl3";

CSdl3Settings::CSdl3Settings() :
	m_bOpen(false)
{
}

CSdl3Settings::~CSdl3Settings()
{
	if (m_bOpen) {
		m_envFile.Close();
		m_bOpen = false;
	}
}

void CSdl3Settings::ResolveFilePath() const
{
	if (!m_strFilePath.empty()) {
		return;
	}
	std::string strDir = ResolveSettingsDirectory();
	if (strDir.empty()) {
		return;
	}
	m_strFilePath = strDir + "/X88000.ini";
}

std::string CSdl3Settings::ResolveSettingsDirectory()
{
#ifdef __APPLE__
	const char* pszHome = getenv("HOME");
	if ((pszHome != NULL) && (*pszHome != '\0')) {
		return std::string(pszHome) + "/Library/Application Support/X88000M";
	}
#else
	const char* pszXdg = getenv("XDG_CONFIG_HOME");
	if ((pszXdg != NULL) && (*pszXdg != '\0')) {
		return std::string(pszXdg) + "/X88000M";
	}
	const char* pszHome = getenv("HOME");
	if ((pszHome != NULL) && (*pszHome != '\0')) {
		return std::string(pszHome) + "/.config/X88000M";
	}
#endif
	return std::string();
}

bool CSdl3Settings::EnsureDirectoryExists(const std::string& strDir)
{
	if (strDir.empty()) {
		return false;
	}
	// Create the path component-by-component so a missing intermediate
	// directory (e.g. ".../Application Support") is also created.
	for (size_t n = 1; n <= strDir.size(); n++) {
		if ((n == strDir.size()) || (strDir[n] == '/')) {
			std::string strSub = strDir.substr(0, n);
			if (strSub.empty()) {
				continue;
			}
			struct stat st;
			if (stat(strSub.c_str(), &st) == 0) {
				if (!S_ISDIR(st.st_mode)) {
					return false;
				}
			} else {
				if (mkdir(strSub.c_str(), 0755) != 0) {
					if (stat(strSub.c_str(), &st) != 0) {
						return false;
					}
					if (!S_ISDIR(st.st_mode)) {
						return false;
					}
				}
			}
		}
	}
	return true;
}

bool CSdl3Settings::Load()
{
	ResolveFilePath();
	if (m_strFilePath.empty()) {
		return false;
	}
	if (m_bOpen) {
		m_envFile.Close();
		m_bOpen = false;
	}
	// Make sure the parent directory exists so the legacy frontend
	// (which is just as picky) and we can both write into it later.
	EnsureDirectoryExists(ResolveSettingsDirectory());
	m_envFile.Open(m_strFilePath);
	m_bOpen = true;
	return true;
}

bool CSdl3Settings::Save()
{
	if (!m_bOpen) {
		return false;
	}
	// Close() flushes pending changes to disk and clears the in-memory
	// line list. Re-open immediately so subsequent Get/Set calls work.
	m_envFile.Close();
	m_bOpen = false;
	m_envFile.Open(m_strFilePath);
	m_bOpen = true;
	return true;
}

void CSdl3Settings::Close()
{
	if (m_bOpen) {
		m_envFile.Close();
		m_bOpen = false;
	}
}

std::string CSdl3Settings::GetSectionString(const std::string& strSection, const std::string& strKey, const std::string& strDefault)
{
	if (!m_bOpen) {
		return strDefault;
	}
	std::string strParam;
	if (!m_envFile.GetEntry(strSection, strKey, strParam)) {
		return strDefault;
	}
	if (strParam.empty()) {
		return strDefault;
	}
	return strParam;
}

std::string CSdl3Settings::GetString(const std::string& strKey, const std::string& strDefault)
{
	return GetSectionString(SECTION_SDL3, strKey, strDefault);
}

int CSdl3Settings::GetInt(const std::string& strKey, int nDefault)
{
	std::string strValue = GetString(strKey, "");
	if (strValue.empty()) {
		return nDefault;
	}
	char* pszEnd = NULL;
	long nParsed = strtol(strValue.c_str(), &pszEnd, 10);
	if ((pszEnd == NULL) || (*pszEnd != '\0')) {
		return nDefault;
	}
	return (int)nParsed;
}

bool CSdl3Settings::GetBool(const std::string& strKey, bool bDefault)
{
	std::string strValue = GetString(strKey, "");
	if (strValue.empty()) {
		return bDefault;
	}
	if ((strValue == "1") || (strValue == "true") || (strValue == "yes") || (strValue == "on")) {
		return true;
	}
	if ((strValue == "0") || (strValue == "false") || (strValue == "no") || (strValue == "off")) {
		return false;
	}
	return bDefault;
}

void CSdl3Settings::SetSectionString(const std::string& strSection, const std::string& strKey, const std::string& strValue)
{
	if (!m_bOpen) {
		return;
	}
	if (strKey.empty()) {
		return;
	}
	m_envFile.SetEntry(strSection, strKey, strValue);
}

void CSdl3Settings::SetString(const std::string& strKey, const std::string& strValue)
{
	SetSectionString(SECTION_SDL3, strKey, strValue);
}

void CSdl3Settings::SetInt(const std::string& strKey, int nValue)
{
	char szBuf[32];
	snprintf(szBuf, sizeof(szBuf), "%d", nValue);
	SetString(strKey, szBuf);
}

void CSdl3Settings::SetBool(const std::string& strKey, bool bValue)
{
	SetString(strKey, bValue? "1": "0");
}

const std::string& CSdl3Settings::GetFilePath() const
{
	ResolveFilePath();
	return m_strFilePath;
}
