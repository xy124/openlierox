/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


// File downloading over HTTP
// Created 13/10/07
// By Karel Petranek, Albert Zeyer and Martin Griffin

#ifdef _MSC_VER
#pragma warning(disable: 4786)  // WARNING: identifier XXX was truncated to 255 characters in the debug info
#pragma warning(disable: 4503)  // WARNING: decorated name length exceeded, name was truncated
#endif

#include "StringUtils.h"
#include "FindFile.h"
#include "FileDownload.h"

//
// Download error strings
//
const std::string sDownloadErrors[] =  {
	"No error",
	"No file specified",
	"No destination specified",
	"No servers available",
	"HTTP error",
	"Error while saving"
};

//
// Main thread function
//
int DownloadThreadMain(void *param)
{
	// Check
	if (param == NULL)
		return -1;

	CFileDownloader *owner = (CFileDownloader *)param;
	owner->Lock();
	std::list<CFileDownload> *downloads = owner->GetDownloads();
	owner->Unlock();

	bool downloading = false;
	while (!owner->ShouldBreakThread())  {
		downloading = false;

		owner->Lock();

		// Process the downloads
		for (std::list<CFileDownload>::iterator i = downloads->begin(); i != downloads->end(); i++)  {
			if (i->GetState() == FILEDL_INITIALIZING || i->GetState() == FILEDL_RECEIVING)  {
				i->ProcessDownload();
				downloading = true;
			}
		}

		owner->Unlock();

		// Nothing to download, just sleep
		if (!downloading)
			SDL_Delay(50);
	}

	return 0;
}

//
// Single file download
//

// HINT: Lock() must be called before accessing any class members to ensure thread safety!

//////////////////
// Start the transfer
void CFileDownload::Start(const std::string& filename, const std::string& dest_dir)
{
	// Checks
	if (filename.size() == 0)  {
		SetDlError(FILEDL_ERROR_NO_FILE);
		return;
	}

	if (dest_dir.size() == 0)  {
		SetDlError(FILEDL_ERROR_NO_DEST);
		return;
	}
		
	if (tDownloadServers == NULL)  {
		SetDlError(FILEDL_ERROR_NO_SERVER);
		return;
	}

	// Stop & clear any previous download
	Stop();

	Lock();

	// Fill in the info
	sFileName = filename;
	if (*dest_dir.rbegin() != '/' && *dest_dir.rbegin() != '\\')
		sDestPath = dest_dir + "/" + filename;
	else
		sDestPath = dest_dir + filename;

	// Create the file
	tFile = OpenGameFile(sDestPath, "wb");
	if (tFile == NULL)  {
		SetDlError(FILEDL_ERROR_SAVING);
		Unlock();
		return;
	}

	// Try to download from first server in the list
	iCurrentServer = 0;
	tHttp.RequestData((*tDownloadServers)[iCurrentServer] + filename);

	// Set the state to initializing
	iState = FILEDL_INITIALIZING;

	Unlock();
}

/////////////////
// Stop the transfer and clear everything
void CFileDownload::Stop()
{
	Lock();

	// Cancel the transfer
	if (tHttp.RequestedData())
		tHttp.CancelProcessing();

	// Clear
	sFileName = "";
	sDestPath = "";
	if (tFile)  {
		fclose(tFile);
		remove(GetFullFileName(sDestPath).c_str());
	}
	tFile = NULL;
	iCurrentServer = 0;
	iState = FILEDL_NO_FILE;

	Unlock();

	SetDlError(FILEDL_ERROR_NO_ERROR);
}

/////////////////
// Process the downloading
void CFileDownload::ProcessDownload()
{
	Lock();

	// Check that there has been no error yet
	if (tError.iError != FILEDL_ERROR_NO_ERROR)  {
		Unlock();
		return;
	}

	// Process the HTTP
	int res = tHttp.ProcessRequest();

	Unlock();

	switch (res)  {
	// Still processing
	case HTTP_PROC_PROCESSING:
		Lock();
		iState = FILEDL_RECEIVING;
		Unlock();
		break;

	// Error
	case HTTP_PROC_ERROR:
		// If the file could not be found, try another server
		Lock();
		if (tHttp.GetError().iError == HTTP_FILE_NOT_FOUND)  {
			iCurrentServer++;
			if ((size_t)iCurrentServer < tDownloadServers->size())  {
				tHttp.RequestData((*tDownloadServers)[iCurrentServer] + sFileName);  // Request the file
				iState = FILEDL_INITIALIZING;
				Unlock();
				break;  // Don't set the error
			}
		}
		Unlock();

		SetHttpError(tHttp.GetError());
		Lock();
		iState = FILEDL_ERROR;
		Unlock();
		break;

	// Finished
	case HTTP_PROC_FINISHED:
		Lock();
		iState = FILEDL_FINISHED;

		// Save the data in the file
		if (tFile)  {
			if (fwrite(tHttp.GetData().data(), tHttp.GetData().size(), 1, tFile) != tHttp.GetData().size())
				SetDlError(FILEDL_ERROR_SAVING);
			fclose(tFile);
			tFile = NULL;
		} else {
			SetDlError(FILEDL_ERROR_SAVING);
		}

		Unlock();
		break;
	}

}

/////////////////
// Set downloading error
void CFileDownload::SetDlError(int id)
{
	Lock();
	tError.iError = id;
	tError.sErrorMsg = sDownloadErrors[id];
	tError.tHttpError = tHttp.GetError();
	Unlock();
}

/////////////////
// Set downloading error (HTTP problem)
void CFileDownload::SetHttpError(HttpError err)
{
	Lock();
	tError.iError = FILEDL_ERROR_HTTP;
	tError.sErrorMsg = "HTTP Error: " + err.sErrorMsg;
	tError.tHttpError = err;
	Unlock();
}

/////////////////
// Get the filename (thread safe)
std::string CFileDownload::GetFileName()
{
	Lock();
	std::string res = sFileName;
	Unlock();

	return res;
}

/////////////////
// Get the state (thread safe)
int CFileDownload::GetState()
{
	Lock();
	int res = iState;
	Unlock();

	return res;
}

////////////////
// Get the file downloading progress (thread safe)
byte CFileDownload::GetProgress()
{
	Lock();
	byte res = 0;
	if (tHttp.GetDataLength() != 0)
		res = (byte)MIN(100, tHttp.GetReceivedDataLen() * 100 / tHttp.GetDataLength());
	Unlock();

	return res;
}

////////////////
// Get the download error (thread safe)
DownloadError CFileDownload::GetError()
{
	Lock();
	DownloadError res = tError;
	Unlock();

	return res;
}

//
// File downloader
//

/////////////////
// Constructor
CFileDownloader::CFileDownloader()
{
	// Load the list of available servers
	FILE *fp = OpenGameFile("cfg/downloadservers.txt", "r");
	if (fp)  {
		while (!feof(fp))  {
			std::string server = ReadUntil(fp, '\n');

			// Check (server with less than 3 characters cannot be valid)
			if (server.size() < 3)
				continue;

			// Delete CR character if present
			if (*server.rbegin() == '\r')
				server.erase(server.size() - 1);

			// Trim & add the server
			TrimSpaces(server);
			tDownloadServers.push_back(server);

			// Skip the LF character
			fseek(fp, ftell(fp)+1, SEEK_SET);
		}
	}

	// Initialize the values
	tThread = NULL;
	bBreakThread = false;

	// Create the thread
	tMutex = SDL_CreateMutex();
	tThread = SDL_CreateThread(DownloadThreadMain, this);
}

///////////////
// Destructor
CFileDownloader::~CFileDownloader()
{
	Lock();
	bBreakThread = true;
	Unlock();

	SDL_WaitThread(tThread, NULL);
	SDL_DestroyMutex(tMutex);
}

//////////////
// Add and start a download
void CFileDownloader::StartFileDownload(const std::string& filename, const std::string& dest_dir)
{
	Lock();

	// Add the download
	CFileDownload new_dl = CFileDownload(&tDownloadServers, tDownloads.size());

	tDownloads.push_back(new_dl);

	// Find the newly added download and start it
	for (std::list<CFileDownload>::iterator i = tDownloads.begin(); i != tDownloads.end(); i++)  {
		if (i->GetID() == tDownloads.size()-1)  {
			i->Start(filename, dest_dir);
			break;
		}
	}

	Unlock();

}

/////////////
// Cancel a download
void CFileDownloader::CancelFileDownload(const std::string& filename)
{
	Lock();

	// Find the download and remove it, the destructor will stop the download automatically
	for (std::list<CFileDownload>::iterator i = tDownloads.begin(); i != tDownloads.end(); i++)
		if (i->GetFileName() == filename)  {
			tDownloads.erase(i);
			break;
		}

	Unlock();
}

//////////////
// Returns true if the file has been successfully downloaded
bool CFileDownloader::IsFileDownloaded(const std::string& filename)
{
	Lock();

	for (std::list<CFileDownload>::iterator i = tDownloads.begin(); i != tDownloads.end(); i++)
		if (i->GetFileName() == filename)  {
			Unlock();
			return i->GetState() == FILEDL_FINISHED;
		}

	Unlock();

	return false;
}

//////////////
// Returns the download error for the specified file
DownloadError CFileDownloader::FileDownloadError(const std::string& filename)
{
	Lock();

	for (std::list<CFileDownload>::iterator i = tDownloads.begin(); i != tDownloads.end(); i++)
		if (i->GetFileName() == filename)  {
			Unlock();
			return i->GetError();
		}

	Unlock();

	// Not found, create a default "no error"
	DownloadError err;
	err.iError = FILEDL_ERROR_NO_ERROR;
	err.sErrorMsg = sDownloadErrors[FILEDL_ERROR_NO_ERROR];
	err.tHttpError.iError = HTTP_NO_ERROR;
	err.tHttpError.sErrorMsg = "No error";
	return err;
}

///////////////
// Get the file download progress in percents
byte CFileDownloader::GetFileProgress(const std::string& filename)
{
	Lock();

	for (std::list<CFileDownload>::iterator i = tDownloads.begin(); i != tDownloads.end(); i++)
		if (i->GetFileName() == filename)  {
			Unlock();
			return i->GetProgress();
		}

	Unlock();

	return 0;
}

////////////////
// Returns break thread signal
bool CFileDownloader::ShouldBreakThread()
{
	Lock();
	bool res = bBreakThread;
	Unlock();
	return res;
}



void CFileDownloaderInGame::setFileToSend( const std::string & name, const std::string & data )
{
	tState = SEND;
	iPos = 0;
	sFilename = name;
	Compress( sFilename + '\0' + data, &sData );
	printf("CFileDownloaderInGame::setFileToSend() filename %s data.size() %i compressed %i\n", sFilename.c_str(), data.size(), sData.size() );
};

void CFileDownloaderInGame::setFileToSend( const std::string & path )
{
	tState = SEND;
	iPos = 0;
	sFilename = path;

	FILE * ff = OpenGameFile( sFilename , "r" );
	if( ff == NULL )
	{
		tState = ERROR;
		return;
	};
	char buf[16384];
	std::string data = "";
	
	while( ! feof( ff ) )
	{
		int readed = fread( buf, 1, sizeof(buf), ff );
		data.append( buf, readed );
	};
	fclose( ff );

	Compress( sFilename + '\0' + data, &sData );
};

bool CFileDownloaderInGame::receive( CBytestream * bs )
{
	if( tState == FINISHED )
	{
		tState = RECEIVE;
		iPos = 0;
		sFilename = "";
		sData = "";
	};
	if( tState != RECEIVE )
	{
		tState = ERROR;
		return true;	// Receive finished (due to error)
	};
	uint chunkSize = bs->readByte();
	if( chunkSize == 0 )
	{
		tState = ERROR;
		if( Decompress( sData, &sFilename ) )
		{
			tState = FINISHED;
			std::string::size_type f = sFilename.find('\0');
			if( f == std::string::npos )
				tState = ERROR;
			else
			{
				sData.assign( sFilename, f+1, sFilename.size() - (f+1) );
				sFilename.resize( f );
				printf("CFileDownloaderInGame::receive() filename %s sData.size() %i\n", sFilename.c_str(), sData.size());
			};
		};
		return true;	// Receive finished
	};
	sData.append( bs->readData(chunkSize) );
	return false;
};

bool CFileDownloaderInGame::send( CBytestream * bs )
{
	if( tState != SEND )
	{
		tState = ERROR;
		return true;	// Send finished (due to error)
	};
	uint chunkSize = MIN( sData.size() - iPos, 255 );
	bs->writeByte( chunkSize );
	if( chunkSize == 0 )
	{
		tState = FINISHED;
		return true;	// Send finished
	};
	bs->writeData( sData.substr( iPos, chunkSize ) );
	iPos += chunkSize;
	return false;
};


