#include "CSVFileWriter.h"

bool CSVFileWriter::OpenEx( const char* filename )
{
	if ( !filename )
	{
		return false;
	}

	mName = filename;

	mFile = fopen( filename, "a");
	if ( !mFile )
	{
		return false;
	}

	return true;
}

bool CSVFileWriter::Open( const char* filename )
{
	if ( !filename )
	{
		return false;
	}

	mName = filename;

	mFile = fopen( filename, "w+t");
	if ( !mFile )
	{
		return false;
	}

	return true;
}

bool CSVFileWriter::OpenA( const char* filename )
{
	if ( !filename )
	{
		return false;
	}

	mName = filename;

	mFile = fopen( filename, "a+t");
	if ( !mFile )
	{
		return false;
	}

	return true;
}

bool CSVFileWriter::Close()
{
	if ( !mFile )
	{
		return false;
	}

	if ( mBufIdx > 0 )
	{
		Flush();
	}

	fclose( mFile );
	mFile = NULL;
	return true;
}

bool CSVFileWriter::Flush()
{
	return NextLine();
}


bool CSVFileWriter::FlushEx()
{
	if ( mFile )
	{
		fflush(mFile);
		return true;
	}
	return false;
}

size_t CSVFileWriter::WriteChar( char val )
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 )
	{
		return 0;
	}

	mBuf[mBufIdx] = val;
	mBuf[mBufIdx+1] = ',';
	mBufIdx += 2;

	return sizeof(val);
}

size_t CSVFileWriter::WriteBoolean( bool val )
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 )
	{
		return 0;
	}

	char tmp[64];

	unsigned int data = val ? 1:0;
	memset( tmp, 0, sizeof(tmp) );
	sprintf( tmp, "%d,", data );

	size_t dataSize = strlen( tmp );
	if ( dataSize + mBufIdx > ZT_MAX_FILE_LINE-1 )
	{
		return -1;
	}

	strcat( mBuf, tmp );

	mBufIdx += dataSize;

	return strlen( tmp );
}

size_t CSVFileWriter::WriteInt( signed int val )
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 )
	{
		return 0;
	}

	char tmp[64];

	memset( tmp, 0, sizeof(tmp) );
	sprintf( tmp, "%d,", val );

	size_t dataSize = strlen( tmp );
	if ( dataSize + mBufIdx > ZT_MAX_FILE_LINE-1 )
	{
		return -1;
	}

	strcat( mBuf, tmp );
	mBufIdx += dataSize;

	return strlen( tmp );
}

size_t CSVFileWriter::WriteLong( signed long long val )
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 )
	{
		return 0;
	}

	char tmp[64];

	memset( tmp, 0, sizeof(tmp) );
	sprintf( tmp, "%lld,", val );

	size_t dataSize = strlen( tmp );
	if ( dataSize + mBufIdx > ZT_MAX_FILE_LINE-1 )
	{
		return -1;
	}

	strcat( mBuf, tmp );
	mBufIdx += dataSize;

	return strlen( tmp );
}

size_t CSVFileWriter::WriteFloat( float val )
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 )
	{
		return 0;
	}

	char tmp[64];

	memset( tmp, 0, sizeof(tmp) );
	sprintf( tmp, "%f,", val );
	size_t dataSize = strlen( tmp );
	if ( dataSize + mBufIdx > ZT_MAX_FILE_LINE-1 )
	{
		return -1;
	}

	strcat( mBuf, tmp );
	mBufIdx += dataSize;

	return strlen( tmp );
}

size_t CSVFileWriter::WriteString( const char* buf, size_t bufSize )
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 || !buf || bufSize == 0 )
	{
		return 0;
	}

	if ( bufSize + mBufIdx > ZT_MAX_FILE_LINE-1 )
	{
		return -1;
	}

	sprintf( &mBuf[mBufIdx], "%s,", buf );

	size_t strSize = strlen( buf );

	strSize = strSize > bufSize ? bufSize: strSize ;

	mBufIdx += (strSize+1);

	return strSize;
}

bool CSVFileWriter::NextLine()
{
	if ( !mFile || mBufIdx >= ZT_MAX_FILE_LINE-1 )
	{
		return false;
	}

	if ( mBufIdx > 0 && mBuf[mBufIdx-1] == ',' )
	{
		mBuf[mBufIdx-1] = '\0';

		char buf[ZT_MAX_FILE_LINE] = {0};

#ifdef _UNICODE
		wcstombs( buf, mBuf, sizeof(buf) );
#else
		memcpy( buf, mBuf, sizeof(buf) );
#endif

		fprintf( mFile, "%s\n", buf );

		mBufIdx = 0;
		memset( mBuf, 0, sizeof(mBuf) );

		FlushEx();

		return true;
	}

	mBufIdx = 0;
	memset( mBuf, 0, sizeof(mBuf) );

	return false;
}
