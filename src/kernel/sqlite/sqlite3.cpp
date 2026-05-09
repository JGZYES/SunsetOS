#include "sqlite3.h"

extern "C" {
extern void* malloc(size_t);
extern void free(void*);
extern size_t strlen(const char*);
extern char* strcpy(char*, const char*);
extern char* strncpy(char*, const char*, size_t);
extern int strcmp(const char*, const char*);
extern int strncmp(const char*, const char*, size_t);
extern int strcasecmp(const char*, const char*);
extern int sscanf(const char*, const char*, ...);
extern char* strdup(const char*);
extern void* memset(void*, int, size_t);
}

#define MAX_TABLES 4
#define MAX_COLUMNS 8
#define MAX_ROWS 64
#define MAX_NAME_LEN 32
#define MAX_VALUE_LEN 256

typedef enum {
    TYPE_INT,
    TYPE_TEXT,
    TYPE_REAL
} ColumnType;

typedef struct {
    char name[MAX_NAME_LEN];
    ColumnType type;
} Column;

typedef struct {
    char name[MAX_NAME_LEN];
    Column columns[MAX_COLUMNS];
    int column_count;
    char* rows[MAX_ROWS][MAX_COLUMNS];
    int row_count;
} Table;

typedef struct sqlite3 {
    Table tables[MAX_TABLES];
    int table_count;
    char error_msg[256];
} sqlite3;

#define SQLITE_DONE 101

static int parse_column_type(const char* type_str) {
    if (strcasecmp(type_str, "INT") == 0 || strcasecmp(type_str, "INTEGER") == 0) {
        return TYPE_INT;
    } else if (strcasecmp(type_str, "TEXT") == 0) {
        return TYPE_TEXT;
    } else if (strcasecmp(type_str, "REAL") == 0 || strcasecmp(type_str, "FLOAT") == 0) {
        return TYPE_REAL;
    }
    return TYPE_TEXT;
}

static int parse_create_table(sqlite3* db, const char* sql) {
    char table_name[MAX_NAME_LEN];
    
    const char* ptr = sql + 13;  // Skip "CREATE TABLE "
    while (*ptr == ' ') ptr++;
    
    int i = 0;
    while (*ptr != ' ' && *ptr != '(' && *ptr != '\0') {
        if (i < MAX_NAME_LEN - 1) {
            table_name[i++] = *ptr;
        }
        ptr++;
    }
    table_name[i] = '\0';
    
    if (table_name[0] == '\0') {
        strcpy(db->error_msg, "Invalid CREATE TABLE syntax");
        return SQLITE_ERROR;
    }
    
    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, table_name) == 0) {
            strcpy(db->error_msg, "Table already exists");
            return SQLITE_ERROR;
        }
    }
    
    if (db->table_count >= MAX_TABLES) {
        strcpy(db->error_msg, "Too many tables");
        return SQLITE_ERROR;
    }
    
    Table* table = &db->tables[db->table_count++];
    strcpy(table->name, table_name);
    table->column_count = 0;
    table->row_count = 0;
    
    while (*ptr != '(' && *ptr != '\0') ptr++;
    if (*ptr == '\0') {
        strcpy(db->error_msg, "Missing opening parenthesis");
        return SQLITE_ERROR;
    }
    ptr++;  // Skip '('
    
    while (*ptr && *ptr != ')') {
        char col_name[MAX_NAME_LEN];
        char col_type[MAX_NAME_LEN];
        
        while (*ptr == ' ' || *ptr == '\t') ptr++;
        if (*ptr == ')' || *ptr == '\0') break;
        
        int n = sscanf(ptr, "%s %s", col_name, col_type);
        if (n < 1) break;
        
        if (table->column_count < MAX_COLUMNS) {
            strcpy(table->columns[table->column_count].name, col_name);
            table->columns[table->column_count].type = (ColumnType)parse_column_type(col_type);
            table->column_count++;
        }
        
        while (*ptr && *ptr != ',' && *ptr != ')') ptr++;
        if (*ptr == ',') ptr++;
    }
    
    return SQLITE_OK;
}

static Table* find_table(sqlite3* db, const char* name) {
    for (int i = 0; i < db->table_count; i++) {
        if (strcmp(db->tables[i].name, name) == 0) {
            return &db->tables[i];
        }
    }
    return NULL;
}

static int parse_insert(sqlite3* db, const char* sql) {
    char table_name[MAX_NAME_LEN];
    
    const char* ptr = sql + 11;  // Skip "INSERT INTO "
    while (*ptr == ' ') ptr++;
    
    int i = 0;
    while (*ptr != ' ' && *ptr != '(' && *ptr != '\0') {
        if (i < MAX_NAME_LEN - 1) {
            table_name[i++] = *ptr;
        }
        ptr++;
    }
    table_name[i] = '\0';
    
    if (table_name[0] == '\0') {
        strcpy(db->error_msg, "Invalid INSERT syntax");
        return SQLITE_ERROR;
    }
    
    Table* table = find_table(db, table_name);
    if (!table) {
        strcpy(db->error_msg, "Table not found");
        return SQLITE_ERROR;
    }
    
    if (table->row_count >= MAX_ROWS) {
        strcpy(db->error_msg, "Table is full");
        return SQLITE_ERROR;
    }
    
    // Find VALUES keyword
    while (*ptr && strncmp(ptr, "VALUES", 6) != 0) {
        ptr++;
    }
    if (!*ptr) {
        strcpy(db->error_msg, "Invalid INSERT syntax");
        return SQLITE_ERROR;
    }
    ptr += 6;  // Skip "VALUES"
    
    // Find opening parenthesis
    while (*ptr && *ptr != '(') {
        ptr++;
    }
    if (!*ptr) {
        strcpy(db->error_msg, "Invalid INSERT syntax");
        return SQLITE_ERROR;
    }
    ptr++;  // Skip '('
    
    char* values_part = (char*)ptr;
    char* values_end = values_part;
    int paren_count = 1;
    while (*values_end && paren_count > 0) {
        if (*values_end == '(') paren_count++;
        if (*values_end == ')') paren_count--;
        values_end++;
    }
    *values_end = '\0';
    
    ptr = values_part;
    int col_idx = 0;
    
    while (*ptr && col_idx < table->column_count) {
        if (*ptr == '(') {
            ptr++;
            continue;
        }
        
        while (*ptr == ' ') ptr++;
        
        char value[MAX_VALUE_LEN] = {0};
        int len = 0;
        
        if (*ptr == '\'') {
            ptr++;
            while (*ptr && *ptr != '\'' && len < MAX_VALUE_LEN - 1) {
                value[len++] = *ptr++;
            }
            if (*ptr == '\'') ptr++;
        } else {
            while (*ptr && *ptr != ',' && *ptr != ')' && len < MAX_VALUE_LEN - 1) {
                value[len++] = *ptr++;
            }
        }
        
        table->rows[table->row_count][col_idx] = (char*)malloc(strlen(value) + 1);
        strcpy(table->rows[table->row_count][col_idx], value);
        col_idx++;
        
        while (*ptr == ' ') ptr++;
        if (*ptr == ',') ptr++;
    }
    
    table->row_count++;
    return SQLITE_OK;
}

static int exec_select(sqlite3* db, const char* sql, sqlite3_callback callback, void* arg) {
    char table_name[MAX_NAME_LEN];
    char columns_str[512];
    
    if (strncmp(sql, "SELECT * FROM ", 14) == 0) {
        strcpy(columns_str, "*");
        const char* ptr = sql + 14;
        while (*ptr == ' ') ptr++;
        int i = 0;
        while (*ptr != ' ' && *ptr != ';' && *ptr != '\0') {
            if (i < MAX_NAME_LEN - 1) {
                table_name[i++] = *ptr;
            }
            ptr++;
        }
        table_name[i] = '\0';
    } else if (sscanf(sql, "SELECT %s FROM %s", columns_str, table_name) != 2) {
        strcpy(db->error_msg, "Invalid SELECT syntax");
        return SQLITE_ERROR;
    }
    
    Table* table = find_table(db, table_name);
    if (!table) {
        strcpy(db->error_msg, "Table not found");
        return SQLITE_ERROR;
    }
    
    char** col_names = (char**)malloc(table->column_count * sizeof(char*));
    for (int i = 0; i < table->column_count; i++) {
        col_names[i] = (char*)malloc(strlen(table->columns[i].name) + 1);
        strcpy(col_names[i], table->columns[i].name);
    }
    
    if (callback) {
        callback(arg, table->column_count, NULL, col_names);
        
        for (int row = 0; row < table->row_count; row++) {
            char** row_data = (char**)malloc(table->column_count * sizeof(char*));
            for (int col = 0; col < table->column_count; col++) {
                row_data[col] = table->rows[row][col] ? (char*)table->rows[row][col] : (char*)"";
            }
            int rc = callback(arg, table->column_count, row_data, NULL);
            free(row_data);
            if (rc != 0) {
                for (int i = 0; i < table->column_count; i++) free(col_names[i]);
                free(col_names);
                return SQLITE_ABORT;
            }
        }
    }
    
    for (int i = 0; i < table->column_count; i++) free(col_names[i]);
    free(col_names);
    return SQLITE_OK;
}

int sqlite3_open(const char* filename, sqlite3** ppDb) {
    (void)filename;
    
    *ppDb = (sqlite3*)malloc(sizeof(sqlite3));
    if (!*ppDb) return SQLITE_NOMEM;
    
    memset(*ppDb, 0, sizeof(sqlite3));
    return SQLITE_OK;
}

int sqlite3_open_v2(const char* filename, sqlite3** ppDb, int flags, const char* zVfs) {
    (void)flags;
    (void)zVfs;
    return sqlite3_open(filename, ppDb);
}

int sqlite3_close(sqlite3* db) {
    if (!db) return SQLITE_OK;
    
    for (int i = 0; i < db->table_count; i++) {
        for (int j = 0; j < db->tables[i].row_count; j++) {
            for (int k = 0; k < db->tables[i].column_count; k++) {
                if (db->tables[i].rows[j][k]) {
                    free(db->tables[i].rows[j][k]);
                }
            }
        }
    }
    
    free(db);
    return SQLITE_OK;
}

const char* sqlite3_errmsg(sqlite3* db) {
    if (!db) return "Invalid database";
    return db->error_msg;
}

int sqlite3_exec(sqlite3* db, const char* sql, sqlite3_callback callback, void* arg, char** errmsg) {
    if (!db || !sql) {
        if (errmsg) *errmsg = (char*)strdup("Invalid arguments");
        return SQLITE_ERROR;
    }
    
    char upper_sql[1024];
    strcpy(upper_sql, sql);
    for (int i = 0; upper_sql[i]; i++) {
        if (upper_sql[i] >= 'a' && upper_sql[i] <= 'z') {
            upper_sql[i] -= 32;
        }
    }
    
    if (strncmp(upper_sql, "CREATE TABLE", 12) == 0) {
        return parse_create_table(db, sql);
    } else if (strncmp(upper_sql, "INSERT", 6) == 0) {
        return parse_insert(db, sql);
    } else if (strncmp(upper_sql, "SELECT", 6) == 0) {
        return exec_select(db, sql, callback, arg);
    } else {
        strcpy(db->error_msg, "Unsupported SQL command");
        if (errmsg) *errmsg = (char*)strdup(db->error_msg);
        return SQLITE_ERROR;
    }
}

int sqlite3_prepare_v2(sqlite3* db, const char* zSql, int nByte, sqlite3_stmt** ppStmt, const char** pzTail) {
    (void)db;
    (void)zSql;
    (void)nByte;
    (void)ppStmt;
    (void)pzTail;
    return SQLITE_OK;
}

int sqlite3_step(sqlite3_stmt* pStmt) {
    (void)pStmt;
    return SQLITE_DONE;
}

int sqlite3_column_count(sqlite3_stmt* pStmt) {
    (void)pStmt;
    return 0;
}

const char* sqlite3_column_name(sqlite3_stmt* pStmt, int iCol) {
    (void)pStmt;
    (void)iCol;
    return "";
}

const char* sqlite3_column_text(sqlite3_stmt* pStmt, int iCol) {
    (void)pStmt;
    (void)iCol;
    return "";
}

int sqlite3_column_int(sqlite3_stmt* pStmt, int iCol) {
    (void)pStmt;
    (void)iCol;
    return 0;
}

double sqlite3_column_double(sqlite3_stmt* pStmt, int iCol) {
    (void)pStmt;
    (void)iCol;
    return 0.0;
}

int sqlite3_finalize(sqlite3_stmt* pStmt) {
    (void)pStmt;
    return SQLITE_OK;
}

int sqlite3_changes(sqlite3* db) {
    (void)db;
    return 0;
}