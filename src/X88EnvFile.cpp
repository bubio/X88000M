////////////////////////////////////////////////////////////
// X88000 Environment Settings File (INI reader/writer)
//
// Originally part of X88Option.cpp. Extracted into its own
// translation unit so that frontends other than the legacy
// GTK build can also read and write the same X88000.ini.
//
// Written by Manuke

////////////////////////////////////////////////////////////
// include

#include "StdHeader.h"

#include "X88EnvFile.h"

#include "X88Utility.h"

#include <stdio.h>
#include <string.h>

#ifdef X88_PLATFORM_WINDOWS
#	include <windows.h>
#endif

using namespace NX88Utility;

////////////////////////////////////////////////////////////
// implementation of CX88EnvFile

////////////////////////////////////////////////////////////
// create & destroy

// default constructor

CX88EnvFile::CX88EnvFile() {

#ifdef X88_PLATFORM_UNIX

	m_bDirty = false;

#endif // X88_PLATFORM_UNIX

}

// standard constructor

CX88EnvFile::CX88EnvFile(const std::string& fstrFileName) {

#ifdef X88_PLATFORM_UNIX

	m_bDirty = false;

#endif // X88_PLATFORM_UNIX

	Open(fstrFileName);
}

// destructor

CX88EnvFile::~CX88EnvFile() {
	if (!m_fstrFileName.empty()) {
		Close();
	}
}

////////////////////////////////////////////////////////////
// operation

// open environment file

bool CX88EnvFile::Open(const std::string& fstrFileName) {
	bool bResult = true;
	if (!m_fstrFileName.empty()) {
		Close();
	}
	m_fstrFileName = fstrFileName;

#ifdef X88_PLATFORM_UNIX

	m_bDirty = false;
	FILE* pFile = fopen(m_fstrFileName.c_str(), "r");
	if (pFile != NULL) {
		char lszBuffer[4096];
		while (fgets(lszBuffer, sizeof(lszBuffer), pFile) != NULL) {
			m_listLines.push_back(SEnvLine());
			SEnvLine& line = m_listLines.back();
			line.lstrLine = lszBuffer;
			char* plszTmp = lszBuffer+strspn(lszBuffer, " \t");
			switch (*plszTmp) {
			case '\0':
			case '\n':
				line.nType = SEnvLine::TYPE_BLANK;
				break;
			case ';':
				line.nType = SEnvLine::TYPE_REMARK;
				break;
			case '[':
				{ // dummy block
					plszTmp++;
					plszTmp += strspn(plszTmp, " \t");
					char* plszBegin = plszTmp;
					plszTmp += strcspn(plszTmp, " \t\n]");
					char* plszEnd = plszTmp;
					plszTmp += strspn(plszTmp, " \t");
					if ((*plszTmp == ']') && (plszBegin != plszEnd)) {
						*plszEnd = '\0';
						line.nType = SEnvLine::TYPE_SECTION;
						line.lstrID = plszBegin;
					}
				}
				break;
			default:
				{ // dummy block
					char* plszBegin = plszTmp;
					plszTmp += strcspn(plszTmp, " \t\n=");
					char* plszEnd = plszTmp;
					plszTmp += strspn(plszTmp, " \t");
					if ((*plszTmp == '=') && (plszBegin != plszEnd)) {
						*plszEnd = '\0';
						line.nType = SEnvLine::TYPE_ENTRY;
						line.lstrID = plszBegin;
					}
				}
				break;
			}
		}
		fclose(pFile);
	}

#endif // X88_PLATFORM

	return bResult;
}

// close environment file

bool CX88EnvFile::Close() {
	bool bResult = true;
	if (m_fstrFileName.empty()) {
		bResult = false;
	} else {

#ifdef X88_PLATFORM_UNIX

		if (m_bDirty) {
			FILE* pFile = fopen(m_fstrFileName.c_str(), "w");
			if (pFile == NULL) {
				bResult = false;
			} else {
				for (
					std::list<SEnvLine>::iterator itLine = m_listLines.begin();
					itLine != m_listLines.end();
					itLine++)
				{
					if (fputs((*itLine).lstrLine.c_str(), pFile) == EOF) {
						bResult = false;
						break;
					}
				}
				if (fclose(pFile) != 0) {
					bResult = false;
				}
			}
		}
		m_listLines.clear();
		m_bDirty = false;

#endif // X88_PLATFORM

		m_fstrFileName = "";
	}
	return bResult;
}

// get entry

bool CX88EnvFile::GetEntry(
	const std::string& strSection,
	const std::string& strEntry,
	std::string& strParam)
{
	bool bResult = true;
	std::string lstrSection = ConvUTF8toLOC(strSection),
		lstrEntry = ConvUTF8toLOC(strEntry);

#ifdef X88_PLATFORM_WINDOWS

	if (m_fstrFileName.empty()) {
		bResult = false;
	} else {
		char lszParam[256];
		GetPrivateProfileString(
			lstrSection.c_str(),
			lstrEntry.c_str(),
			"",
			lszParam, sizeof(lszParam),
			m_fstrFileName.c_str());
		lszParam[255] = '\0';
		strParam = ConvLOCtoUTF8(lszParam);
	}

#elif defined(X88_PLATFORM_UNIX)

	if (m_fstrFileName.empty()) {
		bResult = false;
	} else {
		bool bFoundSection = false, bFoundEntry = false;
		std::list<SEnvLine>::iterator itLine;
		for (
			itLine = m_listLines.begin();
			itLine != m_listLines.end();
			itLine++)
		{
			if (!bFoundSection) {
				if ((*itLine).nType == SEnvLine::TYPE_SECTION) {
					if (StrCaseCmp((*itLine).lstrID, lstrSection) == 0) {
						bFoundSection = true;
					}
				}
			} else {
				if ((*itLine).nType == SEnvLine::TYPE_SECTION) {
					break;
				} else if ((*itLine).nType == SEnvLine::TYPE_ENTRY) {
					if (StrCaseCmp((*itLine).lstrID, lstrEntry) == 0) {
						bFoundEntry = true;
						break;
					}
				}
			}
		}
		if (bFoundEntry) {
			const char* plszData = (*itLine).lstrLine.c_str();
			plszData += strcspn(plszData, "=");
			if (*plszData == '=') {
				plszData++;
				plszData += strspn(plszData, " \t");
				int nDataLength = strcspn(plszData, "\n");
				while (
					(nDataLength > 0) &&
					((plszData[nDataLength-1] == ' ') ||
						(plszData[nDataLength-1] == '\t')))
				{
					nDataLength--;
				}
				strParam = ConvLOCtoUTF8(
					std::string(plszData, plszData+nDataLength));
			}
		}
	}

#endif // X88_PLATFORM

	return bResult;
}

// set entry

bool CX88EnvFile::SetEntry(
	const std::string& strSection,
	const std::string& strEntry,
	const std::string& strParam)
{
	bool bResult = true;
	std::string lstrSection = ConvUTF8toLOC(strSection),
		lstrEntry = ConvUTF8toLOC(strEntry),
		lstrParam = ConvUTF8toLOC(strParam);

#ifdef X88_PLATFORM_WINDOWS

	if (m_fstrFileName.empty()) {
		bResult = false;
	} else {
		WritePrivateProfileString(
			lstrSection.c_str(),
			lstrEntry.c_str(),
			lstrParam.c_str(),
			m_fstrFileName.c_str());
	}

#elif defined(X88_PLATFORM_UNIX)

	if (m_fstrFileName.empty()) {
		bResult = false;
	} else {
		bool bFoundSection = false, bFoundEntry = false;
		std::list<SEnvLine>::iterator itLine,
			itInsert = m_listLines.end();
		for (
			itLine = m_listLines.begin();
			itLine != m_listLines.end();
			itLine++)
		{
			if (!bFoundSection) {
				if ((*itLine).nType == SEnvLine::TYPE_SECTION) {
					if (StrCaseCmp((*itLine).lstrID, lstrSection) == 0) {
						bFoundSection = true;
						itInsert = itLine;
						itInsert++;
					}
				}
			} else {
				if ((*itLine).nType == SEnvLine::TYPE_SECTION) {
					break;
				} else if ((*itLine).nType == SEnvLine::TYPE_ENTRY) {
					if (StrCaseCmp((*itLine).lstrID, lstrEntry) == 0) {
						bFoundEntry = true;
						break;
					}
					itInsert = itLine;
					itInsert++;
				}
			}
		}
		if (!bFoundSection) {
			itLine = m_listLines.insert(itInsert, SEnvLine());
			(*itLine).nType = SEnvLine::TYPE_SECTION;
			(*itLine).lstrID = lstrSection;
			(*itLine).lstrLine = FormatStr(
				"[%s]\n", lstrSection.c_str());
			itInsert = itLine;
			itInsert++;
		}
		if (!bFoundEntry) {
			itLine = m_listLines.insert(itInsert, SEnvLine());
			(*itLine).nType = SEnvLine::TYPE_ENTRY;
			(*itLine).lstrID = lstrEntry;
		}
		(*itLine).lstrLine = FormatStr(
			"%s=%s\n", lstrEntry.c_str(), lstrParam.c_str());
		m_bDirty = true;
	}

#endif // X88_PLATFORM

	return bResult;
}
