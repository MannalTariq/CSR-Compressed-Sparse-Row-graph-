#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

// A simple key-value pair struct.
typedef struct {
    char key[50];
    char value[50];
} KeyValuePair;

// In-memory component of the LSM tree. A simple dynamic array.
typedef struct {
    KeyValuePair *data;
    int size;
    int capacity;
} Memtable;

// A simple struct to represent an on-disk SSTable.
typedef struct {
    char filename[100];
} SSTable;

// main LSM Tree data structure.
typedef struct {
    Memtable memtable;
    SSTable *sstables;
    int sstable_count;
    int sstable_capacity;
    char data_dir[100];
} LSMTree;

// --- Function Prototypes ---
void init_lsm_tree(LSMTree *tree, const char *data_dir);
void destroy_lsm_tree(LSMTree *tree);
void _flush_memtable(LSMTree *tree);
void _compact_sstables(LSMTree *tree);
void lsm_tree_put(LSMTree *tree, const char *key, const char *value);
const char *lsm_tree_get(LSMTree *tree, const char *key);

int compare_keys(const void *a, const void *b) {
    KeyValuePair *pairA = (KeyValuePair *)a;
    KeyValuePair *pairB = (KeyValuePair *)b;
    return strcmp(pairA->key, pairB->key);
}

void init_lsm_tree(LSMTree *tree, const char *data_dir) {
    printf("Initializing LSM Tree...\n");
    
    strncpy(tree->data_dir, data_dir, sizeof(tree->data_dir));
    mkdir(tree->data_dir); 

    tree->memtable.capacity = 10;
    tree->memtable.size = 0;
    tree->memtable.data = (KeyValuePair *)malloc(tree->memtable.capacity * sizeof(KeyValuePair));
    if (tree->memtable.data == NULL) {
        perror("Failed to allocate memory for memtable");
        exit(EXIT_FAILURE);
    }

    tree->sstable_capacity = 5;
    tree->sstable_count = 0;
    tree->sstables = (SSTable *)malloc(tree->sstable_capacity * sizeof(SSTable));
    if (tree->sstables == NULL) {
        perror("Failed to allocate memory for sstables");
        exit(EXIT_FAILURE);
    }
}
void destroy_lsm_tree(LSMTree *tree) {
    printf("Destroying LSM Tree...\n");
    if (tree->memtable.data) {
        free(tree->memtable.data);
    }
    if (tree->sstables) {
        free(tree->sstables);
    }
}
void _flush_memtable(LSMTree *tree) {
    char filename[100];
    if (tree->memtable.size == 0) {
        return;
    }
    printf("Flushing memtable to disk...\n");
    sprintf(filename, "%s/sstable_%ld.txt", tree->data_dir, time(NULL));
    qsort(tree->memtable.data, tree->memtable.size, sizeof(KeyValuePair), compare_keys);
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Failed to open SSTable file");
        return;
    }
    for (int i = 0; i < tree->memtable.size; i++) {
        fprintf(file, "%s:%s\n", tree->memtable.data[i].key, tree->memtable.data[i].value);
    }
    fclose(file);
    if (tree->sstable_count >= tree->sstable_capacity) {
        tree->sstable_capacity *= 2;
        tree->sstables = (SSTable *)realloc(tree->sstables, tree->sstable_capacity * sizeof(SSTable));
    }
    strncpy(tree->sstables[tree->sstable_count].filename, filename, sizeof(tree->sstables[0].filename));
    tree->sstable_count++;

    tree->memtable.size = 0;

    printf("Flush complete. SSTable created: %s\n", filename);

    if (tree->sstable_count >= 3) {
        _compact_sstables(tree);
    }
}

void _compact_sstables(LSMTree *tree) {
    printf("\nStarting compaction...\n");
    
    KeyValuePair *merged_data = NULL;
    int merged_size = 0;
    int merged_capacity = 100;

    merged_data = (KeyValuePair *)malloc(merged_capacity * sizeof(KeyValuePair));
    if (merged_data == NULL) {
        perror("Failed to allocate memory for merged data");
        return;
    }

    for (int i = 0; i < tree->sstable_count; i++) {
        printf("Reading from SSTable: %s\n", tree->sstables[i].filename);
        FILE *file = fopen(tree->sstables[i].filename, "r");
        if (file == NULL) {
            perror("Failed to open SSTable for compaction");
            continue;
        }

        char line[200];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = '\0';
            
            char *key = strtok(line, ":");
            char *value = strtok(NULL, ":");
            
            if (key && value) {
                int found = 0;
                for (int j = 0; j < merged_size; j++) {
                    if (strcmp(merged_data[j].key, key) == 0) {
                        strncpy(merged_data[j].value, value, sizeof(merged_data[0].value));
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    if (merged_size >= merged_capacity) {
                        merged_capacity *= 2;
                        merged_data = (KeyValuePair *)realloc(merged_data, merged_capacity * sizeof(KeyValuePair));
                    }
                    strncpy(merged_data[merged_size].key, key, sizeof(merged_data[0].key));
                    strncpy(merged_data[merged_size].value, value, sizeof(merged_data[0].value));
                    merged_size++;
                }
            }
        }
        fclose(file);
    }
    
    qsort(merged_data, merged_size, sizeof(KeyValuePair), compare_keys);

    char new_filename[100];
    sprintf(new_filename, "%s/compacted_sstable_%ld.txt", tree->data_dir, time(NULL));
    FILE *new_file = fopen(new_filename, "w");
    if (new_file == NULL) {
        perror("Failed to create new compacted SSTable");
        return;
    }
    for (int i = 0; i < merged_size; i++) {
        fprintf(new_file, "%s:%s\n", merged_data[i].key, merged_data[i].value);
    }
    fclose(new_file);
    printf("Compaction successful. New SSTable: %s\n", new_filename);

    for (int i = 0; i < tree->sstable_count; i++) {
        remove(tree->sstables[i].filename);
    }
    free(tree->sstables);
    tree->sstable_count = 1;
    tree->sstable_capacity = 1;
    tree->sstables = (SSTable *)malloc(tree->sstable_capacity * sizeof(SSTable));
    strncpy(tree->sstables[0].filename, new_filename, sizeof(tree->sstables[0].filename));

    free(merged_data);
    printf("Compaction complete.\n\n");
}

void lsm_tree_put(LSMTree *tree, const char *key, const char *value) {
    printf("Putting key '%s' into memtable...\n", key);
    int found = 0;
    for (int i = 0; i < tree->memtable.size; i++) {
        if (strcmp(tree->memtable.data[i].key, key) == 0) {
            strncpy(tree->memtable.data[i].value, value, sizeof(tree->memtable.data[0].value));
            found = 1;
            break;
        }
    }
    
    if (!found) {
        if (tree->memtable.size >= tree->memtable.capacity) {
            _flush_memtable(tree);
        }
        
        strncpy(tree->memtable.data[tree->memtable.size].key, key, sizeof(tree->memtable.data[0].key));
        strncpy(tree->memtable.data[tree->memtable.size].value, value, sizeof(tree->memtable.data[0].value));
        tree->memtable.size++;
    }
}


const char *lsm_tree_get(LSMTree *tree, const char *key) {
    printf("Searching for key '%s'...\n", key);
    for (int i = tree->memtable.size - 1; i >= 0; i--) {
        if (strcmp(tree->memtable.data[i].key, key) == 0) {
            printf("Found '%s' in memtable.\n", key);
            return tree->memtable.data[i].value;
        }
    }
    for (int i = tree->sstable_count - 1; i >= 0; i--) {
        printf("Searching SSTable: %s\n", tree->sstables[i].filename);
        FILE *file = fopen(tree->sstables[i].filename, "r");
        if (file == NULL) {
            perror("Failed to open SSTable for reading");
            continue;
        }
        
        char line[200];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = '\0';
            char *file_key = strtok(line, ":");
            char *file_value = strtok(NULL, ":");
            
            if (file_key && strcmp(file_key, key) == 0) {
                printf("Found '%s' in SSTable.\n", key);
                fclose(file);
                return file_value;
            }
        }
        fclose(file);
    }

    printf("Key '%s' not found.\n", key);
    return NULL;
}

int main() {
    LSMTree lsm_tree;
    init_lsm_tree(&lsm_tree, "lsm_data");

    lsm_tree_put(&lsm_tree, "name", "Alice");
    lsm_tree_put(&lsm_tree, "age", "30");
    lsm_tree_put(&lsm_tree, "city", "New York");
    lsm_tree_put(&lsm_tree, "job", "Engineer");
    lsm_tree_put(&lsm_tree, "id", "12345");
    lsm_tree_put(&lsm_tree, "company", "TechCorp");
    lsm_tree_put(&lsm_tree, "email", "alice@example.com");
    lsm_tree_put(&lsm_tree, "phone", "555-1234");
    lsm_tree_put(&lsm_tree, "hobby", "coding");
    lsm_tree_put(&lsm_tree, "status", "active"); 
    lsm_tree_put(&lsm_tree, "country", "USA");
    lsm_tree_put(&lsm_tree, "lang", "Python");
    lsm_tree_put(&lsm_tree, "role", "developer");
    lsm_tree_put(&lsm_tree, "team", "frontend");
    lsm_tree_put(&lsm_tree, "address", "123 Main St");
    lsm_tree_put(&lsm_tree, "zip", "10001");
    lsm_tree_put(&lsm_tree, "salary", "120000");
    lsm_tree_put(&lsm_tree, "exp", "5 years");
    lsm_tree_put(&lsm_tree, "dept", "R&D");
    lsm_tree_put(&lsm_tree, "manager", "Bob");
    lsm_tree_put(&lsm_tree, "project", "Gemini"); 
    lsm_tree_put(&lsm_tree, "notes", "great employee"); 

    printf("\n--- Retrieving data ---\n");
    const char *value = lsm_tree_get(&lsm_tree, "city");
    if (value) {
        printf("Value for 'city': %s\n", value);
    }
    
    value = lsm_tree_get(&lsm_tree, "job");
    if (value) {
        printf("Value for 'job': %s\n", value);
    }
    
    value = lsm_tree_get(&lsm_tree, "non_existent_key");
    if (value) {
        printf("Value for 'non_existent_key': %s\n", value);
    } else {
        printf("Value for 'non_existent_key' not found.\n");
    }
    
    lsm_tree_put(&lsm_tree, "age", "31");
    value = lsm_tree_get(&lsm_tree, "age");
    if (value) {
        printf("\nUpdated 'age' to 31. New value: %s\n", value);
    }

    destroy_lsm_tree(&lsm_tree);
    
    return 0;
}
