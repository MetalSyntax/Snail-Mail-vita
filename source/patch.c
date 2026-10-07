/*
 * Copyright (C) 2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include <psp2/kernel/clib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <zlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb/stb_image.h"

#include <kubridge.h>
#include <so_util/so_util.h>

#include "audio.h"
#include "utils/logger.h"

extern so_module so_mod;

static void hooked_wprintf(const char *fmt, ...) {
    char buf[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    va_end(va);

    // Strip trailing newline if present
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }
    l_info("[Engine] %s", buf);
}

static FILE *s_datFP = NULL;

static void hooked_JNIDatInit(void *env, void *thiz, void *fd_obj, int start, int length) {
    FILE **gDatFP_ptr = (FILE **)so_symbol(&so_mod, "gDatFP");
    void **gDat_ptr = (void **)so_symbol(&so_mod, "gDat");
    int *gJavaAssetStart_ptr = (int *)so_symbol(&so_mod, "gJavaAssetStart");
    int *gJavaAssetLength_ptr = (int *)so_symbol(&so_mod, "gJavaAssetLength");
    void *gDatHash_ptr = (void *)so_symbol(&so_mod, "gDatHash");
    void (*cRHash_Init)(void *hash, int count, void *fn) = (void *)so_symbol(&so_mod, "_ZN6cRHash4InitEiPFPciE");
    void (*cRHash_Add)(void *hash, char *str, int idx) = (void *)so_symbol(&so_mod, "_ZN6cRHash3AddEPci");
    void *DatHashGetString = (void *)so_symbol(&so_mod, "_Z16DatHashGetStringi");

    l_info("Initializing DAT archive from " DATA_PATH "assets/asm.mp3");
    FILE *fp = fopen(DATA_PATH "assets/asm.mp3", "rb");
    if (!fp) {
        fp = fopen(DATA_PATH "asm.mp3", "rb");
    }
    if (!fp) {
        l_error("Failed to open DAT archive asm.mp3!");
        return;
    }

    s_datFP = fp;
    if (gDatFP_ptr) *gDatFP_ptr = fp;
    if (gJavaAssetStart_ptr) *gJavaAssetStart_ptr = 0;

    fseek(fp, 0, SEEK_END);
    long file_len = ftell(fp);
    if (gJavaAssetLength_ptr) *gJavaAssetLength_ptr = (int)file_len;
    fseek(fp, 0, SEEK_SET);

    char hdr[244];
    if (fread(hdr, 1, 244, fp) != 244) {
        l_error("Failed to read DAT header!");
        return;
    }

    int table_size = *(int *)(hdr + 8);
    l_info("DAT table size: %d bytes", table_size);

    void *dat_mem = malloc(table_size);
    if (!dat_mem) {
        l_error("Failed to allocate memory for DAT table (%d bytes)", table_size);
        return;
    }
    if (gDat_ptr) *gDat_ptr = dat_mem;

    fseek(fp, 0, SEEK_SET);
    fread(dat_mem, 1, table_size, fp);

    int count = *(int *)dat_mem;
    l_info("DAT archive entries: %d", count);

    if (cRHash_Init && gDatHash_ptr) {
        cRHash_Init(gDatHash_ptr, count, DatHashGetString);
    }

    for (int i = 0; i < count; i++) {
        char *entry = ((char *)dat_mem) + (i * 24);
        char **str_ptr = (char **)(entry + 4);
        *str_ptr = ((char *)dat_mem) + (uintptr_t)(*str_ptr);
        if (cRHash_Add && gDatHash_ptr) {
            cRHash_Add(gDatHash_ptr, *str_ptr, i);
        }
    }

    l_success("DAT archive initialized successfully!");
}

static void hooked_UnPng(void *dst, int dst_len, void *src, int src_len, int w, int h) {
    int iw, ih, comp;
    unsigned char *img = stbi_load_from_memory((const stbi_uc *)src, src_len, &iw, &ih, &comp, 4);
    if (img) {
        int row_stride = iw * 4;
        for (int y = 0; y < ih; y++) {
            memcpy((char *)dst + (ih - 1 - y) * row_stride, img + y * row_stride, row_stride);
        }
        stbi_image_free(img);
    } else {
        l_error("stbi_load_from_memory failed for PNG");
    }
}

static void hooked_UnJpg(void *dst, int dst_len, void *src, int src_len, int w, int h) {
    int iw, ih, comp;
    unsigned char *img = stbi_load_from_memory((const stbi_uc *)src, src_len, &iw, &ih, &comp, 4);
    if (img) {
        int row_stride = iw * 4;
        for (int y = 0; y < ih; y++) {
            memcpy((char *)dst + (ih - 1 - y) * row_stride, img + y * row_stride, row_stride);
        }
        stbi_image_free(img);
    } else {
        l_error("stbi_load_from_memory failed for JPG");
    }
}

static void hooked_UnZip(void *dst, int dst_len, void *src, int src_len) {
    const unsigned char *b = (const unsigned char *)src;
    if (src_len >= 30 && b[0] == 0x50 && b[1] == 0x4B && b[2] == 0x03 && b[3] == 0x04) {
        int method = b[8] | (b[9] << 8);
        int comp_size = b[18] | (b[19] << 8) | (b[20] << 16) | (b[21] << 24);
        int uncomp_size = b[22] | (b[23] << 8) | (b[24] << 16) | (b[25] << 24);
        int fname_len = b[26] | (b[27] << 8);
        int extra_len = b[28] | (b[29] << 8);
        int data_offset = 30 + fname_len + extra_len;

        if (method == 0) { // Stored
            memcpy(dst, b + data_offset, uncomp_size);
            return;
        } else if (method == 8) { // Deflated
            z_stream strm;
            memset(&strm, 0, sizeof(strm));
            strm.next_in = (Bytef *)(b + data_offset);
            strm.avail_in = comp_size;
            strm.next_out = (Bytef *)dst;
            strm.avail_out = dst_len;
            if (inflateInit2(&strm, -MAX_WBITS) == Z_OK) {
                inflate(&strm, Z_FINISH);
                inflateEnd(&strm);
            }
            return;
        }
    }

    uLongf dlen = dst_len;
    uncompress((Bytef *)dst, &dlen, (const Bytef *)src, src_len);
}

static void *hooked_PfmLoadFileDat(void *dst_tga, int start, int uncomp_size, int comp_size, int comp_type, uint16_t w, uint16_t h) {
    FILE *fp = s_datFP;
    if (!fp) {
        FILE **gDatFP_ptr = (FILE **)so_symbol(&so_mod, "gDatFP");
        if (gDatFP_ptr) fp = *gDatFP_ptr;
    }
    if (!fp) {
        l_error("PfmLoadFileDat: gDatFP is NULL!");
        return dst_tga;
    }

    int asset_start = 0;
    int *gJavaAssetStart_ptr = (int *)so_symbol(&so_mod, "gJavaAssetStart");
    if (gJavaAssetStart_ptr) asset_start = *gJavaAssetStart_ptr;

    fseek(fp, asset_start + start, SEEK_SET);

    if (comp_type == 0) { // Uncompressed raw data
        fread(dst_tga, 1, uncomp_size, fp);
        return dst_tga;
    }

    void *comp_buf = malloc(comp_size);
    if (!comp_buf) {
        l_error("PfmLoadFileDat: failed to allocate %d bytes for comp_buf", comp_size);
        return dst_tga;
    }
    fread(comp_buf, 1, comp_size, fp);

    if (comp_type == 1) { // Zip
        hooked_UnZip(dst_tga, uncomp_size, comp_buf, comp_size);
    } else if (comp_type == 2) { // JPG -> TGA
        hooked_UnJpg((char *)dst_tga + 18, uncomp_size, comp_buf, comp_size, w, h);
    } else if (comp_type == 3) { // PNG -> TGA
        hooked_UnPng((char *)dst_tga + 18, uncomp_size, comp_buf, comp_size, w, h);
    }

    free(comp_buf);

    if (comp_type == 2 || comp_type == 3) {
        // Construct 18-byte TGA header (32 bpp, uncompressed true-color, bottom-left origin)
        uint8_t *hdr = (uint8_t *)dst_tga;
        memset(hdr, 0, 18);
        hdr[2] = 2; // Uncompressed true-color image
        hdr[12] = (uint8_t)(w & 0xFF);
        hdr[13] = (uint8_t)((w >> 8) & 0xFF);
        hdr[14] = (uint8_t)(h & 0xFF);
        hdr[15] = (uint8_t)((h >> 8) & 0xFF);
        hdr[16] = 32; // 32 bpp
        hdr[17] = 8;  // 8 bits of alpha
    }

    return dst_tga;
}

static int hooked_PfmAudioLoadSample(const char *name) {
    return audio_load_sample(name);
}

static int hooked_PfmAudioPlaySample(int id, float vol, float p2, float p3) {
    return audio_play_sample(id, vol);
}

static void hooked_PfmAudioStopSample(int id) {
    audio_stop_sample(id);
}

static void hooked_PfmAudioPlayMusic(const char *name) {
    audio_play_music(name);
}

static void hooked_PfmAudioStopMusic(void) {
    audio_stop_music();
}

static void hooked_PfmAudioPauseMusic(void) {
    audio_pause_music();
}

static void hooked_PfmAudioUnPauseMusic(void) {
    audio_resume_music();
}

static void hooked_PfmAudioSetMusicVolume(float vol) {
    audio_set_music_volume(vol);
}

static int hooked_PfmFindFile(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    return access(path, F_OK) == 0 ? 1 : 0;
}

static int hooked_PfmSaveFile(const char *name, void *data, int len) {
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(data, 1, len, f);
    fclose(f);
    return 1;
}

static void *hooked_PfmLoadFile(const char *name, void *dst, int *out_len) {
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (!dst) dst = malloc(sz);
    if (dst) fread(dst, 1, sz, f);
    fclose(f);

    if (out_len) *out_len = sz;
    return dst;
}

static void hooked_PfmDeleteFile(const char *name) {
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    remove(path);
}

void so_patch(void) {
    hook_addr(so_symbol(&so_mod, "_Z7wprintfPcz"), (uintptr_t)&hooked_wprintf);
    hook_addr(so_symbol(&so_mod, "Java_com_sandlotgames_snailmail_SnailMailActivity_JNIDatInit"), (uintptr_t)&hooked_JNIDatInit);

    // Hook asset decompression and loading directly at the loader level:
    hook_addr(so_symbol(&so_mod, "_Z14PfmLoadFileDatPviiiitt"), (uintptr_t)&hooked_PfmLoadFileDat);
    hook_addr(so_symbol(&so_mod, "_Z11JAVAC_UnPngPviS_iii"), (uintptr_t)&hooked_UnPng);
    hook_addr(so_symbol(&so_mod, "_Z11JAVAC_UnJpgPviS_iii"), (uintptr_t)&hooked_UnJpg);
    hook_addr(so_symbol(&so_mod, "_Z11JAVAC_UnZipPviS_i"), (uintptr_t)&hooked_UnZip);

    // Audio functions:
    // Note: _Z18PfmAudioLoadSamplePc is only 4 bytes (b _Z14JAVALoadSamplePc); hooking _Z14JAVALoadSamplePc
    // avoids 8-byte hook overwriting adjacent functions.
    hook_addr(so_symbol(&so_mod, "_Z14JAVALoadSamplePc"), (uintptr_t)&hooked_PfmAudioLoadSample);
    hook_addr(so_symbol(&so_mod, "_Z18PfmAudioPlaySampleifff"), (uintptr_t)&hooked_PfmAudioPlaySample);
    hook_addr(so_symbol(&so_mod, "_Z18PfmAudioStopSamplei"), (uintptr_t)&hooked_PfmAudioStopSample);
    hook_addr(so_symbol(&so_mod, "_Z17PfmAudioPlayMusicPc"), (uintptr_t)&hooked_PfmAudioPlayMusic);
    hook_addr(so_symbol(&so_mod, "_Z17PfmAudioStopMusicv"), (uintptr_t)&hooked_PfmAudioStopMusic);
    hook_addr(so_symbol(&so_mod, "_Z18PfmAudioPauseMusicv"), (uintptr_t)&hooked_PfmAudioPauseMusic);
    hook_addr(so_symbol(&so_mod, "_Z20PfmAudioUnPauseMusicv"), (uintptr_t)&hooked_PfmAudioUnPauseMusic);
    hook_addr(so_symbol(&so_mod, "_Z22PfmAudioSetMusicVolumef"), (uintptr_t)&hooked_PfmAudioSetMusicVolume);

    // File I/O functions:
    // Note: PfmFindFile, PfmSaveFile, PfmDeleteFile are 4-byte jump trampolines; hooking the underlying
    // JAVAC* functions is 100% safe as they are 128-360 bytes in size.
    hook_addr(so_symbol(&so_mod, "_Z13JAVACFindFilePc"), (uintptr_t)&hooked_PfmFindFile);
    hook_addr(so_symbol(&so_mod, "_Z13JAVACSaveFilePcPvi"), (uintptr_t)&hooked_PfmSaveFile);
    hook_addr(so_symbol(&so_mod, "_Z11PfmLoadFilePcPvPi"), (uintptr_t)&hooked_PfmLoadFile);
    hook_addr(so_symbol(&so_mod, "_Z15JAVACDeleteFilePc"), (uintptr_t)&hooked_PfmDeleteFile);
}
