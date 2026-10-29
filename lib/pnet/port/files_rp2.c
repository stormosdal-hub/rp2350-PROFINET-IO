/*
 * files_rp2.c - p-net's "files" (station name and IP, I&M data, diagnosis, port data) in the last
 * sectors of the flash, where ESP-IDF had NVS. Each file has two 4 KB sectors and a save goes to
 * the one not holding its newest copy, so a reset during a save leaves the previous copy: the
 * newest record with a good CRC is the file. Flashing new firmware keeps them; TIA Portal's
 * "Reset to factory settings" clears them.
 *
 * A save erases a sector (~45 ms, at worst 400 ms) and programs it. p-net saves only what changed:
 * once at start if needed, then from its background worker (a new name or address, I&M data from
 * the PLC, port data from the first connection; patches/0003 moves the I&M save there). While the
 * flash is erased or programmed nothing may run from it. In a copy_to_ram build nothing does, and
 * the PROFINET tasks keep running; otherwise the interrupts are off for that time.
 */
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"
#include "osal_log.h"
#include "pnal.h"
#include "pnal_rp2_priv.h"

#define MAGIC 0x31464e50u   // "PNF1"

// "pnet_data_<name>.bin" (pf_file.h); the SNMP ones are not saved (PNET_OPTION_SNMP 0)
static const char *const names[] = { "ip", "im", "diagnostics", "pdport_1", "pdport_2", "pdport_3", "pdport_4" };
#define NFILES      (sizeof(names) / sizeof(names[0]))
#define AREA_SIZE   (NFILES * 2 * FLASH_SECTOR_SIZE)
#define AREA_OFFSET (PICO_FLASH_SIZE_BYTES - AREA_SIZE)

typedef struct {
    uint32_t magic;
    uint32_t seq;   // higher (mod 2^32) is newer
    uint32_t len;   // of the data after this header
    uint32_t crc;   // CRC-32 of the data
} record_t;

#define MAX_DATA (FLASH_SECTOR_SIZE - sizeof(record_t))

extern char __flash_binary_end;   // the linker script's: the end of the firmware in flash

static SemaphoreHandle_t lock;
static bool usable;

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xffffffffu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static uint32_t sector_offset(int file, int half)
{
    return AREA_OFFSET + (uint32_t)(file * 2 + half) * FLASH_SECTOR_SIZE;
}

// read through the uncached, untranslated alias: the XIP cache keeps the firmware, not these, and
// the offsets are the physical ones flash_range_erase() and flash_range_program() take
static const record_t *record(int file, int half)
{
    return (const record_t *)(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + sector_offset(file, half));
}

static bool valid(const record_t *r)
{
    return r->magic == MAGIC && r->len <= MAX_DATA && crc32((const uint8_t *)(r + 1), r->len) == r->crc;
}

// the half holding the newest good copy, -1 if none
static int newest(int file)
{
    bool v0 = valid(record(file, 0)), v1 = valid(record(file, 1));
    if (v0 && v1) return (int32_t)(record(file, 1)->seq - record(file, 0)->seq) > 0 ? 1 : 0;
    return v0 ? 0 : v1 ? 1 : -1;
}

// "/dir/pnet_data_ip.bin" -> 0 ("ip"); -1 for a file not kept here
static int file_of(const char *fullpath)
{
    const char *name = strrchr(fullpath, '/');
    name = name ? name + 1 : fullpath;
    if (strncmp(name, "pnet_data_", 10) == 0) name += 10;
    size_t n = strcspn(name, ".");
    for (size_t i = 0; i < NFILES; i++) {
        if (strlen(names[i]) == n && strncmp(names[i], name, n) == 0) return (int)i;
    }
    return -1;
}

static void erase(uint32_t offset)
{
#if PICO_COPY_TO_RAM
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
#else
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    restore_interrupts(irq);
#endif
}

// a page at a time: in an XIP build the interrupts are off for one page (< 3 ms), not all of them
static void program(uint32_t offset, const uint8_t *data, size_t len)
{
    for (size_t done = 0; done < len; done += FLASH_PAGE_SIZE) {
#if PICO_COPY_TO_RAM
        flash_range_program(offset + done, data + done, FLASH_PAGE_SIZE);
#else
        uint32_t irq = save_and_disable_interrupts();
        flash_range_program(offset + done, data + done, FLASH_PAGE_SIZE);
        restore_interrupts(irq);
#endif
    }
}

int pnal_rp2_files_init(void)
{
    if (lock) return usable ? 0 : -1;
    lock = xSemaphoreCreateMutex();
    if (!lock) return -1;
    // the image starts at flash offset 0 (no partition table on this board)
    uint32_t firmware_end = (uint32_t)(uintptr_t)&__flash_binary_end - XIP_BASE;
    usable = firmware_end <= AREA_OFFSET;
    if (!usable) {
        os_log(LOG_LEVEL_ERROR, "files: the firmware (%lu bytes) reaches into the settings area at %lu: nothing saved\n",
               (unsigned long)firmware_end, (unsigned long)AREA_OFFSET);
        return -1;
    }
    return 0;
}

int pnal_save_file(const char *fullpath, const void *object_1, size_t size_1, const void *object_2, size_t size_2)
{
    int file = file_of(fullpath);
    size_t len = size_1 + size_2;
    if (file < 0 || !usable || len > MAX_DATA) {
        os_log(LOG_LEVEL_ERROR, "files: cannot save %s (%u bytes)\n", fullpath, (unsigned)len);
        return -1;
    }
    size_t total = (sizeof(record_t) + len + FLASH_PAGE_SIZE - 1) & ~(size_t)(FLASH_PAGE_SIZE - 1);
    uint8_t *buf = pvPortMalloc(total);
    if (!buf) return -1;
    memset(buf, 0xff, total);
    uint8_t *data = buf + sizeof(record_t);
    if (size_1) memcpy(data, object_1, size_1);
    if (size_2) memcpy(data + size_1, object_2, size_2);
    record_t r = { .magic = MAGIC, .len = len, .crc = crc32(data, len) };

    xSemaphoreTake(lock, portMAX_DELAY);
    int cur = newest(file);
    r.seq = cur < 0 ? 1 : record(file, cur)->seq + 1;
    memcpy(buf, &r, sizeof(r));
    int half = cur == 0 ? 1 : 0;
    erase(sector_offset(file, half));
    program(sector_offset(file, half), buf, total);
    bool ok = valid(record(file, half)) && memcmp(record(file, half) + 1, data, len) == 0;
    xSemaphoreGive(lock);

    vPortFree(buf);
    if (!ok) os_log(LOG_LEVEL_ERROR, "files: %s did not read back as written\n", fullpath);
    return ok ? 0 : -1;
}

// -1 when not saved yet, or saved with another size (another p-net version): p-net then uses defaults
int pnal_load_file(const char *fullpath, void *object_1, size_t size_1, void *object_2, size_t size_2)
{
    int file = file_of(fullpath);
    int ret = -1;
    if (file < 0 || !usable) return -1;
    xSemaphoreTake(lock, portMAX_DELAY);
    int half = newest(file);
    if (half >= 0 && record(file, half)->len == size_1 + size_2) {
        const uint8_t *data = (const uint8_t *)(record(file, half) + 1);
        if (size_1) memcpy(object_1, data, size_1);
        if (size_2) memcpy(object_2, data + size_1, size_2);
        ret = 0;
    }
    xSemaphoreGive(lock);
    return ret;
}

// Each good copy gets its magic programmed to 0, the older one first: one page program each
// (< 3 ms) and no erase, since this runs in the receive task (DCP "Reset to factory") and the next
// save erases the sector anyway. A reset in between leaves the newest copy or none, never an older.
void pnal_clear_file(const char *fullpath)
{
    uint8_t page[FLASH_PAGE_SIZE];
    int file = file_of(fullpath);
    if (file < 0 || !usable) return;
    memset(page, 0xff, sizeof(page));
    memset(page, 0, sizeof(((record_t *)0)->magic));
    xSemaphoreTake(lock, portMAX_DELAY);
    int last = newest(file);
    if (last >= 0) {
        if (valid(record(file, 1 - last))) program(sector_offset(file, 1 - last), page, sizeof(page));
        program(sector_offset(file, last), page, sizeof(page));
    }
    xSemaphoreGive(lock);
}
