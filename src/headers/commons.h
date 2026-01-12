#ifndef SO_2025_COMMONS_H
#define SO_2025_COMMONS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <strings.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>
#include "logger.h"

#define ANSI_RED   "\x1b[31m"
#define ANSI_RESET "\x1b[0m"

#define MAX_COUNT_QUEUE 4096

#ifdef DEBUGLOG
#define perror(fmt, ...)                                                       \
    do {                                                                       \
        struct timeval tv;                                                     \
        gettimeofday(&tv, NULL);                                               \
                                                                               \
        struct tm tm_info;                                                     \
        localtime_r(&tv.tv_sec, &tm_info);                                     \
                                                                               \
        char ts[64];                                                           \
        size_t ts_len = strftime(ts, sizeof(ts),                               \
                                 "%d/%m/%Y - %H:%M:%S", &tm_info);             \
        ts_len += snprintf(ts + ts_len, sizeof(ts) - ts_len,                   \
                           ".%03d", (int)(tv.tv_usec / 1000));                 \
                                                                               \
        fprintf(stdout, ANSI_RED                                               \
                "[%s] - [%s:%d] - [PID:%d] - [%s] --> [" fmt "]\n" ANSI_RESET, \
                "SYSER",                                                       \
                __FILE__,__LINE__,                                             \
                (int)getpid(),                                                 \
                ts,                                                            \
                ##__VA_ARGS__);                                                \
    } while (0)

#else
#define perror(buffer, ...) syserr_log(buffer, ##__VA_ARGS__)
#endif

#ifdef DEBUGLOG
#define printf(fmt, ...)                                                       \
    do {                                                                       \
        struct timeval tv;                                                     \
        gettimeofday(&tv, NULL);                                               \
                                                                               \
        struct tm tm_info;                                                     \
        localtime_r(&tv.tv_sec, &tm_info);                                     \
                                                                               \
        char ts[64];                                                           \
        size_t ts_len = strftime(ts, sizeof(ts),                               \
                                 "%d/%m/%Y - %H:%M:%S", &tm_info);             \
        ts_len += snprintf(ts + ts_len, sizeof(ts) - ts_len,                   \
                           ".%03d", (int)(tv.tv_usec / 1000));                 \
                                                                               \
        fprintf(stdout,                                                        \
                "[%s] - [%s:%d] - [PID:%d] - [%s] --> [" fmt "]\n",            \
                "TRACE",                                                       \
                __FILE__, __LINE__,                                            \
                (int)getpid(),                                                 \
                ts,                                                            \
                ##__VA_ARGS__);                                                \
    } while (0)

#else
#define printf(buffer, ...) appinfo_log(buffer, ##__VA_ARGS__)
#endif

#ifdef DEBUGLOG
#define log_init(filePath) printf("DEBUG LOG STARTED AT %s", filePath)
#else
#define log_init(filePath) log_init(filePath)
#endif

#ifdef DEBUGLOG
#define log_stop() printf("DEBUG LOG STOPPED")
#else
#define log_stop() log_stop()
#endif

struct postal_counter {
    int counter_id;
    int purpose;
    pid_t worker_id;
    pid_t pid_array[MAX_COUNT_QUEUE];

    int queue;
    int served_requests;
};

typedef struct postal_counter counter;

struct simulation_parameter {
    int NOF_WORKER_SEATS;
    int NOF_WORKERS;
    int NOF_USERS;
    int NOF_PAUSE;

    int P_SERV_MIN;
    int P_SERV_MAX;

    int SIM_DURATION; // days
    int N_NANO_SECONDS; // minutes
    int EXPLODE_THRESHOLD;
};
typedef struct simulation_parameter* parameter;

#define SHM_COUNTERS_KEY 0x1234
#define SHM_CONFIG_KEY 0x0209
#define SHM_LOG_KEY 0x9101
#define MSG_KEY 0x5678
#define SEM_KEY 0x9100
#define LOGSEM_KEY 0x1606

#ifndef TEST_ERROR
#define TEST_ERROR                                 \
    if (errno) {                                   \
        fprintf(stderr,                            \
                "%s:%d: PID=%5d: Error %d (%s)\n", \
                __FILE__,                          \
                __LINE__,                          \
                getpid(),                          \
                errno,                             \
                strerror(errno));                  \
    }
#endif

#define MASTER_PATH "./build/master"
#define OPERATOR_PATH "./build/operator"
#define USER_PATH "./build/user"
#define EROGATORE_TICKET_PATH "./build/erogatore_ticket"

#define GENERIC_TICKET 0 // generic ticket for all purposes (slower than taking specific ticket, I guess)
#define PACKET_TICKET 1 // shipping and receiving packets
#define LETTER_TICKET 2 // shipping and receiving letters
#define WIDE_TICKET 3 // withdrawal and deposit
#define PAYMENT_TICKET 4 // postal bills payment
#define BUYFP_TICKET 5 // buy financial product
#define BUYCB_TICKET 6 // buy clocks and bracelets

struct msgbuffer {
    long mtype;
    pid_t mtext;
};

struct msgbuffer_time {
    long mtype;
    double mtext;
};

int wait_times[] = {20, 10, 8, 6, 8, 20, 20};
#endif //SO_2025_COMMONS_H
