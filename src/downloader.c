#include <assert.h>
#include "wincommon.h"
#include "downloader.h"
#include "sync.h"
#include <Shlobj.h>

#include "alphalobby.h"
#include "battleroom.h"
#include "client_message.h"
#include "settings.h"
#include "data.h"
#include <winhttp.h>
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include "gzip.h"
#include "md5.h"



//determines minimum resolution of progressbar
// values below ~16KB will degrade throughput.
#define CHUNK_SIZE (64 * 1024) 

//Used with with rapid streamer.
//More concurrent requests improves throughput to a point, has some overhead.
#define MAX_REQUESTS 10
#define MIN_REQUEST_SIZE (512 * 1024) 

enum DLG_ID {
	DLG_PROGRESS,
	DLG_PROGRESS_BUTTON_,
	DLG_TAB_BUTTON,
	DLG_LAST = DLG_TAB_BUTTON,
};

static const DialogItem dlgItems[] = {
	[DLG_PROGRESS] = {
		.class = PROGRESS_CLASS,
		.style = WS_VISIBLE | PBS_MARQUEE,
	}, [DLG_PROGRESS_BUTTON_] = {
		.class = WC_BUTTON,
		.name = L"Cancel",
		.style = WS_VISIBLE | BS_PUSHBUTTON,
	}
};

typedef enum DownloadStatus {
	DL_INACTIVE               = 0x00,
	DL_ACTIVE                 = 0x01,
	DL_MAP                    = 0x02,
	
	DL_ONLY_ALLOW_ONE_REQUEST = 0x04,
	DL_HAVE_ONE_REQUEST       = 0x08,
	DL_DONT_ALLOW_MASK        = DL_ONLY_ALLOW_ONE_REQUEST | DL_HAVE_ONE_REQUEST,
	
	DL_HAVE_PACKAGE_MASK      = 0x10,
}DownloadStatus;


typedef struct RequestContext {
	struct SessionContext *ses;
	struct ConnectContext *con;
	void (*onFinish)(struct RequestContext *);
	uint8_t *buffer;
	size_t contentLength;
	size_t fetchedBytes;
	union {
		wchar_t wcharParam[0];
		uint8_t md5Param[0][MD5_LENGTH];
		char charParam[0];
	};
}RequestContext;

typedef struct ConnectContext {
	HINTERNET handle;
	struct SessionContext *ses;
	LONG requests;
}ConnectContext;

typedef struct SessionContext {
	intptr_t status;
	HINTERNET handle;
	LONG requests;
	wchar_t name[MAX_PATH], packagePath[MAX_PATH];
	uint8_t totalFiles, currentFiles, fetchedFiles;
	size_t fetchedBytes, totalBytes;
	DWORD startTime;
	HWND progressBar, button;
	void *packageBytes; size_t packageLen;
	char *error;
}SessionContext;

static SessionContext sessions[10];
int nbDownloads;


#define THREAD_ONFINISH_FLAG 0x80000000
#define DEFINE_THREADED(_func) ((typeof(_func) *)((intptr_t)_func | THREAD_ONFINISH_FLAG))

static void _handleStream(RequestContext *req);
#define handleStream DEFINE_THREADED(_handleStream)
static ConnectContext *  makeConnect(SessionContext *, const wchar_t *domain);
static void downloadPackage(SessionContext *ses);
static void downloadFile(SessionContext *ses);
static void _handlePackage(RequestContext *req);
#define handlePackage DEFINE_THREADED(_handlePackage)
static void handleRepoList(RequestContext *req);
static void handleRepo(RequestContext *req);

void EndDownload(SessionContext *ses)
{
	if (!ses->status)
		return;
	ses->error = NULL;
	WinHttpCloseHandle(ses->handle);
}

__attribute__((always_inline, optimize(("O3"))))
static void getPathFromMD5(uint8_t *md5, wchar_t *path)
{
	static const char conv[] = "0123456789abcdef";
	((uint32_t *)path)[0] = 'p' | 'o' << 16;
	((uint32_t *)path)[1] = 'o' | 'l' << 16;
	((uint32_t *)path)[2] = '/' | conv[(md5[0] & 0xF0) >> 4] << 16;
	((uint32_t *)path)[3] = conv[md5[0] & 0x0F] | '/' << 16;
	for (int i=1; i<16; ++i)
		((uint32_t *)path)[3+i] = conv[md5[i] & 0x0F] << 16 | conv[(md5[i] & 0xF0) >> 4];
	((uint32_t *)path)[19] = '.' | 'g' << 16;
	((uint32_t *)path)[20] = 'z';
}

static void writeFile(wchar_t *path, const void *buffer, size_t len)
{
	wchar_t tmpPath[MAX_PATH];
	GetTempPath(lengthof(tmpPath), tmpPath);
	GetTempFileName(tmpPath, NULL, 0, tmpPath);
	HANDLE file = CreateFile(tmpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	DWORD unused;
	WriteFile(file, buffer, len, &unused, NULL);
	CloseHandle(file);
	MoveFileEx(tmpPath, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

static void initializeProgressBar(SessionContext *ses)
{
	HWND progressBar = ses->progressBar;
	SetWindowLongPtr(progressBar, GWL_STYLE, GetWindowLong(progressBar, GWL_STYLE) & ~PBS_MARQUEE);
	SendMessage(progressBar, PBM_SETRANGE32, 0, ses->totalBytes);
}

DWORD WINAPI exeucuteOnFinishHelper(HINTERNET requestHandle)
{
	puts("exeucuteOnFinishHelper 1");
	RequestContext *req; DWORD reqSize = sizeof(req);
	WinHttpQueryOption(requestHandle, WINHTTP_OPTION_CONTEXT_VALUE, &req, &reqSize);
	((typeof(req->onFinish))((intptr_t)req->onFinish & ~THREAD_ONFINISH_FLAG))(req);
	WinHttpCloseHandle(requestHandle);
	puts("exeucuteOnFinishHelper 2");
	return 0;
}


void CALLBACK callback(HINTERNET hRequest, RequestContext *req,
                              DWORD dwInternetStatus,
                              LPVOID lpvStatusInformation,
                              DWORD dwStatusInformationLength)
{
	printf("dwInternetStatus = %d %x\n", dwInternetStatus, dwInternetStatus);
	if (*(int *)req & 1) {	//Magic number means its actually a session, handles are aligned on DWORD borders
		SessionContext *ses = (void *)req;
		if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
			puts("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING 1");
			if (ses->handle != hRequest)
				return;
			puts("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING 2");

			free(ses->packageBytes);
			--nbDownloads;
			ses->status = 0;
			SendMessage(gMainWindow, WM_DESTROY_WINDOW, 0, (LPARAM)ses->progressBar);
			SendMessage(gMainWindow, WM_DESTROY_WINDOW, 0, (LPARAM)ses->button);
			UpdateStatusBar();
			ReloadMapsAndMod();
			if (ses->error)
				MyMessageBox("Download Failed", ses->error);
			puts("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING 3");
		}
		return;
	}
			

	switch (dwInternetStatus) {
		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
			puts("WINHTTP_CALLBACK_STATUS_READ_COMPLETE 1");
			if (dwStatusInformationLength <= 0) {
				puts("WINHTTP_CALLBACK_STATUS_READ_COMPLETE 3");
				if ((intptr_t)req->onFinish & THREAD_ONFINISH_FLAG)
					CreateThread(NULL, 1, exeucuteOnFinishHelper, hRequest, 0, 0);
				else {
					if (req->onFinish)
						req->onFinish(req);
					WinHttpCloseHandle(hRequest);
				}
				puts("WINHTTP_CALLBACK_STATUS_READ_COMPLETE 4");
				return;
			}
			
			if (req->ses->totalBytes) {
				req->ses->fetchedBytes += dwStatusInformationLength;
				SendMessage(req->ses->progressBar, PBM_SETPOS, req->ses->fetchedBytes, 0);
				UpdateStatusBar();
			}
			
			req->fetchedBytes += dwStatusInformationLength;
			read_data:
			WinHttpReadData(hRequest, req->buffer + req->fetchedBytes, min(req->contentLength - req->fetchedBytes, CHUNK_SIZE), NULL);
			
			puts("WINHTTP_CALLBACK_STATUS_READ_COMPLETE 2");
			break;
		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
			wchar_t buff[128]; DWORD buffSize = sizeof(buff);
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, buff, &buffSize, WINHTTP_NO_HEADER_INDEX);
			if (memcmp(buff, L"text/html", sizeof(L"text/html") - sizeof(wchar_t)) && (__sync_fetch_and_or(&req->ses->status, DL_HAVE_ONE_REQUEST) & DL_DONT_ALLOW_MASK) != DL_DONT_ALLOW_MASK) {
				buffSize = sizeof(req->contentLength);
				WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &req->contentLength, &buffSize, WINHTTP_NO_HEADER_INDEX);
				if (!req->contentLength) {
					assert(0);
					goto closeHandle;
				}
				req->buffer = malloc(req->contentLength);

				if (req->ses->totalFiles) {
					req->ses->totalBytes += req->contentLength;
					++req->ses->currentFiles;
					if (req->ses->totalFiles == req->ses->currentFiles)
						initializeProgressBar(req->ses);
				}
				goto read_data;
			}
		}	// FALLTHROUGH:
		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: closeHandle:
			puts("WINHTTP_CALLBACK_STATUS_REQUEST_ERROR 1");
			WinHttpCloseHandle(hRequest);
			puts("WINHTTP_CALLBACK_STATUS_REQUEST_ERROR 2");
			return;
		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
			WinHttpReceiveResponse(hRequest, NULL);
			return;
		case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
			puts("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING 1");
			if (!--req->con->requests) {
				WinHttpCloseHandle(req->con->handle);
				free(req->con);
			}

			if (!--req->ses->requests)
				WinHttpCloseHandle(req->ses->handle);

			free(req->buffer);
			free(req);
			puts("WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING 2");
			return;
	}
}

static ConnectContext * makeConnect(SessionContext *ses, const wchar_t *domain)
{
	ConnectContext *con = malloc(sizeof(ConnectContext));
	*con = (ConnectContext){WinHttpConnect(ses->handle, domain, INTERNET_DEFAULT_HTTP_PORT, 0), ses};
	assert(con->handle);
	return con;
}

static void sendRequest(RequestContext *req, const wchar_t *objectName)
{
	++req->con->requests;
	++req->ses->requests;
	HINTERNET handle = WinHttpOpenRequest(req->con->handle, NULL, objectName, NULL, WINHTTP_NO_REFERER, NULL, 0);
	WinHttpSendRequest(handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, (DWORD_PTR)req);
}

static void saveFile(RequestContext *req)
{	
	writeFile(req->wcharParam, req->buffer, req->contentLength);
	req->ses->error = NULL;
}

static const char messageTemplateStart[] =
R"(<?xml version="1.0" encoding="utf-8"?><soap12:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:soap12="http://www.w3.org/2003/05/soap-envelope"><soap12:Body>    <DownloadFile xmlns="http://tempuri.org/"><internalName>)";
static const char messageTemplateEnd[] = R"(</internalName></DownloadFile></soap12:Body></soap12:Envelope>)";

void handleMapSources(RequestContext *req)
//NB: this function is stupidly dangerous
{
	req->ses->error = "Couldn't find a download source.";
	
	req->buffer[req->contentLength - 1] = 0;
	
	req->ses->status = (req->ses->status & ~DL_DONT_ALLOW_MASK) | DL_ONLY_ALLOW_ONE_REQUEST;
	char *s = strstr((char *)req->buffer, "<links>");
	if (!s)
		return;
	*strstr(s, "</links>") = 0;

	while ((s = strstr(s, "<string>"))) {
		s += sizeof("<string>") - 1;
		char *end = strchr(s, '<');
		*end = 0;
		wchar_t path[256], host[256];
		if (FALSE == WinHttpCrackUrl(utf8to16(s), 0, ICU_DECODE, &(URL_COMPONENTS){
				.dwStructSize = sizeof(URL_COMPONENTS),
				.lpszHostName = host, .dwHostNameLength = lengthof(host),
				.lpszUrlPath = path, .dwUrlPathLength = lengthof(path)}))
			continue;
		wchar_t *name = wcsrchr(path, '/');
		size_t size = sizeof(wchar_t) * (wcslen(name) + gWritableDataDirectoryLen + lengthof(L"maps"));
		
		RequestContext *req2 = calloc(1, sizeof(*req2) + size);
		swprintf(req2->wcharParam, L"%.*s%s%s", gWritableDataDirectoryLen, gWritableDataDirectory, req->ses->status & DL_MAP ? L"maps" : L"mods", name);
		req2->ses = req->ses;
		req2->con = makeConnect(req->ses, host);
		
		req2->onFinish = saveFile;
		
		sendRequest(req2, path);
		req->ses->startTime = GetTickCount();
		req->ses->totalFiles = 1;
		s = end+1;
	}
}

void ForEachDownload(void (*func)(HWND, HWND, const wchar_t *))
{
	FOR_EACH(s, sessions) {
		if (!s->status)
			continue;
		wchar_t *text;
		if (s->fetchedBytes && s->totalFiles == s->currentFiles) {
			SendMessage(s->progressBar, PBM_SETPOS, s->fetchedBytes, 0);
			DWORD eta = (uint64_t)(GetTickCount() - s->startTime) * (s->totalBytes - s->fetchedBytes) / s->fetchedBytes / 1000;
			swprintf((text = alloca(128)), L"%s - %ld:%02ld remaining - %.2fMB of %.2fMB - %.2fKB/s", s->name, eta / 60, eta % 60, s->fetchedBytes * 1E-6f, s->totalBytes * 1E-6f, (float)s->fetchedBytes / (GetTickCount() - s->startTime + 1));
		} else 
			text = s->name;
		func(s->progressBar, s->button, text);
	}
}


static void downloadPackage(SessionContext *ses)
{
	ses->error = "Couldn't retreive list of repositories from \"repos.caspring.org\".";
	
	//Check cached repos for files:
	//saves approximately 2 seconds
	WIN32_FIND_DATA findFileData;
	struct {
		RequestContext req;
		wchar_t _extra[MAX_PATH];
	} req = {{.ses = ses}};
	wchar_t *pathEnd = req.req.wcharParam + swprintf(req.req.wcharParam, L"%.*srepos\\*", gWritableDataDirectoryLen, gWritableDataDirectory) - 1;
	
	HANDLE find = FindFirstFile(req.req.wcharParam, &findFileData);
	do {
		if (findFileData.cFileName[0] == L'.')
			continue;
		swprintf(pathEnd, L"%s\\versions.gz", findFileData.cFileName);
		HANDLE file = CreateFile(req.req.wcharParam, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (file != INVALID_HANDLE_VALUE) {
			DWORD fileSize = GetFileSize(file, NULL);
			if (fileSize != INVALID_FILE_SIZE) {
				uint8_t buffer[fileSize];
				if (ReadFile(file, buffer, fileSize, (void *)&req.req.contentLength, NULL)) {
					req.req.buffer = buffer;
					handleRepo(&req.req);
				}
			}
			CloseHandle(file);
		}
	} while (FindNextFile(find, &findFileData));
	FindClose(find);

	//Cached repos don't contain package, download master repo list:
	if (!ses->requests) {
		RequestContext *req2 = malloc(sizeof(RequestContext));
	
		*req2 = (RequestContext){
			.ses = ses,
			.con = makeConnect(ses, L"repos.caspring.org"),
			.onFinish = handleRepoList,
		};
		sendRequest(req2, L"repos.gz");
	}
}

static void downloadFile(SessionContext *ses)
{
	ConnectContext * con = makeConnect(ses, L"zero-k.info");
	HINTERNET handle = WinHttpOpenRequest(con->handle, L"POST", L"ContentService.asmx", NULL, WINHTTP_NO_REFERER, NULL, 0);

	RequestContext *req = calloc(1, sizeof(RequestContext) + sizeof(messageTemplateStart) + sizeof(messageTemplateEnd) + MAX_TITLE);
	req->onFinish = handleMapSources;
	req->ses = ses;
	ses->error = "Couldn't retrieve download sources from to \"http://zero-k.info\".";
	req->con = con;
	++ses->requests;
	++con->requests;
	size_t messageLength = sprintf(req->charParam, "%s%ls%s", messageTemplateStart, ses->name, messageTemplateEnd);
	WinHttpSendRequest(handle, L"Content-Type: application/soap+xml; charset=utf-8", -1, req->charParam, messageLength, messageLength, (DWORD_PTR)req);
}

void DownloadFile(const char *name, int isMap)
{
	SessionContext *ses = NULL;
	wchar_t buff[128];
	swprintf(buff, L"%hs", name);
	FOR_EACH(s, sessions) {
		if (s->status && !wcscmp(s->name, buff))
			return;
		ses = ses ?: !s->status ? s : NULL;
	}
	if (!ses)
		return;

	++nbDownloads;
	
	HINTERNET handle = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
	
	*ses = (SessionContext){
		.status = DL_ACTIVE | isMap * DL_MAP,
		.progressBar = (HWND)SendMessage(gMainWindow, WM_CREATE_DLG_ITEM, DLG_PROGRESS_BAR, (LPARAM)&dlgItems[DLG_PROGRESS]),
		.button = (HWND)SendMessage(gMainWindow, WM_CREATE_DLG_ITEM, DLG_PROGRESS_BUTTON, (LPARAM)&dlgItems[DLG_PROGRESS_BUTTON_]),
		.handle = handle,
	};
	swprintf(ses->name, L"%hs", name);
	SetWindowLongPtr(ses->button, GWLP_USERDATA, (LONG_PTR)ses);
	SendMessage(ses->progressBar, PBM_SETMARQUEE, 1, 0);
	UpdateStatusBar();
	
	WinHttpSetOption(handle, WINHTTP_OPTION_CONTEXT_VALUE, &ses, sizeof(ses));
	
	WinHttpSetStatusCallback(handle, (void *)callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES | WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);
	if (isMap)
		downloadFile(ses);
	else
		downloadPackage(ses);
		
	//This can take 200ms or so (if all have to be created), do now while requests are in progress:
	//Unsafe ok since this function called from main thread;
	CreateDirectory(GetWritablePath_unsafe(L"maps"), NULL);
	CreateDirectory(GetWritablePath_unsafe(L"mods"), NULL);
	CreateDirectory(GetWritablePath_unsafe(L"packages"), NULL);
	CreateDirectory(GetWritablePath_unsafe(L"pool"), NULL);
	for (int i=0; i<256; ++i) {
		wchar_t path[MAX_PATH];
		swprintf(path, GetWritablePath_unsafe(L"pool/%02x"), i);
		CreateDirectory(path, NULL);
	}
}

static void _handleStream(RequestContext *req)
{
	wchar_t path[MAX_PATH];
	memcpy(path, gWritableDataDirectory, gWritableDataDirectoryLen * sizeof(wchar_t));
	
	for (int i=0, j=0; i<req->contentLength;++j) {
		size_t fileSize = ntohl(*(uint32_t *)&req->buffer[i]);
		getPathFromMD5(req->md5Param[j], path + gWritableDataDirectoryLen);
		writeFile(path, req->buffer + i + 4, fileSize);
		i += fileSize + 4;
	}
	
	if (req->ses->totalFiles == ++req->ses->fetchedFiles) {
		wchar_t path[MAX_PATH];
		GetWritablePath(req->ses->packagePath, path);
		writeFile(path, req->ses->packageBytes, req->ses->packageLen);
		req->ses->error = NULL;
	}
}

static void _handlePackage(RequestContext *req)
{
	req->ses->startTime = GetTickCount();
	req->ses->error = "Couldn't retrieve all required files for target.";
	__attribute__((packed))
	struct fileData {
		union {
			uint8_t md5[MD5_LENGTH];
			uint32_t firstInt;
		};
		uint32_t crc32, fileSizeNBO;
	};
	req->ses->totalFiles = 1;
	
	ALLOC_AND_INFLATE_GZIP(package, req->buffer, req->contentLength);

	req->ses->packageBytes = req->buffer;
	req->ses->packageLen = req->contentLength;
	req->buffer = NULL;
	
	size_t nbFilesInPackage = 0;
	for (int i = 0; i < sizeof(package); ) {
		i += 1 + package[i] + sizeof(struct fileData);
		++nbFilesInPackage;
	}

	union {
		uint32_t firstInt;
		uint8_t md5[MD5_LENGTH];
	} *checkSums = malloc(nbFilesInPackage * sizeof(*checkSums));
	
	uint8_t (*bitArrays)[(nbFilesInPackage + 7) / 8] = calloc((nbFilesInPackage + 7) / 8, MAX_REQUESTS);
	size_t bitArraySizes[MAX_REQUESTS] = {};
	size_t bitArrayNumber[MAX_REQUESTS] = {};
	RequestContext *requestContexts[MAX_REQUESTS] = {};
	
	wchar_t path[MAX_PATH];
	size_t pathLen = gWritableDataDirectoryLen;
	memcpy(path, gWritableDataDirectory, pathLen * sizeof(wchar_t));
	
	wchar_t objectName[lengthof(L"streamer.cgi?") + 32] = L"streamer.cgi?";
	memcpy(objectName + lengthof(L"streamer.cgi?") - 1, req->ses->packagePath + lengthof(L"packages/") - 1, 32 * sizeof(wchar_t));
	
	
	#ifndef NDEBUG
		FILE *md5CheckFD = tmpfile();
	#endif
	
	void dispatchRequest(int i)
	{	
		++req->ses->totalFiles;
		++req->ses->requests;
		++req->con->requests;
		
		RequestContext *newReq = requestContexts[i];
		
		*newReq = (RequestContext) {
			.ses = req->ses,
			.con = req->con,
			.onFinish = handleStream,
		};
		
		size_t len = (nbFilesInPackage + 7) / 8;
		newReq->buffer = DeflateGzip(bitArrays[i], &len);
		
		HINTERNET handle = WinHttpOpenRequest(newReq->con->handle, L"POST", objectName, NULL, WINHTTP_NO_REFERER, NULL, 0);
		WinHttpSendRequest(handle, NULL, 0, (void *)newReq->buffer, len, len, (DWORD_PTR)newReq);

		memset(bitArrays[i], 0, sizeof(*bitArrays));
		bitArraySizes[i] = 0;
		bitArrayNumber[i] = 0;
		requestContexts[i] = NULL;
	}

	
	int fileIndex = 0, nbFilesToFetch = 0;

	for (char *p=package; p < package + sizeof(package);++fileIndex) {
		struct fileData *fileData = (void *)(p + 1 + *p);
		
		#ifndef NDEBUG
			fwrite(GetMD5Sum_unsafe(p+1, *p), 1, MD5_LENGTH, md5CheckFD);
			fwrite(fileData->md5, 1, MD5_LENGTH, md5CheckFD);
		#endif
		
		p = (void *)(fileData + 1);
		
		for (int i=0; i<nbFilesToFetch; ++i)
			if (__builtin_expect(checkSums[i].firstInt == fileData->firstInt, 0)
					&& !memcmp(checkSums[i].md5, fileData->md5, MD5_LENGTH))
				goto double_continue;
		//This is expensive operation, on 5000 file package takes me 20s. (from fresh boot on hdd)
		getPathFromMD5(fileData->md5, path+pathLen);
		if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
			continue;

		memcpy(checkSums[nbFilesToFetch].md5, fileData->md5, MD5_LENGTH);
		size_t fileSize = ntohl(fileData->fileSizeNBO);
		
		int requestIndex;
		//handlePackage function uses 1 request, so checking for < 2 _real_ requests.
		//if so, rush largest request out so we dont have idle network.
		if (req->ses->requests < 3) {
			requestIndex = 0;
			for (int i=1; i<MAX_REQUESTS; ++i)
				requestIndex = bitArraySizes[i] > bitArraySizes[requestIndex] ? i : requestIndex;
			goto done;
		}
		
		//Don't deprive downloading thread of too much cpu time
		Sleep(0);
		
		//Have some requests in progress, so take time and balance buckets as much as possible:
		for (requestIndex=0; requestIndex<MAX_REQUESTS; ++requestIndex)
			if (bitArraySizes[requestIndex] < MIN_REQUEST_SIZE)
				goto done;
		requestIndex=0;
		for (int j=1; j<MAX_REQUESTS; ++j)
			requestIndex = bitArraySizes[j] < bitArraySizes[requestIndex] ? j : requestIndex;

		done:

		bitArrays[requestIndex][fileIndex/8] |= 1 << fileIndex % 8;
		bitArraySizes[requestIndex] += fileSize;
		
		if (!requestContexts[requestIndex])
			requestContexts[requestIndex] = malloc(sizeof(*requestContexts) + MD5_LENGTH * nbFilesInPackage);
		memcpy(requestContexts[requestIndex]->md5Param[bitArrayNumber[requestIndex]++], fileData->md5, MD5_LENGTH);
		++nbFilesToFetch;
		
		if (req->ses->requests < 3) {
			int maxSizeIndex = 0;
			for (int i=1; i<MAX_REQUESTS; ++i)
				maxSizeIndex = bitArraySizes[i] > bitArraySizes[maxSizeIndex] ? i : maxSizeIndex;
			if (bitArraySizes[maxSizeIndex] > MIN_REQUEST_SIZE) {
				dispatchRequest(maxSizeIndex);
			}
		}

		double_continue:;
	}
	
	#ifndef NDEBUG
		assert(ftell(md5CheckFD) % 32 == 0);
		
		size_t md5CheckLen = ftell(md5CheckFD);
		uint8_t *md5Check = malloc(md5CheckLen);
		rewind(md5CheckFD);
		fread(md5Check, 1, md5CheckLen, md5CheckFD);
		fclose(md5CheckFD);
		assert(!memcmp(GetMD5Sum_unsafe(md5Check, md5CheckLen), req->md5Param[0], MD5_LENGTH));
		free(md5Check);
	#endif
	
	while (1) {
		int i=0;
		for (int j=0; j<MAX_REQUESTS; ++j)
			i = bitArraySizes[j] > bitArraySizes[i] ? j : i;
		if (!bitArraySizes[i])
			break;
		dispatchRequest(i);
		Sleep(0);
	}

	if (nbFilesToFetch == 0) {
		req->contentLength = 0;
		_handleStream(req);
	}
	
	free(checkSums);
	
	--req->ses->totalFiles;
}

static void handleRepo(RequestContext *req)
{
	const char *name = utf16to8(req->ses->name);
	size_t len = strlen(name);
	
	ALLOC_AND_INFLATE_GZIP(buff, req->buffer, req->contentLength);
	for (char *s=buff;;) {
		char *end = memchr(s, '\n', buff - s + sizeof(buff) - 1);
		// "ct:revision:830,facbc765308dae8cad3fe939422a9dd6,,Conflict Terra test-830\n"
		if (!memcmp(name, (end ?: buff + sizeof(buff) - 1) - len, len)) {
			if (__sync_fetch_and_or(&req->ses->status, DL_HAVE_PACKAGE_MASK) & DL_HAVE_PACKAGE_MASK)
				break;
			#ifdef NDEBUG
				RequestContext *req2 = calloc(1, sizeof(*req2));
			#else
				RequestContext *req2 = calloc(1, sizeof(*req2) + 16);
				FromBase16(strchr(s, ',') + 1, req2->md5Param[0]);
			#endif
			
			swprintf(req->ses->packagePath, L"packages/%.32hs.sdp", strchr(s, ',') + 1);

			req2->ses = req->ses;
			if (req->con)
				req2->con = req->con;
			else {
				wchar_t *domain = wcsrchr(req->wcharParam, '\\');
				*domain = '\0';
				while (domain[-1] != '\\')
					--domain;
				req2->con = makeConnect(req->ses, domain);
			}
			req2->onFinish = handlePackage;
			sendRequest(req2, req->ses->packagePath);
			break;
		}
		if (!end)
			break;
		s = end + 1;
	}
	if (req->con) {
		wchar_t path[MAX_PATH];
		memcpy(path, gWritableDataDirectory, gWritableDataDirectoryLen * sizeof(wchar_t));
		wchar_t *lastSlash = path + gWritableDataDirectoryLen + swprintf(path + gWritableDataDirectoryLen, L"repos\\%s\\versions.gz", req->wcharParam) - lengthof("versions.gz");
		*lastSlash = 0;
		SHCreateDirectoryEx(NULL, path, NULL);
		*lastSlash = L'\\';
		writeFile(path, req->buffer, req->contentLength);
	}
}

static void handleRepoList(RequestContext *req)
{
	req->ses->error = "Couldn't find target in repositories.";
	ALLOC_AND_INFLATE_GZIP(buff, req->buffer, req->contentLength);
	for (char *s=buff; s > (char *)1; s = memchr(s, '\n', buff - s + sizeof(buff) - 1) + 1) {
		/* "main,http://packages.springrts.com,,\n" */
		char *hostName = strstr(s, ",http://") + sizeof(",http://") - 1;
		char *hostNameEnd = strchr(hostName, ',');
		*hostNameEnd = 0;
		RequestContext *req2 = calloc(1, sizeof(*req2) + 2 * (hostNameEnd - hostName + 1));
		MultiByteToWideChar(CP_UTF8, 0, hostName, -1, req2->wcharParam, hostNameEnd - hostName + 1);
		req2->ses = req->ses;
		req2->con = makeConnect(req->ses, req2->wcharParam);
		req2->onFinish = handleRepo;
		sendRequest(req2, L"versions.gz");
	}
}
