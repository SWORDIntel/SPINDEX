#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// A simplified log entry structure (in reality, it would match your JSON)
typedef struct {
    int message_id;
    long timestamp_ms; // Unix timestamp in milliseconds
    char* sender_username;
    char* text;
} LogEntry;

// Function to be called from Python
// Searches for a keyword in the 'text' field of LogEntry array.
// Returns the count of matches.
// For simplicity, memory for LogEntry array and its contents is assumed
// to be managed by the caller (Python, via ctypes).
int search_log_entries_c(LogEntry* entries, int num_entries, const char* keyword) {
    if (!entries || num_entries <= 0 || !keyword) {
        return 0;
    }

    int match_count = 0;
    for (int i = 0; i < num_entries; ++i) {
        if (entries[i].text && strstr(entries[i].text, keyword) != NULL) {
            match_count++;
            // In a real scenario, you might want to return the matching entries,
            // which adds complexity for memory management across C/Python boundary.
        }
    }
    return match_count;
}

// Example of a more complex function that might allocate memory
// (illustrates need for a corresponding free function)
LogEntry* find_first_match_c(LogEntry* entries, int num_entries, const char* keyword, int* out_index) {
    if (!entries || num_entries <= 0 || !keyword || !out_index) {
        if(out_index) *out_index = -1;
        return NULL;
    }

    for (int i = 0; i < num_entries; ++i) {
        if (entries[i].text && strstr(entries[i].text, keyword) != NULL) {
            // Create a COPY to return (caller must free)
            LogEntry* match = (LogEntry*)malloc(sizeof(LogEntry));
            if (!match) return NULL; // Allocation failed

            match->message_id = entries[i].message_id;
            match->timestamp_ms = entries[i].timestamp_ms;

            // Strdup allocates new memory for the strings
            match->sender_username = entries[i].sender_username ? strdup(entries[i].sender_username) : NULL;
            match->text = entries[i].text ? strdup(entries[i].text) : NULL;

            if ((entries[i].sender_username && !match->sender_username) || (entries[i].text && !match->text)) {
                // strdup failed for one of the strings
                if (match->sender_username) free(match->sender_username);
                if (match->text) free(match->text);
                free(match);
                if(out_index) *out_index = -1;
                return NULL;
            }
            *out_index = i;
            return match;
        }
    }
    if(out_index) *out_index = -1;
    return NULL;
}

void free_log_entry_c(LogEntry* entry) {
    if (entry) {
        if (entry->sender_username) free(entry->sender_username);
        if (entry->text) free(entry->text);
        free(entry);
    }
}
