/*
 * NexusLang C Runtime Library Implementation
 * Provides standard library functions for compiled NexusLang programs
 */

#include "nxl_runtime.h"
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/types.h>
#endif

// =======================
// File I/O Functions
// =======================

char* nxl_read_file(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    // Read contents
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    
    fclose(file);
    return buffer;
}

bool nxl_write_file(const char* filepath, const char* content) {
    FILE* file = fopen(filepath, "w");
    if (!file) {
        return false;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    return written == len;
}

bool nxl_append_file(const char* filepath, const char* content) {
    FILE* file = fopen(filepath, "a");
    if (!file) {
        return false;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    return written == len;
}

bool nxl_file_exists(const char* filepath) {
    struct stat st;
    return stat(filepath, &st) == 0;
}

long nxl_file_size(const char* filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return -1;
    }
    return st.st_size;
}

bool nxl_delete_file(const char* filepath) {
    return remove(filepath) == 0;
}

bool nxl_copy_file(const char* src, const char* dst) {
    FILE* source = fopen(src, "rb");
    if (!source) {
        return false;
    }
    
    FILE* dest = fopen(dst, "wb");
    if (!dest) {
        fclose(source);
        return false;
    }
    
    char buffer[8192];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dest) != bytes_read) {
            fclose(source);
            fclose(dest);
            return false;
        }
    }
    
    fclose(source);
    fclose(dest);
    return true;
}

bool nxl_create_directory(const char* path) {
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

bool nxl_is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool nxl_is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

// =======================
// String Utilities
// =======================

char* nxl_uppercase(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        result[i] = toupper((unsigned char)str[i]);
    }
    result[len] = '\0';
    
    return result;
}

char* nxl_lowercase(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        result[i] = tolower((unsigned char)str[i]);
    }
    result[len] = '\0';
    
    return result;
}

char* nxl_concat(const char* str1, const char* str2) {
    if (!str1) str1 = "";
    if (!str2) str2 = "";
    
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    
    char* result = (char*)malloc(len1 + len2 + 1);
    if (!result) return NULL;
    
    strcpy(result, str1);
    strcat(result, str2);
    
    return result;
}

int nxl_string_length(const char* str) {
    if (!str) return 0;
    return (int)strlen(str);
}

bool nxl_string_contains(const char* str, const char* substr) {
    if (!str || !substr) return false;
    return strstr(str, substr) != NULL;
}

char* nxl_substring(const char* str, int start, int length) {
    if (!str) return NULL;
    
    int str_len = (int)strlen(str);
    if (start < 0 || start >= str_len || length <= 0) {
        char* empty = (char*)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    if (start + length > str_len) {
        length = str_len - start;
    }
    
    char* result = (char*)malloc(length + 1);
    if (!result) return NULL;
    
    strncpy(result, str + start, length);
    result[length] = '\0';
    
    return result;
}

// =======================
// Console I/O
// =======================

char* nxl_read_line(void) {
    char buffer[4096];
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        return NULL;
    }
    
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return strdup(buffer);
}

int nxl_read_int(void) {
    int value;
    if (scanf("%d", &value) != 1) {
        return 0;
    }
    // Clear remaining characters
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return value;
}

double nxl_read_float(void) {
    double value;
    if (scanf("%lf", &value) != 1) {
        return 0.0;
    }
    // Clear remaining characters
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
    return value;
}
