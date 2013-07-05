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

#define LENGTH(x) (sizeof x / sizeof *x)

//determines minimum resolution of progressbar
// values below ~16KB will degrade throughput.
#define CHUNK_SIZE (64 * 1024)

//Used with with rapid streamer.
//More concurrent requests improves throughput to a point, has some overhead.
#define MAX_REQUESTS 10
#define MIN_REQUEST_SIZE (512 * 1024)

#define THREAD_ONFINISH_FLAG 0x80000000
#define DEFINE_THREADED(_func) ((typeof(_func) *)((uintptr_t)_func | THREAD_ONFINISH_FLAG))

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
	void (*on_finish)(struct RequestContext *);
	uint8_t *buf;
	size_t content_len;
	size_t fetched_bytes;
	union {
		wchar_t wchar_param[0];
		uint8_t md5_param[0][MD5_LENGTH];
		char char_param[0];
	};
} RequestContext;

typedef struct ConnectContext {
	HINTERNET handle;
	struct SessionContext *ses;
	int32_t requests;
} ConnectContext;

typedef struct SessionContext {
	intptr_t status;
	HINTERNET handle;
	int32_t requests;
	wchar_t name[MAX_PATH], package_path[MAX_PATH];
	uint8_t total_files, current_files, fetched_files;
	size_t fetched_bytes, total_bytes;
	uint32_t start_time;
	void *package_bytes; size_t package_len;
	const char *error;
	void (*on_finish)(void);
} SessionContext;

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

static SessionContext s_sessions[10];

	__attribute__((optimize(("O3"))))
static inline void
get_path_from_md5(uint8_t *md5, wchar_t *path)
{
	static const uint8_t conv[] = "0123456789abcdef";
	((uint32_t *)path)[0] = 'p' | 'o' << 16;
	((uint32_t *)path)[1] = 'o' | 'l' << 16;
	((uint32_t *)path)[2] = '/' | (uint32_t)conv[(md5[0] & 0xF0) >> 4] << 16;
	((uint32_t *)path)[3] = (uint32_t)conv[md5[0] & 0x0F] | '/' << 16;
	for (int i=1; i<16; ++i)
		((uint32_t *)path)[3+i] = (uint32_t)conv[md5[i] & 0x0F] << 16 | (uint32_t)conv[(md5[i] & 0xF0) >> 4];
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
	uint32_t unused;
	WriteFile(file, buf, len, (void *)&unused, NULL);
	CloseHandle(file);
	MoveFileEx(tmp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

static uint32_t WINAPI
execute_on_finish_helper(HINTERNET request_handle)
{
	puts("execute_on_finish_helper 1");
	RequestContext *req; uint32_t req_size = sizeof req;
	WinHttpQueryOption(request_handle, WINHTTP_OPTION_CONTEXT_VALUE, &req, (void *)&req_size);
	((typeof(req->on_finish))((uintptr_t)req->on_finish & ~THREAD_ONFINISH_FLAG))(req);
	WinHttpCloseHandle(request_handle);
	puts("execute_on_finish_helper 2");
	return 0;
}

static void
on_session_end(SessionContext *ses)
{
	if (ses->on_finish)
		ses->on_finish();

	free(ses->package_bytes);
	ses->status = 0;
	if (ses->name) {
		/* Minimap_redraw(); */
		DownloadList_remove(ses->name);
		Sync_reload();
	}
	if (ses->error)
		MainWindow_msg_box("Download Failed", ses->error);
}

static void
on_read_complete(HINTERNET handle, RequestContext *req, uint32_t num_read)
{
	if (num_read <= 0) {
		if ((uintptr_t)req->on_finish & THREAD_ONFINISH_FLAG)
			CreateThread(NULL, 1, (void *)execute_on_finish_helper, handle, 0, 0);
		else {
			if (req->on_finish)
				req->on_finish(req);
			WinHttpCloseHandle(handle);
		}
		return;
	}

	if (req->ses->total_bytes) {
		req->ses->fetched_bytes += num_read;
		assert(req->ses->total_bytes);
		/* Minimap_redraw(); */
		wchar_t text[128];
		_swprintf(text, L"%.1f of %.1f MB  (%.2f%%)",
				(float)req->ses->fetched_bytes / 1000000,
				(float)req->ses->total_bytes / 1000000,
				100 * (float)req->ses->fetched_bytes / ((float)req->ses->total_bytes?:1));
		DownloadList_update(req->ses->name, text);
	}

	req->fetched_bytes += num_read;
	WinHttpReadData(handle, req->buf + req->fetched_bytes,
			min(req->content_len - req->fetched_bytes, CHUNK_SIZE),
			NULL);
}

static void
on_headers_available(HINTERNET handle, RequestContext *req)
{
	wchar_t buf[128];
	uint32_t buf_len = sizeof buf;

	WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_TYPE,
			WINHTTP_HEADER_NAME_BY_INDEX, buf, (void *)&buf_len,
			WINHTTP_NO_HEADER_INDEX);
	if (!memcmp(buf, L"text/html", sizeof L"text/html" - sizeof *buf)
			|| (__sync_fetch_and_or(&req->ses->status, DL_HAVE_ONE_REQUEST) & DL_DONT_ALLOW_MASK) == DL_DONT_ALLOW_MASK) {
		WinHttpCloseHandle(handle);
		return;
	}

	buf_len = sizeof req->content_len;

	WinHttpQueryHeaders(handle,
			WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX,
			&req->content_len, (void *)&buf_len,
			WINHTTP_NO_HEADER_INDEX);

	if (!req->content_len) {
		assert(0);
		WinHttpCloseHandle(handle);
		return;
	}
	req->buf = malloc(req->content_len);

	if (req->ses->total_files) {
		req->ses->total_bytes += req->content_len;
		++req->ses->current_files;
	}
	WinHttpReadData(handle, req->buf + req->fetched_bytes,
			min(req->content_len - req->fetched_bytes, CHUNK_SIZE),
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
		uint32_t status, __attribute__((unused)) void * info, uint32_t info_len)
{
	if (*(int *)req & 1) {	//Magic number means its actually a session, handles are aligned on uint32_t borders
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
	ConnectContext *con = calloc(1, sizeof *con);
	con->handle = WinHttpConnect(ses->handle, domain, INTERNET_DEFAULT_HTTP_PORT, 0);
	con->ses = ses;
	assert(con->handle);
	return con;
}

static void
send_request(RequestContext *req, const wchar_t *object_name)
{
	++req->con->requests;
	++req->ses->requests;
	HINTERNET handle = WinHttpOpenRequest(req->con->handle, NULL,
			object_name, NULL, WINHTTP_NO_REFERER, NULL, 0);
	WinHttpSendRequest(handle, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			NULL, 0, 0, (uintptr_t)req);
}

static void
save_file(RequestContext *req)
{
	write_file(req->wchar_param, req->buf, req->content_len);
	req->ses->error = NULL;
}

static const char message_template_start[] =
R"(<?xml version="1.0" encoding="utf-8"?><soap12:Envelope xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:soap12="http://www.w3.org/2003/05/soap-envelope"><soap12:Body>    <Downloader_get_file xmlns="http://tempuri.org/"><internal_name>)";
static const char message_template_end[] = R"(</internal_name></Downloader_get_file></soap12:Body></soap12:Envelope>)";

static void
handle_map_sources(RequestContext *req)
	//NB: this function is stupidly dangerous
{
	req->ses->error = "Couldn't find a download source.";

	req->buf[req->content_len - 1] = 0;

	req->ses->status = (req->ses->status & ~DL_DONT_ALLOW_MASK) | DL_ONLY_ALLOW_ONE_REQUEST;
	char *s = strstr((char *)req->buf, "<links>");
	if (!s)
		return;
	*strstr(s, "</links>") = 0;

	while ((s = strstr(s, "<string>"))) {
		s += sizeof "<string>" - 1;
		char *end = strchr(s, '<');
		*end = 0;
		wchar_t path[256], host[256];
		if (FALSE == WinHttpCrackUrl(utf8to16(s), 0, ICU_DECODE, &(URL_COMPONENTS){
					.dwStructSize = sizeof(URL_COMPONENTS),
					.lpszHostName = host, .dwHostNameLength = LENGTH(host),
					.lpszUrlPath = path, .dwUrlPathLength = LENGTH(path)}))
			continue;
		wchar_t *name = wcsrchr(path, '/');
		size_t size = (wcslen(name) + wcslen(g_data_dir) + LENGTH(L"maps")) * sizeof *name;

		RequestContext *req2 = calloc(1, sizeof *req2 + size);
		_swprintf(req2->wchar_param, L"%s%s%s",
				g_data_dir, req->ses->status & DL_MAP ? L"maps" : L"mods", name);
		req2->ses = req->ses;
		req2->con = make_connect(req->ses, host);

		req2->on_finish = save_file;

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
	SessionContext *ses = &s_sessions[0];

	*ses = (SessionContext){
		.status = DL_ACTIVE,
			.handle = handle,
			.on_finish = Downloader_get_selected_packages,
	};
	WinHttpSetOption(handle, WINHTTP_OPTION_CONTEXT_VALUE, &ses, sizeof ses);
	WinHttpSetStatusCallback(handle, (void *)callback,
			WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS |
			WINHTTP_CALLBACK_FLAG_HANDLES |
			WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0);

	RequestContext *req = malloc(sizeof *req);

	*req = (RequestContext){
		.ses = ses,
			.con = make_connect(ses, L"repos.caspring.org"),
			.on_finish = handle_repo_list,
	};
	send_request(req, L"repos.gz");
}

static void
download_package(SessionContext *ses)
{
	ses->error = "Couldn't retreive list of repositories from \"repos.caspring.org\".";

	WIN32_FIND_DATA find_file_data;
	wchar_t path[MAX_PATH];
	wchar_t *pathEnd = path + _swprintf(path, L"%srepos\\*",
			g_data_dir) - 1;

	HANDLE find = FindFirstFile(path, &find_file_data);
	do {
		if (find_file_data.cFileName[0] == L'.')
			continue;
		_swprintf(pathEnd, L"%s\\versions.gz", find_file_data.cFileName);
		handle_repo(path, ses);
	} while (FindNextFile(find, &find_file_data));
	FindClose(find);
}

void
Downloader_get_selected_packages(void)
{
	for (size_t i=0; g_mod_len == 0 && i<10; ++i) {
		if (i==9) {
			assert(0);
			return;
		}
		Sleep(1000); //hack, instead of waiting for unitsync to init.
	}
	WIN32_FIND_DATA find_file_data;
	wchar_t path[MAX_PATH];
	wchar_t *pathEnd = path + _swprintf(path, L"%srepos\\*",
			g_data_dir) - 1;

	HANDLE find = FindFirstFile(path, &find_file_data);
	do {
		if (find_file_data.cFileName[0] == L'.')
			continue;
		_swprintf(pathEnd, L"%s\\versions.gz", find_file_data.cFileName);
		handle_repo_2(path);
	} while (FindNextFile(find, &find_file_data));
	FindClose(find);
}

static void
handle_repo_2(const wchar_t *path)
{
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0,
			NULL);

	if (file == INVALID_HANDLE_VALUE)
		return;

	uint32_t file_size = GetFileSize(file, NULL);
	if (file_size == INVALID_FILE_SIZE) {
		assert(0);
		CloseHandle(file);
		return;
	}

	uint8_t gzipped_buf[file_size];
	if (!ReadFile(file, gzipped_buf, sizeof gzipped_buf, (void *)&file_size, NULL)
			|| file_size != sizeof gzipped_buf) {
		assert(0);
		CloseHandle(file);
		return;
	}

	CloseHandle(file);

	ALLOC_AND_INFLATE_GZIP(buf, gzipped_buf, sizeof gzipped_buf);

	for (char *s=buf;;) {
		char *end = memchr(s, '\n', (size_t)(buf - s) + sizeof buf - 1);
		// "ct:revision:830,facbc765308dae8cad3fe939422a9dd6,,Conflict Terra test-830\n"

		void
doit(char *name)
		{
			size_t len = strlen(name);
			if (!memcmp(name, s, len)) {
				char *end2 = end ?: buf + sizeof buf - 1;
				*end2 = '\0';
				char *long_name = end2;
				while (long_name[-1] != ',')
					--long_name;
				for (int i=0; i<g_mod_len; ++i)
					if (!strcmp(g_mods[i], long_name))
						return;
				DownloadMod(long_name);
			}
		}

		if (g_settings.selected_packages) {
			size_t len = strlen(g_settings.selected_packages);
			char buf[len + 1];
			buf[len] = '\0';
			char *start = buf;
			for (size_t i=0; i<len; ++i) {
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

	RequestContext *req = calloc(1, sizeof *req +
			sizeof message_template_start +
			sizeof message_template_end + MAX_TITLE);

	req->on_finish = handle_map_sources;
	req->ses = ses;
	ses->error = "Couldn't retrieve download sources from to \"http://zero-k.info\".";
	req->con = con;
	++ses->requests;
	++con->requests;
	int message_len = sprintf(req->char_param, "%s%ls%s",
			message_template_start, ses->name, message_template_end);
	WinHttpSendRequest(handle,
			L"Content-Type: application/soap+xml; charset=utf-8",
			(uint32_t)-1, req->char_param, (uint32_t)message_len,
			(uint32_t)message_len,
			(uintptr_t)req);
}

/* void
GetDownloadMessage(char *text) */
/* { */
/* if (g_my_battle == NULL) */
/* return; */
/* text[0] = '\0'; */
/* FOR_EACH(s, s_sessions) { */
/* if (s->status && !wcscmp(s->name, utf8to16(g_my_battle->map_name))) { */
/* text += sprintf(text, "Downloading map:\n%.1f of %.1f MB\n(%.2f%%)\n\n", */
/* (float)s->fetched_bytes / 1000000, */
/* (float)s->total_bytes / 1000000, */
/* (float)100 * s->fetched_bytes / (s->total_bytes?:1)); */
/* } */
/* } */
/* FOR_EACH(s, s_sessions) { */
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
	for (size_t i=0; i<LENGTH(s_sessions); ++i) {
		if (s_sessions[i].status && !wcscmp(s_sessions[i].name, buf))
			return;
		ses = ses ?: !s_sessions[i].status ? &s_sessions[i] : NULL;
	}
	if (!ses)
		return;

	HINTERNET handle = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
			WINHTTP_FLAG_ASYNC);

	*ses = (SessionContext){
		.status = DL_ACTIVE | (type == DLTYPE_MAP ? DL_MAP : type == DLTYPE_SHORTMOD ? DL_USE_SHORT_NAME : 0),
			// .progress_bar = (HWND)SendMessage(g_main_window, WM_CREATE_DLG_ITEM, DLG_PROGRESS_BAR, (uintptr_t)&DIALOG_ITEMS[DLG_PROGRESS]),
			// .button = (HWND)SendMessage(g_main_window, WM_CREATE_DLG_ITEM, DLG_PROGRESS_BUTTON, (uintptr_t)&DIALOG_ITEMS[DLG_PROGRESS_BUTTON_]),
			.handle = handle,
	};
	_swprintf(ses->name, L"%hs", name);
	DownloadList_update(ses->name, L"Initializing");
	// SetWindowLongPtr(ses->button, GWLP_USERDATA, (intptr_t)ses);
	// SendMessage(ses->progress_bar, PBM_SETMARQUEE, 1, 0);
	// UpdateStatusBar();

	WinHttpSetOption(handle, WINHTTP_OPTION_CONTEXT_VALUE, &ses, sizeof ses);

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

	for (size_t i=0, j=0; i<req->content_len;++j) {
		size_t file_size = ntohl(*(uint32_t *)&req->buf[i]);
		get_path_from_md5(req->md5_param[j], path + wcslen(g_data_dir));
		write_file(path, req->buf + i + 4, file_size);
		i += file_size + 4;
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
		struct file_data {
			union {
				uint8_t md5[MD5_LENGTH];
				uint32_t first_int;
			};
			uint32_t crc32, file_size_n_b_o;
		};
	req->ses->total_files = 1;

	ALLOC_AND_INFLATE_GZIP(package, req->buf, req->content_len);

	req->ses->package_bytes = req->buf;
	req->ses->package_len = req->content_len;
	req->buf = NULL;

	size_t files_in_package_len = 0;
	for (size_t i = 0; i < sizeof package; ) {
		i += 1u + (uint8_t)package[i] + sizeof(struct file_data);
		++files_in_package_len;
	}

	union {
		uint32_t first_int;
		uint8_t md5[MD5_LENGTH];
	} *check_sums = malloc(files_in_package_len * sizeof *check_sums);

	uint8_t (*bit_arrays)[(files_in_package_len + 7) / 8] = calloc((files_in_package_len + 7) / 8, MAX_REQUESTS);
	size_t bit_array_sizes[MAX_REQUESTS] = {};
	size_t bit_array_number[MAX_REQUESTS] = {};
	RequestContext *request_contexts[MAX_REQUESTS] = {};

	wchar_t path[MAX_PATH];
	size_t path_len = wcslen(g_data_dir);
	wcscpy(path, g_data_dir);

	wchar_t object_name[LENGTH(L"streamer.cgi?") + 32] = L"streamer.cgi?";
	memcpy(object_name + LENGTH(L"streamer.cgi?") - 1,
			req->ses->package_path + LENGTH(L"packages/") - 1,
			32 * sizeof *object_name);


#ifndef NDEBUG
	FILE *md5Check_f_d = tmpfile();
#endif

	void
dispatch_request(int i)
	{
		++req->ses->total_files;
		++req->ses->requests;
		++req->con->requests;

		RequestContext *new_req = request_contexts[i];

		*new_req = (RequestContext) {
			.ses = req->ses,
				.con = req->con,
				.on_finish = handle_stream,
		};

		size_t len = (files_in_package_len + 7) / 8;
		new_req->buf = Gzip_deflate(bit_arrays[i], &len);

		HINTERNET handle = WinHttpOpenRequest(new_req->con->handle,
				L"POST", object_name, NULL, WINHTTP_NO_REFERER,
				NULL, 0);
		WinHttpSendRequest(handle, NULL, 0, (void *)new_req->buf,
				len, len, (uintptr_t)new_req);

		memset(bit_arrays[i], 0, sizeof *bit_arrays);
		bit_array_sizes[i] = 0;
		bit_array_number[i] = 0;
		request_contexts[i] = NULL;
	}


	int file_index = 0, files_to_fetch_len = 0;

	for (char *p=package; p < package + sizeof package;++file_index) {
		struct file_data *file_data = (void *)(p + 1 + *p);

#ifndef NDEBUG
		fwrite(MD5_calc_checksum_unsafe(p+1, (uint8_t)*p), 1, MD5_LENGTH, md5Check_f_d);
		fwrite(file_data->md5, 1, MD5_LENGTH, md5Check_f_d);
#endif

		p = (void *)(file_data + 1);

		for (int i=0; i<files_to_fetch_len; ++i)
			if (__builtin_expect(check_sums[i].first_int == file_data->first_int, 0)
					&& !memcmp(check_sums[i].md5, file_data->md5, MD5_LENGTH))
				goto double_continue;
		//This is expensive operation, on 5000 file package takes me 20s. (from fresh boot on hdd)
		get_path_from_md5(file_data->md5, path+path_len);
		if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
			continue;

		memcpy(check_sums[files_to_fetch_len].md5, file_data->md5, MD5_LENGTH);
		size_t file_size = ntohl(file_data->file_size_n_b_o);

		int request_index;
		//handle_package function uses 1 request, so checking for < 2 _real_ requests.
		//if so, rush largest request out so we dont have idle network.
		if (req->ses->requests < 3) {
			request_index = 0;
			for (int i=1; i<MAX_REQUESTS; ++i)
				request_index = bit_array_sizes[i] > bit_array_sizes[request_index] ? i : request_index;
			goto done;
		}

		//Don't deprive downloading thread of too much cpu time
		Sleep(0);

		//Have some requests in progress, so take time and balance buckets as much as possible:
		for (request_index=0; request_index<MAX_REQUESTS; ++request_index)
			if (bit_array_sizes[request_index] < MIN_REQUEST_SIZE)
				goto done;
		request_index=0;
		for (int j=1; j<MAX_REQUESTS; ++j)
			request_index = bit_array_sizes[j] < bit_array_sizes[request_index] ? j : request_index;

done:

		bit_arrays[request_index][file_index/8] |= (uint8_t)(1 << file_index % 8);
		bit_array_sizes[request_index] += file_size;

		if (!request_contexts[request_index])
			request_contexts[request_index] = malloc(sizeof *request_contexts + MD5_LENGTH * files_in_package_len);
		memcpy(request_contexts[request_index]->md5_param[bit_array_number[request_index]++], file_data->md5, MD5_LENGTH);
		++files_to_fetch_len;

		if (req->ses->requests < 3) {
			int max_size_index = 0;
			for (int i=1; i<MAX_REQUESTS; ++i)
				max_size_index = bit_array_sizes[i] > bit_array_sizes[max_size_index] ? i : max_size_index;
			if (bit_array_sizes[max_size_index] > MIN_REQUEST_SIZE) {
				dispatch_request(max_size_index);
			}
		}

double_continue:;
	}

#ifndef NDEBUG
	assert(ftell(md5Check_f_d) % 32 == 0);

	size_t md5_check_len = (size_t)ftell(md5Check_f_d);
	uint8_t *md5_check = malloc(md5_check_len);
	rewind(md5Check_f_d);
	fread(md5_check, 1, md5_check_len, md5Check_f_d);
	fclose(md5Check_f_d);
	assert(!memcmp(MD5_calc_checksum_unsafe(md5_check, md5_check_len), req->md5_param[0], MD5_LENGTH));
	free(md5_check);
#endif

	while (1) {
		int i=0;
		for (int j=0; j<MAX_REQUESTS; ++j)
			i = bit_array_sizes[j] > bit_array_sizes[i] ? j : i;
		if (!bit_array_sizes[i])
			break;
		dispatch_request(i);
		Sleep(0);
	}

	if (files_to_fetch_len == 0) {
		req->content_len = 0;
		_handle_stream(req);
	}

	free(check_sums);

	--req->ses->total_files;
}


static void
handle_repo(const wchar_t *path, SessionContext *ses)
{
	HANDLE file = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0,
			NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;

	uint32_t file_size = GetFileSize(file, NULL);
	if (file_size == INVALID_FILE_SIZE) {
		assert(0);
		CloseHandle(file);
		return;
	}

	uint8_t gzipped_buf[file_size];
	if (!ReadFile(file, gzipped_buf, sizeof gzipped_buf, (void *)&file_size, NULL)
			|| file_size != sizeof gzipped_buf) {
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

	ALLOC_AND_INFLATE_GZIP(buf, gzipped_buf, sizeof gzipped_buf);

	for (char *s=buf;;) {
		char *end = memchr(s, '\n', (size_t)(buf - s) + sizeof buf - 1);
		char *line_name = ses->status & DL_USE_SHORT_NAME ? s : end ? end - len : buf + sizeof buf - 1 - len;
		// "ct:revision:830,facbc765308dae8cad3fe939422a9dd6,,Conflict Terra test-830\n"

		if (!memcmp(name, line_name, len)) {
			if (__sync_fetch_and_or(&ses->status, DL_HAVE_PACKAGE_MASK) & DL_HAVE_PACKAGE_MASK)
				break;
#ifdef NDEBUG
			RequestContext *req = calloc(1, sizeof *req);
#else
			RequestContext *req = calloc(1, sizeof *req + 16);
			MD5_from_base_16(strchr(s, ',') + 1, req->md5_param[0]);
#endif

			_swprintf(ses->package_path, L"packages/%.32hs.sdp", strchr(s, ',') + 1);

			req->ses = ses;

			req->con = make_connect(ses, domain);

			req->on_finish = handle_package;
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
	ALLOC_AND_INFLATE_GZIP(buf, req->buf, req->content_len);
	for (char *s=buf; s > (char *)1; s = (char *)memchr(s, '\n', (size_t)(buf - s) + sizeof buf - 1) + 1) {
		/* "main,http://packages.springrts.com,,\n" */
		char *host_name = strstr(s, ",http://") + sizeof ",http://" - 1;
		char *host_name_end = strchr(host_name, ',');
		*host_name_end = 0;
		RequestContext *req2 = calloc(1, sizeof *req2 + 2 * (MAX_PATH));

		wchar_t path[host_name_end - host_name + 1];

		MultiByteToWideChar(CP_UTF8, 0, host_name, -1, path, host_name_end - host_name + 1);

		_swprintf(req2->wchar_param + wcslen(g_data_dir), L"%srepos\\%s\\", g_data_dir, path);
		CreateDirectory(req2->wchar_param, NULL);
		wcscat(req2->wchar_param, L"versions.gz");

		req2->ses = req->ses;
		req2->con = make_connect(req->ses, path);

		req2->on_finish = save_file;
		send_request(req2, L"versions.gz");
	}
}
