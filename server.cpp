#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <strings.h>       /* Windows: not standard; use <cstring> + memset() instead of bzero */
#include <unordered_set>
#include <string>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>     /* Unix socket headers; Windows: use winsock2.h, ws2tcpip.h */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>        /* Unix; Windows: close() -> closesocket() for sockets */
#include "utils.hpp"
/* Windows: call WSAStartup() before any socket use and WSACleanup() before exit */

using namespace std;

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE    256

// list of valid commands
unordered_set<string> valid_commands = {
    "BUY",
    "SELL",
    "BALANCE",
    "LIST",
    "SHUTDOWN",
    "QUIT"
};


int main(int argc, char* argv[]) {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    const char *sql;

    struct sockaddr_in sin;
    char buf[MAX_LINE];
    int buf_len;
    socklen_t addr_len;
    int s, new_s;          /* Windows: use SOCKET type; check for INVALID_SOCKET instead of < 0 */

    // Open the database and check for errors
    rc = sqlite3_open("users_and_stocks.db", &db);
    if( rc ) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }


    // Create the Users table
    sql = "CREATE TABLE IF NOT EXISTS Users (" \
        "ID INTEGER PRIMARY KEY AUTOINCREMENT," \
        "email TEXT NOT NULL," \
        "first_name TEXT," \
        "last_name TEXT," \
        "user_name TEXT NOT NULL," \
        "password TEXT," \
        "usd_balance DOUBLE NOT NULL);" ;

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Table created successfully\n");
    }
    

    // Create the Stocks table
    sql = "CREATE TABLE IF NOT EXISTS Stocks (" \
        "ID INTEGER PRIMARY KEY AUTOINCREMENT," \
        "stock_symbol VARCHAR(4) NOT NULL," \
        "stock_name VARCHAR(20) NOT NULL," \
        "stock_balance DOUBLE," \
        "user_id INTEGER," \
        "FOREIGN KEY (user_id) REFERENCES Users (ID) );";

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Table created successfully\n");
    }


    // Check if there are any users and if not manually create one
    sql = "SELECT COUNT(*) FROM Users;";
    int user_count = 0;
    cout << "checking user count" << endl;

    rc = sqlite3_exec(db, sql, count_users, (void*) &user_count, &zErrMsg);
    if( rc != SQLITE_OK ){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Operation done successfully\n");
    }

    if (user_count == 0) {
        fprintf(stdout, "Creating a user because no users currently exist\n");

        sql = "INSERT INTO Users (ID, email, first_name, last_name, user_name, password, usd_balance)" \
            "VALUES (1, 'joeshmoe@default.com', 'Joe', 'Shmoe', 'Default_User', 'password1', 100.00 );";

        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
        if( rc != SQLITE_OK ){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Records created successfully\n");
        }
    } else {
        fprintf(stdout, "There are currently %d users in the database\n", user_count);
    }


    /* build address data structure */
    /* Windows: call WSAStartup(MAKEWORD(2,2), &wsaData) here before socket/bind/listen */
    bzero((char *)&sin, sizeof(sin));  /* set sin to 0; Windows: use memset(&sin, 0, sizeof(sin)) */
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);


    /* setup passive open */
    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {  /* Windows: check == INVALID_SOCKET */
        perror("socket error");
        exit(1);
    }
    if ((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {  /* Windows: different error style */
        perror("bind error");
        exit(1);
    }
    listen(s, MAX_PENDING);


    /* wait for connection, then receive and print text */
    while(1) {
        printf("Waiting on connection\n");
        if ((new_s = accept(s, (struct sockaddr *)&sin, &addr_len)) < 0) {  /* Windows: INVALID_SOCKET */
            perror("accept error");
            exit(1);
        }
        printf("Connected\n");
        while ((buf_len = recv(new_s, buf, sizeof(buf), 0))) {
            printf("Request recieved: ");
            fputs(buf, stdout); // print the text recieved

            string request(buf);  //convert to string object to work with it easier
            if (request.find("BUY", 0) == 0) {
                buy_command(new_s, buf, db);
            } else if (request.find("SELL", 0) == 0) {
                // call sell_command
            } else if (request.find("LIST", 0) == 0) {
                list_command(new_s, buf, db);
            } else if (request.find("BALANCE", 0) == 0) {
                balance_command(s, buf, db);
            } else if (request.find("SHUTDOWN", 0) == 0) {
                // call shutdown_command
            } else if (request.find("QUIT", 0) == 0) {
                quit_command(s, buf, db);
                break; //break from this while loop to close the connection and wait to recieve a new connection
            } else {
                fprintf(stderr, "Invalid message request: %s\n", request.c_str());
                const char* error_code = "400 invalid command\nPlease use BUY, SELL, LIST, BALANCE, SHUTDOWN, or QUIT commands\n";
                send(new_s, error_code, strlen(error_code), 0);
            }


        }
        close(new_s);      /* Windows: use closesocket(new_s) */
    }


    sqlite3_close(db);
    /* Windows: call WSACleanup() here before return */
    return 0;
}