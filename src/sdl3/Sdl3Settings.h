////////////////////////////////////////////////////////////
// SDL3 frontend persistent settings
//
// Thin wrapper around CX88EnvFile so the frontend can share a
// single X88000.ini file with the existing core settings.
// Settings created by the SDL3 frontend live in their own [sdl3]
// section to avoid colliding with the legacy [option] section.

#ifndef Sdl3Settings_DEFINED
#define Sdl3Settings_DEFINED

#include <string>

#include "X88EnvFile.h"

class CSdl3Settings {
public:
	// Section name used for entries written by Set*() / read by Get*().
	static const char SECTION_SDL3[];

	CSdl3Settings();
	~CSdl3Settings();

	// Resolve the X88000.ini path and open it. Missing file is OK —
	// the file will be created on Save().
	bool Load();
	// Flush pending changes back to disk. Re-opens the file afterwards
	// so subsequent Get*() / Set*() calls keep working.
	bool Save();
	// Close the underlying env file. After this, Get/Set calls are no-ops
	// and the destructor will not attempt to traverse the list.
	void Close();

	// Typed accessors. Missing keys return the supplied default.
	std::string GetString(const std::string& strKey, const std::string& strDefault = "");
	int         GetInt   (const std::string& strKey, int nDefault = 0);
	bool        GetBool  (const std::string& strKey, bool bDefault = false);

	// Same accessors but explicit section, for sharing legacy [option] keys.
	std::string GetSectionString(const std::string& strSection, const std::string& strKey, const std::string& strDefault = "");

	void SetString(const std::string& strKey, const std::string& strValue);
	void SetInt   (const std::string& strKey, int nValue);
	void SetBool  (const std::string& strKey, bool bValue);

	void SetSectionString(const std::string& strSection, const std::string& strKey, const std::string& strValue);

	// Path of the settings file (X88000.ini), resolved on first call.
	const std::string& GetFilePath() const;

private:
	mutable std::string m_strFilePath;
	CX88EnvFile m_envFile;
	bool m_bOpen;

	void ResolveFilePath() const;
	static std::string ResolveSettingsDirectory();
	static bool EnsureDirectoryExists(const std::string& strDir);
};

#endif // Sdl3Settings_DEFINED
