#include "headers/operator.h"

static parameter config;
static counter *counters;
static worker_info *workers;
static simulation_stats *stats;
static service_queue *queues;
static int semid = -1;
static int msgid = -1;

static int worker_index = -1;
static volatile sig_atomic_t day_started = 0;
static volatile sig_atomic_t day_stop_requested = 0;
static volatile sig_atomic_t terminate_requested = 0;

static void handle_operator_signal(int signal) {
    if (signal == SIGUSR1) {
        day_started = 1;
    } else if (signal == SIGUSR2) {
        day_stop_requested = 1;
    } else if (signal == SIGTERM) {
        terminate_requested = 1;
        day_stop_requested = 1;
        day_started = 1;
    }
}

static int sem_op_interruptible(int sem_num, short sem_op) {
    struct sembuf operation = {.sem_num = (unsigned short) sem_num, .sem_op = sem_op, .sem_flg = 0};

    while (semop(semid, &operation, 1) == -1) {
        if (errno == EINTR) {
            if (terminate_requested || day_stop_requested) {
                return 1;
            }

            continue;
        }

        return -1;
    }

    return 0;
}

static void lock_stats(void) {
    if (sem_op_interruptible(stats_mutex_sem(config->NOF_WORKER_SEATS), -1) != 0) {
        perror("lock stats");
        exit(EXIT_FAILURE);
    }
}

static void unlock_stats(void) {
    if (sem_op_interruptible(stats_mutex_sem(config->NOF_WORKER_SEATS), 1) != 0) {
        perror("unlock stats");
        exit(EXIT_FAILURE);
    }
}

static void send_director_message(int kind) {
    director_message message;

    message.mtype = DIRECTOR_MTYPE;
    message.kind = kind;
    message.index = worker_index;
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
            continue;
        }

        perror("msgsnd user");
        exit(EXIT_FAILURE);
    }
}

static int claim_counter_for_service(int service) {
    int i;

    while (!terminate_requested && config->DAY_ACTIVE && !day_stop_requested) {
        int wait_rc = sem_op_interruptible(service_free_counter_sem(config->NOF_WORKER_SEATS, service), -1);
        if (wait_rc != 0) {
            return -1;
        }

        if (!config->DAY_ACTIVE || day_stop_requested || terminate_requested) {
            sem_op_interruptible(service_free_counter_sem(config->NOF_WORKER_SEATS, service), 1);
            return -1;
        }

        for (i = 0; i < config->NOF_WORKER_SEATS; ++i) {
            if (sem_op_interruptible(counter_mutex_sem(i), -1) != 0) {
                return -1;
            }

            if (counters[i].service == service && counters[i].worker_pid == -1) {
                counters[i].worker_pid = getpid();
                workers[worker_index].current_counter = i;
                workers[worker_index].active_today = 1;
                workers[worker_index].active_ever = 1;

                if (sem_op_interruptible(counter_mutex_sem(i), 1) != 0) {
                    return -1;
                }

                return i;
            }

            if (sem_op_interruptible(counter_mutex_sem(i), 1) != 0) {
                return -1;
            }
        }

        sem_op_interruptible(service_free_counter_sem(config->NOF_WORKER_SEATS, service), 1);
    }

    return -1;
}

static void release_counter(int counter_id, int service) {
    if (counter_id < 0) {
        return;
    }

    if (sem_op_interruptible(counter_mutex_sem(counter_id), -1) != 0) {
        return;
    }

    counters[counter_id].worker_pid = -1;
    workers[worker_index].current_counter = -1;

    if (sem_op_interruptible(counter_mutex_sem(counter_id), 1) != 0) {
        return;
    }

    if (sem_op_interruptible(service_free_counter_sem(config->NOF_WORKER_SEATS, service), 1) != 0) {
        return;
    }
}

static int wait_for_ticket(int service) {
    return sem_op_interruptible(service_jobs_sem(config->NOF_WORKER_SEATS, service), -1);
}

static int pop_ticket_for_service(int service, queue_ticket *ticket) {
    if (sem_op_interruptible(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), -1) != 0) {
        return 1;
    }

    *ticket = service_queue_pop(&queues[service]);

    if (sem_op_interruptible(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), 1) != 0) {
        return 1;
    }

    return ticket->user_pid > 0 ? 0 : 1;
}

static int random_service_duration_minutes(int service) {
    int mean = service_mean_minutes[service];
    int min = mean / 2;
    int max = mean + (mean / 2);

    if (min <= 0) {
        min = 1;
    }

    return min + (rand() % (max - min + 1));
}

static void sleep_service_duration(long long duration_ns) {
    struct timespec request;
    struct timespec remaining;

    request.tv_sec = duration_ns / 1000000000LL;
    request.tv_nsec = duration_ns % 1000000000LL;

    while (nanosleep(&request, &remaining) == -1) {
        if (errno != EINTR) {
            perror("nanosleep service");
            exit(EXIT_FAILURE);
        }

        request = remaining;
    }
}

static void update_pause_statistics(void) {
    workers[worker_index].pauses_taken += 1;
    workers[worker_index].pauses_today += 1;

    lock_stats();
    stats->day_pause_total += 1;
    stats->total_pause_total += 1;
    unlock_stats();
}

static int should_take_pause(void) {
    if (!config->DAY_ACTIVE || workers[worker_index].pauses_taken >= config->NOF_PAUSE) {
        return 0;
    }

    return (rand() % 100) < 10;
}

static void serve_ticket(int service, const queue_ticket *ticket) {
    long long service_start_ns = now_monotonic_ns();
    long long waiting_ns = service_start_ns - ticket->requested_at_ns;
    int service_minutes = random_service_duration_minutes(service);
    long long service_duration_ns = minutes_to_ns(service_minutes, config->N_NANO_SECONDS);

    sleep_service_duration(service_duration_ns);

    lock_stats();
    stats->day_users_served += 1;
    stats->total_users_served += 1;
    stats->day_services_served += 1;
    stats->total_services_served += 1;
    stats->day_wait_minutes_sum += ns_to_minutes(waiting_ns, config->N_NANO_SECONDS);
    stats->total_wait_minutes_sum += ns_to_minutes(waiting_ns, config->N_NANO_SECONDS);
    stats->day_service_minutes_sum += (double) service_minutes;
    stats->total_service_minutes_sum += (double) service_minutes;
    stats->day_by_service[service].served += 1;
    stats->total_by_service[service].served += 1;
    stats->day_by_service[service].wait_minutes_sum += ns_to_minutes(waiting_ns, config->N_NANO_SECONDS);
    stats->total_by_service[service].wait_minutes_sum += ns_to_minutes(waiting_ns, config->N_NANO_SECONDS);
    stats->day_by_service[service].service_minutes_sum += (double) service_minutes;
    stats->total_by_service[service].service_minutes_sum += (double) service_minutes;
    unlock_stats();

    send_user_reply(ticket->user_pid, USER_REPLY_SERVED, service);
}

static void run_day(void) {
    int service = workers[worker_index].service;
    int counter_id = claim_counter_for_service(service);

    if (counter_id >= 0) {
        while (!terminate_requested) {
            queue_ticket ticket;
            int wait_rc;

            if (day_stop_requested && !config->DAY_ACTIVE) {
                break;
            }

            wait_rc = wait_for_ticket(service);
            if (wait_rc != 0) {
                break;
            }

            if (pop_ticket_for_service(service, &ticket) != 0) {
                continue;
            }

            serve_ticket(service, &ticket);

            if (terminate_requested || day_stop_requested || !config->DAY_ACTIVE) {
                break;
            }

            if (should_take_pause()) {
                update_pause_statistics();
                day_stop_requested = 1;
                break;
            }
        }

        release_counter(counter_id, service);
    }

    send_director_message(DIRECTOR_MSG_DAY_FINISHED);
}

static void wait_for_next_day(void) {
    sigset_t empty_mask;

    sigemptyset(&empty_mask);
    while (!day_started && !terminate_requested) {
        sigsuspend(&empty_mask);
    }
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    char *endptr;
    long director_pid;

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <worker_index> <director_pid>\n", argv[0]);
        return EXIT_FAILURE;
    }

    worker_index = (int) strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "worker_index non valido\n");
        return EXIT_FAILURE;
    }

    director_pid = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "director_pid non valido\n");
        return EXIT_FAILURE;
    }
    (void) director_pid;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_operator_signal;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    config = shmat(shmget(SHM_CONFIG_KEY, sizeof(struct simulation_parameter), 0666), NULL, 0);
    if (config == (void *) -1) {
        perror("operator config attach");
        return EXIT_FAILURE;
    }

    counters = shmat(shmget(SHM_COUNTERS_KEY, sizeof(counter) * config->NOF_WORKER_SEATS, 0666), NULL, 0);
    workers = shmat(shmget(SHM_WORKERS_KEY, sizeof(worker_info) * config->NOF_WORKERS, 0666), NULL, 0);
    stats = shmat(shmget(SHM_STATS_KEY, sizeof(simulation_stats), 0666), NULL, 0);
    queues = shmat(shmget(SHM_QUEUES_KEY, sizeof(service_queue) * SERVICE_COUNT, 0666), NULL, 0);
    semid = semget(SEM_KEY, total_sems(config->NOF_WORKER_SEATS), 0666);
    msgid = msgget(MSG_KEY, 0666);

    if (counters == (void *) -1 || workers == (void *) -1 ||
        stats == (void *) -1 || queues == (void *) -1 || semid == -1 || msgid == -1) {
        perror("operator attach");
        return EXIT_FAILURE;
    }

    srand((unsigned int) (time(NULL) ^ getpid()));
    send_director_message(DIRECTOR_MSG_INIT);

    while (!terminate_requested) {
        wait_for_next_day();
        if (terminate_requested) {
            break;
        }

        day_started = 0;
        day_stop_requested = 0;
        run_day();
    }

    shmdt(config);
    shmdt(counters);
    shmdt(workers);
    shmdt(stats);
    shmdt(queues);
    return EXIT_SUCCESS;
}
