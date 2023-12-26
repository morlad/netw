#include "netw.h"

#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <winhttp.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma GCC diagnostic ignored "-Wunused-macros"

#ifdef MINIMOD_LOG_ENABLE
#define LOG(FMT, ...) printf("[netw] " FMT "\n", ##__VA_ARGS__)
#else
#define LOG(...)
#endif
#define LOGE(FMT, ...) fprintf(stderr, "[netw] " FMT "\n", ##__VA_ARGS__)

#define ASSERT(in_condition)                      \
	do                                            \
	{                                             \
		if (__builtin_expect(!(in_condition), 0)) \
		{                                         \
			LOGE(                                 \
			  "[assertion] %s:%i: '%s'",          \
			  __FILE__,                           \
			  __LINE__,                           \
			  #in_condition);                     \
			__asm__ volatile("int $0x03");        \
			__builtin_unreachable();              \
		}                                         \
	} while (__LINE__ == -1)

#pragma GCC diagnostic pop

#define LOG_ERR(X) LOGE(X " failed %lu", GetLastError())

// size of download -> file buffer
#define BUFFERSIZE 4096
#define USER_AGENT L"minimod/0.1"
// only enable TLS 1.2 by default
#define SEC_PROTS (WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2)

struct netw
{
	HINTERNET session;
	int error_rate;
	int min_delay;
	int max_delay;
};
static struct netw l_netw;


static void
random_delay(void)
{
	if (l_netw.max_delay > 0)
	{
		int delay = (l_netw.max_delay > l_netw.min_delay)
		  ? l_netw.min_delay + (rand() % (l_netw.max_delay - l_netw.min_delay))
		  : l_netw.min_delay;
		LOG("adding delay: %i ms", delay);
		Sleep((unsigned)delay);
	}
}


static bool
is_random_server_error(void)
{
	if (l_netw.error_rate > (rand() % 100))
	{
		random_delay();
		return true;
	}
	return false;
}


/*
 * Paramters:
 *  out_chars - set to the number of characters in the resulting
 *    string. Multiply by 2 to get the number of bytes.
 *    Add another 1 char (2 bytes) on top of that for the terminating NUL.
 */
static char *
utf8_from_utf16(wchar_t const *in_utf16, size_t *out_chars)
{
	if (!in_utf16)
	{
		return NULL;
	}
	// return value is the number of bytes required to hold the string,
	// including the terminating NUL iff NUL was part of the input.
	int r = WideCharToMultiByte(CP_UTF8, 0, in_utf16, -1, NULL, 0, NULL, NULL);
	// at least a NUL character should be written
	if (r < 1)
	{
		return NULL;
	}
	char *utf8 = malloc((size_t)r);
	r = WideCharToMultiByte(CP_UTF8, 0, in_utf16, -1, utf8, r, NULL, NULL);
	if (out_chars)
	{
		*out_chars = (size_t)r;
	}
	return utf8;
}


/*
 * Paramters:
 *  out_chars - set to the number of characters in the resulting
 *    string.
 *    Add another 1 char on top of that for the terminating NUL.
 */
static wchar_t *
utf16_from_utf8(char const *in_utf8, size_t *out_chars)
{
	if (!in_utf8)
	{
		return NULL;
	}
	// return value is the number of CHARS required to hold the string.
	// including the terminating NUL iff NUL was part of the input.
	int r = MultiByteToWideChar(CP_UTF8, 0, in_utf8, -1, NULL, 0);
	// at least a NUL character should be written
	if (r < 1)
	{
		return NULL;
	}
	wchar_t *utf16 = malloc((size_t)r * sizeof *utf16);
	r = MultiByteToWideChar(CP_UTF8, 0, in_utf8, -1, utf16, r);
	if (out_chars)
	{
		*out_chars = (size_t)r;
	}
	return utf16;
}


bool
netw_init(void)
{
	// Windows 8.1 and above supports WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
	// to use the system's proxy settings.
	// But since Windows 7 should still be supported
	// WINHTTP_ACCESS_TYPE_DEFAULT_PROXY is the only valid choice and
	// proxy settings need to be applied manually.
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxy_cfg = { 0 };
	WinHttpGetIEProxyConfigForCurrentUser(&proxy_cfg);
	if (proxy_cfg.lpszProxy)
	{
		l_netw.session = WinHttpOpen(
		  USER_AGENT,
		  WINHTTP_ACCESS_TYPE_NAMED_PROXY,
		  proxy_cfg.lpszProxy,
		  proxy_cfg.lpszProxyBypass,
		  0);
		GlobalFree(proxy_cfg.lpszProxy);
		GlobalFree(proxy_cfg.lpszProxyBypass);
		GlobalFree(proxy_cfg.lpszAutoConfigUrl);
	}
	else
	{
		l_netw.session = WinHttpOpen(
		  USER_AGENT,
		  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		  WINHTTP_NO_PROXY_NAME,
		  WINHTTP_NO_PROXY_BYPASS,
		  0);
	}

	if (!l_netw.session)
	{
		LOG_ERR("HttpOpen");
		return false;
	}

	// enable/disable protocols explicitly as Win7 does not
	// support TLS 1.2 by default, while all support SSL3 and TLS 1.0,
	// which are already deprecated.
	unsigned long sec_prots = SEC_PROTS;
	WinHttpSetOption(
	  l_netw.session,
	  WINHTTP_OPTION_SECURE_PROTOCOLS,
	  &sec_prots,
	  sizeof sec_prots);

	return true;
}


void
netw_deinit(void)
{
	WinHttpCloseHandle(l_netw.session);
}


static char *
combine_headers(char const *const in_headers[], size_t *out_len)
{
	size_t len_headers = 0;
	char const *const *h = in_headers;
	while (*h)
	{
		len_headers += strlen(*(h++));
		len_headers += 2; /* ": " or "\r\n" */
	}
	char *headers = malloc(len_headers + 1 /*NUL*/);

	h = in_headers;
	char *hdrptr = headers;
	while (*h)
	{
		// key
		size_t l = strlen(*h);
		memcpy(hdrptr, *h, l);
		hdrptr += l;
		*(hdrptr++) = ':';
		*(hdrptr++) = ' ';
		++h;

		// value
		l = strlen(*h);
		memcpy(hdrptr, *h, l);
		hdrptr += l;
		*(hdrptr++) = '\r';
		*(hdrptr++) = '\n';
		++h;
	}

	// NUL terminate
	*hdrptr = '\0';

	if (out_len)
	{
		*out_len = len_headers;
	}

	return headers;
}


static wchar_t *
wcstrndup(wchar_t const *in, size_t in_len)
{
	wchar_t *ptr = malloc(sizeof *ptr * (in_len + 1));
	ptr[in_len] = '\0';
	memcpy(ptr, in, sizeof *ptr * in_len);
	return ptr;
}


static void *
memdup(void const *in, size_t bytes)
{
	void *dup = malloc(bytes);
	memcpy(dup, in, bytes);
	return dup;
}


struct task
{
	wchar_t *host;
	wchar_t *path;
	wchar_t *header;
	wchar_t const *verb;
	union
	{
		netw_request_callback request;
		netw_download_callback download;
	} callback;
	void *udata;
	void *payload;
	size_t payload_bytes;
	uint16_t port;
	FILE *file;
};


struct netw_header
{
	char **keys;
	char **values;
	size_t nkeys;
	size_t nreserved;
};


static void
netw_header_from_raw_header(struct netw_header *hdr, char *buffer)
{
	char *ptr = buffer;
	while (*ptr)
	{
		// resize buffer if necessary
		if (hdr->nkeys == hdr->nreserved)
		{
			hdr->nreserved = hdr->nreserved ? 2 * hdr->nreserved : 16;
			hdr->keys = realloc(hdr->keys, sizeof *hdr->keys * hdr->nreserved);
			hdr->keys[hdr->nkeys] = NULL;
			hdr->values =
			  realloc(hdr->values, sizeof *hdr->values * hdr->nreserved);
		}

		// check if line contains a colon
		char *end_of_line = strchr(ptr, '\r');
		ASSERT(end_of_line);
		char *colon = memchr(ptr, ':', (size_t)(end_of_line - ptr));
		if (colon)
		{
			hdr->keys[hdr->nkeys] = ptr;

			// find end of header-line, store it and advance ptr to next line
			char *ptr_end = end_of_line;

			*colon = '\0';

			hdr->values[hdr->nkeys] = colon + 1;
			// trim leading whitespace
			while (isspace(*hdr->values[hdr->nkeys]))
			{
				hdr->values[hdr->nkeys] += 1;
			}
			// trim trailing whitespace
			while (isspace(*ptr_end))
			{
				*ptr_end = '\0';
				ptr_end -= 1;
			}

			LOG(
			  "hdr: '%s' -> '%s'",
			  hdr->keys[hdr->nkeys],
			  hdr->values[hdr->nkeys]);
			hdr->nkeys += 1;

			ptr = end_of_line + 2;
		}
		else
		{
			ptr = end_of_line + 2;
		}
	}
}


static void
free_netw_header(struct netw_header *hdr)
{
	free(hdr->keys);
	free(hdr->values);
}


static DWORD
task_handler(LPVOID context)
{
	struct task *task = context;

	HINTERNET hconnection =
	  WinHttpConnect(l_netw.session, task->host, task->port, 0);
	if (!hconnection)
	{
		LOG_ERR("HttpConnect");
		goto err_early;
	}

	HINTERNET hrequest = WinHttpOpenRequest(
	  hconnection,
	  task->verb,
	  task->path,
	  NULL,
	  WINHTTP_NO_REFERER,
	  WINHTTP_DEFAULT_ACCEPT_TYPES,
	  WINHTTP_FLAG_SECURE);
	if (!hrequest)
	{
		LOG_ERR("HttpOpenRequest");
		goto err_w_connection;
	}

	LOG("Sending request (payload: %zuB)...", task->payload_bytes);
	BOOL ok = WinHttpSendRequest(
	  hrequest,
	  task->header,
	  (DWORD)-1,
	  task->payload,
	  (DWORD)task->payload_bytes,
	  (DWORD)task->payload_bytes,
	  1);
	if (!ok)
	{
		LOG_ERR("HttpSendRequest");
		goto err_w_request;
	}

	LOG("Waiting for response...");
	ok = WinHttpReceiveResponse(hrequest, NULL);
	if (!ok)
	{
		LOG_ERR("HttpReceiveResponse");
		goto err_w_request;
	}

	LOG("Query headers...");
	DWORD status_code;
	DWORD sc_bytes = sizeof status_code;
	ok = WinHttpQueryHeaders(
	  hrequest,
	  WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
	  WINHTTP_HEADER_NAME_BY_INDEX,
	  &status_code,
	  &sc_bytes,
	  WINHTTP_NO_HEADER_INDEX);
	if (!ok)
	{
		LOG_ERR("HttpQueryHeaders");
		goto err_w_request;
	}
	LOG("status code of response: %lu", status_code);

	// read headers
	DWORD header_bytes;
	ok = WinHttpQueryHeaders(
	  hrequest,
	  WINHTTP_QUERY_RAW_HEADERS_CRLF,
	  WINHTTP_HEADER_NAME_BY_INDEX,
	  WINHTTP_NO_OUTPUT_BUFFER,
	  &header_bytes,
	  WINHTTP_NO_HEADER_INDEX);
	ASSERT(ok == FALSE);
	LOG("header-bytes: %lu", header_bytes);

	LPWSTR header_buffer = malloc(header_bytes);
	ok = WinHttpQueryHeaders(
	  hrequest,
	  WINHTTP_QUERY_RAW_HEADERS_CRLF,
	  WINHTTP_HEADER_NAME_BY_INDEX,
	  header_buffer,
	  &header_bytes,
	  WINHTTP_NO_HEADER_INDEX);
	ASSERT(ok == TRUE);

	char *header_utf8 = utf8_from_utf16(header_buffer, NULL);
	free(header_buffer);

	struct netw_header hdr = { 0 };
	netw_header_from_raw_header(&hdr, header_utf8);

	LOG("Read content...");
	uint8_t *buffer = NULL;
	if (task->file)
	{
		buffer = malloc(BUFFERSIZE);
		DWORD avail_bytes = 0;
		do
		{
			ok = WinHttpQueryDataAvailable(hrequest, &avail_bytes);
			if (!ok)
			{
				LOG_ERR("HttpQueryDataAvailable");
				goto err_w_request;
			}
			if (avail_bytes > 0)
			{
				DWORD m = avail_bytes <= BUFFERSIZE ? avail_bytes : BUFFERSIZE;
				DWORD actual_bytes_read = 0;
				WinHttpReadData(hrequest, buffer, m, &actual_bytes_read);
				LOG("Read %lu from %lu bytes", actual_bytes_read, avail_bytes);
				size_t nitems =
				  fwrite(buffer, actual_bytes_read, 1, task->file);
				ASSERT(nitems == 1);
			}
		} while (avail_bytes > 0);

		random_delay();
		task->callback
		  .download(task->udata, task->file, (int)status_code, &hdr);
	}
	else
	{
		size_t bytes = 0;
		DWORD avail_bytes = 0;
		do
		{
			ok = WinHttpQueryDataAvailable(hrequest, &avail_bytes);
			if (!ok)
			{
				LOG_ERR("HttpQueryDataAvailable");
				goto err_w_request;
			}
			if (avail_bytes > 0)
			{
				buffer = realloc(buffer, bytes + avail_bytes);
				DWORD actual_bytes = 0;
				WinHttpReadData(
				  hrequest,
				  buffer + bytes,
				  avail_bytes,
				  &actual_bytes);
				bytes += actual_bytes;
				LOG("Read %lu from %lu bytes", actual_bytes, avail_bytes);
			}
		} while (avail_bytes > 0);

		random_delay();
		task->callback
		  .request(task->udata, buffer, bytes, (int)status_code, &hdr);
	}

	// free local data
	WinHttpCloseHandle(hrequest);
	WinHttpCloseHandle(hconnection);

	free_netw_header(&hdr);
	free(buffer);

	// free task data
	free(task->host);
	free(task->path);
	free(task->header);
	free(task->payload);

	// free actual task
	free(task);

	return 0;

err_w_request:
	WinHttpCloseHandle(hrequest);

err_w_connection:
	WinHttpCloseHandle(hconnection);

err_early:
	if (task->file)
	{
		task->callback.download(task->udata, task->file, -1, NULL);
	}
	else
	{
		task->callback.request(task->udata, NULL, 0, -1, NULL);
	}

	// free task data
	free(task->host);
	free(task->path);
	free(task->header);
	free(task->payload);

	// free actual task
	free(task);

	return 1;
}


static wchar_t const *l_verbs[] = { L"GET", L"POST", L"PUT", L"DELETE" };


bool
netw_request(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const in_headers[],
  void const *body,
  size_t nbody_bytes,
  netw_request_callback in_callback,
  void *in_userdata)
{
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		in_callback(in_userdata, NULL, 0, 500, NULL);
		return true;
	}

	LOG("request: %s", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->callback.request = in_callback;
	task->udata = in_userdata;
	task->verb = l_verbs[in_verb];
	if (body && nbody_bytes > 0)
	{
		task->payload = memdup(body, nbody_bytes);
		task->payload_bytes = nbody_bytes;
	}

	// convert/extract URI information
	size_t urilen = 0;
	wchar_t *uri = utf16_from_utf8(in_uri, &urilen);
	URL_COMPONENTS url_components = {
		.dwStructSize = sizeof url_components,
		.dwHostNameLength = (DWORD)-1,
		.dwUrlPathLength = (DWORD)-1,
	};
	WinHttpCrackUrl(uri, (DWORD)urilen, 0, &url_components);

	task->port = url_components.nPort;
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);

	free(uri);

	// combine/convert headers
	if (in_headers)
	{
		char *header = combine_headers(in_headers, NULL);
		task->header = utf16_from_utf8(header, NULL);
		free(header);
	}

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		LOGE("failed to create thread");
	}

	return true;
}


bool
netw_download_to(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const in_headers[],
  void const *in_body,
  size_t in_nbytes,
  FILE *fout,
  netw_download_callback in_callback,
  void *in_userdata)
{
	ASSERT(fout);
	if (l_netw.error_rate > 0 && is_random_server_error())
	{
		LOG("Failing request: %s", in_uri);
		in_callback(in_userdata, fout, 500, NULL);
		return true;
	}
	LOG("download_request: %s", in_uri);

	struct task *task = calloc(sizeof *task, 1);
	task->callback.download = in_callback;
	task->udata = in_userdata;
	task->verb = l_verbs[in_verb];
	task->file = fout;
	if (in_body && in_nbytes > 0)
	{
		task->payload = memdup(in_body, in_nbytes);
		task->payload_bytes = in_nbytes;
	}

	// convert/extract URI information
	size_t urilen = 0;
	wchar_t *uri = utf16_from_utf8(in_uri, &urilen);
	URL_COMPONENTS url_components = {
		.dwStructSize = sizeof url_components,
		.dwHostNameLength = (DWORD)-1,
		.dwUrlPathLength = (DWORD)-1,
	};
	WinHttpCrackUrl(uri, (DWORD)urilen, 0, &url_components);

	task->port = url_components.nPort;
	task->host =
	  wcstrndup(url_components.lpszHostName, url_components.dwHostNameLength);
	task->path =
	  wcstrndup(url_components.lpszUrlPath, url_components.dwUrlPathLength);

	free(uri);

	// combine/convert headers
	if (in_headers)
	{
		char *header = combine_headers(in_headers, NULL);
		task->header = utf16_from_utf8(header, NULL);
		free(header);
	}

	HANDLE h = CreateThread(NULL, 0, task_handler, task, 0, NULL);
	if (h)
	{
		CloseHandle(h);
	}
	else
	{
		LOGE("failed to create thread");
	}

	return true;
}


void
netw_set_error_rate(int in_percentage)
{
	ASSERT(in_percentage >= 0 && in_percentage <= 100);
	l_netw.error_rate = in_percentage;
}


void
netw_set_delay(int in_min, int in_max)
{
	ASSERT(in_min >= 0);
	ASSERT(in_max >= in_min);
	l_netw.min_delay = in_min;
	l_netw.max_delay = in_max;
}


char const *
netw_get_header(struct netw_header const *in_hdr, char const *in_key)
{
	for (size_t i = 0; i < in_hdr->nkeys; ++i)
	{
		if (_stricmp(in_key, in_hdr->keys[i]) == 0)
		{
			return in_hdr->values[i];
		}
	}
	return NULL;
}
