#include <common.h>

// NOTE(aalhendi): ASM-verified NTSC-U 926 0x800b8810-0x800b885c
char *CS_Credits_GetNextString(char *str)
{
#if defined(CTR_NATIVE)
	if (str == NULL)
	{
		// NOTE(aalhendi): Retail blindly reads the input pointer. Native
		// returns the same "no next string" result when credits epilogue text
		// reaches the end and the next pointer is PS1 null-space.
		return NULL;
	}
#endif

	char c = *str;
	while (c != '\0')
	{
		if (c == '\r')
			return str + 1;
		str++;
		c = *str;
	}
	if (*str != '\r')
		return 0;
	return str + 1;
}
