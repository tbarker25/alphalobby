/*
 * Copyright (c) 2013, Thomas Barker
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * It under the terms of the GNU General Public License as published by
 * The Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * Along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdio.h>

#include <windows.h>
#include <winhttp.h>
#include <winsock2.h>

#include "battle.h"
#include "battleroom.h"
#include "common.h"
#include "downloader.h"
#include "downloadtab.h"
#include "gzip.h"
#include "mainwindow.h"
#include "md5.h"
#include "mybattle.h"
#include "settings.h"
#include "sync.h"
#include "user.h"

#define LENGTH(x) (sizeof(x) / sizeof(*x))

//determines minimum resolution of progressbar
// values below ~16KB will degrade throughput.
#define CHUNK_SIZE (64 * 1024)

//Used with with rapid streamer.
//More concurrent requests improves throughput to a point, has some overhead.
#define MAX_REQUESTS 10
#define MIN_REQUEST_SIZE (512 * 1024)

typedef enum DownloadStatus {
	DL_INACTIVE               = 0x00,
	DL_ACTIVE                 = 0x01,
	DL_MAP                    = 0x02,
	DL_USE_SHORT_NAME         = 0x20,

	DL_ONLY_ALLOW_ONE_REQUEST = 0x04,
	DL_HAVE_ONE_REQUEST       = 0x08,
	DL_DONT_ALLOW_MASK        = DL_ONLY_ALLOW_ONE_REQUEST | DL_HAVE_ONE_REQUEST,

	DL_HAVE_PACKAGE_MASK      = 0x10,
}DownloadStatus;


typedef struct RequestContext {
	struct SessionContext *ses;
	struct ConnectContext *con;
	void (*onFinish)(struct RequestContext *);
	uint8_t *buf;
	size_t content_length;
	size_t fetched_bytes;
	union {
		wchar_t wchar_param[0];
		uint8_t md5_param[0][MD5_LENGTH];
		char char_param[0];
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
	wchar_t name[MAX_PATH], package_path[MAX_PATH];
	uint8_t total_files, current_files, fetched_files;
	size_t fetched_bytes, total_bytes;
	DWORD start_time;
	void *package_bytes; size_t package_len;
	char *error;
	void (*onFinish)(void);
}SessionContext;

static SessionContext sessions[10];


#define THREAD_ONFINISH_FLAG 0x80000000
#define DEFINE_THREADED(_func) ((typeof(_func) *)((intptr_t)_func | THREAD_ONFINISH_FLAG))

static void _handle_stream(RequestContext *req);
#define handle_stream DEFINE_THREADED(_handle_stream)
static ConnectContext *  make_connect(SessionContext *, const wchar_t *domain);
static void download_package(SessionContext *ses);
static void download_file(SessionContext *ses);
static void _handle_package(RequestContext *req);
#define handle_package DEFINE_THREADED(_handle_package)
static void handle_repo_list(RequestContext *req);
static void handle_repo(const wchar_t *path, SessionContext *ses);
static void handle_repo_2(const wchar_t *path);
void Downloader_get_selected_packages(void);


	__attribute__((optimize(("O3"))))
static inline void
get_path_from_md5(uint8_t *md5, wchar_t *path)
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

static void
write_file(wchar_t *path, const void *buf, size_t len)
{
	wchar_t tmp_path[MAX_PATH];
	GetTempPath(LENGTH(tmp_path), tmp_path);
	GetTempFileName(tmp_path, NULL, 0, tmp_path);
	HANDLE file = CreateFile(tmp_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	DWORD unused;
	WriteFile(file, buf, len, &unused, NULL);
	CloseHandle(file);
	MoveFileEx(tmp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

static DWORD WINAPI
execute_on_finish_helper(HINTERNET requestHandle)
{
	puts("execute_on_finish_helper 1");
	RequestContext *req; DWORD reqSize = sizeof(req);
	WinHttpQueryOption(requestHandle, WINHTTP_OPTION_CONTEXT_VALUE, &req, &reqSize);
	((typeof(req->onFinish))((intptr_t)req->onFinish & ~THREAD_ONFINISH_FLAG))(req);
	WinHttpCloseHandle(requestHandle);
	puts("execute_on_finish_helper 2");
	return 0;
}

static void
on_session_end(SessionContext *ses)
{
	if (ses->onFinish)
		ses->onFinish();

	free(ses->package_bytes);
	ses->status = 0;
	if (ses->name) {
		/* BattleRoom_redraw_minimap(); */
		DownloadList_remove(ses->name);
		Sync_reload();
	}
	if (ses->error)
		MainWindow_msg_box("Download Failed", ses->error);
}

static void
on_read_complete(HINTERNET handle, RequestContext *req, DWORD numRead)
{
	if (numRead <= 0) {
		if ((intptr_t)req->onFinish & THREAD_ONFINISH_FLAG)
			CreateThread(NULL, 1, execute_on_finish_helper, handle, 0, 0);
		else {
			if (req->onFinish)
				req->onFinish(req);
			WinHttpCloseHandle(handle);
		}
		return;
	}

	if (req->ses->total_bytes) {
		req->ses->fetched_bytes += numRead;
		assert(req->ses->total_bytes);
		/* BattleRoom_redraw_minimap(); */
		wchar_t text[128];
		_swprintf(text, L"%.1f of %.1f MB  (%.2f%%)",
				(float)req->ses->fetched_bytes / 1000000,
				(float)req->ses->total_bytes / 1000000,
				(float)100 * req->ses->fetched_bytes / (req->ses->total_bytes?:1));
		DownloadList_update(req->ses->name, text);
	}

	req->fetched_bytes += numRead;
	WinHttpReadData(handle, req->buf + req->fetched_bytes,
			min(req->content_length - req->fetched_bytes, CHUNK_SIZE),
			NULL);
}

static void
on_headers_available(HINTERNET handle, RequestContext *req)
{
	wchar_t buf[128]; DWORD buf_size = sizeof(buf);
	WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_TYPE,
			WINHTTP_HEADER_NAME_BY_INDEX, buf, &buf_size,
			WINHTTP_NO_HEADER_INDEX);
	if (!memcmp(buf, L"text/html", sizeof(L"text/html") - sizeof(wchar_t))
			|| (__sync_fetch_and_or(&req->ses->status, DL_HAVE_ONE_REQUEST) & DL_DONT_ALLOW_MASK) == DL_DONT_ALLOW_MASK) {
		WinHttpCloseHandle(handle);
		return;
	}

	buf_size = sizeof(req->content_length);

	WinHttpQueryHeaders(handle,
			WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&req->content_length, &buf_size,
			WINHTTP_NO_HEADER_INDEX);

	if (!req->content_length) {
		assert(0);
		WinHttpCloseHandle(handle);
		return;
	}
	req->buf = malloc(req->content_length);

	if (req->ses->total_files) {
		req->ses->total_bytes += req->content_length;
		++req->ses->current_files;
	}
	WinHttpReadData(handle, req->buf + req->fetched_bytes,
			min(req->content_length - req->fetched_bytes, CHUNK_SIZE),
			NULL);
}

static void
on_closing_handle(RequestContext *req)
{
	if (!--req->con->requests) {
		WinHttpCloseHandle(req->con->handle);
		free(req->con);
	}

	if (!--req->ses->requests)
		WinHttpCloseHandle(req->ses->handle);

	free(req->buf);
	free(req);
}

static void
CALLBACK callback(HINTERNET handle, RequestContext *req,
		DWORD status, LPVOID info, DWORD info_len)
{
	if (*(int *)req & 1) {	//Magic number means its actually a session, handles are aligned on DWORD borders
		if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING
				// no idea why this next bit is needed:
				&& (((SessionContext *)req)->handle == handle))
			on_session_end((SessionContext *)req);
		return;
	}


	switch (status) {
	case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		on_read_complete(handle, req, info_len);
		return;
	case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		on_headers_available(handle, req);
		return;
	case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		WinHttpCloseHandle(handle);
		return;
	case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		WinHttpReceiveResponse(handle, NULL);
		return;
	case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		on_closing_handle(req);
		return;
	}
}

static ConnectContext *
make_connect(SessionContext *ses, const wchar_t *domain)
{
	ConnectContext *con = malloc(sizeof(ConnectContext));
	*con = (ConnectContext) {
		WinHttpConnect(ses->handle, domain, INTERNET_DEFAULT_HTTP_PORT, 0),
			ses};
	assert(con->handle);
	return con;
}

static void
send_request(RequestContext *req, const wchar_t *objectName)
{
	++req->con->requests;
	++req->ses->requests;
	HINTERNET handle = WinHttpOpenRequest(req->con->handle, NULL,
			objectName, NULL, WINHTTP_NO_REFERER, NULL, 0);
	WinHttpSendRequest(handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			NULL, 0, 0, (DWORD_PTR)req);
}

static void
save_file(RequestContext *req)
{
	write_file(req->wchar_param, req->buf, req->content_length);
	req->ses->error = NULL;
}

static const char messageTemplateStart[] =
R"(<?xml version="1.0" encoding="utf-8"?><soap12:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:soap12="http://www.w3.org/2003/05/soap-envelope"><soap12:Body>    <Downloader_get_file xmlns="http://tempuri.org/"><internalName>)";
static const char messageTemplateEnd[] = R"(</internalName></Downloader_get_file></soap12:Body></soap12:Envelope>)";

static void
handle_map_sources(RequestContext *req)
	//NB: this function is stupidly dangerous
{
	req->ses->error = "Couldn't find a download source.";

	req->buf[req->content_length - 1] = 0;

	req->ses->status = (req->ses->status & ~DL_DONT_ALLOW_MASK) | DL_ONLY_ALLOW_ONE_REQUEST;
	char *s = strstr((char *)req->buf, "<links>");
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
					.lpszHostName = host, .dwHostNameLength = LENGTH(host),
					.lpszUrlPath = path, .dwUrlPathLength = LENGTH(path)}))
			continue;
		wchar_t *name = wcsrchr(path, '/');
		size_t size = sizeof(wchar_t) * (wcslen(name) + wcslen(g_data_dir) + LENGTH(L"maps"));

		RequestContext *req2 = calloc(1, sizeof(*req2) + size);
		_swprintf(req2->wchar_param, L"%s%s%s",
				g_data_dir, req->ses->status & DL_MAP ? L"maps" : L"mods", name);
		req2->ses = req->ses;
		req2->con = make_connect(req->ses, host);

		req2->onFinish = save_file;

		send_request(req2, path);
		req->ses->start_time = GetTickCount();
		req->ses->total_files = 1;
		s = end+1;
	}
}

void
Downloader_init(void)
{
	CreateDirectory(Settings_get_data_dir(L"maps"), NULL);
	CreateDirectory(Settings_get_data_dir(L"mods"), NULL);
	CreateDirectory(Settings_get_data_dir(L"packages"), NULL);
	CreateDirectory(Settings_get_data_dir(L"pool"), NULL);
	CreateDirectory(Settings_get_data_dir(L"repos"), NULL);

	for (int i=0; i<256; ++i) {
		wchar_t path[MAX_PATH];
		_swprintf(path, L"%spool/%02x", g_data_dir, i);
		CreateDirectory(path, NULL);
	}

	HINTERNET handle = WinHttpOpen(NULL,
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
			WINHTTP_FLAG_ASYNC);
	SessionContext *ses = &sessions[0];

	*ses = (SessionContext){
		.status = DL_ACTIVE,
			.handle = handle,
			.onFinish = Downloader_get_selected_packages,
	};
	WinHttpSetOption(handle, WINHTTP_OPTION_CONTEXT_VALUE, &ses, sizeof(ses));
	WinHttpSetStatusCallback(handle, (void *)callback,
			WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
			WINHTTP_CALLBACK_FLAG_HANDLES |
			WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	RequestContext *req = malloc(sizeof(RequestContext));

	*req = (RequestContext){
		.ses = ses,
			.con = make_connect(ses, L"repos.caspring.org"),
			.onFinish = handle_repo_list,
	};
	send_request(req, L"repos.gz");
}

static void
download_package(SessionContext *ses)
{
	ses->error = "Couldn't retreive list of repositories from \"repos.caspring.org\".";

	WIN32_FIND_DATA find_fileData;
	wchar_t path[MAX_PATH];
	wchar_t *path_end = path + _swprintf(path, L"%srepos\\*",
			g_data_dir) - 1;

	HANDLE find = FindFirstFile(path, &find_fileData);
	do {
		if (find_fileData.cFileName[0] == L'.')
			continue;
		_swprintf(path_end, L"%s\\versions.gz", find_fileData.cFileName);
		handle_repo(path, ses);
	} while (FindNextFile(find, &find_fileData));
	FindClose(find);
}

void
Downloader_get_selected_packages(void)
{
	for (int i=0; g_mod_count < 0 && i<10; ++i) {
		if (i==9) {
			assert(0);
			return;
		}
		Sleep(1000); //hack, instead of waiting for unitsync to init.
	}
	WIN32_FIND_DATA find_fileData;
	wchar_t path[MAX_PATH];
	wchar_t *path_end = path + _swprintf(path, L"%srepos\\*",
			g_data_dir) - 1;

	HANDLE find = FindFirstFile(path, &find_fileData);
	do {
		if (find_fileData.cFileName[0] == L'.')
			continue;
		_swprintf(path_end, L"%s\\versions.gz", find_fileData.cFileName);
		handle_repo_2(path);
	} while (FindNextFile(find, &find_fileData));
	FindClose(find);
}

static void
handle_repo_2(const wchar_t *path)
{
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0,
			NULL);

	if (file == INVALID_HANDLE_VALUE)
		return;

	DWORD fileSize = GetFileSize(file, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
		assert(0);
		CloseHandle(file);
		return;
	}

	uint8_t gzipped_buf[fileSize];
	if (!ReadFile(file, gzipped_buf, sizeof(gzipped_buf), &fileSize, NULL)
			|| fileSize != sizeof(gzipped_buf)) {
		assert(0);
		CloseHandle(file);
		return;
	}

	CloseHandle(file);

	ALLOC_AND_INFLATE_GZIP(buf, gzipped_buf, sizeof(gzipped_buf));

	for (char *s=buf;;) {
		char *end = memchr(s, '\n', buf - s + sizeof(buf) - 1);
		// "ct:revision:830,facbc765308dae8cad3fe939422a9dd6,,Conflict Terra test-830\n"

		void
doit(char *name)
		{
			size_t len = strlen(name);
			if (!memcmp(name, s, len)) {
				char *end2 = end ?: buf + sizeof(buf) - 1;
				*end2 = '\0';
				char *longName = end2;
				while (longName[-1] != ',')
					--longName;
				for (int i=0; i<g_mod_count; ++i)
					if (!strcmp(g_mods[i], longName))
						return;
				DownloadMod(longName);
			}
		}

		if (g_settings.selected_packages) {
			size_t len = strlen(g_settings.selected_packages);
			char buf[len + 1];
			buf[len] = '\0';
			char *start = buf;
			for (int i=0; i<len; ++i) {
				buf[i] = g_settings.selected_packages[i];
				if (buf[i] != ';')
					continue;
				buf[i] = '\0';
				doit(start);
				start = buf + i + 1;
			}
			doit(start);
		}

		if (!end)
			break;
		s = end + 1;
	}
}


static void
download_file(SessionContext *ses)
{
	ConnectContext * con = make_connect(ses, L"zero-k.info");
	HINTERNET handle = WinHttpOpenRequest(con->handle, L"POST",
			L"ContentService.asmx", NULL, WINHTTP_NO_REFERER, NULL,
			0);

	RequestContext *req = calloc(1, sizeof(RequestContext) +
			sizeof(messageTemplateStart) +
			sizeof(messageTemplateEnd) + MAX_TITLE);

	req->onFinish = handle_map_sources;
	req->ses = ses;
	ses->error = "Couldn't retrieve download sources from to \"http://zero-k.info\".";
	req->con = con;
	++ses->requests;
	++con->requests;
	size_t message_length = sprintf(req->char_param, "%s%ls%s",
			messageTemplateStart, ses->name, messageTemplateEnd);
	WinHttpSendRequest(handle,
			L"Content-Type: application/soap+xml; charset=utf-8",
			-1, req->char_param, message_length, message_length,
			(DWORD_PTR)req);
}

/* void
GetDownloadMessage(char *text) */
/* { */
/* if (g_my_battle == NULL) */
/* return; */
/* text[0] = '\0'; */
/* FOR_EACH(s, sessions) { */
/* if (s->status && !wcscmp(s->name, utf8to16(g_my_battle->map_name))) { */
/* text += sprintf(text, "Downloading map:\n%.1f of %.1f MB\n(%.2f%%)\n\n", */
/* (float)s->fetched_bytes / 1000000, */
/* (float)s->total_bytes / 1000000, */
/* (float)100 * s->fetched_bytes / (s->total_bytes?:1)); */
/* } */
/* } */
/* FOR_EACH(s, sessions) { */
/* if (s->status && !wcscmp(s->name, utf8to16(g_my_battle->mod_name))) { */
/* sprintf(text, "Downloading mod:\n%.1f of %.1f MB\n(%.2f%%)\n", */
/* (float)s->fetched_bytes / 1000000, */
/* (float)s->total_bytes / 1000000, */
/* (float)100 * s->fetched_bytes / (s->total_bytes?:1)); */
/* } */
/* } */
/* } */

void
Downloader_get_file(const char *name, enum DLTYPE type)
{
	SessionContext *ses = NULL;
	wchar_t buf[128];
	_swprintf(buf, L"%hs", name);
	for (int i=0; i<LENGTH(sessions); ++i) {
		if (sessions[i].status && !wcscmp(sessions[i].name, buf))
			return;
		ses = ses ?: !sessions[i].status ? &sessions[i] : NULL;
	}
	if (!ses)
		return;

	HINTERNET handle = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
			WINHTTP_FLAG_ASYNC);

	*ses = (SessionContext){
		.status = DL_ACTIVE | (type == DLTYPE_MAP ? DL_MAP : type == DLTYPE_SHORTMOD ? DL_USE_SHORT_NAME : 0),
			// .progressBar = (HWND)SendMessage(g_main_window, WM_CREATE_DLG_ITEM, DLG_PROGRESS_BAR, (LPARAM)&dialog_items[DLG_PROGRESS]),
			// .button = (HWND)SendMessage(g_main_window, WM_CREATE_DLG_ITEM, DLG_PROGRESS_BUTTON, (LPARAM)&dialog_items[DLG_PROGRESS_BUTTON_]),
			.handle = handle,
	};
	_swprintf(ses->name, L"%hs", name);
	DownloadList_update(ses->name, L"Initializing");
	// SetWindowLongPtr(ses->button, GWLP_USERDATA, (LONG_PTR)ses);
	// SendMessage(ses->progressBar, PBM_SETMARQUEE, 1, 0);
	// UpdateStatusBar();

	WinHttpSetOption(handle, WINHTTP_OPTION_CONTEXT_VALUE, &ses, sizeof(ses));

	WinHttpSetStatusCallback(handle, (void *)callback,
			WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
			WINHTTP_CALLBACK_FLAG_HANDLES |
			WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	if (type == DLTYPE_MAP)
		download_file(ses);
	else
		download_package(ses);
}

static void
_handle_stream(RequestContext *req)
{
	wchar_t path[MAX_PATH];
	wcscpy(path, g_data_dir);

	for (int i=0, j=0; i<req->content_length;++j) {
		size_t fileSize = ntohl(*(uint32_t *)&req->buf[i]);
		get_path_from_md5(req->md5_param[j], path + wcslen(g_data_dir));
		write_file(path, req->buf + i + 4, fileSize);
		i += fileSize + 4;
	}

	if (req->ses->total_files == ++req->ses->fetched_files) {
		wchar_t *path = Settings_get_data_dir(req->ses->package_path);
		write_file(path, req->ses->package_bytes, req->ses->package_len);
		req->ses->error = NULL;
	}
}

static void
_handle_package(RequestContext *req)
{
	req->ses->start_time = GetTickCount();
	req->ses->error = "Couldn't retrieve all required files for target.";
	__attribute__((packed))
		struct fileData {
			union {
				uint8_t md5[MD5_LENGTH];
				uint32_t firstInt;
			};
			uint32_t crc32, fileSizeNBO;
		};
	req->ses->total_files = 1;

	ALLOC_AND_INFLATE_GZIP(package, req->buf, req->content_length);

	req->ses->package_bytes = req->buf;
	req->ses->package_len = req->content_length;
	req->buf = NULL;

	size_t nb_filesInPackage = 0;
	for (int i = 0; i < sizeof(package); ) {
		i += 1 + package[i] + sizeof(struct fileData);
		++nb_filesInPackage;
	}

	union {
		uint32_t firstInt;
		uint8_t md5[MD5_LENGTH];
	} *checkSums = malloc(nb_filesInPackage * sizeof(*checkSums));

	uint8_t (*bitArrays)[(nb_filesInPackage + 7) / 8] = calloc((nb_filesInPackage + 7) / 8, MAX_REQUESTS);
	size_t bitArraySizes[MAX_REQUESTS] = {};
	size_t bitArrayNumber[MAX_REQUESTS] = {};
	RequestContext *requestContexts[MAX_REQUESTS] = {};

	wchar_t path[MAX_PATH];
	size_t path_len = wcslen(g_data_dir);
	wcscpy(path, g_data_dir);

	wchar_t objectName[LENGTH(L"streamer.cgi?") + 32] = L"streamer.cgi?";
	memcpy(objectName + LENGTH(L"streamer.cgi?") - 1,
			req->ses->package_path + LENGTH(L"packages/") - 1,
			32 * sizeof(wchar_t));


#ifndef NDEBUG
	FILE *md5CheckFD = tmpfile();
#endif

	void
dispatchRequest(int i)
	{
		++req->ses->total_files;
		++req->ses->requests;
		++req->con->requests;

		RequestContext *newReq = requestContexts[i];

		*newReq = (RequestContext) {
			.ses = req->ses,
				.con = req->con,
				.onFinish = handle_stream,
		};

		size_t len = (nb_filesInPackage + 7) / 8;
		newReq->buf = Gzip_deflate(bitArrays[i], &len);

		HINTERNET handle = WinHttpOpenRequest(newReq->con->handle,
				L"POST", objectName, NULL, WINHTTP_NO_REFERER,
				NULL, 0);
		WinHttpSendRequest(handle, NULL, 0, (void *)newReq->buf,
				len, len, (DWORD_PTR)newReq);

		memset(bitArrays[i], 0, sizeof(*bitArrays));
		bitArraySizes[i] = 0;
		bitArrayNumber[i] = 0;
		requestContexts[i] = NULL;
	}


	int fileIndex = 0, nb_filesToFetch = 0;

	for (char *p=package; p < package + sizeof(package);++fileIndex) {
		struct fileData *fileData = (void *)(p + 1 + *p);

#ifndef NDEBUG
		fwrite(MD5_calc_checksum_unsafe(p+1, *p), 1, MD5_LENGTH, md5CheckFD);
		fwrite(fileData->md5, 1, MD5_LENGTH, md5CheckFD);
#endif

		p = (void *)(fileData + 1);

		for (int i=0; i<nb_filesToFetch; ++i)
			if (__builtin_expect(checkSums[i].firstInt == fileData->firstInt, 0)
					&& !memcmp(checkSums[i].md5, fileData->md5, MD5_LENGTH))
				goto double_continue;
		//This is expensive operation, on 5000 file package takes me 20s. (from fresh boot on hdd)
		get_path_from_md5(fileData->md5, path+path_len);
		if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
			continue;

		memcpy(checkSums[nb_filesToFetch].md5, fileData->md5, MD5_LENGTH);
		size_t fileSize = ntohl(fileData->fileSizeNBO);

		int requestIndex;
		//handle_package function uses 1 request, so checking for < 2 _real_ requests.
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
			requestContexts[requestIndex] = malloc(sizeof(*requestContexts) + MD5_LENGTH * nb_filesInPackage);
		memcpy(requestContexts[requestIndex]->md5_param[bitArrayNumber[requestIndex]++], fileData->md5, MD5_LENGTH);
		++nb_filesToFetch;

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

	size_t md5Check_len = ftell(md5CheckFD);
	uint8_t *md5Check = malloc(md5Check_len);
	rewind(md5CheckFD);
	fread(md5Check, 1, md5Check_len, md5CheckFD);
	fclose(md5CheckFD);
	assert(!memcmp(MD5_calc_checksum_unsafe(md5Check, md5Check_len), req->md5_param[0], MD5_LENGTH));
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

	if (nb_filesToFetch == 0) {
		req->content_length = 0;
		_handle_stream(req);
	}

	free(checkSums);

	--req->ses->total_files;
}


static void
handle_repo(const wchar_t *path, SessionContext *ses)
{
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0,
			NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;

	DWORD fileSize = GetFileSize(file, NULL);
	if (fileSize == INVALID_FILE_SIZE) {
		assert(0);
		CloseHandle(file);
		return;
	}

	uint8_t gzipped_buf[fileSize];
	if (!ReadFile(file, gzipped_buf, sizeof(gzipped_buf), &fileSize, NULL)
			|| fileSize != sizeof(gzipped_buf)) {
		assert(0);
		CloseHandle(file);
		return;
	}

	CloseHandle(file);

	const char *name = utf16to8(ses->name);
	size_t len = strlen(name);

	wchar_t *domain = wcsrchr(path, '\\');
	*domain = '\0';
	while (domain[-1] != '\\')
		--domain;

	ALLOC_AND_INFLATE_GZIP(buf, gzipped_buf, sizeof(gzipped_buf));

	for (char *s=buf;;) {
		char *end = memchr(s, '\n', buf - s + sizeof(buf) - 1);
		char *lineName = ses->status & DL_USE_SHORT_NAME ? s : end ? end - len : buf + sizeof(buf) - 1 - len;
		// "ct:revision:830,facbc765308dae8cad3fe939422a9dd6,,Conflict Terra test-830\n"

		if (!memcmp(name, lineName, len)) {
			if (__sync_fetch_and_or(&ses->status, DL_HAVE_PACKAGE_MASK) & DL_HAVE_PACKAGE_MASK)
				break;
#ifdef NDEBUG
			RequestContext *req = calloc(1, sizeof(*req));
#else
			RequestContext *req = calloc(1, sizeof(*req) + 16);
			MD5_from_base_16(strchr(s, ',') + 1, req->md5_param[0]);
#endif

			_swprintf(ses->package_path, L"packages/%.32hs.sdp", strchr(s, ',') + 1);

			req->ses = ses;

			req->con = make_connect(ses, domain);

			req->onFinish = handle_package;
			send_request(req, ses->package_path);
			break;
		}
		if (!end)
			break;
		s = end + 1;
	}
}

static void
handle_repo_list(RequestContext *req)
{
	req->ses->error = "Couldn't find target in repositories.";
	ALLOC_AND_INFLATE_GZIP(buf, req->buf, req->content_length);
	for (char *s=buf; s > (char *)1; s = memchr(s, '\n', buf - s + sizeof(buf) - 1) + 1) {
		/* "main,http://packages.springrts.com,,\n" */
		char *host_name = strstr(s, ",http://") + sizeof(",http://") - 1;
		char *host_name_end = strchr(host_name, ',');
		*host_name_end = 0;
		RequestContext *req2 = calloc(1, sizeof(*req2) + 2 * (MAX_PATH));

		wchar_t path[host_name_end - host_name + 1];

		MultiByteToWideChar(CP_UTF8, 0, host_name, -1, path, host_name_end - host_name + 1);

		_swprintf(req2->wchar_param + wcslen(g_data_dir), L"%srepos\\%s\\", g_data_dir, path);
		CreateDirectory(req2->wchar_param, NULL);
		wcscat(req2->wchar_param, L"versions.gz");

		req2->ses = req->ses;
		req2->con = make_connect(req->ses, path);

		req2->onFinish = save_file;
		send_request(req2, L"versions.gz");
	}
}
