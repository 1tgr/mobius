/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <libc/stubs.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
/*#include <libc/bss.h>*/
#include <os/syscall.h>
#include <os/defs.h>

static char *tmp_dir;
static int tmp_len;
/*static int tmp_bss_count = -1;*/

/*static void
try(const char *var)
{
  static char buf[L_tmpnam];

  char *t = getenv(var);
  if (t == 0)
    return;

  tmp_len = strlen(t);
  strcpy(buf, t);
  if (buf[tmp_len - 1] != '/' && buf[tmp_len - 1] != '\\')
    buf[tmp_len++] = '/', buf[tmp_len] = 0;

  if (access(buf, 0))
    return;

  tmp_dir = buf;
}*/

char *
tmpnam(char *s)
{
  static char static_buf[L_tmpnam];
  static wchar_t wcs_buf[L_tmpnam];
  static char tmpcount[] = "dj000000";
  int i;
  size_t len;

  /*if (tmp_bss_count != __bss_count)
  {
    tmp_bss_count = __bss_count;*/

    /*if (tmp_dir == 0) try("TMPDIR");
    if (tmp_dir == 0) try("TEMP");
    if (tmp_dir == 0) try("TMP");*/
    if (tmp_dir == 0)
    {
      static char def[] = "c:/";
      tmp_dir = def;
      tmp_len = 3;
    }
  /*}*/

  if (!s)
    s = static_buf;
  strcpy(s, tmp_dir);

  do {
    /* increment the "count" starting at the first digit (backwards order) */
    for (i=2; tmpcount[i] == '9' && i < 8; tmpcount[i] = '0', i++);
    if (i < 8)
      tmpcount[i]++;

    strcpy(s+tmp_len, tmpcount);
    len = mbstowcs(wcs_buf, s, _countof(wcs_buf) - 1);
    if (len == -1)
        len = 0;
    wcs_buf[len] = '\0';

  } while (!FsQueryFile(wcs_buf, FILE_QUERY_NONE, NULL, 0)); /* until file doesn't exist */

  return s;
}
