// Read-only AppleSMC/VirtualSMC key enumerator.
// No SMC writes are performed.
#include <IOKit/IOKitLib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define KERNEL_INDEX_SMC 2
#define SMC_CMD_READ_BYTES 5
#define SMC_CMD_READ_INDEX 8
#define SMC_CMD_READ_KEYINFO 9

typedef struct {
    char major;
    char minor;
    char build;
    char reserved[1];
    uint16_t release;
} vers_t;

typedef struct {
    uint16_t version;
    uint16_t length;
    uint32_t cpuPLimit;
    uint32_t gpuPLimit;
    uint32_t memPLimit;
} plimit_t;

typedef struct {
    uint32_t dataSize;
    uint32_t dataType;
    char dataAttributes;
} keyinfo_t;

typedef struct {
    uint32_t key;
    vers_t vers;
    plimit_t pLimitData;
    keyinfo_t keyInfo;
    char result;
    char status;
    char data8;
    uint32_t data32;
    uint8_t bytes[32];
} SMCKeyData_t;

static io_connect_t gConn;

static kern_return_t smc_call(int index, SMCKeyData_t *in, SMCKeyData_t *out) {
    size_t out_size = sizeof(SMCKeyData_t);
    return IOConnectCallStructMethod(
        gConn, index, in, sizeof(SMCKeyData_t), out, &out_size);
}

static uint32_t key_to_u32(const char *s) {
    return ((uint32_t)(uint8_t)s[0] << 24) |
           ((uint32_t)(uint8_t)s[1] << 16) |
           ((uint32_t)(uint8_t)s[2] << 8) |
           (uint32_t)(uint8_t)s[3];
}

static void u32_to_key(uint32_t key, char *s) {
    s[0] = (char)(key >> 24);
    s[1] = (char)(key >> 16);
    s[2] = (char)(key >> 8);
    s[3] = (char)key;
    s[4] = '\0';
}

static int get_key_info(uint32_t key, keyinfo_t *info) {
    SMCKeyData_t in = {0};
    SMCKeyData_t out = {0};
    in.key = key;
    in.data8 = SMC_CMD_READ_KEYINFO;
    kern_return_t result = smc_call(KERNEL_INDEX_SMC, &in, &out);
    if (result != kIOReturnSuccess) {
        return (int)result;
    }
    *info = out.keyInfo;
    return 0;
}

static int get_key_by_index(uint32_t index, uint32_t *key) {
    SMCKeyData_t in = {0};
    SMCKeyData_t out = {0};
    in.data8 = SMC_CMD_READ_INDEX;
    in.data32 = index;
    kern_return_t result = smc_call(KERNEL_INDEX_SMC, &in, &out);
    if (result != kIOReturnSuccess) {
        return (int)result;
    }
    *key = out.key;
    return 0;
}

static int read_key_bytes(uint32_t key, uint8_t *dest, uint32_t size) {
    SMCKeyData_t in = {0};
    SMCKeyData_t out = {0};
    size_t out_size = sizeof(SMCKeyData_t);
    in.key = key;
    in.keyInfo.dataSize = size;
    in.data8 = SMC_CMD_READ_BYTES;
    kern_return_t result = IOConnectCallStructMethod(
        gConn, KERNEL_INDEX_SMC, &in, sizeof(SMCKeyData_t), &out, &out_size);
    if (result != kIOReturnSuccess) {
        return (int)result;
    }
    memcpy(dest, out.bytes, size > sizeof(out.bytes) ? sizeof(out.bytes) : size);
    return 0;
}

static void describe(uint32_t type, const uint8_t *bytes, uint32_t size,
                     char *buf, size_t buf_size) {
    char type_name[5];
    u32_to_key(type, type_name);

    if (!strcmp(type_name, "sp78") && size >= 2) {
        int16_t value = (int16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
        snprintf(buf, buf_size, "%.2f", value / 256.0);
    } else if (!strcmp(type_name, "flt ") && size >= 4) {
        float value;
        uint8_t little_endian[4] = {bytes[3], bytes[2], bytes[1], bytes[0]};
        memcpy(&value, little_endian, sizeof(value));
        snprintf(buf, buf_size, "%.3f", value);
    } else if (!strcmp(type_name, "ui8 ") && size >= 1) {
        snprintf(buf, buf_size, "%u", bytes[0]);
    } else if (!strcmp(type_name, "ui16") && size >= 2) {
        snprintf(buf, buf_size, "%u",
                 (unsigned)(((uint16_t)bytes[0] << 8) | bytes[1]));
    } else if (!strcmp(type_name, "ui32") && size >= 4) {
        unsigned value = ((unsigned)bytes[0] << 24) |
                         ((unsigned)bytes[1] << 16) |
                         ((unsigned)bytes[2] << 8) |
                         (unsigned)bytes[3];
        snprintf(buf, buf_size, "%u", value);
    } else if (!strcmp(type_name, "flag")) {
        snprintf(buf, buf_size, "%s", bytes[0] ? "on" : "off");
    } else {
        snprintf(buf, buf_size, "-");
    }
}

static void print_hex(const uint8_t *bytes, uint32_t size) {
    for (uint32_t i = 0; i < size && i < 32; i++) {
        printf("%02x", bytes[i]);
    }
}

int main(void) {
    io_service_t device = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("AppleSMC"));
    if (!device) {
        printf("ERROR: no AppleSMC service\n");
        return 1;
    }

    kern_return_t open_result = IOServiceOpen(
        device, mach_task_self(), 0, &gConn);
    if (open_result != kIOReturnSuccess) {
        printf("ERROR: IOServiceOpen rc=0x%x\n", open_result);
        return 2;
    }

    keyinfo_t count_info = {0};
    if (get_key_info(key_to_u32("#KEY"), &count_info) != 0) {
        printf("ERROR: cannot read #KEY info\n");
        return 3;
    }

    uint8_t count_bytes[4] = {0};
    if (read_key_bytes(key_to_u32("#KEY"), count_bytes, count_info.dataSize) != 0) {
        printf("ERROR: cannot read #KEY value\n");
        return 4;
    }

    uint32_t count = ((uint32_t)count_bytes[0] << 24) |
                     ((uint32_t)count_bytes[1] << 16) |
                     ((uint32_t)count_bytes[2] << 8) |
                     (uint32_t)count_bytes[3];
    printf("SMC total keys (#KEY): %u\n", count);
    printf("index key  type size value hex\n");

    for (uint32_t index = 0; index < count; index++) {
        uint32_t key = 0;
        if (get_key_by_index(index, &key) != 0 || key == 0) {
            continue;
        }

        keyinfo_t info = {0};
        if (get_key_info(key, &info) != 0) {
            continue;
        }

        uint8_t bytes[32] = {0};
        int read_result = read_key_bytes(key, bytes, info.dataSize);
        char key_name[5];
        char type_name[5];
        char value[40];
        u32_to_key(key, key_name);
        u32_to_key(info.dataType, type_name);
        if (read_result == 0) {
            describe(info.dataType, bytes, info.dataSize, value, sizeof(value));
        } else {
            snprintf(value, sizeof(value), "READ_ERR_0x%x", read_result);
        }

        printf("%-5u %-4s %-4s %-4u %-12s ", index, key_name, type_name,
               info.dataSize, value);
        print_hex(bytes, info.dataSize);
        printf("\n");
    }

    IOServiceClose(gConn);
    IOObjectRelease(device);
    return 0;
}
