/* $Id: profile.c,v 1.1.1.1 2002/12/21 09:49:27 pavlovskii Exp $ */

#include <kernel/fs.h>
#include <kernel/profile.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>

/*#define DEBUG*/
#include <kernel/debug.h>

typedef struct profile_key_t profile_key_t;
typedef struct profile_value_t profile_value_t;


struct profile_key_t
{
    spinlock_t spin;
    unsigned num_buckets;
    profile_value_t *buckets[1];
};


struct profile_value_t
{
    profile_value_t *hash_next;
    bool is_subkey;
    union
    {
        wchar_t *value;
        profile_key_t *subkey;
    } u;
    wchar_t name[1];
};

profile_key_t *pro_root;


static unsigned ProGetHash(profile_key_t *key, const wchar_t *name)
{
    unsigned ret;
    int i;

    ret = 0;
    while (*name != '\0')
    {
        i = (int) towlower(*name);
        ret ^= i;
        ret <<= 1;
        name++;
    }

    return ret % key->num_buckets;
}


static profile_key_t *ProCreateKey(void)
{
    profile_key_t *key;
    key = malloc(sizeof(*key) + sizeof(profile_value_t*) * 17);
    memset(key, 0, sizeof(*key) + sizeof(profile_value_t*) * 17);
    key->num_buckets = 17;
    return key;
}


static void ProAddItem(profile_key_t *key, profile_value_t *value)
{
    unsigned hash;

    hash = ProGetHash(key, value->name);
    assert(hash < key->num_buckets);

    SpinAcquire(&key->spin);
    value->hash_next = key->buckets[hash];
    key->buckets[hash] = value;
    SpinRelease(&key->spin);
}


static profile_value_t *ProFindItem(profile_key_t *key, const wchar_t *name)
{
    unsigned hash;
    profile_value_t *value;

    hash = ProGetHash(key, name);
    SpinAcquire(&key->spin);
    for (value = key->buckets[hash]; value != NULL; value = value->hash_next)
        if (_wcsicmp(value->name, name) == 0)
        {
            SpinRelease(&key->spin);
            return value;
        }

    SpinRelease(&key->spin);
    return NULL;
}


static void ProAddValue(profile_key_t *key, const wchar_t *name, const wchar_t *value)
{
    profile_value_t *val;
    val = malloc(sizeof(*val) + wcslen(name) * sizeof(wchar_t));
    val->is_subkey = false;
    val->u.value = _wcsdup(value);
    wcscpy(val->name, name);
    ProAddItem(key, val);
}


static void ProAddKey(profile_key_t *key, const wchar_t *name, profile_key_t *child)
{
    profile_value_t *val;
    val = malloc(sizeof(*val) + wcslen(name) * sizeof(wchar_t));
    val->is_subkey = true;
    val->u.subkey = child;
    wcscpy(val->name, name);
    ProAddItem(key, val);
}


static profile_key_t *ProOpenSubkey(profile_key_t *root, wchar_t *key)
{
    wchar_t *ch;
    profile_value_t *value;

    ch = wcschr(key, '/');
    if (ch == NULL)
    {
        if (*key == '\0')
            return root;
        else
        {
            value = ProFindItem(root, key);
            if (value == NULL)
                return NULL;

            assert(value->is_subkey);
            return value->u.subkey;
        }
    }
    else
    {
        *ch = '\0';
        ch++;
        if (*key == '\0')
            return root;
        else
        {
            value = ProFindItem(root, key);
            if (value == NULL)
                return NULL;

            assert(value->is_subkey);
            return ProOpenSubkey(value->u.subkey, ch);
        }
    }
}


static profile_key_t *ProOpenKey(const wchar_t *key)
{
    wchar_t *temp;
    profile_key_t *k;

    if (pro_root == NULL)
        pro_root = ProCreateKey();

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
                child = ProCreateKey();
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
    file_handle_t *fd;
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

    fd = FsOpen(NULL, file, FILE_READ);
    if (fd == NULL)
    {
        free(wbuf);
        free(buf);
        return false;
    }

    FsRead(fd, buf, 0, di.length, &size);
    HndClose(&fd->hdr);

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

    v = ProFindItem(k, value);
    if (v == NULL)
    {
        TRACE1("ProGetBoolean: value %s not found\n", value);
        return _default;
    }
    else
    {
        assert(!v->is_subkey);
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

    v = ProFindItem(k, value);
    if (v == NULL)
    {
        TRACE1("ProGetString: value %s not found\n", value);
        return _default;
    }
    else
    {
        assert(!v->is_subkey);
        return v->u.value;
    }
}


bool ProEnumValues(const wchar_t *keyname, void *param, 
                   bool (*func)(void *, const wchar_t *, const wchar_t *))
{
    profile_key_t *k;
    profile_value_t *v;
    unsigned hash;

    k = ProOpenKey(keyname);
    if (k == NULL)
    {
        TRACE1("ProEnumValues: key %s not found\n", keyname);
        return false;
    }

    for (hash = 0; hash < k->num_buckets; hash++)
        for (v = k->buckets[hash]; v != NULL; v = v->hash_next)
        {
            if (v->is_subkey)
                func(param, v->name, NULL);
            else
                func(param, v->name, v->u.value);
        }

    return true;
}
