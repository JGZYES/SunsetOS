#include "sqlite_shell.h"
#include "sqlite3.h"
#include "vga.h"
#include "libc.h"
#include "kbd.h"

extern "C" {
extern void free(void*);
extern void reset_heap();
}

namespace sqlite_shell {

static sqlite3* current_db = nullptr;

static int print_callback(void* arg, int argc, char** argv, char** col_names) {
    (void)arg;
    
    static bool first_row = true;
    if (first_row && col_names) {
        for (int i = 0; i < argc; i++) {
            vga::print("%s", col_names[i]);
            if (i < argc - 1) vga::print(" | ");
        }
        vga::print("\n");
        for (int i = 0; i < argc; i++) {
            for (size_t j = 0; j < (col_names[i] ? strlen(col_names[i]) : 4); j++) {
                vga::print("-");
            }
            if (i < argc - 1) vga::print("-+-");
        }
        vga::print("\n");
        first_row = false;
    }
    
    if (argv) {
        for (int i = 0; i < argc; i++) {
            vga::print("%s", argv[i] ? argv[i] : "NULL");
            if (i < argc - 1) vga::print(" | ");
        }
        vga::print("\n");
    }
    
    return 0;
}

void cmd_sqlite3(int argc, char* args[]) {
    reset_heap();
    
    if (argc < 2) {
        vga::print("Usage: sqlite3 <database> [sql_command]\n");
        vga::print("  Opens or creates a database and optionally executes SQL\n");
        vga::print("  Example: sqlite3 test.db CREATE TABLE t1(id INT, name TEXT)\n");
        vga::print("  Example: sqlite3 test.db SELECT * FROM t1\n");
        vga::print("  Example: sqlite3 test.db INSERT INTO t1 VALUES(1, 'Hello')\n");
        return;
    }

    const char* db_name = args[1];
    const char* sql = (argc > 2) ? args[2] : nullptr;

    vga::print("SQLite3 for SunsetOS\n");
    vga::print("Database: %s\n", db_name);

    if (current_db) {
        sqlite3_close(current_db);
        current_db = nullptr;
    }

    int rc = sqlite3_open(db_name, &current_db);
    if (rc != SQLITE_OK) {
        if (!current_db) {
            vga::print("Error opening database: Cannot allocate memory\n");
        } else {
            vga::print("Error opening database: %s\n", sqlite3_errmsg(current_db));
        }
        return;
    }

    if (!sql) {
        vga::print("Interactive mode: Enter SQL commands or 'exit' to quit\n");
        
        char buffer[512];
        while (true) {
            vga::print("sqlite> ");
            
            int len = 0;
            while (len < 511) {
                char c = kbd::read_char();
                if (c == '\n') {
                    buffer[len] = '\0';
                    break;
                } else if (c == '\b' && len > 0) {
                    len--;
                    vga::print("\b \b");
                } else if (c >= ' ' && c <= '~') {
                    buffer[len++] = c;
                    vga::print("%c", c);
                }
            }
            
            vga::print("\n");
            
            if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
                break;
            }
            
            if (strlen(buffer) > 0) {
                char* errmsg = nullptr;
                rc = sqlite3_exec(current_db, buffer, print_callback, nullptr, &errmsg);
                
                if (rc != SQLITE_OK) {
                    vga::print("Error: %s\n", errmsg ? errmsg : sqlite3_errmsg(current_db));
                    if (errmsg) free(errmsg);
                } else {
                    vga::print("OK\n");
                }
            }
        }
        
        sqlite3_close(current_db);
        current_db = nullptr;
        return;
    }

    vga::print("SQL: %s\n", sql);
    
    char* errmsg = nullptr;
    rc = sqlite3_exec(current_db, sql, print_callback, nullptr, &errmsg);
    
    if (rc != SQLITE_OK) {
        vga::print("Error: %s\n", errmsg ? errmsg : sqlite3_errmsg(current_db));
        if (errmsg) free(errmsg);
    } else {
        vga::print("OK\n");
    }
    
    sqlite3_close(current_db);
    current_db = nullptr;
}

int process_sqlite_command(const char* db_name, const char* sql) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_name, &db);
    
    if (rc != SQLITE_OK) {
        return rc;
    }
    
    rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    sqlite3_close(db);
    return rc;
}

void init() {
    current_db = nullptr;
    reset_heap();
    vga::print("SQLite3 shell initialized\n");
}

}