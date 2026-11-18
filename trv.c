#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
    char entrypath[500];
    char extension[50];
    char signature[50];
    int level;
    bool is_directory;
    bool is_probable;
} Entry;

// Fields to print
struct {
    char field[15];
    bool show;
} headers[] = {
    {"Extension", true},
    {"Signature", true},
    {"Level", true},
    {"Directory", true}
};

Entry entries[1000];

// Current entry number
int entry_num = 0;

// Store longest file path
int longest_path = 0;

bool get_home_path(char *start_path, int size_start_path);
bool manage_entry(char *path, int level);
bool manage_directory(char *path, DIR *directorystream, Entry *entry, int level);
bool manage_file(char *path, Entry *entry, int level);
char *get_extension(char *file_path);
char *get_signature(char *file_path);
char *get_probable_type(char *file_path);
void exclude_headers(int argc, char *argv[]);
void print_headers();
void print_separators();
void print_rows();
void update_longest_path(char *file_path);
bool copy(char *destination, int size, char *source);
bool entry_limit();

// Magic numbers for the signatures
unsigned char jpeg[] = {0xFF, 0xD8, 0xFF};
unsigned char zip[] = {0x50, 0x4B, 0x03, 0x04};
unsigned char pdf[] = {0x25, 0x50, 0x44, 0x46, 0x2D};
unsigned char gif87[] = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};
unsigned char gif89[] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};
unsigned char png[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
unsigned char sqlite[] = {0x53, 0x51, 0x4C, 0x69, 0x74, 0x65, 0x20, 0x66, 0x6F, 0x72, 0x6D, 0x61, 0x74, 0x20, 0x33, 0x00};

int main(int argc, char *argv[]) {
    char start_path[500];

    // Missing arguments
    if (argc < 2) {
        fprintf(stderr, "trv: error: missing directory argument\nUsage: trv <directory> [options]\n");
        return 1;
    }

    // Store path into start_path
    if (!copy(start_path, sizeof(start_path), argv[1])) {
        return 1;
    }

    // Use home directory
    if (start_path[0] == '~') {
        if(!get_home_path(start_path, sizeof(start_path))) return 1;
    }

    if(!manage_entry(start_path, 0)) {
        fprintf(stderr, "trv: error: file or directory does not exist\n");
        return 1;
    }

    exclude_headers(argc, argv);
    print_headers();
    print_rows();
    return 0;
}

bool get_home_path(char *start_path, int size_start_path) {
    char *home = getenv("HOME");
    if (home == NULL) return false;

    // Store home into home_path
    char home_path[500];
    if (!copy(home_path, sizeof(home_path), home)) return false;

    // Store start_path into temp_path
    char temp_path[500];
    if (!copy(temp_path, sizeof(temp_path), start_path)) return false;

    // Store home_path into start_path
    if(snprintf(start_path, size_start_path, "%s%s", home_path, temp_path + 1) >= size_start_path) {
        printf("trv: error: path too long\n");
        return false;
    }
    return true;
}

bool manage_entry(char *path, int level) {
    update_longest_path(path);

    if (entry_limit()) {
        return false;
    }

    Entry *entry = &entries[entry_num];

    DIR *directorystream = opendir(path);
    if (directorystream == NULL) { 
        if (!manage_file(path, entry, level)) {
            return false;
        }

        return true;
    }

    if (!manage_directory(path, directorystream, entry, level)) return false;

    return true;
}

bool manage_directory(char *path, DIR *directorystream, Entry *entry, int level) {
     // Assign values into Entry
    if ((!copy(entry->entrypath, sizeof(entry->entrypath), path))) {
        closedir(directorystream);
        return false;
    }
    snprintf(entry->extension, sizeof(entry->extension), "-");
    snprintf(entry->signature, sizeof(entry->signature), "-");
    entry->level = level;
    entry->is_directory = true;
    entry->is_probable = false;

    entry_num ++;

    // Traverse the entry
    struct dirent *read_entry;
    while ((read_entry = readdir(directorystream)) != NULL && !entry_limit()) {
        if (strcmp(read_entry->d_name, ".") == 0 || strcmp(read_entry->d_name, "..") == 0) {
            continue;
        }

        char new_path[500];
        if (snprintf(new_path, sizeof(new_path), "%s/%s", path, read_entry->d_name) >= sizeof(new_path)) {
            fprintf(stderr, "trv: warning: path too long, entry skipped\n");
            continue;
        }

        if (!manage_entry(new_path, level + 1)) {
            fprintf(stderr, "trv: warning: unable to process %s\n", new_path);
            continue;
        }
    }

    closedir(directorystream);

    return true;
}

bool manage_file(char *path, Entry *entry, int level) {
    
    // Open file to read
    FILE *filestream;
    if ((filestream = fopen(path, "rb")) == NULL) {
        return false;
    }
    
    // Assign values to the Entry
    entry->level = level;
    entry->is_directory = false;
    entry->is_probable = false;
    if (!copy(entry->entrypath, sizeof(entry->entrypath), path)) {
        fclose(filestream);
        return false;
    }
    if(!copy(entry->extension, sizeof(entry->extension), get_extension(path))) {
        fclose(filestream);
        return false;
    }
    char *signature = get_signature(path);
    if (strcmp(signature, "-") == 0) {
        char *probable = get_probable_type(path);
        if (strcmp(probable, "-") != 0) {
            entry->is_probable = true;
        }
        if(!copy(entry->signature, sizeof(entry->signature), probable)) {
            fclose(filestream);
            return false;
        } 
    }
    else if(!copy(entry->signature, sizeof(entry->signature), signature)) {
            fclose(filestream);
            return false;
    }
    
    fclose(filestream);

    entry_num ++;

    return true;
}

char *get_extension(char *file_path) {
    
    char *file_name = strrchr(file_path, '/');
    if (file_name == NULL) {
        file_name = file_path;
    }
    char *extension = strrchr(file_name, '.');
    if (extension == NULL) {
        return "-";
    }
    return extension += 1;    
}

char *get_signature(char *file_path) {
    char *signature = "-";

    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        return signature;
    }
    
    // Read first 16 bytes
    unsigned char header_bytes[16];
    size_t num_bytes = fread(header_bytes, 1, 16, file);
    if (num_bytes >= 3 && memcmp(header_bytes, jpeg, 3) == 0) {
        signature = "jpeg";
    }
    else if (num_bytes >= 4 && memcmp(header_bytes, zip, 4) == 0) {
        signature = "zip";
    }
    else if (num_bytes >= 5 && memcmp(header_bytes, pdf, 5) == 0) {
        signature = "pdf";
    }
    else if (num_bytes >= 6 && memcmp(header_bytes, gif87, 6) == 0) {
        signature = "gif";
    }
    else if (num_bytes >= 6 && memcmp(header_bytes, gif89, 6) == 0) {
        signature = "gif";
    }
    else if (num_bytes >= 8 && memcmp(header_bytes, png, 8) == 0) {
        signature = "png";
    }
    else if (num_bytes >= 16 && memcmp(header_bytes, sqlite, 16) == 0) {
        signature = "sqlite";
    }
    fclose(file);
    return signature;
}

char *get_probable_type(char *file_path) {
    char *probable_type = "-";

    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        return probable_type;
    }

    unsigned char bytes[4097];
    size_t num_bytes = fread(bytes, 1, 4096, file);
    bytes[num_bytes] = '\0';
    for (int i = 0; i < num_bytes; i++) {
        bytes[i] = tolower(bytes[i]);
    }
    if (strstr((char *)bytes, "<!doctype html>") != NULL) {
        probable_type = "html";
    }
    else if (strstr((char *)bytes, "{% extends") != NULL && strstr((char *)bytes, ".html") != NULL) {
        probable_type = "html";
    }
    fclose(file);
    return probable_type;
}

void exclude_headers(int argc, char *argv[]) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--no-e") == 0) {
            headers[0].show = false;
        }
        else if (strcmp(argv[i], "--no-s") == 0) {
            headers[1].show = false;
        }
        else if (strcmp(argv[i], "--no-l") == 0 ) {
            headers[2].show = false;
        }
        else if (strcmp(argv[i], "--no-d") == 0) {
            headers[3].show = false;
        }
    }
}

void print_headers() {
    for (int i = 0; i < longest_path; i++) {
        printf("-");
    }
    print_separators();
    
    printf("%-*s", longest_path + 3, "Path");
    for (int i = 0; i < 4; i++) {
        if (headers[i].show) {
            printf("   %-15s", headers[i].field);
        }
    }
    printf("\n");

    for (int i = 0; i < longest_path; i++) {
        printf("-");
    }
    print_separators();
}

void print_separators(){
    for (int i = 0; i < 4; i++) {
        if (headers[i].show) {
            printf("-------------------");
        }
    }
    printf("\n");
}

void print_rows() {
    for (int i = 0; i < entry_num; i++) {
        printf("%-*s", longest_path + 3, entries[i].entrypath);
        if (headers[0].show) {
            printf("   %-15s", entries[i].extension);
        }
        if (headers[1].show) {
            if (entries[i].is_probable) {
                char with_asterisk[50];
                if (copy(with_asterisk, sizeof(with_asterisk), entries[i].signature)) {
                    printf("   %-15s", strcat(with_asterisk, "*"));                    
                }
            }
            else {
               printf("   %-15s", entries[i].signature); 
            }
        }
        if (headers[2].show) {
            printf("   %-15d", entries[i].level);
        }
        if (headers[3].show) {
            if (entries[i].is_directory) {
                printf("   %-15s", "yes");
            }
            else {
                printf("   %-15s", "no");
            }
        }
        printf("\n");
    }
}

void update_longest_path(char *file_path) {
    if ((strlen(file_path)) > longest_path) {
        longest_path = strlen(file_path);
    }
}

bool copy(char *destination, int size, char *source) {
    if (snprintf(destination, size, "%s", source) >= size) {
        fprintf(stderr, "trv: error: path too long\n");
        return false;
    }
    return true;
}

bool entry_limit() {
    if (entry_num >= 1000) {
        fprintf(stderr, "trv: maximum of 1000 entries reached\n");
        return true;
    }
    return false;
}
