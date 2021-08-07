#include "netw.h"

#include <stdlib.h>


static bool
is_unreserved(char x)
{
	/* clang-format off */
	return (x >= 'A' && x <= 'Z')
		|| (x >= 'a' && x <= 'z')
		|| (x >= '0' && x <= '9')
		|| x == '-'
		|| x == '_'
		|| x == '.'
		|| x == '~';
	/* clang-format on */
}


char *
netw_percent_encode(char const *input, size_t len, size_t *out_len)
{
	char const nibbles[] = "0123456789ABCDEF";
	char *output, *o;

	size_t nlen = 0;
	{
		size_t i;
		for (i = 0; i < len; ++i) {
			nlen += is_unreserved(input[i]) ? 1 : 3;
		}
	}
	output = malloc(nlen + 1); /* NUL terminate */
	o = output;

	{
		size_t i;
		for (i = 0; i < len; ++i) {
			if (is_unreserved(input[i])) {
				*(o++) = input[i];
			} else {
				*(o++) = '%';
				*(o++) = nibbles[(input[i] >> 4) & 0xf];
				*(o++) = nibbles[input[i] & 0xf];
			}
		}
	}
	*o = '\0';

	if (out_len)
	{
		*out_len = (size_t)(o - output);
	}

	return output;
}
