/** 
 * @file llupdatedownloader.cpp
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <curl/curl.h>
#include "lldir.h"
#include "llfile.h"
#include "llmd5.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llthread.h"
#include "llupdatedownloader.h"


class LLUpdateDownloader::Implementation:
	public LLThread
{
public:
	Implementation(LLUpdateDownloader::Client & client);
	~Implementation();
	void cancel(void);
	void download(LLURI const & uri, std::string const & hash);
	bool isDownloading(void);
	size_t onHeader(void * header, size_t size);
	size_t onBody(void * header, size_t size);
	void resume(void);
	
private:
	bool mCancelled;
	LLUpdateDownloader::Client & mClient;
	CURL * mCurl;
	LLSD mDownloadData;
	llofstream mDownloadStream;
	std::string mDownloadRecordPath;
	
	void initializeCurlGet(std::string const & url, bool processHeader);
	void resumeDownloading(size_t startByte);
	void run(void);
	void startDownloading(LLURI const & uri, std::string const & hash);
	void throwOnCurlError(CURLcode code);
	bool validateDownload(void);

	LOG_CLASS(LLUpdateDownloader::Implementation);
};


namespace {
	class DownloadError:
		public std::runtime_error
	{
	public:
		DownloadError(const char * message):
			std::runtime_error(message)
		{
			; // No op.
		}
	};

		
	const char * gSecondLifeUpdateRecord = "SecondLifeUpdateDownload.xml";
};



// LLUpdateDownloader
//-----------------------------------------------------------------------------



std::string LLUpdateDownloader::downloadMarkerPath(void)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_LOGS, gSecondLifeUpdateRecord);
}


LLUpdateDownloader::LLUpdateDownloader(Client & client):
	mImplementation(new LLUpdateDownloader::Implementation(client))
{
	; // No op.
}


void LLUpdateDownloader::cancel(void)
{
	mImplementation->cancel();
}


void LLUpdateDownloader::download(LLURI const & uri, std::string const & hash)
{
	mImplementation->download(uri, hash);
}


bool LLUpdateDownloader::isDownloading(void)
{
	return mImplementation->isDownloading();
}


void LLUpdateDownloader::resume(void)
{
	mImplementation->resume();
}



// LLUpdateDownloader::Implementation
//-----------------------------------------------------------------------------


namespace {
	size_t write_function(void * data, size_t blockSize, size_t blocks, void * downloader)
	{
		size_t bytes = blockSize * blocks;
		return reinterpret_cast<LLUpdateDownloader::Implementation *>(downloader)->onBody(data, bytes);
	}


	size_t header_function(void * data, size_t blockSize, size_t blocks, void * downloader)
	{
		size_t bytes = blockSize * blocks;
		return reinterpret_cast<LLUpdateDownloader::Implementation *>(downloader)->onHeader(data, bytes);
	}
}


LLUpdateDownloader::Implementation::Implementation(LLUpdateDownloader::Client & client):
	LLThread("LLUpdateDownloader"),
	mCancelled(false),
	mClient(client),
	mCurl(0),
	mDownloadRecordPath(LLUpdateDownloader::downloadMarkerPath())
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL); // Just in case.
	llverify(code == CURLE_OK); // TODO: real error handling here. 
}


LLUpdateDownloader::Implementation::~Implementation()
{
	if(mCurl) curl_easy_cleanup(mCurl);
}


void LLUpdateDownloader::Implementation::cancel(void)
{
	mCancelled = true;
}
	

void LLUpdateDownloader::Implementation::download(LLURI const & uri, std::string const & hash)
{
	if(isDownloading()) mClient.downloadError("download in progress");
	
	mDownloadData = LLSD();
	try {
		startDownloading(uri, hash);
	} catch(DownloadError const & e) {
		mClient.downloadError(e.what());
	}
}


bool LLUpdateDownloader::Implementation::isDownloading(void)
{
	return !isStopped();
}


void LLUpdateDownloader::Implementation::resume(void)
{
	llifstream dataStream(mDownloadRecordPath);
	if(!dataStream) {
		mClient.downloadError("no download marker");
		return;
	}
	
	LLSDSerialize::fromXMLDocument(mDownloadData, dataStream);
	
	if(!mDownloadData.asBoolean()) {
		mClient.downloadError("no download information in marker");
		return;
	}
	
	std::string filePath = mDownloadData["path"].asString();
	try {
		if(LLFile::isfile(filePath)) {		
			llstat fileStatus;
			LLFile::stat(filePath, &fileStatus);
			if(fileStatus.st_size != mDownloadData["size"].asInteger()) {
				resumeDownloading(fileStatus.st_size);
			} else if(!validateDownload()) {
				LLFile::remove(filePath);
				download(LLURI(mDownloadData["url"].asString()), mDownloadData["hash"].asString());
			} else {
				mClient.downloadComplete(mDownloadData);
			}
		} else {
			download(LLURI(mDownloadData["url"].asString()), mDownloadData["hash"].asString());
		}
	} catch(DownloadError & e) {
		mClient.downloadError(e.what());
	}
}


size_t LLUpdateDownloader::Implementation::onHeader(void * buffer, size_t size)
{
	char const * headerPtr = reinterpret_cast<const char *> (buffer);
	std::string header(headerPtr, headerPtr + size);
	size_t colonPosition = header.find(':');
	if(colonPosition == std::string::npos) return size; // HTML response; ignore.
	
	if(header.substr(0, colonPosition) == "Content-Length") {
		try {
			size_t firstDigitPos = header.find_first_of("0123456789", colonPosition);
			size_t lastDigitPos = header.find_last_of("0123456789");
			std::string contentLength = header.substr(firstDigitPos, lastDigitPos - firstDigitPos + 1);
			size_t size = boost::lexical_cast<size_t>(contentLength);
			LL_INFOS("UpdateDownload") << "download size is " << size << LL_ENDL;
			
			mDownloadData["size"] = LLSD(LLSD::Integer(size));
			llofstream odataStream(mDownloadRecordPath);
			LLSDSerialize::toPrettyXML(mDownloadData, odataStream);
		} catch (std::exception const & e) {
			LL_WARNS("UpdateDownload") << "unable to read content length (" 
				<< e.what() << ")" << LL_ENDL;
		}
	} else {
		; // No op.
	}
	
	return size;
}


size_t LLUpdateDownloader::Implementation::onBody(void * buffer, size_t size)
{
	if(mCancelled) return 0; // Forces a write error which will halt curl thread.
	
	mDownloadStream.write(reinterpret_cast<const char *>(buffer), size);
	return size;
}


void LLUpdateDownloader::Implementation::run(void)
{
	CURLcode code = curl_easy_perform(mCurl);
	mDownloadStream.close();
	if(code == CURLE_OK) {
		LLFile::remove(mDownloadRecordPath);
		if(validateDownload()) {
			LL_INFOS("UpdateDownload") << "download successful" << LL_ENDL;
			mClient.downloadComplete(mDownloadData);
		} else {
			LL_INFOS("UpdateDownload") << "download failed hash check" << LL_ENDL;
			std::string filePath = mDownloadData["path"].asString();
			if(filePath.size() != 0) LLFile::remove(filePath);
			mClient.downloadError("failed hash check");
		}
	} else if(mCancelled && (code == CURLE_WRITE_ERROR)) {
		LL_INFOS("UpdateDownload") << "download canceled by user" << LL_ENDL;
		// Do not call back client.
	} else {
		LL_WARNS("UpdateDownload") << "download failed with error '" << 
			curl_easy_strerror(code) << "'" << LL_ENDL;
		LLFile::remove(mDownloadRecordPath);
		mClient.downloadError("curl error");
	}
}


void LLUpdateDownloader::Implementation::initializeCurlGet(std::string const & url, bool processHeader)
{
	if(mCurl == 0) {
		mCurl = curl_easy_init();
	} else {
		curl_easy_reset(mCurl);
	}
	
	if(mCurl == 0) throw DownloadError("failed to initialize curl");
	
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_NOSIGNAL, true));
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_FOLLOWLOCATION, true));
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_WRITEFUNCTION, &write_function));
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_WRITEDATA, this));
	if(processHeader) {
	   throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_HEADERFUNCTION, &header_function));
	   throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_HEADERDATA, this));
	}
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_HTTPGET, true));
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_URL, url.c_str()));
}


void LLUpdateDownloader::Implementation::resumeDownloading(size_t startByte)
{
	initializeCurlGet(mDownloadData["url"].asString(), false);
	
	// The header 'Range: bytes n-' will request the bytes remaining in the
	// source begining with byte n and ending with the last byte.
	boost::format rangeHeaderFormat("Range: bytes=%u-");
	rangeHeaderFormat % startByte;
	curl_slist * headerList = 0;
	headerList = curl_slist_append(headerList, rangeHeaderFormat.str().c_str());
	if(headerList == 0) throw DownloadError("cannot add Range header");
	throwOnCurlError(curl_easy_setopt(mCurl, CURLOPT_HTTPHEADER, headerList));
	curl_slist_free_all(headerList);
	
	mDownloadStream.open(mDownloadData["path"].asString(),
						 std::ios_base::out | std::ios_base::binary | std::ios_base::app);
	start();
}


void LLUpdateDownloader::Implementation::startDownloading(LLURI const & uri, std::string const & hash)
{
	mDownloadData["url"] = uri.asString();
	mDownloadData["hash"] = hash;
	LLSD path = uri.pathArray();
	if(path.size() == 0) throw DownloadError("no file path");
	std::string fileName = path[path.size() - 1].asString();
	std::string filePath = gDirUtilp->getExpandedFilename(LL_PATH_TEMP, fileName);
	mDownloadData["path"] = filePath;

	LL_INFOS("UpdateDownload") << "downloading " << filePath
		<< " from " << uri.asString() << LL_ENDL;
	LL_INFOS("UpdateDownload") << "hash of file is " << hash << LL_ENDL;
		
	llofstream dataStream(mDownloadRecordPath);
	LLSDSerialize::toPrettyXML(mDownloadData, dataStream);
	
	mDownloadStream.open(filePath, std::ios_base::out | std::ios_base::binary);
	initializeCurlGet(uri.asString(), true);
	start();
}


void LLUpdateDownloader::Implementation::throwOnCurlError(CURLcode code)
{
	if(code != CURLE_OK) {
		const char * errorString = curl_easy_strerror(code);
		if(errorString != 0) {
			throw DownloadError(curl_easy_strerror(code));
		} else {
			throw DownloadError("unknown curl error");
		}
	} else {
		; // No op.
	}
}


bool LLUpdateDownloader::Implementation::validateDownload(void)
{
	std::string filePath = mDownloadData["path"].asString();
	llifstream fileStream(filePath, std::ios_base::in | std::ios_base::binary);
	if(!fileStream) return false;

	std::string hash = mDownloadData["hash"].asString();
	if(hash.size() != 0) {
		LL_INFOS("UpdateDownload") << "checking hash..." << LL_ENDL;
		char digest[33];
		LLMD5(fileStream).hex_digest(digest);
		if(hash != digest) {
			LL_WARNS("UpdateDownload") << "download hash mismatch; expeted " << hash <<
				" but download is " << digest << LL_ENDL;
		}
		return hash == digest;
	} else {
		return true; // No hash check provided.
	}
}
