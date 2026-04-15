#ifndef SO_2025_COMMONS_H
#define SO_2025_COMMONS_H

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"

#define SERVICE_COUNT 6
#define WORKDAY_MINUTES 480
#define MAX_COUNT_QUEUE 4096

#define SHM_COUNTERS_KEY 0x1234
#define SHM_CONFIG_KEY 0x0209
#define SHM_WORKERS_KEY 0x020A
#define SHM_USERS_KEY 0x020B
#define SHM_STATS_KEY 0x020C
#define SHM_QUEUES_KEY 0x020D
#define MSG_KEY 0x5678
#define SEM_KEY 0x9100

#define DIRECTOR_MTYPE 1L
#define TICKET_REQUEST_MTYPE 2L
#define USER_REPLY_BASE 100000L

enum service_type {
    SERVICE_PACKAGES = 0,
    SERVICE_LETTERS = 1,
    SERVICE_BANKING = 2,
    SERVICE_PAYMENTS = 3,
    SERVICE_FINANCIAL = 4,
    SERVICE_GIFTS = 5
};

enum director_message_kind {
    DIRECTOR_MSG_INIT = 1,
    DIRECTOR_MSG_DAY_FINISHED = 2
};

enum user_reply_status {
    USER_REPLY_SERVED = 1,
    USER_REPLY_NOT_SERVED = 0
};

enum termination_cause {
    TERMINATION_NONE = 0,
    TERMINATION_TIMEOUT = 1,
    TERMINATION_EXPLODE = 2
};

typedef struct postal_counter {
    int counter_id;
    int service;
    pid_t worker_pid;
} counter;

typedef struct queue_ticket {
    pid_t user_pid;
    int user_index;
    long long requested_at_ns;
} queue_ticket;

typedef struct service_queue {
    queue_ticket entries[MAX_COUNT_QUEUE];
    int head;
    int tail;
    int length;
} service_queue;

typedef struct worker_info {
    pid_t pid;
    int service;
    int pauses_taken;
    int pauses_today;
    int active_today;
    int active_ever;
    int current_counter;
} worker_info;

typedef struct user_info {
    pid_t pid;
    int go_probability;
} user_info;

typedef struct service_stats {
    long requests;
    long served;
    long not_served;
    double wait_minutes_sum;
    double service_minutes_sum;
} service_stats;

typedef struct simulation_stats {
    long day_users_served;
    long day_services_served;
    long day_services_not_served;
    double day_wait_minutes_sum;
    double day_service_minutes_sum;
    service_stats day_by_service[SERVICE_COUNT];

    long total_users_served;
    long total_services_served;
    long total_services_not_served;
    double total_wait_minutes_sum;
    double total_service_minutes_sum;
    service_stats total_by_service[SERVICE_COUNT];

    long day_pause_total;
    long total_pause_total;
} simulation_stats;

struct simulation_parameter {
    int NOF_WORKER_SEATS;
    int NOF_WORKERS;
    int NOF_USERS;
    int NOF_PAUSE;

    int P_SERV_MIN;
    int P_SERV_MAX;

    int SIM_DURATION;
    int N_NANO_SECONDS;
    int EXPLODE_THRESHOLD;

    int CURRENT_DAY;
    int DAY_ACTIVE;
    int TERMINATION_CAUSE;
};

typedef struct simulation_parameter *parameter;

typedef struct director_message {
    long mtype;
    int kind;
    int index;
    pid_t pid;
} director_message;

typedef struct ticket_message {
    long mtype;
    int user_index;
    pid_t user_pid;
    int service;
} ticket_message;

typedef struct user_reply_message {
    long mtype;
    int status;
    int service;
} user_reply_message;

static const char *const service_names[SERVICE_COUNT] = {
    "Invio e ritiro pacchi",
    "Invio e ritiro lettere e raccomandate",
    "Prelievi e versamenti Bancoposta",
    "Pagamento bollettini postali",
    "Acquisto prodotti finanziari",
    "Acquisto orologi e braccialetti"
};

static const int service_mean_minutes[SERVICE_COUNT] = {8, 8, 6, 8, 20, 20};

static inline int counter_mutex_sem(int counter_id) {
    return counter_id;
}

static inline int service_queue_mutex_sem(int total_counters, int service) {
    return total_counters + service;
}

static inline int service_jobs_sem(int total_counters, int service) {
    return total_counters + SERVICE_COUNT + service;
}

static inline int service_free_counter_sem(int total_counters, int service) {
    return total_counters + (SERVICE_COUNT * 2) + service;
}

static inline int stats_mutex_sem(int total_counters) {
    return total_counters + (SERVICE_COUNT * 3);
}

static inline int total_sems(int total_counters) {
    return total_counters + (SERVICE_COUNT * 3) + 1;
}

static inline long long now_monotonic_ns(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((long long) ts.tv_sec * 1000000000LL) + ts.tv_nsec;
}

static inline double ns_to_minutes(long long duration_ns, int minute_ns) {
    if (minute_ns <= 0) {
        return 0.0;
    }

    return (double) duration_ns / (double) minute_ns;
}

static inline long long minutes_to_ns(int minutes, int minute_ns) {
    return (long long) minutes * (long long) minute_ns;
}

static inline int clamp_percentage(int value) {
    if (value < 0) {
        return 0;
    }

    if (value > 100) {
        return 100;
    }

    return value;
}

static inline void service_queue_reset(service_queue *queue) {
    memset(queue, 0, sizeof(*queue));
}

static inline int service_queue_is_full(const service_queue *queue) {
    return queue->length >= MAX_COUNT_QUEUE;
}

static inline int service_queue_push(service_queue *queue, queue_ticket ticket) {
    if (service_queue_is_full(queue)) {
        return -1;
    }

    queue->entries[queue->tail] = ticket;
    queue->tail = (queue->tail + 1) % MAX_COUNT_QUEUE;
    queue->length += 1;
    return 0;
}

static inline int service_queue_length(const service_queue *queue) {
    return queue->length;
}

static inline queue_ticket service_queue_pop(service_queue *queue) {
    queue_ticket ticket = {.user_pid = -1, .user_index = -1, .requested_at_ns = 0};

    if (queue->length <= 0) {
        return ticket;
    }

    ticket = queue->entries[queue->head];
    queue->entries[queue->head].user_pid = -1;
    queue->entries[queue->head].user_index = -1;
    queue->entries[queue->head].requested_at_ns = 0;
    queue->head = (queue->head + 1) % MAX_COUNT_QUEUE;
    queue->length -= 1;
    return ticket;
}

#endif
