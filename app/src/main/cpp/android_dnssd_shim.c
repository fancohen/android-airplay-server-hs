/*
 * Android dnssd.h implementation, registration via NsdManager
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "dnssd.h"
#include "dnssdint.h"
#include "global.h"
#include "utils.h"

#define MAX_SERVNAME 256
#define MAX_TXT_ENTRIES 32
#define MAX_TXT_KEY 32
#define MAX_TXT_VAL 128
#define MAX_TXT_WIRE 2048

typedef struct {
    char key[MAX_TXT_KEY];
    char val[MAX_TXT_VAL];
} txt_entry_t;

typedef struct {
    txt_entry_t entries[MAX_TXT_ENTRIES];
    int count;
} txt_record_t;

struct dnssd_s {
    txt_record_t raop_record;
    txt_record_t airplay_record;

    char *name;
    int name_len;
    char *hw_addr;
    int hw_addr_len;
    char *pk;

    uint32_t features1;
    uint32_t features2;
    unsigned char pin_pw;

    char codec_cn[16]; /* dynamic "cn" value, e.g. "0,1,2,3" */

    char raop_servname[MAX_SERVNAME];

    /* wire-format copies of the txt records, returned by GET /info txtAirPlay/txtRAOP */
    char raop_txt[MAX_TXT_WIRE];
    char airplay_txt[MAX_TXT_WIRE];
};

static void _txt_set(txt_record_t *rec, const char *key, const char *val) {
    for (int i = 0; i < rec->count; i++) {
        if (strcmp(rec->entries[i].key, key) == 0) {
            strncpy(rec->entries[i].val, val, MAX_TXT_VAL - 1);
            return;
        }
    }
    if (rec->count < MAX_TXT_ENTRIES) {
        strncpy(rec->entries[rec->count].key, key, MAX_TXT_KEY - 1);
        strncpy(rec->entries[rec->count].val, val, MAX_TXT_VAL - 1);
        rec->count++;
    }
}

dnssd_t *
dnssd_init(const char *name, int name_len, const char *hw_addr, int hw_addr_len, int *error, unsigned char pin_pw)
{
    if (error) *error = DNSSD_ERROR_NOERROR;
    dnssd_t *dnssd = (dnssd_t *) calloc(1, sizeof(dnssd_t));
    if (dnssd) {
        dnssd->name = strndup(name, name_len);
        dnssd->hw_addr = malloc(hw_addr_len);
    }
    if (!dnssd || !dnssd->name || !dnssd->hw_addr) {
        dnssd_destroy(dnssd);
        if (error) *error = DNSSD_ERROR_OUTOFMEM;
        return NULL;
    }
    dnssd->name_len = name_len;
    memcpy(dnssd->hw_addr, hw_addr, hw_addr_len);
    dnssd->hw_addr_len = hw_addr_len;
    dnssd->features1 = (uint32_t) strtoul(FEATURES_1, NULL, 16);
    dnssd->features2 = (uint32_t) strtoul(FEATURES_2, NULL, 16);
    dnssd->pin_pw = pin_pw;
    strncpy(dnssd->codec_cn, RAOP_CN, sizeof(dnssd->codec_cn) - 1);
    return dnssd;
}

void
dnssd_destroy(dnssd_t *dnssd)
{
    if (dnssd) {
        free(dnssd->name);
        free(dnssd->hw_addr);
        free(dnssd);
    }
}

int
dnssd_register_raop(dnssd_t *dnssd, unsigned short port)
{
    char features[22] = {0};
    assert(dnssd);

    snprintf(features, sizeof(features), "0x%X,0x%X", dnssd->features1, dnssd->features2);

    txt_record_t *rec = &dnssd->raop_record;
    rec->count = 0;

    _txt_set(rec, "ch", RAOP_CH);
    _txt_set(rec, "cn", dnssd->codec_cn);
    _txt_set(rec, "da", RAOP_DA);
    _txt_set(rec, "et", RAOP_ET);
    _txt_set(rec, "vv", RAOP_VV);
    _txt_set(rec, "ft", features);
    _txt_set(rec, "am", GLOBAL_MODEL);
    _txt_set(rec, "md", RAOP_MD);
    _txt_set(rec, "rhd", RAOP_RHD);

    switch (dnssd->pin_pw) {
    case 2:
    case 3:
        _txt_set(rec, "pw", "true");
        _txt_set(rec, "sf", "0x84");
        break;
    case 1:
        _txt_set(rec, "pw", "true");
        _txt_set(rec, "sf", "0x8c");
        break;
    default:
        _txt_set(rec, "pw", "false");
        _txt_set(rec, "sf", RAOP_SF);
        break;
    }

    _txt_set(rec, "sr", RAOP_SR);
    _txt_set(rec, "ss", RAOP_SS);
    _txt_set(rec, "sv", RAOP_SV);
    _txt_set(rec, "tp", RAOP_TP);
    _txt_set(rec, "txtvers", RAOP_TXTVERS);
    _txt_set(rec, "vs", RAOP_VS);
    _txt_set(rec, "vn", RAOP_VN);
    if (dnssd->pk) {
        _txt_set(rec, "pk", dnssd->pk);
    }

    /* Build service name: HW@Name */
    if (utils_hwaddr_raop(dnssd->raop_servname, sizeof(dnssd->raop_servname),
                          dnssd->hw_addr, dnssd->hw_addr_len) < 0) {
        return -1;
    }
    strncat(dnssd->raop_servname, "@", sizeof(dnssd->raop_servname) - strlen(dnssd->raop_servname) - 1);
    strncat(dnssd->raop_servname, dnssd->name, sizeof(dnssd->raop_servname) - strlen(dnssd->raop_servname) - 1);
    return 0;
}

int
dnssd_register_airplay(dnssd_t *dnssd, unsigned short port)
{
    char device_id[3 * MAX_HWADDR_LEN];
    char features[22] = {0};
    assert(dnssd);

    snprintf(features, sizeof(features), "0x%X,0x%X", dnssd->features1, dnssd->features2);

    if (utils_hwaddr_airplay(device_id, sizeof(device_id), dnssd->hw_addr, dnssd->hw_addr_len) < 0) {
        return -1;
    }

    txt_record_t *rec = &dnssd->airplay_record;
    rec->count = 0;

    _txt_set(rec, "deviceid", device_id);
    _txt_set(rec, "features", features);

    switch (dnssd->pin_pw) {
    case 1:
    case 2:
    case 3:
        _txt_set(rec, "pw", "true");
        break;
    default:
        _txt_set(rec, "pw", "false");
        break;
    }
    _txt_set(rec, "flags", "0x4");
    _txt_set(rec, "model", GLOBAL_MODEL);
    if (dnssd->pk) {
        _txt_set(rec, "pk", dnssd->pk);
    }
    _txt_set(rec, "pi", AIRPLAY_PI);
    _txt_set(rec, "srcvers", AIRPLAY_SRCVERS);
    _txt_set(rec, "vv", AIRPLAY_VV);
    return 0;
}

/* unregisters with NsdManager */
void dnssd_unregister_raop(dnssd_t *dnssd) {}
void dnssd_unregister_airplay(dnssd_t *dnssd) {}

/* serialize to DNS TXT wire format (length-prefixed "key=value" strings); older
   macOS senders request this via GET /info and drop the connection if it is empty */
static int _txt_wire(const txt_record_t *rec, char *buf, int cap) {
    int pos = 0;
    for (int i = 0; i < rec->count; i++) {
        int klen = (int) strlen(rec->entries[i].key);
        int vlen = (int) strlen(rec->entries[i].val);
        int n = klen + 1 + vlen;
        if (n > 255 || pos + 1 + n > cap) break;
        buf[pos++] = (char) n;
        memcpy(buf + pos, rec->entries[i].key, klen);
        pos += klen;
        buf[pos++] = '=';
        memcpy(buf + pos, rec->entries[i].val, vlen);
        pos += vlen;
    }
    return pos;
}

const char *dnssd_get_raop_txt(dnssd_t *dnssd, int *length) {
    int len = _txt_wire(&dnssd->raop_record, dnssd->raop_txt, sizeof(dnssd->raop_txt));
    if (length) *length = len;
    return dnssd->raop_txt;
}

const char *dnssd_get_airplay_txt(dnssd_t *dnssd, int *length) {
    int len = _txt_wire(&dnssd->airplay_record, dnssd->airplay_txt, sizeof(dnssd->airplay_txt));
    if (length) *length = len;
    return dnssd->airplay_txt;
}

const char *dnssd_get_name(dnssd_t *dnssd, int *length) {
    *length = dnssd->name_len;
    return dnssd->name;
}

const char *dnssd_get_hw_addr(dnssd_t *dnssd, int *length) {
    *length = dnssd->hw_addr_len;
    return dnssd->hw_addr;
}

void dnssd_set_pk(dnssd_t *dnssd, char *pk_str) {
    dnssd->pk = pk_str;
}

void dnssd_set_airplay_features(dnssd_t *dnssd, int bit, int val) {
    if (bit < 0 || bit > 63 || val < 0 || val > 1) return;
    uint32_t *features = bit < 32 ? &dnssd->features1 : &dnssd->features2;
    uint32_t mask = 1u << (bit % 32);
    *features = val ? (*features | mask) : (*features & ~mask);
}

uint64_t dnssd_get_airplay_features(dnssd_t *dnssd) {
    return ((uint64_t) dnssd->features2 << 32) | dnssd->features1;
}

/* --- Android-specific accessors for JNI layer --- */

int android_dnssd_get_raop_txt_count(dnssd_t *dnssd) {
    return dnssd->raop_record.count;
}

const char *android_dnssd_get_raop_txt_key(dnssd_t *dnssd, int index) {
    txt_record_t *rec = &dnssd->raop_record;
    if (index < 0 || index >= rec->count) return NULL;
    return rec->entries[index].key;
}

const char *android_dnssd_get_raop_txt_val(dnssd_t *dnssd, int index) {
    txt_record_t *rec = &dnssd->raop_record;
    if (index < 0 || index >= rec->count) return NULL;
    return rec->entries[index].val;
}

int android_dnssd_get_airplay_txt_count(dnssd_t *dnssd) {
    return dnssd->airplay_record.count;
}

const char *android_dnssd_get_airplay_txt_key(dnssd_t *dnssd, int index) {
    txt_record_t *rec = &dnssd->airplay_record;
    if (index < 0 || index >= rec->count) return NULL;
    return rec->entries[index].key;
}

const char *android_dnssd_get_airplay_txt_val(dnssd_t *dnssd, int index) {
    txt_record_t *rec = &dnssd->airplay_record;
    if (index < 0 || index >= rec->count) return NULL;
    return rec->entries[index].val;
}

void android_dnssd_set_codecs(dnssd_t *dnssd, int alac, int aac) {
    /* Build cn string: 0=PCM (always), 1=ALAC, 2=AAC, 3=AAC-ELD */
    char buf[16];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "0");
    if (alac) pos += snprintf(buf + pos, sizeof(buf) - pos, ",1");
    if (aac) pos += snprintf(buf + pos, sizeof(buf) - pos, ",2,3");
    strncpy(dnssd->codec_cn, buf, sizeof(dnssd->codec_cn) - 1);
}

const char *android_dnssd_get_raop_servname(dnssd_t *dnssd) {
    return dnssd->raop_servname;
}
