#include <klibc/string.h>

uint32_t strlen(char str[]) {
    uint32_t len = 0;
    while (str[len++] != '\0');
    return len - 1;
}

void strcpy(char *src, char *dst) {
    uint32_t len = strlen(src);
    for (uint32_t i = 0; i < len; i++)
        *dst++ = *src++;
    *dst = '\0';
}

int strcmp(char *s1, char *s2) {
    if (strlen(s1) != strlen(s2)) return 1;
    while (*s1 != '\0' && *s2 != '\0') {
        if (*s1 != *s2) return 1;
        s1++;
        s2++;
    }
    return 0;
}

int strncmp(char *s1, char *s2, int len) {
    for (int i = 0; i < len; i++) {
        if (s1[i] != s2[i]) return 1;
    }
    return 0;
}

void reverse(char str[]) {
    uint32_t start_index = 0;
    uint32_t end_index = 0;
    if (strlen(str) > 0) {
        end_index = strlen(str) - 1;
    }
    char temp_buffer = 0;

    // Two indexes, one starts at 0, the other starts at strlen - 1
    // On each iteration, store the char at start_index in temp_buffer,
    // Store the char at end_index in the space at start_index,
    // And then put the char in temp buffer in the space at end_index
    // And finally decrement end_index and increment start_index
    while (start_index < end_index) {
        temp_buffer = str[start_index];
        str[start_index] = str[end_index];
        str[end_index] = temp_buffer;
        start_index++;
        end_index--;
    }
}

void itoa(int32_t n, char str[]) {
    uint32_t i;
    i = 0;
    int32_t n_inv = abs(n); // inverted n, if it was negative
    uint32_t abs_n = (uint32_t) n_inv;
    if (abs_n == 0) {
        str[i++] = '0';
    }
    while (abs_n > 0) {
        str[i++] = (abs_n % 10) + '0'; // Calculate the current lowest value digit and convert it
        abs_n /= 10;
    }

    if (n_inv != n) {
        str[i++] = '-';
    }

    str[i] = '\0';

    reverse(str);
}

void utoa(uint32_t n, char str[]) {
    uint32_t i;
    i = 0;
    if (n == 0) {
        str[i++] = '0';
    }
    while (n > 0) {
        str[i++] = (n % 10) + '0'; // Calculate the current lowest value digit and convert it
        n /= 10;
    }

    str[i] = '\0';

    reverse(str);
}

uint32_t atou(char str[]) {
    uint32_t result = 0;
    for(; *str; ++str) {
        result *= 10;
        result += *str - '0';
    }

    return result;
}

void htoa(uint32_t in, char str[]) {
    uint32_t pos = 0;
    uint8_t tmp;

    str[pos++] = '0';
    str[pos++] = 'x';

    for (uint16_t i = 60; i > 0; i -= 4) {
        tmp = (uint8_t)((in >> i) & 0xf);
        if (tmp >= 0xa) {
            str[pos++] = (tmp - 0xa) + 'A';
        } else {
            str[pos++] = tmp + '0';
        }
    }

    tmp = (uint8_t)(in & 0xf);
    if (tmp >= 0xa) {
        str[pos++] = (tmp - 0xa) + 'A';
    } else {
        str[pos++] = tmp + '0';
    }
    str[pos] = '\0'; // nullify 
}

void strcat(char *dst, char *src) {
    char *end = dst + strlen(dst);

    while (*src != '\0') *end++ = *src++;
    *end = '\0';
}

void memcpy(uint8_t *src, uint8_t *dst, uint32_t count) {
    for (uint32_t i = 0; i<count; i++)
        *dst++ = *src++;
}

void memcpy32(uint32_t *src, uint32_t *dst, uint32_t count) {
    for (uint32_t i = 0; i<count; i++)
        *dst++ = *src++;
}

void memset(uint8_t *dst, uint8_t data, uint32_t count) {
    for (uint32_t i = 0; i<count; i++)
        *dst++ = data;
}

void memset32(uint32_t *dst, uint32_t data, uint32_t count) {
    for (uint32_t i = 0; i<count; i++)
        *dst++ = data;
}