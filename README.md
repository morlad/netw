# netw
*netw* is a low-level abstraction layer to make HTTP requests via
system-specific HTTP/network libraries.

- Windows: WinHTTP (system component since Windows XP)
- MacOS: NSURLSession (part of MacOS since 10.9)
- Linux: libcurl
- FreeBSD: libcurl

It also allows setting random [delays](#netw_set_delay) and [errors](#netw_set_error_rate) to test the
handeling of those in the client code.

# Quick Start
Before using any of *netw*'s request functions in your program [netw_init()](#netw_init)
needs to be called.

[netw_request()](#netw_request) and [netw_download_to()](#netw_download_to) spin off requests
asynchronously.

When a request is completed (either success or failure)
their respective callbacks ([netw_request_callback](#netw_request_callback),
[netw_download_callback](#netw_download_callback)) are called.

Finally call [netw_deinit()](#netw_deinit) to shut down netw.

# Examples
## Download homepage of github
```c
#include "netw.h"

static void
on_recv(void *in_userdata, void const *in_data, size_t in_bytes, int in_error, struct netw_header const *in_header) {
    // in_error is the HTTP status code. 200 signals success.
    if (in_error == 200) {
      // in_data now contains the homepage of github
    }
    // in_userdata is a pointer to wait in main().
    // Since we are done, set it to 0 for main() to exit.
    *((int *)in_userdata) = 0;
}

int
main(void) {
    // initialize netw
    if (!netw_init()) { return -1; }
    // send a request to fetch the homepage of github and get notified
    // via the callback on_recv when finished.
    int wait = 1;
    netw_request(NETW_VERB_GET, "https://github.com", NULL, NULL, 0, on_recv, &wait);
    // wait until on_recv gives the all-clear
    while (wait) { /* sleep */ }
    // clean-up
    netw_deinit();
}
```

# Usage
## Compilation
To use *netw* in your project integrate the source file for your platform into your build process:
- Windows: `netw-win.c`
- macOS: `netw-macos.m`
- Linux, FreeBSD: `netw-libcurl.c`

If you want to use [netw_percent_encode()](#netw_percent_encode) you also need to integrate
the system agnostic file `netw.c` into your build.

## Linking
*netw* depends on the following libraries:
- Windows: `winhttp.lib`
- macOS: `framework Foundation`
- Linux, FreeBSD: `pthread`, `curl`

# API
* Functions
    - [netw_init()](#netw_init)
    - [netw_deinit()](#netw_deinit)
    - [netw_request()](#netw_request)
    - [netw_download_to()](#netw_download_to)
    - [netw_get_header()](#netw_get_header)
    - [netw_set_error_rate()](#netw_set_error_rate)
    - [netw_set_delay()](#netw_set_delay)
    - [netw_percent_encode()](#netw_percent_encode)
* Callbacks
    - [netw_request_callback()](#netw_request_callback)
    - [netw_download_callback()](#netw_download_callback)
* Enums
    - [netw_verb](#netw_verb)

## Enums

### netw_verb
Available HTTP 'verbs' for [netw_request()](#netw_request) and [netw_download_to()](#netw_download_to)
- NETW_VERB_GET
- NETW_VERB_POST
- NETW_VERB_PUT
- NETW_VERB_DELETE


## Functions

### netw_init()
Initializes internal *netw* data structures.
Needs to be called before any other call to *netw*.

**Note:**
    Do not forget to call [netw_deinit()](#netw_deinit) when you are done.

```c
bool
netw_init(void);
```

### netw_deinit()
Shuts down *netw* and frees all resources.

```c
void
netw_deinit(void);
```

### netw_request()
Sends an HTTP request out into the world (or locally, if you so desire).
On completion *in_callback* is called with the results.

netw_request() stores the HTTP response in-memory, so it is only
really suited for smaller responses, not for downloading files &c.

See [netw_download_to()](#netw_download_to) if you need to download more data.

**Parameters:**
* in_verb - HTTP verb to use. See [netw_verb](#netw_verb)
* in_uri - URI of the request (i.e. "https://github.com/404.html")
* in_headers - See [Headers](#headers) for more
* in_body - any data to be sent as the request-body
* in_nbody_bytes - byte-size of *in_body*
* in_callback - the function to be called when netw_request() finishes
    receiving the response from the server or fails
* in_userdata - userdata handed straight to the callback function
    
```c
bool
netw_request(
  enum netw_verb in_verb,
  char const *in_uri,
  char const *const in_headers[],
  void const *in_body,
  size_t in_nbody_bytes,
  netw_request_callback in_callback,
  void *in_userdata);
```

### netw_download_to()
Same as [netw_request()](#netw_request) but received data is written to FILE* *in_file*
instead of in-memory buffer.

**Parameters:**
 *	in_verb - HTTP verb to use. See [netw_verb](#netw_verb).
 *	in_uri - URI of the request (i.e. "https://github.com/404.html")
 *	in_headers - See [Headers](#headers) for more.
 *	in_body - any data to be sent as the request-body
 *	in_nbody_bytes - byte-size of *in_body*
 *	in_file - needs to be an open file handle with write permissions.
      Either 'w' or 'a'. Keep in mind that 'b' might also be necessary
      depending on what data is downloaded.
 *	in_callback - the function to be called when netw_download_to() finishes
      receiving the response from the server or fails
 *	in_userdata - userdata handed straight to the callback function
    
```c
bool
netw_download_to(
  enum netw_verb verb,
  char const *in_uri,
  char const *const in_headers[],
  void const *in_body,
  size_t in_nbody_bytes,
  FILE *in_file,
  netw_download_callback in_callback,
  void *in_userdata);
```

### netw_get_header()
Can be used to access a HTTP header field during a callback.

**Returns:**
    Value of the requested HTTP header field. NULL if no header with
    that name is part of the response.
    
```c
char const *
netw_get_header(struct netw_header const *header, char const *name);
```

### netw_set_error_rate()
Set percentage of requests resulting in an HTTP status code 500
and no data transmitted.

**Parameters:**
 *  in_percentage - between 0-100. 0 Deactivates this feature.

```c
void
netw_set_error_rate(int in_percentage);
```

### netw_set_delay()
Set range for random delays before a response is received.
Set both values to 0 to deactive this feature.

**Note:**
    in_max needs to be >= in_min.

**Parameters:**
*	in_min - minimum delay in milliseconds.
*	in_max - maximum delay in milliseconds.

```c
void
netw_set_delay(int in_min, int in_max);
```

### netw_percent_encode()
Percent-encode *in_len* bytes of the string at *in_input* into a newly
malloc()ed string, which is the return value.

**Parameters:**
*	in_input - pointer to the data to encode
*	in_len - number of bytes to encode
*	out_len - Can be NULL. Set to the byte-size of the returned string
        w/o terminating NULL

**Returns:**
    A newly allocated (malloc) percent-encoded string.
    Do not forget to free() it.

```c
char *
netw_percent_encode(char const *in_input, size_t in_len, size_t *out_len);
```

## Callbacks

### netw_request_callback()
Called when [netw_request()](#netw_request) has finished receiving a response (or failed).

**Parameters:**
*	in_userdata - in_userdata from [netw_request](#netw_request)
*	in_data - pointer to the data received. Only valid during this call.
        if you want to keep this data around for longer you need to copy it.
*	in_bytes - number of bytes in *in_data*
*	in_error - HTTP status code of the response
*	in_header - To be used in conjunction with [netw_get_header()](#netw_get_header)

```c
typedef void (*netw_request_callback)(
  void *in_userdata,
  void const *in_data,
  size_t in_bytes,
  int error,
  struct netw_header const *in_header);
```
 
### netw_download_callback()
Called when [netw_download_to()](#netw_download_to) has downloaded the data (or failed).
 
**Parameters:**
*	in_userdata - *in_userdata* from [netw_download_to()](#netw_download_to)
*	in_file - the very FILE* specified in [netw_download_to()](#netw_download_to)
*	in_error - HTTP status code of the response
*	in_header - To be used in conjunction with [netw_get_header()](#netw_get_header)

```c
typedef void (*netw_download_callback)(
  void *in_userdata,
  FILE *in_file,
  int in_error,
  struct netw_header const *in_header);
```

## Headers
To specify HTTP headers for a request use a _NULL terminated array_ of
_NULL terminated strings_, with key and value being one element **each**.

```c
char const *const headers[] = {
    "Accept", "application/json",
    "Content-Type", "application/x-www-form-urlencoded",
    NULL
};
```
