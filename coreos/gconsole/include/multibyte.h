/* $Id$ */
#ifndef MULTIBYTE_H__
#define MULTIBYTE_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct con_mbstate_t con_mbstate_t;
struct con_mbstate_t
{
	char spare[2];
	int num_spare;
};

void ConInitMbState(con_mbstate_t *state);
bool ConIsLeadByte(char ch);
size_t ConMultiByteToWideChar(wchar_t *dest, const char *src, 
							  size_t src_bytes, con_mbstate_t *state);

#ifdef __cplusplus
}
#endif

#endif
