#ifndef __CSV_FILE_WRITER_H__
#define __CSV_FILE_WRITER_H__

#include <stdio.h>
#include <string.h>

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifdef __cplusplus

#include <string>
#include <vector>
#include <list>
#include <map>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <locale>

typedef std::string			MBSString;

#ifdef _UNICODE
typedef std::wstring	String;
#else
typedef std::string		String;
#endif

#endif // __cplusplus

#define ZT_MAX_FILE_LINE	20480
#define ZT_MAX_PATH			256

class CSVFileWriter
{
public:
	CSVFileWriter();

	bool			OpenEx( const char* filename );
	bool			Open( const char* filename );
	bool			OpenA( const char* filename );
	bool			Close();
	bool			Flush();
	bool			FlushEx();

	const char*	GetFileName() const;
	bool			IsOpened() const;

	size_t			WriteChar( char val );
	size_t			WriteBoolean( bool val );
	size_t			WriteInt( signed int val );
	size_t			WriteLong( signed long long val );
	size_t			WriteFloat( float val );
	size_t			WriteString( const char* buf, size_t bufSize );

	bool			NextLine();
protected:
	FILE*			mFile;
	String			mName;
	char			mBuf[ZT_MAX_FILE_LINE];
	size_t			mBufIdx;
};

inline CSVFileWriter::CSVFileWriter():mFile(NULL),mBufIdx(0)
{
	memset ( mBuf, 0, sizeof(mBuf) );
}

inline const char* CSVFileWriter::GetFileName() const
{
	return mName.c_str();
}

inline bool CSVFileWriter::IsOpened() const
{
	return (mFile != NULL);
}

#endif
