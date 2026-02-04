#include "utils.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sqlite3.h>
#include <string>
#include <sys/socket.h>    /* Unix; Windows: use winsock2.h for send() */

using namespace std;

int callback(void *data, int argc, char **argv, char **azColName) {
    fprintf(stderr, "%s: ", (const char*)data);

    for (int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}


int count_users(void *count, int argc, char **argv, char **azColName) {
    if (argc == 1) {
        *static_cast<int*>(count) = atoi(argv[0]);
    } else {
        fprintf(stderr, "Invalid use of count_users()");
    }
    return 0;
}


int buy_command(int socket, char* request, sqlite3* db) {
    char *buy = strtok(request, " ");
    char *stock_symbol = strtok(NULL, " ");
    char *stock_amount = strtok(NULL, " ");
    char *price = strtok(NULL, " ");
    char *user_id = strtok(NULL, " ");

    // check for format errors
    if (!buy || !stock_symbol || !stock_amount || !price || !user_id) {
        const char* error_code = "403 message format error\nCorrect format: BUY <stock_symbol> <amount> <price> <user_id>\n";
        send(socket, error_code, strlen(error_code), 0);
        return -1;
    }

    // TODO finish

    return 0;
}


int sell_command(int socket, char* request, sqlite3* db) {

    // TODO

    return 0;
}


int shutdown_command(int socket, char* request, sqlite3* db) {

    // TODO

    return 0;
}


static int list_callback(void *data, int argc, char **argv, char **azColName) {
    string* result = static_cast<string*>(data);
   
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            *result += argv[i];
        } else {
            *result += "NULL";
        }
        if (i < argc - 1) {
            *result += " ";
        }
    }
    *result += "\n";
   
    return 0;
}

int list_command(int socket, char* request, sqlite3* db) {
    /* default user id */
    int user_id = 1;
   
    string response;
    string records;
    char *zErrMsg = 0;
   
    /* query stocks for user */
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT ID, stock_symbol, stock_balance, user_id FROM Stocks WHERE user_id = %d;",
             user_id);
   
    int rc = sqlite3_exec(db, sql, list_callback, &records, &zErrMsg);
   
    if (rc != SQLITE_OK) {
        response = "403 message format error\n";
        response += "Database error: ";
        response += zErrMsg;
        response += "\n";
        sqlite3_free(zErrMsg);
    } else {
        response = "200 OK\n";
        response += "The list of records in the Stocks database for user ";
        response += to_string(user_id);
        response += ":\n";
       
        if (records.empty()) {
            response += "(No stocks found)\n";
        } else {
            response += records;
        }
    }
   
    send(socket, response.c_str(), response.length(), 0);
    return 0;
}

int balance_command(int socket, char* request, sqlite3* db) {
    /* default user id */
    int user_id = 1;
   
    string response;
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT first_name, last_name, usd_balance FROM Users WHERE ID = %d;",
             user_id);
   
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
   
    if (rc != SQLITE_OK) {
        response = "403 message format error\n";
        response += "Database error: ";
        response += sqlite3_errmsg(db);
        response += "\n";
        send(socket, response.c_str(), response.length(), 0);
        return -1;
    }
   
    rc = sqlite3_step(stmt);
   
    if (rc == SQLITE_ROW) {
        /* user found */
        const char* first_name = (const char*)sqlite3_column_text(stmt, 0);
        const char* last_name = (const char*)sqlite3_column_text(stmt, 1);
        double usd_balance = sqlite3_column_double(stmt, 2);
       
        response = "200 OK\n";
        response += "Balance for user ";
        if (first_name) response += first_name;
        response += " ";
        if (last_name) response += last_name;
       
        char balance_str[50];
        snprintf(balance_str, sizeof(balance_str), ": $%.2f\n", usd_balance);
        response += balance_str;
       
    } else if (rc == SQLITE_DONE) {
        /* user not found */
        response = "403 message format error\n";
        response += "User ";
        response += to_string(user_id);
        response += " doesn't exist\n";
    } else {
        /* database error */
        response = "403 message format error\n";
        response += "Database error: ";
        response += sqlite3_errmsg(db);
        response += "\n";
    }
   
    sqlite3_finalize(stmt);
    send(socket, response.c_str(), response.length(), 0);
    return 0;
}

int quit_command(int socket, char* request, sqlite3* db) {
    /* acknowledge quit */
    const char* response = "200 OK\n";
    send(socket, response, strlen(response), 0);
   
    /* signal server to close connection */
    return 1;
}
