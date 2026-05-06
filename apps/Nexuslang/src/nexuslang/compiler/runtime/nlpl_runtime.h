/*
 * NexusLang C Runtime Library Header
 * Provides standard library functions for compiled NexusLang programs
 */

#ifndef NLPL_RUNTIME_H
#define NLPL_RUNTIME_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

// =======================
// File I/O Functions
// =======================

/**
 * Read entire file contents as a string.
 * @param filepath Path to the file to read
 * @return Allocated string with file contents (caller must free)
 */
char* nxl_read_file(const char* filepath);

/**
 * Write string to a file (overwrites if exists).
 * @param filepath Path to the file to write
 * @param content Content to write
 * @return true on success, false on failure
 */
bool nxl_write_file(const char* filepath, const char* content);

/**
 * Append string to a file.
 * @param filepath Path to the file to append to
 * @param content Content to append
 * @return true on success, false on failure
 */
bool nxl_append_file(const char* filepath, const char* content);

/**
 * Check if file exists.
 * @param filepath Path to check
 * @return true if file exists, false otherwise
 */
bool nxl_file_exists(const char* filepath);

/**
 * Get file size in bytes.
 * @param filepath Path to the file
 * @return File size in bytes, or -1 on error
 */
long nxl_file_size(const char* filepath);

/**
 * Delete a file.
 * @param filepath Path to the file to delete
 * @return true on success, false on failure
 */
bool nxl_delete_file(const char* filepath);

/**
 * Copy a file.
 * @param src Source file path
 * @param dst Destination file path
 * @return true on success, false on failure
 */
bool nxl_copy_file(const char* src, const char* dst);

/**
 * Create a directory.
 * @param path Directory path to create
 * @return true on success, false on failure
 */
bool nxl_create_directory(const char* path);

/**
 * Check if path is a directory.
 * @param path Path to check
 * @return true if directory, false otherwise
 */
bool nxl_is_directory(const char* path);

/**
 * Check if path is a regular file.
 * @param path Path to check
 * @return true if regular file, false otherwise
 */
bool nxl_is_file(const char* path);

// =======================
// String Utilities
// =======================

/**
 * Convert string to uppercase (allocates new string).
 * @param str Input string
 * @return New allocated uppercase string (caller must free)
 */
char* nxl_uppercase(const char* str);

/**
 * Convert string to lowercase (allocates new string).
 * @param str Input string
 * @return New allocated lowercase string (caller must free)
 */
char* nxl_lowercase(const char* str);

/**
 * Concatenate two strings (allocates new string).
 * @param str1 First string
 * @param str2 Second string
 * @return New allocated concatenated string (caller must free)
 */
char* nxl_concat(const char* str1, const char* str2);

/**
 * Get string length.
 * @param str Input string
 * @return Length of string
 */
int nxl_string_length(const char* str);

/**
 * Check if string contains substring.
 * @param str String to search in
 * @param substr Substring to find
 * @return true if found, false otherwise
 */
bool nxl_string_contains(const char* str, const char* substr);

/**
 * Create substring.
 * @param str Source string
 * @param start Start index (0-based)
 * @param length Number of characters
 * @return New allocated substring (caller must free)
 */
char* nxl_substring(const char* str, int start, int length);

// =======================
// Console I/O
// =======================

/**
 * Read a line from stdin.
 * @return Allocated string with input (caller must free)
 */
char* nxl_read_line(void);

/**
 * Read an integer from stdin.
 * @return Integer value
 */
int nxl_read_int(void);

/**
 * Read a float from stdin.
 * @return Float value
 */
double nxl_read_float(void);

#ifdef __cplusplus
}
#endif

#endif /* NLPL_RUNTIME_H */
