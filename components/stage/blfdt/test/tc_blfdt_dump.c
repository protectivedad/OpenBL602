/*
 * Copyright (c) 2020 Bouffalolab.
 *
 * This file is part of
 *     *** Bouffalolab Software Dev Kit ***
 *      (see www.bouffalolab.com).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of Bouffalo Lab nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * fdtdump.c - Contributed by Pantelis Antoniou <pantelis.antoniou AT gmail.com>
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

#include <libfdt.h>

#include <utils_log.h>
// #include "log/log.h"

#define FDT_MAGIC_SIZE    4
#define MAX_VERSION 17

#define ALIGN(x, a)    (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((void *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p)    (p += 4, *((const fdt32_t *)(p-4)))

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const char *tagname(uint32_t tag)
{
    static const char * const names[] = {
#define TN(t) [t] = #t
        TN(FDT_BEGIN_NODE),
        TN(FDT_END_NODE),
        TN(FDT_PROP),
        TN(FDT_NOP),
        TN(FDT_END),
#undef TN
    };
    if (tag < ARRAY_SIZE(names)) {
        if (names[tag]) {
            return names[tag];
        }
    }

    return "FDT_???";
}

#define dumpf(fmt, args...) \
    do { if (debug) printf("// " fmt, ## args); } while (0)

bool util_is_printable_string(const void *data, int len)
{
    const char *s = data;
    const char *ss, *se;

    /* zero length is not */
    if (len == 0) {
        return 0;
    }

    /* must terminate with zero */
    if (s[len - 1] != '\0') {
        return 0;
    }

    se = s + len;

    while (s < se) {
        ss = s;
        while (s < se && *s && isprint((unsigned char)*s)) {
            s++;
        }

        /* not zero, or not done yet */
        if (*s != '\0' || s == ss) {
            return 0;
        }

        s++;
    }

    return 1;
}

void utilfdt_print_data(const char *data, int len)
{
    int i;
    const char *s;

    /* no data, don't print */
    if (len == 0) {
        return;
    }

    if (util_is_printable_string(data, len)) {
        printf(" = ");

        s = data;
        do {
            printf("\"%s\"", s);
            s += strlen(s) + 1;
            if (s < data + len)
                printf(", ");
        } while (s < data + len);

    } else if ((len % 4) == 0) {
        const fdt32_t *cell = (const fdt32_t *)data;

        printf(" = <");
        for (i = 0, len /= 4; i < len; i++) {
            printf("0x%08x%s", (unsigned int)fdt32_to_cpu(cell[i]),
                   i < (len - 1) ? " " : "");
        }
        printf(">");
    } else {
        const unsigned char *p = (const unsigned char *)data;
        printf(" = [");
        for (i = 0; i < len; i++) {
            printf("%02x%s", *p++, i < len - 1 ? " " : "");
        }
        printf("]");
    }
}

static void dump_blob(void *blob, bool debug)
{
    uintptr_t blob_off = (uintptr_t)blob;
    struct fdt_header *bph = blob;
    uint32_t off_mem_rsvmap = fdt32_to_cpu(bph->off_mem_rsvmap);
    uint32_t off_dt = fdt32_to_cpu(bph->off_dt_struct);
    uint32_t off_str = fdt32_to_cpu(bph->off_dt_strings);
    struct fdt_reserve_entry *p_rsvmap =
        (struct fdt_reserve_entry *)((char *)blob + off_mem_rsvmap);
    const char *p_struct = (const char *)blob + off_dt;
    const char *p_strings = (const char *)blob + off_str;
    uint32_t version = fdt32_to_cpu(bph->version);
    uint32_t totalsize = fdt32_to_cpu(bph->totalsize);
    uint32_t tag;
    const char *p, *s, *t;
    int depth, sz, shift;
    int i;
    uint64_t addr, size;

    depth = 0;
    shift = 4;

    printf("/dts-v1/;\r\n");
    printf("// magic:\t\t0x%"PRIx32"\r\n", fdt32_to_cpu(bph->magic));
    printf("// totalsize:\t\t0x%"PRIx32" (%"PRIu32")\r\n",
           totalsize, totalsize);
    printf("// off_dt_struct:\t0x%"PRIx32"\r\n", off_dt);
    printf("// off_dt_strings:\t0x%"PRIx32"\r\n", off_str);
    printf("// off_mem_rsvmap:\t0x%"PRIx32"\r\n", off_mem_rsvmap);
    printf("// version:\t\t%"PRIu32"\r\n", version);
    printf("// last_comp_version:\t%"PRIu32"\r\n",
           fdt32_to_cpu(bph->last_comp_version));
    if (version >= 2) {
        printf("// boot_cpuid_phys:\t0x%"PRIx32"\r\n",
               fdt32_to_cpu(bph->boot_cpuid_phys));
    }

    if (version >= 3) {
        printf("// size_dt_strings:\t0x%"PRIx32"\r\n",
               fdt32_to_cpu(bph->size_dt_strings));
    }

    if (version >= 17) {
        printf("// size_dt_struct:\t0x%"PRIx32"\r\n",
               fdt32_to_cpu(bph->size_dt_struct));
    }

    printf("\r\n");

    for (i = 0; ; i++) {
        addr = fdt64_to_cpu(p_rsvmap[i].address);
        size = fdt64_to_cpu(p_rsvmap[i].size);
        if (addr == 0 && size == 0)
            break;

        printf("/memreserve/ %#"PRIx64" %#"PRIx64";\r\n",
               addr, size);
    }

    p = p_struct;

    // uint32_t index = 0;
    while ((tag = fdt32_to_cpu(GET_CELL(p))) != FDT_END) {

        // printf("index = %d\r\n", index++);
        dumpf("%04"PRIxPTR": tag: 0x%08"PRIx32" (%s)\r\n",
                (uintptr_t)p - blob_off - 4, tag, tagname(tag));

        if (tag == FDT_BEGIN_NODE) {
            s = p;
            p = PALIGN(p + strlen(s) + 1, 4);

            if (*s == '\0')
                s = "/";

            printf("%*s%s {\r\n", depth * shift, "", s);

            depth++;
            continue;
        }

        if (tag == FDT_END_NODE) {
            depth--;

            printf("%*s};\r\n", depth * shift, "");
            continue;
        }

        if (tag == FDT_NOP) {
            printf("%*s// [NOP]\r\n", depth * shift, "");
            continue;
        }

        if (tag != FDT_PROP) {
            log_error("%*s ** Unknown tag 0x%08"PRIx32"\r\n", depth * shift, "", tag);
            break;
        }
        sz = fdt32_to_cpu(GET_CELL(p));
        s = p_strings + fdt32_to_cpu(GET_CELL(p));
        if (version < 16 && sz >= 8)
            p = PALIGN(p, 8);
        t = p;

        p = PALIGN(p + sz, 4);

        dumpf("%04"PRIxPTR": string: %s\r\n", (uintptr_t)s - blob_off, s);
        dumpf("%04"PRIxPTR": value\r\n", (uintptr_t)t - blob_off);
        printf("%*s%s", depth * shift, "", s);
        utilfdt_print_data(t, sz);
        printf(";\r\n");
    }
}


static bool valid_header(char *p, uint32_t len)
{
    if (len < sizeof(struct fdt_header) ||
        fdt_magic(p) != FDT_MAGIC ||
        fdt_version(p) > MAX_VERSION ||
        fdt_last_comp_version(p) > MAX_VERSION ||
        fdt_totalsize(p) >= len ||
        fdt_off_dt_struct(p) >= len ||
        fdt_off_dt_strings(p) >= len) {
            return 0;
        }
        return 1;
}
/*
  input_buf
  input_len
  debug   Dump debug information while decoding the file
  scan    Scan for an embedded fdt in file
*/

static int blfdtdump(const unsigned char *input_buf, const uint32_t input_len, uint8_t arg_debug, uint8_t arg_scan)
{
    const char *file = "fdtbuff";
    char *buf = (char *)input_buf;
    uint8_t debug = arg_debug;
    uint8_t scan = arg_scan;
    uint32_t len = (uint32_t)input_len;
    uint32_t this_len = 0;

    // log_info("**** fdtdump is a low-level debugging tool, not meant for general use.\r\n"
    //         "**** If you want to decompile a dtb, you probably want\r\n"
    //         "****     dtc -I dtb -O dts <filename>\r\n\r\n"
    //     );

    /* try and locate an embedded fdt in a bigger blob */
    if (scan) {
        unsigned char smagic[FDT_MAGIC_SIZE];
        char *p = buf;
        char *endp = buf + len;

        fdt_set_magic(smagic, FDT_MAGIC);

        /* poor man's memmem */
        while ((endp - p) >= FDT_MAGIC_SIZE) {
            p = memchr(p, smagic[0], endp - p - FDT_MAGIC_SIZE);
            if (!p) {
                break;
            }
            if (fdt_magic(p) == FDT_MAGIC) {
                /* try and validate the main struct */
                this_len = endp - p;
                if (valid_header(p, this_len)) {
                    break;
                }
                if (debug) {
                    printf("%s: skipping fdt magic at offset %#tx\r\n",
                        file, p - buf);
                }
            }
            ++p;
        }
        if (!p || endp - p < sizeof(struct fdt_header)) {
            log_error("%s: could not locate fdt magic\r\n", file);
            return -1;
        }
        printf("%s: found fdt at offset %#tx\r\n", file, p - buf);
        buf = p;
    } else if (!valid_header(buf, len)) {
        log_error("%s: header is not valid\r\n", file);
        return -1;
    }

    log_info("dump_blob.");

    dump_blob(buf, debug);

    return 0;
}

#define TC_WIFI_DTB_LEN    (4779 + 4)
const uint8_t tc_wifi_dtb[TC_WIFI_DTB_LEN] = {
  0xd0, 0x0d, 0xfe, 0xed, 0x00, 0x00, 0x12, 0xab, 0x00, 0x00, 0x00, 0x38,
  0x00, 0x00, 0x10, 0xc0, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xeb,
  0x00, 0x00, 0x10, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x13,
  0x00, 0x00, 0x00, 0x00, 0x62, 0x6c, 0x20, 0x62, 0x6c, 0x36, 0x30, 0x78,
  0x20, 0x41, 0x56, 0x42, 0x20, 0x62, 0x6f, 0x61, 0x72, 0x64, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x06,
  0x62, 0x6c, 0x2c, 0x62, 0x6c, 0x36, 0x30, 0x78, 0x2d, 0x73, 0x61, 0x6d,
  0x70, 0x6c, 0x65, 0x00, 0x62, 0x6c, 0x2c, 0x62, 0x6c, 0x36, 0x30, 0x78,
  0x2d, 0x63, 0x6f, 0x6d, 0x6d, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x69, 0x70, 0x63, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x69, 0x70, 0x63, 0x40, 0x34, 0x30, 0x30, 0x31, 0x43, 0x30, 0x30, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x2c, 0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x69, 0x32, 0x73, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x69, 0x32, 0x73, 0x40, 0x34, 0x30, 0x30, 0x31, 0x37, 0x30, 0x30, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
  0x00, 0x00, 0x00, 0x2c, 0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x06,
  0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x69, 0x32, 0x73, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x01, 0x70, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x37, 0x6f, 0x6b, 0x61, 0x79,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x41,
  0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x69, 0x32, 0x73, 0x40, 0x34, 0x30, 0x30, 0x31,
  0x37, 0x31, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c, 0x6f, 0x6b, 0x61, 0x79,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a,
  0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x69, 0x32,
  0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x33, 0x40, 0x01, 0x71, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x37,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x01,
  0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x1d,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x4e,
  0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x69, 0x32, 0x63, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x01, 0x69, 0x32, 0x63, 0x40, 0x34, 0x30, 0x30, 0x31,
  0x31, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c, 0x6f, 0x6b, 0x61, 0x79,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a,
  0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x69, 0x32,
  0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x33, 0x40, 0x01, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x58,
  0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x64, 0x65, 0x76, 0x69, 0x63, 0x65, 0x73, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x18,
  0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x17,
  0x00, 0x00, 0x00, 0x66, 0x69, 0x32, 0x63, 0x5f, 0x65, 0x73, 0x38, 0x33,
  0x31, 0x31, 0x00, 0x69, 0x32, 0x63, 0x5f, 0x67, 0x63, 0x30, 0x33, 0x30,
  0x38, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x69, 0x32, 0x63, 0x40, 0x34, 0x30, 0x30, 0x31,
  0x31, 0x31, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c, 0x64, 0x69, 0x73, 0x61,
  0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a,
  0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x69, 0x32,
  0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x33, 0x40, 0x01, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x6d, 0x6a, 0x70, 0x65, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x6d, 0x6a, 0x70, 0x65,
  0x67, 0x40, 0x34, 0x30, 0x30, 0x31, 0x36, 0x30, 0x30, 0x30, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c,
  0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30,
  0x78, 0x5f, 0x6d, 0x6a, 0x70, 0x65, 0x67, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33, 0x40, 0x01, 0x60, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x74, 0x69, 0x6d, 0x65, 0x72, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x74, 0x69, 0x6d, 0x65, 0x72, 0x40, 0x34, 0x30, 0x30, 0x31, 0x34, 0x30,
  0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x2c, 0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x06,
  0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x74, 0x69, 0x6d, 0x65, 0x72, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x01, 0x40, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x74, 0x69, 0x6d, 0x65, 0x72, 0x40, 0x34, 0x30,
  0x30, 0x31, 0x34, 0x31, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c, 0x64, 0x69, 0x73, 0x61,
  0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0c,
  0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x74, 0x69,
  0x6d, 0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x33, 0x40, 0x01, 0x41, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x70, 0x77, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x77, 0x6d, 0x40, 0x34, 0x30, 0x30, 0x31,
  0x32, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c, 0x64, 0x69, 0x73, 0x61,
  0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a,
  0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x70, 0x77,
  0x6d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x33, 0x40, 0x01, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x75, 0x61, 0x72, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x75, 0x61, 0x72, 0x74,
  0x40, 0x34, 0x30, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06,
  0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x75, 0x61, 0x72, 0x74, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x75,
  0x2f, 0x64, 0x65, 0x76, 0x2f, 0x74, 0x74, 0x79, 0x53, 0x30, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x7a, 0x00, 0x01, 0xc2, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x66, 0x65, 0x61, 0x74, 0x75, 0x72, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x7e,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x82, 0x64, 0x69, 0x73, 0x61,
  0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x86, 0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x89,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x75, 0x61, 0x72, 0x74,
  0x40, 0x34, 0x30, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06,
  0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x75, 0x61, 0x72, 0x74, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x75,
  0x2f, 0x64, 0x65, 0x76, 0x2f, 0x74, 0x74, 0x79, 0x53, 0x31, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x01, 0xc2, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x66, 0x65, 0x61, 0x74, 0x75, 0x72, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x7e,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x82, 0x64, 0x69, 0x73, 0x61,
  0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x86, 0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x89,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x75, 0x61, 0x72, 0x74,
  0x40, 0x34, 0x30, 0x30, 0x31, 0x30, 0x32, 0x30, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c,
  0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x06,
  0x62, 0x6c, 0x36, 0x30, 0x78, 0x5f, 0x75, 0x61, 0x72, 0x74, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x75,
  0x2f, 0x64, 0x65, 0x76, 0x2f, 0x74, 0x74, 0x79, 0x53, 0x32, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x4c, 0x4b, 0x40,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x89, 0x00, 0x00, 0x00, 0x0e,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x86,
  0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x66, 0x65, 0x61, 0x74, 0x75, 0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x89, 0x6f, 0x6b, 0x61, 0x79,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
  0x00, 0x00, 0x00, 0x86, 0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x82,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x7e, 0x64, 0x69, 0x73, 0x61,
  0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x73, 0x70, 0x69, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x73, 0x70, 0x69, 0x40, 0x34, 0x30, 0x30, 0x30, 0x46, 0x30, 0x30, 0x30,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08,
  0x00, 0x00, 0x00, 0x2c, 0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33,
  0x40, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x67, 0x70, 0x69, 0x70,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x01, 0x61, 0x64, 0x63, 0x5f, 0x6b, 0x65, 0x79, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c,
  0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x95, 0x00, 0x00, 0x00, 0x09,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x99,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x14,
  0x00, 0x00, 0x00, 0xa3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64,
  0x00, 0x00, 0x01, 0x90, 0x00, 0x00, 0x01, 0x2c, 0x00, 0x00, 0x01, 0xf4,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0xab,
  0x53, 0x57, 0x31, 0x00, 0x53, 0x57, 0x32, 0x00, 0x53, 0x57, 0x33, 0x00,
  0x53, 0x57, 0x34, 0x00, 0x53, 0x57, 0x35, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xb3, 0x55, 0x73, 0x72, 0x31,
  0x00, 0x55, 0x73, 0x72, 0x32, 0x00, 0x53, 0x74, 0x61, 0x72, 0x74, 0x00,
  0x55, 0x70, 0x00, 0x44, 0x6f, 0x77, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0xbd, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x70, 0x64, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x70, 0x64, 0x6d, 0x40,
  0x34, 0x30, 0x30, 0x30, 0x43, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33, 0x40, 0x00, 0xc0, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x63, 0x61, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x63, 0x61, 0x6d, 0x40,
  0x34, 0x30, 0x30, 0x30, 0x42, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c,
  0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30,
  0x78, 0x5f, 0x63, 0x61, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33, 0x40, 0x00, 0xb0, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xc5,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0xcd, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd7, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xe0,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0xe9, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xf2, 0x00, 0x00, 0x00, 0x05,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xfb,
  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x0d, 0x00, 0x00, 0x00, 0x1a,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x16,
  0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x01, 0x1f, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x28, 0x00, 0x00, 0x00, 0x0f,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x31,
  0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x66, 0x65, 0x61, 0x74, 0x75, 0x72, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x01, 0x3d, 0x48, 0x41, 0x52, 0x44,
  0x57, 0x41, 0x52, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x63, 0x6f, 0x6e, 0x66, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x01, 0x42,
  0x61, 0x75, 0x74, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x71, 0x73, 0x70, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x71, 0x73, 0x70, 0x69,
  0x40, 0x34, 0x30, 0x30, 0x30, 0x41, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x2c,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33, 0x40, 0x00, 0xa0, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x73, 0x64, 0x68, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x73, 0x64, 0x68, 0x40,
  0x34, 0x30, 0x30, 0x30, 0x33, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2c,
  0x6f, 0x6b, 0x61, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x06, 0x62, 0x6c, 0x36, 0x30,
  0x78, 0x5f, 0x73, 0x64, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33, 0x40, 0x00, 0x30, 0x00,
  0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x70, 0x69, 0x6e, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x42,
  0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x01, 0x49, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x4d, 0x00, 0x00, 0x00, 0x12,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x52,
  0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x01, 0x57, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x5c, 0x00, 0x00, 0x00, 0x15,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x77, 0x69, 0x66, 0x69, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
  0x72, 0x65, 0x67, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x61, 0x00, 0x00, 0x00, 0x56,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x6d, 0x61, 0x63, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x01, 0x6e,
  0xc8, 0x43, 0x57, 0x82, 0x73, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x01, 0x7b, 0xc8, 0x43, 0x57, 0x82,
  0x73, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
  0x61, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0f,
  0x00, 0x00, 0x01, 0x87, 0x42, 0x4c, 0x36, 0x30, 0x78, 0x5f, 0x43, 0x61,
  0x6d, 0x65, 0x72, 0x61, 0x30, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x8c, 0x31, 0x32, 0x33, 0x34,
  0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x61, 0x00, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x90, 0x00, 0x00, 0x00, 0x0b,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x01, 0x9b,
  0x64, 0x69, 0x73, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x01, 0x62, 0x72, 0x64, 0x5f, 0x72, 0x66, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x01, 0xac,
  0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0xb1, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xba,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0xa7, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9f,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x95, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x81,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x48,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x01,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0a,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x01, 0xbb,
  0x14, 0xf0, 0x00, 0x00, 0x14, 0xfb, 0x1c, 0x71, 0x15, 0x06, 0x38, 0xe3,
  0x15, 0x11, 0x55, 0x55, 0x15, 0x1c, 0x71, 0xc7, 0x15, 0x27, 0x8e, 0x38,
  0x15, 0x32, 0xaa, 0xaa, 0x15, 0x3d, 0xc7, 0x1c, 0x15, 0x48, 0xe3, 0x8e,
  0x15, 0x54, 0x00, 0x00, 0x15, 0x5f, 0x1c, 0x71, 0x15, 0x6a, 0x38, 0xe3,
  0x15, 0x75, 0x55, 0x55, 0x15, 0x90, 0x00, 0x00, 0x15, 0xc0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x01, 0xcd,
  0x00, 0x00, 0xa7, 0x80, 0x00, 0x00, 0xa7, 0xd8, 0x00, 0x00, 0xa8, 0x31,
  0x00, 0x00, 0xa8, 0x8a, 0x00, 0x00, 0xa8, 0xe3, 0x00, 0x00, 0xa9, 0x3c,
  0x00, 0x00, 0xa9, 0x95, 0x00, 0x00, 0xa9, 0xee, 0x00, 0x00, 0xaa, 0x47,
  0x00, 0x00, 0xaa, 0xa0, 0x00, 0x00, 0xaa, 0xf8, 0x00, 0x00, 0xab, 0x51,
  0x00, 0x00, 0xab, 0xaa, 0x00, 0x00, 0xac, 0x80, 0x00, 0x00, 0x00, 0x03,
  0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0xdf, 0x00, 0x00, 0x08, 0x00,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x09, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x00, 0x63, 0x6f,
  0x6d, 0x70, 0x61, 0x74, 0x69, 0x62, 0x6c, 0x65, 0x00, 0x23, 0x61, 0x64,
  0x64, 0x72, 0x65, 0x73, 0x73, 0x2d, 0x63, 0x65, 0x6c, 0x6c, 0x73, 0x00,
  0x23, 0x73, 0x69, 0x7a, 0x65, 0x2d, 0x63, 0x65, 0x6c, 0x6c, 0x73, 0x00,
  0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x00, 0x72, 0x65, 0x67, 0x00, 0x6d,
  0x63, 0x6c, 0x6b, 0x5f, 0x6f, 0x6e, 0x6c, 0x79, 0x00, 0x6d, 0x63, 0x6c,
  0x6b, 0x00, 0x62, 0x63, 0x6c, 0x6b, 0x00, 0x66, 0x73, 0x00, 0x64, 0x6f,
  0x00, 0x64, 0x69, 0x00, 0x73, 0x63, 0x6c, 0x00, 0x73, 0x64, 0x61, 0x00,
  0x6c, 0x69, 0x73, 0x74, 0x5f, 0x61, 0x64, 0x64, 0x72, 0x00, 0x6c, 0x69,
  0x73, 0x74, 0x5f, 0x64, 0x72, 0x69, 0x76, 0x65, 0x72, 0x00, 0x69, 0x64,
  0x00, 0x70, 0x61, 0x74, 0x68, 0x00, 0x63, 0x66, 0x67, 0x00, 0x72, 0x74,
  0x73, 0x00, 0x63, 0x74, 0x73, 0x00, 0x72, 0x78, 0x00, 0x74, 0x78, 0x00,
  0x62, 0x61, 0x75, 0x64, 0x72, 0x61, 0x74, 0x65, 0x00, 0x70, 0x69, 0x6e,
  0x00, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x72, 0x75, 0x70, 0x74, 0x00, 0x6b,
  0x65, 0x79, 0x5f, 0x76, 0x6f, 0x6c, 0x00, 0x6b, 0x65, 0x79, 0x5f, 0x70,
  0x63, 0x62, 0x00, 0x6b, 0x65, 0x79, 0x5f, 0x65, 0x76, 0x65, 0x6e, 0x74,
  0x00, 0x6b, 0x65, 0x79, 0x5f, 0x72, 0x61, 0x77, 0x00, 0x50, 0x49, 0x58,
  0x5f, 0x43, 0x4c, 0x4b, 0x00, 0x46, 0x52, 0x41, 0x4d, 0x45, 0x5f, 0x56,
  0x4c, 0x44, 0x00, 0x4c, 0x49, 0x4e, 0x45, 0x5f, 0x56, 0x4c, 0x44, 0x00,
  0x50, 0x49, 0x58, 0x5f, 0x44, 0x41, 0x54, 0x30, 0x00, 0x50, 0x49, 0x58,
  0x5f, 0x44, 0x41, 0x54, 0x31, 0x00, 0x50, 0x49, 0x58, 0x5f, 0x44, 0x41,
  0x54, 0x32, 0x00, 0x50, 0x49, 0x58, 0x5f, 0x44, 0x41, 0x54, 0x33, 0x00,
  0x50, 0x49, 0x58, 0x5f, 0x44, 0x41, 0x54, 0x34, 0x00, 0x50, 0x49, 0x58,
  0x5f, 0x44, 0x41, 0x54, 0x35, 0x00, 0x50, 0x49, 0x58, 0x5f, 0x44, 0x41,
  0x54, 0x36, 0x00, 0x50, 0x49, 0x58, 0x5f, 0x44, 0x41, 0x54, 0x37, 0x00,
  0x43, 0x41, 0x4d, 0x5f, 0x50, 0x57, 0x44, 0x4e, 0x00, 0x43, 0x41, 0x4d,
  0x5f, 0x52, 0x45, 0x46, 0x5f, 0x43, 0x4c, 0x4b, 0x00, 0x6d, 0x6f, 0x64,
  0x65, 0x00, 0x73, 0x65, 0x6e, 0x73, 0x6f, 0x72, 0x00, 0x63, 0x6d, 0x64,
  0x00, 0x64, 0x61, 0x74, 0x30, 0x00, 0x64, 0x61, 0x74, 0x31, 0x00, 0x64,
  0x61, 0x74, 0x32, 0x00, 0x64, 0x61, 0x74, 0x33, 0x00, 0x63, 0x6f, 0x75,
  0x6e, 0x74, 0x72, 0x79, 0x5f, 0x63, 0x6f, 0x64, 0x65, 0x00, 0x73, 0x74,
  0x61, 0x5f, 0x6d, 0x61, 0x63, 0x5f, 0x61, 0x64, 0x64, 0x72, 0x00, 0x61,
  0x70, 0x5f, 0x6d, 0x61, 0x63, 0x5f, 0x61, 0x64, 0x64, 0x72, 0x00, 0x73,
  0x73, 0x69, 0x64, 0x00, 0x70, 0x77, 0x64, 0x00, 0x61, 0x70, 0x5f, 0x63,
  0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x00, 0x61, 0x75, 0x74, 0x6f, 0x5f,
  0x63, 0x68, 0x61, 0x6e, 0x5f, 0x64, 0x65, 0x74, 0x65, 0x63, 0x74, 0x00,
  0x78, 0x74, 0x61, 0x6c, 0x00, 0x70, 0x77, 0x72, 0x5f, 0x74, 0x61, 0x62,
  0x6c, 0x65, 0x00, 0x63, 0x68, 0x61, 0x6e, 0x6e, 0x65, 0x6c, 0x5f, 0x64,
  0x69, 0x76, 0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x00, 0x63, 0x68, 0x61,
  0x6e, 0x6e, 0x65, 0x6c, 0x5f, 0x63, 0x6e, 0x74, 0x5f, 0x74, 0x61, 0x62,
  0x6c, 0x65, 0x00, 0x6c, 0x6f, 0x5f, 0x66, 0x63, 0x61, 0x6c, 0x5f, 0x64,
  0x69, 0x76, 0x00
};

int tc_blfdtdump(void)
{
    int result;

    result = blfdtdump(tc_wifi_dtb, TC_WIFI_DTB_LEN, true, true);
 
    if (result) {
        printf("dump failed\r\n");
    } else {
        printf("dump successed\r\n");
    }
    
    return result;
   
    // blfdtdump(tc_wifi_dtb, TC_WIFI_DTB_LEN, false, true);
    // blfdtdump(tc_wifi_dtb, TC_WIFI_DTB_LEN, false, false);
}
