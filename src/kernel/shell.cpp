#include "shell.h"
#include "vga.h"
#include "fat32.h"
#include "kbd.h"
#include "libc.h"
#include "net.h"
#include "user.h"
#include "syscall.h"
#include "sqlite/sqlite_shell.h"
#include "elf.h"

namespace shell {

constexpr size_t MAX_CMD_LEN = 256;
constexpr size_t MAX_ARGS = 16;

static char command[MAX_CMD_LEN];
static char* args[MAX_ARGS];

void init() {
    vga::init();
    if (!fat32::init()) {
        vga::print("Warning: FAT32 filesystem not found, formatting...\n");
        fat32::format_drive(0);
        fat32::init();
    }
    net::init();
    user::init();
    syscall::init();
    sqlite_shell::init();
}

static void print_welcome() {
    vga::put_string("========================================\n", vga::Color::Yellow, vga::Color::Black);
    vga::put_string("    Welcome to SunsetOS v1.0!\n", vga::Color::LightCyan, vga::Color::Black);
    vga::put_string("========================================\n", vga::Color::Yellow, vga::Color::Black);
    vga::print("Type 'help' for available commands.\n\n");
}

static int parse_command(const char* cmd) {
    memset(command, 0, sizeof(command));
    memset(args, 0, sizeof(args));

    strncpy(command, cmd, MAX_CMD_LEN - 1);

    int argc = 0;
    char* token = strtok(command, " \t\n");

    while (token && argc < MAX_ARGS) {
        args[argc++] = token;
        token = strtok(nullptr, " \t\n");
    }

    return argc;
}

static void ls_callback(const char* name, uint8_t attributes, uint32_t size) {
    if (attributes & fat32::ATTR_DIRECTORY) {
        vga::print("%s/\n", name);
    } else {
        vga::print("%s (%d bytes)\n", name, size);
    }
}

static void cmd_help() {
    vga::print("Available commands:\n");
    vga::print("  help      - Show this help message\n");
    vga::print("  ls        - List directory contents\n");
    vga::print("  mkdir     - Create directory\n");
    vga::print("  mkfile    - Create empty file\n");
    vga::print("  fview     - View file contents\n");
    vga::print("  fedit     - Edit file contents\n");
    vga::print("  rename    - Rename file or directory\n");
    vga::print("  rm        - Remove file or directory\n");
    vga::print("  cd        - Change directory\n");
    vga::print("  pwd       - Print current directory\n");
    vga::print("  clear     - Clear screen\n");
    vga::print("  curl      - Fetch URL content\n");
    vga::print("  wget      - Download file from URL\n");
    vga::print("  useradd   - Create new user (admin only)\n");
    vga::print("  userdel   - Delete user (admin only)\n");
    vga::print("  login     - Login as user\n");
    vga::print("  logout    - Logout current user\n");
    vga::print("  passwd    - Change password\n");
    vga::print("  users     - List all users (admin only)\n");
    vga::print("  syscall   - Test system calls\n");
    vga::print("  sqlite3   - SQLite3 database client\n");
    vga::print("  exec      - Execute ELF binary\n");
    vga::print("  fsinfo    - Show filesystem information\n");
    vga::print("  format    - Format disk (WARNING: destroys all data)\n");
    vga::print("  shutdown  - Shutdown the system\n");
    vga::print("  reboot    - Reboot the system\n");
}

static void cmd_ls(int argc) {
    const char* path = (argc == 1) ? fat32::get_current_dir() : args[1];
    if (fat32::list_directory(path, ls_callback) != 0) {
        vga::print("Error: Cannot list directory '%s'\n", path);
    }
}

static void cmd_mkdir(int argc) {
    if (argc < 2) {
        vga::print("Usage: mkdir <directory>\n");
        return;
    }
    if (fat32::create_directory(args[1]) != 0) {
        vga::print("Error: Cannot create directory '%s'\n", args[1]);
    } else {
        vga::print("Directory '%s' created\n", args[1]);
    }
}

static void cmd_mkfile(int argc) {
    if (argc < 2) {
        vga::print("Usage: mkfile <filename>\n");
        return;
    }
    if (fat32::create_file(args[1], fat32::ATTR_ARCHIVE) != 0) {
        vga::print("Error: Cannot create file '%s'\n", args[1]);
    } else {
        vga::print("File '%s' created\n", args[1]);
    }
}

static void cmd_fview(int argc) {
    if (argc < 2) {
        vga::print("Usage: fview <filename>\n");
        return;
    }
    
    uint8_t buffer[4096];
    uint32_t bytes_read;
    
    if (fat32::read_file(args[1], buffer, sizeof(buffer), &bytes_read) != 0) {
        vga::print("Error: Cannot read file '%s'\n", args[1]);
        return;
    }
    
    for (uint32_t i = 0; i < bytes_read; i++) {
        vga::put_char(buffer[i]);
    }
    vga::newline();
}

static void cmd_fedit(int argc) {
    if (argc < 3) {
        vga::print("Usage: fedit <filename> <content>\n");
        return;
    }

    char content[MAX_CMD_LEN];
    content[0] = '\0';
    for (int i = 2; i < argc; i++) {
        if (i > 2) strcat(content, " ");
        strcat(content, args[i]);
    }
    
    if (fat32::write_file(args[1], reinterpret_cast<uint8_t*>(content), strlen(content)) != 0) {
        vga::print("Error: Cannot write to file '%s'\n", args[1]);
    } else {
        vga::print("File '%s' written\n", args[1]);
    }
}

static void cmd_rename(int argc) {
    if (argc != 3) {
        vga::print("Usage: rename <old_name> <new_name>\n");
        return;
    }
    vga::print("Rename not yet implemented\n");
}

static void cmd_rm(int argc) {
    if (argc != 2) {
        vga::print("Usage: rm <filename>\n");
        return;
    }
    if (fat32::delete_file(args[1]) != 0) {
        vga::print("Error: Cannot delete '%s'\n", args[1]);
    } else {
        vga::print("'%s' deleted\n", args[1]);
    }
}

static void cmd_cd(int argc) {
    if (argc < 2) {
        vga::print("Usage: cd <directory>\n");
        return;
    }
    fat32::set_current_dir(args[1]);
}

static void cmd_pwd() {
    vga::print("%s\n", fat32::get_current_dir());
}

static void cmd_clear() {
    vga::clear();
}

static void cmd_curl(int argc) {
    if (argc < 2) {
        vga::print("Usage: curl <url>\n");
        return;
    }
    net::curl(args[1]);
}

static void cmd_wget(int argc) {
    if (argc < 3) {
        vga::print("Usage: wget <url> <filename>\n");
        return;
    }
    net::wget(args[1], args[2]);
}

static void cmd_useradd(int argc) {
    if (argc < 3) {
        vga::print("Usage: useradd <username> <password> [--admin]\n");
        return;
    }
    int is_admin = 0;
    if (argc >= 4 && strcmp(args[3], "--admin") == 0) {
        is_admin = 1;
    }
    user::create_user(args[1], args[2], is_admin);
}

static void cmd_userdel(int argc) {
    if (argc < 2) {
        vga::print("Usage: userdel <username>\n");
        return;
    }
    user::delete_user(args[1]);
}

static void cmd_login(int argc) {
    if (argc < 3) {
        vga::print("Usage: login <username> <password>\n");
        return;
    }
    user::login(args[1], args[2]);
}

static void cmd_logout(int argc) {
    (void)argc;
    user::logout();
}

static void cmd_passwd(int argc) {
    if (argc < 4) {
        vga::print("Usage: passwd <username> <old_password> <new_password>\n");
        return;
    }
    user::change_password(args[1], args[2], args[3]);
}

static void cmd_users(int argc) {
    (void)argc;
    user::list_users();
}

static void cmd_syscall(int argc) {
    if (argc < 2) {
        vga::print("Usage: syscall <command>\n");
        vga::print("Available syscalls:\n");
        vga::print("  print <string>  - Test print syscall\n");
        vga::print("  getpid          - Get process ID\n");
        vga::print("  gettime         - Get timestamp\n");
        return;
    }

    if (strcmp(args[1], "print") == 0) {
        if (argc < 3) {
            vga::print("Usage: syscall print <string>\n");
            return;
        }
        syscall::print(args[2]);
        vga::print("\n");
    } else if (strcmp(args[1], "getpid") == 0) {
        uint32_t pid = syscall::get_pid();
        vga::print("PID: %d\n", pid);
    } else if (strcmp(args[1], "gettime") == 0) {
        uint32_t time = syscall::get_time();
        vga::print("Timestamp: %u\n", time);
    } else {
        vga::print("Unknown syscall command: %s\n", args[1]);
    }
}

static void cmd_exec(int argc) {
    if (argc < 2) {
        vga::print("Usage: exec <filename>\n");
        return;
    }
    
    uint8_t buffer[1024 * 1024];
    uint32_t bytes_read;
    
    if (fat32::read_file(args[1], buffer, sizeof(buffer), &bytes_read) != 0) {
        vga::print("Error: Cannot read file '%s'\n", args[1]);
        return;
    }
    
    if (!elf::is_valid_elf(buffer)) {
        vga::print("Error: '%s' is not a valid ELF file\n", args[1]);
        return;
    }
    
    uint32_t entry_point;
    if (elf::load_elf_safe(buffer, bytes_read, &entry_point) != 0) {
        vga::print("Error: Failed to load ELF file\n");
        return;
    }
    
    vga::print("Executing ELF at 0x%X...\n", entry_point);
    
    vga::print("Preparing to execute...\n");
    
    __asm__ volatile (
        "movl %0, %%esp\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "jmp *%1\n"
        : : "r"(elf::USER_STACK_TOP), "r"(entry_point)
    );
    
    vga::print("ELF program returned\n");
}

static void cmd_fsinfo(int argc) {
    (void)argc;
    fat32::print_filesystem_info();
}

static void cmd_format(int argc) {
    (void)argc;
    vga::print("WARNING: This will erase all data on the disk!\n");
    vga::print("Are you sure? Type 'YES' to confirm: ");
    
    char confirm[4];
    int len = 0;
    while (len < 3) {
        char c = kbd::read_char();
        if (c == '\n') break;
        if (c >= ' ' && c <= '~') {
            confirm[len++] = c;
            vga::put_char(c);
        }
    }
    confirm[len] = '\0';
    vga::newline();
    
    if (strcmp(confirm, "YES") == 0) {
        fat32::format_drive(0);
    } else {
        vga::print("Format cancelled\n");
    }
}

static void cmd_shutdown(int argc) {
    (void)argc;
    vga::print("System halted. Please power off manually.\n");
    for (;;) asm ("hlt");
}

static void cmd_reboot(int argc) {
    (void)argc;
    vga::print("Rebooting...\n");
    asm volatile ("movw $0x1234, %ax");
    asm volatile ("movw %ax, 0x472");
    asm volatile ("ljmp $0xFFFF, $0x0000");
}

static void execute_command(const char* cmd) {
    int argc = parse_command(cmd);

    if (argc == 0) return;

    if (strcmp(args[0], "help") == 0) {
        cmd_help();
    } else if (strcmp(args[0], "ls") == 0) {
        cmd_ls(argc);
    } else if (strcmp(args[0], "mkdir") == 0) {
        cmd_mkdir(argc);
    } else if (strcmp(args[0], "mkfile") == 0) {
        cmd_mkfile(argc);
    } else if (strcmp(args[0], "fview") == 0) {
        cmd_fview(argc);
    } else if (strcmp(args[0], "fedit") == 0) {
        cmd_fedit(argc);
    } else if (strcmp(args[0], "rename") == 0) {
        cmd_rename(argc);
    } else if (strcmp(args[0], "rm") == 0) {
        cmd_rm(argc);
    } else if (strcmp(args[0], "cd") == 0) {
        cmd_cd(argc);
    } else if (strcmp(args[0], "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(args[0], "clear") == 0) {
        cmd_clear();
    } else if (strcmp(args[0], "curl") == 0) {
        cmd_curl(argc);
    } else if (strcmp(args[0], "wget") == 0) {
        cmd_wget(argc);
    } else if (strcmp(args[0], "useradd") == 0) {
        cmd_useradd(argc);
    } else if (strcmp(args[0], "userdel") == 0) {
        cmd_userdel(argc);
    } else if (strcmp(args[0], "login") == 0) {
        cmd_login(argc);
    } else if (strcmp(args[0], "logout") == 0) {
        cmd_logout(argc);
    } else if (strcmp(args[0], "passwd") == 0) {
        cmd_passwd(argc);
    } else if (strcmp(args[0], "users") == 0) {
        cmd_users(argc);
    } else if (strcmp(args[0], "syscall") == 0) {
        cmd_syscall(argc);
    } else if (strcmp(args[0], "sqlite3") == 0) {
        sqlite_shell::cmd_sqlite3(argc, args);
    } else if (strcmp(args[0], "exec") == 0) {
        cmd_exec(argc);
    } else if (strcmp(args[0], "fsinfo") == 0) {
        cmd_fsinfo(argc);
    } else if (strcmp(args[0], "format") == 0) {
        cmd_format(argc);
    } else if (strcmp(args[0], "shutdown") == 0) {
        cmd_shutdown(argc);
    } else if (strcmp(args[0], "reboot") == 0) {
        cmd_reboot(argc);
    } else {
        vga::print("Unknown command: %s. Type 'help' for available commands.\n", args[0]);
    }
}

void run() {
    print_welcome();

    char input[MAX_CMD_LEN];
    int input_len = 0;

    while (true) {
        vga::put_string(user::get_current_user(), vga::Color::LightBlue, vga::Color::Black);
        vga::put_string("@sunsetos:", vga::Color::Yellow, vga::Color::Black);
        vga::put_string(fat32::get_current_dir(), vga::Color::LightGreen, vga::Color::Black);
        vga::put_string(" $ ", vga::Color::Yellow, vga::Color::Black);

        input_len = 0;
        memset(input, 0, sizeof(input));

        while (true) {
            char c = kbd::read_char();

            if (c == '\n') {
                vga::newline();
                if (input_len > 0) {
                    execute_command(input);
                }
                break;
            } else if (c == '\b' && input_len > 0) {
                input_len--;
                input[input_len] = '\0';
                vga::put_char('\b');
                vga::put_char(' ');
                vga::put_char('\b');
            } else if (c >= ' ' && c <= '~' && input_len < MAX_CMD_LEN - 1) {
                input[input_len++] = c;
                vga::put_char(c);
            }
        }
    }
}

}
