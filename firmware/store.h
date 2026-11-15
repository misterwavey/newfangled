/* SPDX-License-Identifier: MIT */
#ifndef NEWFANGLED_STORE_H
#define NEWFANGLED_STORE_H
#ifdef BIA_BUILD
#include "bia/settings.h"
#else
#include "settings.h"
#endif
typedef struct {
    void *context;
    bool (*read)(void *,unsigned,void *,unsigned);
    bool (*erase)(void *,unsigned); /* clear app-local bank 0 or 1 to FF, 4096 bytes */
    bool (*program)(void *,unsigned,const void *,unsigned); /* write bytes; journal only programs cleared records */
} NfStoreIo;
typedef enum {NF_STORE_OK,NF_STORE_EMPTY,NF_STORE_CORRUPT,NF_STORE_IO,NF_STORE_FUTURE,NF_STORE_INVALID} NfStoreResult;
typedef struct {
    NfStoreIo io;
    uint32_t generation,sequence,slot_sequence[NF_PRESET_SLOTS];
    uint16_t slot_address[NF_PRESET_SLOTS];
    uint16_t record_bytes;
    uint8_t mask,last_slot;int8_t bank;
    bool future;
    uint8_t buffer[NF_RECORD_BYTES];
} NfStore;
uint32_t nf_store_crc32(const void *bytes,unsigned length);
NfStoreResult nf_store_open(NfStore *store,NfStoreIo io);
NfStoreResult nf_store_load(NfStore *store,unsigned slot,NfSettings *settings);
NfStoreResult nf_store_save(NfStore *store,unsigned slot,const NfSettings *settings);
/* UI request handling is independent of hardware. Caller serializes app_sync
 * after a load and resets clock acquisition after an I/O operation. */
bool nf_presets_execute(NfStore *store,OriginalUi *ui,bool disarmed);
bool nf_presets_boot(NfStore *store,OriginalUi *ui);
#endif
