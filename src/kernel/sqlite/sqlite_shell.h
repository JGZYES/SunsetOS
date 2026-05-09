#ifndef SQLITE_SHELL_H
#define SQLITE_SHELL_H

namespace sqlite_shell {

void init();
void cmd_sqlite3(int argc, char* args[]);
int process_sqlite_command(const char* db_name, const char* sql);

}

#endif