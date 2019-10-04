# netw

**netw** is a low-level abstraction layer to make HTTP requests via
system-specific HTTP/network libraries.

- Windows: WinHTTP (system component since Windows XP)
- MacOS: NSURLSession (part of MacOS since 10.9)
- Linux: libcurl
- FreeBSD: libcurl

# Introduction

**netw_request()** and **netw_download_to()** spin off the request
asynchronously.

When the request is completed (either success or failure)
their respective callbacks (**netw_request_callback**,
**netw_download_callback**) are called.

Before using any of **netw**'s request functions in your program **netw_init()**
needs to be called.

# Examples

## Download homepage of github

```c
#include "netw.h"

static void
on_recv(void *in_userdata, void const *in_data, size_t in_bytes, int in_error, struct netw_header const *in_header)
{
    // in_error is the HTTP status code. 200 signals success.
    if (in_error == 200)
    {
      // in_data now contains the homepage of github
    }

    // in_userdata is a pointer to wait in main().
    // Since we are done, set it to 0 for main() to exit.
    *((int *)in_userdata) = 0;
}

int
main(void)
{
    // initialize netw
    if (!netw_init())
    {
        return -1;
    }

    // send a request to fetch the homepage of github and get notified
    // via the callback on_recv when finished.
    int wait = 1;
    netw_request(NETW_VERB_GET, "https://github.com", NULL, NULL, 0, on_recv, &wait);

    // wait until on_recv gives the all-clear
    while (wait)
    {
        // sleep
    }

    // clean-up
    netw_deinit();
}
```
