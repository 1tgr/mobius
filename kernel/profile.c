/* $Id: profile.c,v 1.1 2002/12/21 09:49:27 pavlovskii Exp $ */

#include <kernel/fs.h>
#include <kernel/profile.h>
#include <os/hash.h>
#include <wchar.h>
#include <stdio.h>

/*#define DEBUG*/
#include <kernel/debug.h>

typedef hash_table profile_key_t;
typedef struct profile_value_t profile_value_t;
struct profile_value_t
{
    bool is_subkey;
    union
    {
	wchar_t *value;
	profile_key_t *subkey;
    } u;
};

profile_key_t *pro_root;

static void ProAddValue(profile_key_t *key, const wchar_t *name, const wchar_t *value)
{
    profile_value_t *val;
    val = malloc(sizeof(profile_value_t));
    val->is_subkey = false;
    val->u.value = _wcsdup(value);
    HashInsertItem(key, val, name);
}

static void ProAddKey(profile_key_t *key, const wchar_t *name, profile_key_t *child)
{
    profile_value_t *val;
    val = malloc(sizeof(profile_value_t));
    val->is_subkey = true;
    val->u.subkey = child;
    HashInsertItem(key, val, name);
}

static profile_key_t *ProOpenSubkey(profile_key_t *root, wchar_t *key)
{
    wchar_t *ch;
    profile_value_t *value;

    ch = wcschr(key, '/');
    if (ch == NULL)
    {
	/*TRACE1("ProOpenSubkey: end: %s\n", key);*/
        if (*key == '\0')
            return root;
        else
        {
	    HashSearch(root, key);
	    if (root->found)
	    {
	        value = item(root);
	        return value->u.subkey;
	    }
	    else
	        return NULL;
        }
    }
    else
    {
	*ch = '\0';
	ch++;
	/*TRACE2("ProOpenSubkey: looking for %s in %s\n", ch, key);*/
	if (*key == '\0')
	    return root;
	else
	{
	    HashSearch(root, key);
	    if (root->found)
	    {
		value = item(root);
		return ProOpenSubkey(value->u.subkey, ch);
	    }
	    else
		return NULL;
	}
    }
}

static profile_key_t *ProOpenKey(const wchar_t *key)
{
    wchar_t *temp;
    profile_key_t *k;

    if (pro_root == NULL)
	pro_root = HashCreate(256);

    temp = _wcsdup(key);
    k = ProOpenSubkey(pro_root, temp);
    free(temp);
    return k;
}

static wchar_t *ProParseBlock(profile_key_t *parent, wchar_t *line)
{
    wchar_t *space, *eol, *next, *equals;
    profile_key_t *child;

    while (*line != '\0')
    {
	while (*line != '\0' &&
	    iswspace(*line))
	    line++;

	if (*line == '\0')
	    break;

	eol = wcschr(line, '\n');
	if (eol == NULL)
	    eol = line + wcslen(line);
	else
	    *eol = '\0';

	space = wcschr(line, '\r');
	if (space != NULL)
	    *space = '\0';

	next = eol + 1;

	equals = wcschr(line, '=');
	if (line[0] == ';' ||
	    line[0] == '#' ||
	    line[0] == '\0')
	    TRACE0("ProParseBlock: comment\n");
	else if (equals == NULL)
	{
	    space = wcschr(line, ' ');
	    if (space == NULL)
		space = line;
	    else
	    {
		*space = '\0';
		space++;
	    }
	    
	    if (_wcsicmp(line, L"end") == 0)
	    {
		TRACE0("ProParseBlock: end of block\n");
		return next;
	    }
	    else if (_wcsicmp(line, L"key") == 0)
	    {
		TRACE1("ProParseBlock: key %s\n", space);
		child = HashCreate(256);
		ProAddKey(parent, space, child);
		next = ProParseBlock(child, next);
	    }
	}
	else
	{
	    *equals = '\0';
	    equals++;
	    TRACE2("ProParseBlock: value %s = %s\n", line, equals);
	    ProAddValue(parent, line, equals);
	}

	line = next;
    }

    return line;
}

bool ProLoadProfile(const wchar_t *file, const wchar_t *root)
{
    profile_key_t *key;
    handle_t fd;
    dirent_standard_t di;
    char *buf;
    wchar_t *wbuf;
    size_t size;
    
    if (!FsQueryFile(file, FILE_QUERY_STANDARD, &di, sizeof(di)))
	return false;

    wbuf = malloc(sizeof(wchar_t) * (di.length + 1));
    if (wbuf == NULL)
    	return false;
    
    buf = malloc(di.length + 1);
    if (buf == NULL)
    {
	free(wbuf);
	return false;
    }

    fd = FsOpen(file, FILE_READ);
    if (fd == NULL)
    {
	free(wbuf);
	free(buf);
	return false;
    }

    FsReadSync(fd, buf, di.length, &size);
    HndClose(NULL, fd, 'file');

    size = mbstowcs(wbuf, buf, size);
    free(buf);
    if (size != -1)
	wbuf[size] = '\0';

    key = ProOpenKey(root);
    ProParseBlock(key, wbuf);
    free(wbuf);
    TRACE0("ProLoadProfile: end of file\n");
    return true;
}

bool ProGetBoolean(const wchar_t *key, const wchar_t *value, bool _default)
{
    profile_key_t *k;
    profile_value_t *v;

    k = ProOpenKey(key);
    if (k == NULL)
    {
	TRACE1("ProGetBoolean: key %s not found\n", key);
	return _default;
    }

    HashSearch(k, value);
    if (k->found)
    {
	v = item(k);
	if (_wcsicmp(v->u.value, L"true") == 0 || 
	    _wcsicmp(v->u.value, L"1") == 0 || 
	    _wcsicmp(v->u.value, L"yes") == 0)
	    return true;
	else if (_wcsicmp(v->u.value, L"false") == 0 || 
	    _wcsicmp(v->u.value, L"0") == 0 || 
	    _wcsicmp(v->u.value, L"no") == 0)
	    return false;
	else
	    return _default;
    }
    else
    {
	TRACE1("ProGetBoolean: value %s not found\n", value);
	return _default;
    }
}

const wchar_t *ProGetString(const wchar_t *key, const wchar_t *value, const wchar_t *_default)
{
    profile_key_t *k;
    profile_value_t *v;

    k = ProOpenKey(key);
    if (k == NULL)
    {
	TRACE1("ProGetString: key %s not found\n", key);
	return _default;
    }

    HashSearch(k, value);
    if (k->found)
    {
	v = item(k);
	return v->u.value;
    }
    else
    {
	TRACE1("ProGetString: value %s not found\n", value);
	return _default;
    }
}

bool ProEnumValues(const wchar_t *keyname, void *param, 
		   bool (*func)(void *, const wchar_t *, const wchar_t *))
{
    profile_key_t *k;
    profile_value_t *v;

    k = ProOpenKey(keyname);
    if (k == NULL)
    {
	TRACE1("ProEnumValues: key %s not found\n", keyname);
	return false;
    }

    start(k);
    while (!k->off)
    {
	v = item(k);
	
	if (v->is_subkey)
	    func(param, HashGetKey(k), NULL);
	else
	    func(param, HashGetKey(k), v->u.value);

	forth(k);
    }

    return true;
}
