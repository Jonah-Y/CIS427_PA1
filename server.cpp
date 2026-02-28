#include <cstring>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"
#include <strings.h>
#include <unordered_set>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "utils.hpp"

using namespace std;

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE    256

// Valid commands including PA2 additions
unordered_set<string> valid_commands = {
    "BUY", "SELL", "BALANCE", "LIST", "SHUTDOWN", "QUIT",
    "LOGIN", "LOGOUT"
};

int main(int argc, char* argv[]) {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    struct sockaddr_in sin;
    char buf[MAX_LINE];
    int buf_len;
    socklen_t addr_len;
    int s, new_s;

    // Open the database
    rc = sqlite3_open("users_and_stocks.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return 0;
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }

    // Create tables and seed data
    create_users(db);
    create_stocks(db);
    seed_pa2_users(db); // inserts root/mary/john/moe if they don't exist yet

    // Build address structure
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port        = htons(SERVER_PORT);

    if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    // Allows server to restart quickly without getting "Address already in use"
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if ((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
        perror("bind error");
        exit(1);
    }
    listen(s, MAX_PENDING);

    fprintf(stdout, "Server listening on port %d\n", SERVER_PORT);

    // Main accept loop - handles one client at a time
    while (1) {
        printf("Waiting on connection\n");

        addr_len = sizeof(sin);
        if ((new_s = accept(s, (struct sockaddr *)&sin, &addr_len)) < 0) {
            perror("accept error");
            exit(1);
        }

        // Log the connecting client's IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin.sin_addr, client_ip, sizeof(client_ip));
        printf("Client connected from %s\n", client_ip);

        // Session state - reset fresh for every new connection
        bool   logged_in        = false;
        int    session_user_id  = -1;
        string session_username = "";

        // Receive loop for this client
        while ((buf_len = recv(new_s, buf, sizeof(buf) - 1, 0)) > 0) {

            // Null-terminate and strip trailing newline
            buf[buf_len] = '\0';
            if (buf_len > 0 && (buf[buf_len-1] == '\n' || buf[buf_len-1] == '\r'))
                buf[buf_len-1] = '\0';

            printf("Received: %s\n", buf);
            fflush(stdout);

            string request(buf);

            // QUIT is always allowed even without login
            if (request == "QUIT") {
                quit_command(new_s, buf, db);
                printf("Client sent QUIT. Closing connection.\n");
                break;
            }

            // LOGIN - parse credentials and verify against the database
            if (request.find("LOGIN ", 0) == 0) {
                if (logged_in) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                             "400 Already logged in as %s\n",
                             session_username.c_str());
                    send(new_s, msg, strlen(msg), 0);
                    continue;
                }

                string cmd, username, password;
                istringstream iss(request);
                iss >> cmd >> username >> password;

                if (username.empty() || password.empty()) {
                    const char* fmt_err =
                        "400 message format error\n"
                        "Correct format: LOGIN <UserID> <Password>\n";
                    send(new_s, fmt_err, strlen(fmt_err), 0);
                    continue;
                }

                int found_id = -1;
                if (verify_login(db, username, password, found_id)) {
                    logged_in        = true;
                    session_user_id  = found_id;
                    session_username = username;

                    printf("[LOGIN] '%s' logged in successfully.\n",
                           session_username.c_str());

                    const char* ok = "200 OK\n";
                    send(new_s, ok, strlen(ok), 0);
                } else {
                    const char* fail = "403 Wrong UserID or Password\n";
                    printf("[LOGIN] Failed attempt for user '%s'.\n", username.c_str());
                    send(new_s, fail, strlen(fail), 0);
                }
                continue;
            }

            // LOGOUT - clear session and close this connection
            if (request == "LOGOUT") {
                if (!logged_in) {
                    const char* err = "400 Not logged in\n";
                    send(new_s, err, strlen(err), 0);
                    continue;
                }

                printf("[LOGOUT] '%s' logged out.\n", session_username.c_str());

                logged_in        = false;
                session_user_id  = -1;
                session_username = "";

                const char* ok = "200 OK\n";
                send(new_s, ok, strlen(ok), 0);

                printf("Connection closed after LOGOUT.\n");
                break;
            }

            // All other commands require the user to be logged in
            if (!logged_in) {
                const char* warning = "Please login first\n";
                fprintf(stderr, "[AUTH] Blocked command - not logged in.\n");
                send(new_s, warning, strlen(warning), 0);
                continue;
            }

            // Check root status once, used by LIST and SHUTDOWN
            bool is_root = (strcasecmp(session_username.c_str(), "root") == 0);

            if (request.find("BUY", 0) == 0) {
                buy_command(new_s, buf, db);

            } else if (request.find("SELL", 0) == 0) {
                sell_command(new_s, buf, db);

            } else if (request == "LIST") {
                list_command(new_s, buf, db,
                             session_user_id, session_username, is_root);

            } else if (request == "BALANCE") {
                balance_command(new_s, buf, db, session_user_id);

            } else if (request == "SHUTDOWN") {
                int result = shutdown_command(new_s, buf, db, is_root);
                if (result == -99) {
                    close(new_s);
                    close(s);
                    exit(0);
                }

            } else {
                fprintf(stderr, "Invalid message request: %s\n", request.c_str());
                const char* error_code =
                    "400 invalid command\n"
                    "Valid commands: LOGIN, LOGOUT, BUY, SELL, LIST, "
                    "BALANCE, SHUTDOWN, QUIT\n";
                send(new_s, error_code, strlen(error_code), 0);
            }

        } // end recv loop

        close(new_s);
        printf("Connection closed. Waiting for next client.\n");

    } // end accept loop

    sqlite3_close(db);
    return 0;
}