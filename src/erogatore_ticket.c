#include "headers/erogatore_ticket.h"

static parameter config;
static counter *counters;
static simulation_stats *stats;
static service_queue *queues;
static int semid = -1;
static int msgid = -1;

static volatile sig_atomic_t terminate_requested = 0;

static void handle_ticket_signal(int signal) {
    if (signal == SIGTERM) {
        terminate_requested = 1;
    }
}

static int sem_op_retry(int sem_num, short sem_op) {
    struct sembuf operation = {.sem_num = (unsigned short) sem_num, .sem_op = sem_op, .sem_flg = 0};

    while (semop(semid, &operation, 1) == -1) {
        if (errno == EINTR) {
            if (terminate_requested) {
                return 1;
            }

            continue;
        }

        return -1;
    }

    return 0;
}

static void lock_stats(void) {
    if (sem_op_retry(stats_mutex_sem(config->NOF_WORKER_SEATS), -1) != 0) {
        perror("lock stats");
        exit(EXIT_FAILURE);
    }
}

static void unlock_stats(void) {
    if (sem_op_retry(stats_mutex_sem(config->NOF_WORKER_SEATS), 1) != 0) {
        perror("unlock stats");
        exit(EXIT_FAILURE);
    }
}

static void send_director_message(int kind) {
    director_message message;

    message.mtype = DIRECTOR_MTYPE;
    message.kind = kind;
    message.index = -1;
    message.pid = getpid();

    while (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("msgsnd director");
        exit(EXIT_FAILURE);
    }
}

static void send_user_reply(pid_t user_pid, int status, int service) {
    user_reply_message reply;

    reply.mtype = USER_REPLY_BASE + user_pid;
    reply.status = status;
    reply.service = service;

    while (msgsnd(msgid, &reply, sizeof(reply) - sizeof(long), 0) == -1) {
        if (errno == EINTR) {
            if (terminate_requested) {
                return;
            }

            continue;
        }

        perror("msgsnd user");
        exit(EXIT_FAILURE);
    }
}

static int service_is_offered_today(int service) {
    int i;

    for (i = 0; i < config->NOF_WORKER_SEATS; ++i) {
        if (counters[i].service == service) {
            return 1;
        }
    }

    return 0;
}

static void mark_not_served(int service) {
    lock_stats();
    stats->day_services_not_served += 1;
    stats->total_services_not_served += 1;
    stats->day_by_service[service].not_served += 1;
    stats->total_by_service[service].not_served += 1;
    unlock_stats();
}

static void mark_request(int service) {
    lock_stats();
    stats->day_by_service[service].requests += 1;
    stats->total_by_service[service].requests += 1;
    unlock_stats();
}

static void enqueue_user_request(const ticket_message *message) {
    queue_ticket ticket;
    int service = message->service;

    mark_request(service);

    if (!config->DAY_ACTIVE || !service_is_offered_today(service)) {
        mark_not_served(service);
        send_user_reply(message->user_pid, USER_REPLY_NOT_SERVED, service);
        return;
    }

    if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), -1) != 0) {
        perror("lock queue");
        exit(EXIT_FAILURE);
    }

    if (!config->DAY_ACTIVE || service_queue_is_full(&queues[service])) {
        if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), 1) != 0) {
            perror("unlock queue");
            exit(EXIT_FAILURE);
        }

        mark_not_served(service);
        send_user_reply(message->user_pid, USER_REPLY_NOT_SERVED, service);
        return;
    }

    ticket.user_pid = message->user_pid;
    ticket.user_index = message->user_index;
    ticket.requested_at_ns = now_monotonic_ns();

    if (service_queue_push(&queues[service], ticket) != 0) {
        if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), 1) != 0) {
            perror("unlock queue");
            exit(EXIT_FAILURE);
        }

        mark_not_served(service);
        send_user_reply(message->user_pid, USER_REPLY_NOT_SERVED, service);
        return;
    }

    if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), 1) != 0) {
        perror("unlock queue");
        exit(EXIT_FAILURE);
    }

    if (sem_op_retry(service_jobs_sem(config->NOF_WORKER_SEATS, service), 1) != 0) {
        perror("notify jobs");
        exit(EXIT_FAILURE);
    }
}

int main(void) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_ticket_signal;
    sigaction(SIGTERM, &sa, NULL);

    config = shmat(shmget(SHM_CONFIG_KEY, sizeof(struct simulation_parameter), 0666), NULL, 0);
    if (config == (void *) -1) {
        perror("ticket config attach");
        return EXIT_FAILURE;
    }

    counters = shmat(shmget(SHM_COUNTERS_KEY, sizeof(counter) * config->NOF_WORKER_SEATS, 0666), NULL, 0);
    stats = shmat(shmget(SHM_STATS_KEY, sizeof(simulation_stats), 0666), NULL, 0);
    queues = shmat(shmget(SHM_QUEUES_KEY, sizeof(service_queue) * SERVICE_COUNT, 0666), NULL, 0);
    semid = semget(SEM_KEY, total_sems(config->NOF_WORKER_SEATS), 0666);
    msgid = msgget(MSG_KEY, 0666);

    if (counters == (void *) -1 || stats == (void *) -1 ||
        queues == (void *) -1 || semid == -1 || msgid == -1) {
        perror("ticket attach");
        return EXIT_FAILURE;
    }

    send_director_message(DIRECTOR_MSG_INIT);

    while (!terminate_requested) {
        ticket_message message;

        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), TICKET_REQUEST_MTYPE, 0) == -1) {
            if (errno == EINTR && terminate_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            perror("msgrcv ticket");
            exit(EXIT_FAILURE);
        }

        if (message.service < 0 || message.service >= SERVICE_COUNT) {
            send_user_reply(message.user_pid, USER_REPLY_NOT_SERVED, 0);
            continue;
        }

        enqueue_user_request(&message);
    }

    shmdt(config);
    shmdt(counters);
    shmdt(stats);
    shmdt(queues);
    return EXIT_SUCCESS;
}
