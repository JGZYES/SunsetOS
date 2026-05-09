#include "user.h"
#include "vga.h"
#include "libc.h"

namespace user {

static User users[MAX_USERS];
static int user_count = 0;
static char current_user[MAX_USERNAME_LEN] = "root";
static int is_logged_in = 1;

void init() {
    memset(users, 0, sizeof(users));
    
    strcpy(users[0].username, "root");
    strcpy(users[0].password, "root");
    users[0].is_active = 1;
    users[0].is_admin = 1;
    user_count = 1;
    
    vga::print("User system initialized (default: root/root)\n");
}

int create_user(const char* username, const char* password, int is_admin) {
    if (!is_logged_in || !users[0].is_admin) {
        vga::print("Error: Permission denied\n");
        return -1;
    }
    
    if (user_count >= MAX_USERS) {
        vga::print("Error: Maximum users reached\n");
        return -1;
    }
    
    if (strlen(username) >= MAX_USERNAME_LEN || strlen(password) >= MAX_PASSWORD_LEN) {
        vga::print("Error: Username or password too long\n");
        return -1;
    }
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            vga::print("Error: User already exists\n");
            return -1;
        }
    }
    
    strcpy(users[user_count].username, username);
    strcpy(users[user_count].password, password);
    users[user_count].is_active = 1;
    users[user_count].is_admin = is_admin ? 1 : 0;
    user_count++;
    
    vga::print("User '%s' created successfully\n", username);
    return 0;
}

int delete_user(const char* username) {
    if (!is_logged_in || !users[0].is_admin) {
        vga::print("Error: Permission denied\n");
        return -1;
    }
    
    if (strcmp(username, "root") == 0) {
        vga::print("Error: Cannot delete root user\n");
        return -1;
    }
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            for (int j = i; j < user_count - 1; j++) {
                users[j] = users[j + 1];
            }
            user_count--;
            vga::print("User '%s' deleted successfully\n", username);
            return 0;
        }
    }
    
    vga::print("Error: User not found\n");
    return -1;
}

int login(const char* username, const char* password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && users[i].is_active) {
            if (strcmp(users[i].password, password) == 0) {
                strcpy(current_user, username);
                is_logged_in = 1;
                vga::print("Login successful. Welcome, %s!\n", username);
                return 0;
            } else {
                vga::print("Error: Incorrect password\n");
                return -1;
            }
        }
    }
    
    vga::print("Error: User not found\n");
    return -1;
}

void logout() {
    strcpy(current_user, "");
    is_logged_in = 0;
    vga::print("Logged out successfully\n");
}

int change_password(const char* username, const char* old_password, const char* new_password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            if (!is_logged_in) {
                vga::print("Error: Not logged in\n");
                return -1;
            }
            
            if (strcmp(current_user, username) != 0 && !users[0].is_admin) {
                vga::print("Error: Permission denied\n");
                return -1;
            }
            
            if (strcmp(users[i].password, old_password) != 0) {
                vga::print("Error: Incorrect password\n");
                return -1;
            }
            
            if (strlen(new_password) >= MAX_PASSWORD_LEN) {
                vga::print("Error: Password too long\n");
                return -1;
            }
            
            strcpy(users[i].password, new_password);
            vga::print("Password changed successfully\n");
            return 0;
        }
    }
    
    vga::print("Error: User not found\n");
    return -1;
}

void list_users() {
    if (!is_logged_in || !users[0].is_admin) {
        vga::print("Error: Permission denied\n");
        return;
    }
    
    vga::print("Users:\n");
    for (int i = 0; i < user_count; i++) {
        vga::print("  %s (%s)\n", users[i].username, users[i].is_admin ? "admin" : "user");
    }
}

const char* get_current_user() {
    return current_user;
}

int is_admin() {
    if (!is_logged_in) return 0;
    
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, current_user) == 0) {
            return users[i].is_admin;
        }
    }
    return 0;
}

}