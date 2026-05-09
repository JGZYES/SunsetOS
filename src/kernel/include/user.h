#pragma once

#include <stdint.h>
#include <stddef.h>

namespace user {

constexpr size_t MAX_USERS = 10;
constexpr size_t MAX_USERNAME_LEN = 32;
constexpr size_t MAX_PASSWORD_LEN = 32;

struct User {
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    uint8_t is_active;
    uint8_t is_admin;
};

void init();
int create_user(const char* username, const char* password, int is_admin);
int delete_user(const char* username);
int login(const char* username, const char* password);
void logout();
int change_password(const char* username, const char* old_password, const char* new_password);
void list_users();
const char* get_current_user();
int is_admin();

}