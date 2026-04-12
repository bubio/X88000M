////////////////////////////////////////////////////////////
// X88000 Environment Settings File (INI reader/writer)
//
// Originally part of X88Option.h. Extracted into its own
// translation unit so that multiple frontends can also read
// and write the same X88000.ini
// without pulling in the option-processing classes.
//
// Written by Manuke

#ifndef X88EnvFile_DEFINED
#define X88EnvFile_DEFINED

#include <list>
#include <string>

////////////////////////////////////////////////////////////
// declaration of CX88EnvFile

class CX88EnvFile {
// struct
protected:

#ifdef X88_PLATFORM_UNIX

	// declaration and implementation of SEnvLine
	struct SEnvLine {
	// enum
	public:
		// kind
		enum {
			TYPE_UNKNOWN,
			TYPE_BLANK,
			TYPE_REMARK,
			TYPE_SECTION,
			TYPE_ENTRY
		};
	// attribute
	public:
		// kind
		int nType;
		// line buffer(locale encoding)
		std::string lstrLine;
		// id(locale encoding)
		std::string lstrID;
	// create & destroy
	public:
		// default constructor
		SEnvLine() :
			nType(TYPE_UNKNOWN)
		{
		}
		// destructor
		~SEnvLine() {
		}
	};

#endif // X88_PLATFORM_UNIX

// attribute
protected:
	// file name(filesystem encoding)
	std::string m_fstrFileName;

#ifdef X88_PLATFORM_UNIX

	// line
	std::list<SEnvLine> m_listLines;
	// dirty
	bool m_bDirty;

#endif // X88_PLATFORM_UNIX

public:
	// get file name(filesystem encoding)
	std::string GetFileName() const {
		return m_fstrFileName;
	}

// create & destroy
public:
	// default constructor
	CX88EnvFile();
	// standard constructor
	CX88EnvFile(const std::string& fstrFileName);
	// destructor
	virtual ~CX88EnvFile();

// operation
public:
	// open environment file
	virtual bool Open(const std::string& fstrFileName);
	// close environment file
	virtual bool Close();
	// get entry
	virtual bool GetEntry(
		const std::string& strSection,
		const std::string& strEntry,
		std::string& strParam);
	// set entry
	virtual bool SetEntry(
		const std::string& strSection,
		const std::string& strEntry,
		const std::string& strParam);
};

#endif // X88EnvFile_DEFINED
