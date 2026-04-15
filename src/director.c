#include "headers/director.h"

#define DIRECTOR_PATH "./build/director"
#define OPERATOR_PATH "./build/operator"
#define USER_PATH "./build/user"
#define TICKET_PATH "./build/erogatore_ticket"

static parameter config;
static counter *counters;
static worker_info *workers;
static user_info *users;
static simulation_stats *stats;
static service_queue *queues;

static int shmid_config = -1;
static int shmid_counters = -1;
static int shmid_workers = -1;
static int shmid_users = -1;
static int shmid_stats = -1;
static int shmid_queues = -1;
static int semid = -1;
static int msgid = -1;

static pid_t ticket_pid = -1;
static volatile sig_atomic_t shutdown_requested = 0;

static void handle_director_signal(int signal) {
    (void) signal;
    shutdown_requested = 1;
}

static void cleanup_stale_ipc_resources(void) {
    int id;

    id = msgget(MSG_KEY, 0666);
    if (id != -1) {
        msgctl(id, IPC_RMID, NULL);
    }

    id = semget(SEM_KEY, 1, 0666);
    if (id != -1) {
        semctl(id, 0, IPC_RMID, 0);
    }

    id = shmget(SHM_CONFIG_KEY, 1, 0666);
    if (id != -1) {
        shmctl(id, IPC_RMID, NULL);
    }

    id = shmget(SHM_COUNTERS_KEY, 1, 0666);
    if (id != -1) {
        shmctl(id, IPC_RMID, NULL);
    }

    id = shmget(SHM_WORKERS_KEY, 1, 0666);
    if (id != -1) {
        shmctl(id, IPC_RMID, NULL);
    }

    id = shmget(SHM_USERS_KEY, 1, 0666);
    if (id != -1) {
        shmctl(id, IPC_RMID, NULL);
    }

    id = shmget(SHM_STATS_KEY, 1, 0666);
    if (id != -1) {
        shmctl(id, IPC_RMID, NULL);
    }

    id = shmget(SHM_QUEUES_KEY, 1, 0666);
    if (id != -1) {
        shmctl(id, IPC_RMID, NULL);
    }
}

static void set_sem_value(int sem_num, int value) {
    if (semctl(semid, sem_num, SETVAL, value) == -1) {
        perror("semctl SETVAL");
        exit(EXIT_FAILURE);
    }
}

static int sem_op_retry(int sem_num, short sem_op) {
    struct sembuf operation = {.sem_num = (unsigned short) sem_num, .sem_op = sem_op, .sem_flg = 0};

    while (semop(semid, &operation, 1) == -1) {
        if (errno == EINTR) {
            if (shutdown_requested) {
                return -1;
            }

            continue;
        }

        return -1;
    }

    return 0;
}

static void lock_stats(void) {
    if (sem_op_retry(stats_mutex_sem(config->NOF_WORKER_SEATS), -1) == -1) {
        perror("lock stats");
        exit(EXIT_FAILURE);
    }
}

static void unlock_stats(void) {
    if (sem_op_retry(stats_mutex_sem(config->NOF_WORKER_SEATS), 1) == -1) {
        perror("unlock stats");
        exit(EXIT_FAILURE);
    }
}

static void reset_day_statistics(void) {
    lock_stats();
    memset(&stats->day_by_service, 0, sizeof(stats->day_by_service));
    stats->day_users_served = 0;
    stats->day_services_served = 0;
    stats->day_services_not_served = 0;
    stats->day_wait_minutes_sum = 0.0;
    stats->day_service_minutes_sum = 0.0;
    stats->day_pause_total = 0;
    unlock_stats();
}

static void reset_workers_day_state(void) {
    int i;

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        workers[i].active_today = 0;
        workers[i].pauses_today = 0;
        workers[i].current_counter = -1;
    }
}

static int read_config_file(const char *path) {
    FILE *fp;
    char key[128];
    int value;

    fp = fopen(path, "r");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    while (fscanf(fp, "%127s %d", key, &value) == 2) {
        if (strcmp(key, "NOF_WORKER_SEATS") == 0) {
            config->NOF_WORKER_SEATS = value;
        } else if (strcmp(key, "NOF_WORKERS") == 0) {
            config->NOF_WORKERS = value;
        } else if (strcmp(key, "NOF_USERS") == 0) {
            config->NOF_USERS = value;
        } else if (strcmp(key, "NOF_PAUSE") == 0) {
            config->NOF_PAUSE = value;
        } else if (strcmp(key, "P_SERV_MIN") == 0) {
            config->P_SERV_MIN = value;
        } else if (strcmp(key, "P_SERV_MAX") == 0) {
            config->P_SERV_MAX = value;
        } else if (strcmp(key, "SIM_DURATION") == 0) {
            config->SIM_DURATION = value;
        } else if (strcmp(key, "N_NANO_SECONDS") == 0) {
            config->N_NANO_SECONDS = value;
        } else if (strcmp(key, "EXPLODE_THRESHOLD") == 0) {
            config->EXPLODE_THRESHOLD = value;
        } else {
            fprintf(stderr, "Parametro sconosciuto nel file di config: %s\n", key);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);

    if (config->NOF_WORKER_SEATS <= 0 || config->NOF_WORKERS <= 0 ||
        config->NOF_USERS <= 0 || config->SIM_DURATION <= 0 ||
        config->N_NANO_SECONDS <= 0) {
        fprintf(stderr, "Configurazione non valida\n");
        return -1;
    }

    if (config->P_SERV_MIN > config->P_SERV_MAX) {
        fprintf(stderr, "Intervallo P_SERV non valido\n");
        return -1;
    }

    config->P_SERV_MIN = clamp_percentage(config->P_SERV_MIN);
    config->P_SERV_MAX = clamp_percentage(config->P_SERV_MAX);
    config->CURRENT_DAY = 0;
    config->DAY_ACTIVE = 0;
    config->TERMINATION_CAUSE = TERMINATION_NONE;
    return 0;
}

static void allocate_ipc(void) {
    shmid_counters = shmget(SHM_COUNTERS_KEY, sizeof(counter) * config->NOF_WORKER_SEATS, IPC_CREAT | 0666);
    shmid_workers = shmget(SHM_WORKERS_KEY, sizeof(worker_info) * config->NOF_WORKERS, IPC_CREAT | 0666);
    shmid_users = shmget(SHM_USERS_KEY, sizeof(user_info) * config->NOF_USERS, IPC_CREAT | 0666);
    shmid_stats = shmget(SHM_STATS_KEY, sizeof(simulation_stats), IPC_CREAT | 0666);
    shmid_queues = shmget(SHM_QUEUES_KEY, sizeof(service_queue) * SERVICE_COUNT, IPC_CREAT | 0666);
    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    semid = semget(SEM_KEY, total_sems(config->NOF_WORKER_SEATS), IPC_CREAT | 0666);

    if (shmid_counters == -1 || shmid_workers == -1 || shmid_users == -1 ||
        shmid_stats == -1 || shmid_queues == -1 || msgid == -1 || semid == -1) {
        perror("IPC allocation");
        exit(EXIT_FAILURE);
    }

    counters = shmat(shmid_counters, NULL, 0);
    workers = shmat(shmid_workers, NULL, 0);
    users = shmat(shmid_users, NULL, 0);
    stats = shmat(shmid_stats, NULL, 0);
    queues = shmat(shmid_queues, NULL, 0);

    if (counters == (void *) -1 || workers == (void *) -1 || users == (void *) -1 ||
        stats == (void *) -1 || queues == (void *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
}

static void initialize_shared_state(void) {
    int i;

    memset(counters, 0, sizeof(counter) * config->NOF_WORKER_SEATS);
    memset(workers, 0, sizeof(worker_info) * config->NOF_WORKERS);
    memset(users, 0, sizeof(user_info) * config->NOF_USERS);
    memset(stats, 0, sizeof(simulation_stats));
    memset(queues, 0, sizeof(service_queue) * SERVICE_COUNT);

    for (i = 0; i < config->NOF_WORKER_SEATS; ++i) {
        counters[i].counter_id = i;
        counters[i].service = -1;
        counters[i].worker_pid = -1;
    }

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        workers[i].service = rand() % SERVICE_COUNT;
        workers[i].current_counter = -1;
    }

    for (i = 0; i < config->NOF_USERS; ++i) {
        int interval = config->P_SERV_MAX - config->P_SERV_MIN + 1;
        users[i].go_probability = config->P_SERV_MIN + (rand() % interval);
    }

    for (i = 0; i < config->NOF_WORKER_SEATS; ++i) {
        set_sem_value(counter_mutex_sem(i), 1);
    }

    for (i = 0; i < SERVICE_COUNT; ++i) {
        set_sem_value(service_queue_mutex_sem(config->NOF_WORKER_SEATS, i), 1);
        set_sem_value(service_jobs_sem(config->NOF_WORKER_SEATS, i), 0);
        set_sem_value(service_free_counter_sem(config->NOF_WORKER_SEATS, i), 0);
        service_queue_reset(&queues[i]);
    }

    set_sem_value(stats_mutex_sem(config->NOF_WORKER_SEATS), 1);
}

static void send_signal_to_users(int signal) {
    int i;

    for (i = 0; i < config->NOF_USERS; ++i) {
        if (users[i].pid > 0) {
            kill(users[i].pid, signal);
        }
    }
}

static void send_signal_to_workers(int signal) {
    int i;

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, signal);
        }
    }
}

static pid_t spawn_process(const char *path, const char *arg1, const char *arg2) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        if (arg1 != NULL && arg2 != NULL) {
            execl(path, path, arg1, arg2, (char *) NULL);
        } else if (arg1 != NULL) {
            execl(path, path, arg1, (char *) NULL);
        } else {
            execl(path, path, (char *) NULL);
        }

        perror("execl");
        _exit(EXIT_FAILURE);
    }

    return pid;
}

static void create_children(void) {
    int i;
    char index_arg[32];
    char director_pid_arg[32];

    snprintf(director_pid_arg, sizeof(director_pid_arg), "%ld", (long) getpid());

    ticket_pid = spawn_process(TICKET_PATH, NULL, NULL);

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        snprintf(index_arg, sizeof(index_arg), "%d", i);
        workers[i].pid = spawn_process(OPERATOR_PATH, index_arg, director_pid_arg);
    }

    for (i = 0; i < config->NOF_USERS; ++i) {
        snprintf(index_arg, sizeof(index_arg), "%d", i);
        users[i].pid = spawn_process(USER_PATH, index_arg, director_pid_arg);
    }
}

static void wait_for_initialization(void) {
    int expected = config->NOF_WORKERS + config->NOF_USERS + 1;
    int received = 0;

    while (received < expected) {
        director_message message;

        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), DIRECTOR_MTYPE, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("msgrcv init");
            exit(EXIT_FAILURE);
        }

        if (message.kind == DIRECTOR_MSG_INIT) {
            received += 1;
        }
    }
}

static void assign_daily_counters(int counters_per_service[SERVICE_COUNT]) {
    int i;

    memset(counters_per_service, 0, sizeof(int) * SERVICE_COUNT);

    for (i = 0; i < config->NOF_WORKER_SEATS; ++i) {
        int service = rand() % SERVICE_COUNT;

        if (sem_op_retry(counter_mutex_sem(i), -1) == -1) {
            perror("lock counter");
            exit(EXIT_FAILURE);
        }

        counters[i].service = service;
        counters[i].worker_pid = -1;

        if (sem_op_retry(counter_mutex_sem(i), 1) == -1) {
            perror("unlock counter");
            exit(EXIT_FAILURE);
        }

        counters_per_service[service] += 1;
    }

    for (i = 0; i < SERVICE_COUNT; ++i) {
        set_sem_value(service_jobs_sem(config->NOF_WORKER_SEATS, i), 0);
        set_sem_value(service_free_counter_sem(config->NOF_WORKER_SEATS, i), counters_per_service[i]);
        service_queue_reset(&queues[i]);
    }
}

static int count_workers_for_service(int service) {
    int i;
    int count = 0;

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        if (workers[i].service == service) {
            count += 1;
        }
    }

    return count;
}

static void print_service_statistics_line(const char *label,
                                          const service_stats *daily,
                                          const service_stats *total,
                                          int day_number) {
    double daily_wait = daily->served > 0 ? daily->wait_minutes_sum / daily->served : 0.0;
    double total_wait = total->served > 0 ? total->wait_minutes_sum / total->served : 0.0;
    double daily_service = daily->served > 0 ? daily->service_minutes_sum / daily->served : 0.0;
    double total_service = total->served > 0 ? total->service_minutes_sum / total->served : 0.0;

    printf("  %s\n", label);
    printf("    giornata: serviti=%ld non_serviti=%ld attesa_media=%.2f min erogazione_media=%.2f min\n",
           daily->served, daily->not_served, daily_wait, daily_service);
    printf("    totale:   serviti=%ld non_serviti=%ld media_serviti_giorno=%.2f media_non_serviti_giorno=%.2f attesa_media=%.2f min erogazione_media=%.2f min\n",
           total->served,
           total->not_served,
           (double) total->served / day_number,
           (double) total->not_served / day_number,
           total_wait,
           total_service);
}

static void print_daily_statistics(int day_number, const int counters_per_service[SERVICE_COUNT]) {
    int i;
    int active_today = 0;
    int active_total = 0;
    long day_users_served;
    long total_users_served;
    long day_services_served;
    long total_services_served;
    long day_not_served;
    long total_not_served;
    double day_wait_avg;
    double total_wait_avg;
    double day_service_avg;
    double total_service_avg;
    long day_pause_total;
    long total_pause_total;

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        if (workers[i].active_today) {
            active_today += 1;
        }
        if (workers[i].active_ever) {
            active_total += 1;
        }
    }

    lock_stats();
    day_users_served = stats->day_users_served;
    total_users_served = stats->total_users_served;
    day_services_served = stats->day_services_served;
    total_services_served = stats->total_services_served;
    day_not_served = stats->day_services_not_served;
    total_not_served = stats->total_services_not_served;
    day_wait_avg = stats->day_services_served > 0 ? stats->day_wait_minutes_sum / stats->day_services_served : 0.0;
    total_wait_avg = stats->total_services_served > 0 ? stats->total_wait_minutes_sum / stats->total_services_served : 0.0;
    day_service_avg = stats->day_services_served > 0 ? stats->day_service_minutes_sum / stats->day_services_served : 0.0;
    total_service_avg = stats->total_services_served > 0 ? stats->total_service_minutes_sum / stats->total_services_served : 0.0;
    day_pause_total = stats->day_pause_total;
    total_pause_total = stats->total_pause_total;

    printf("\n=== GIORNO %d ===\n", day_number);
    printf("Utenti serviti: giornata=%ld totale=%ld media_giornaliera=%.2f\n",
           day_users_served,
           total_users_served,
           (double) total_users_served / day_number);
    printf("Servizi erogati: giornata=%ld totale=%ld media_giornaliera=%.2f\n",
           day_services_served,
           total_services_served,
           (double) total_services_served / day_number);
    printf("Servizi non erogati: giornata=%ld totale=%ld media_giornaliera=%.2f\n",
           day_not_served,
           total_not_served,
           (double) total_not_served / day_number);
    printf("Tempo medio di attesa: giornata=%.2f min simulazione=%.2f min\n", day_wait_avg, total_wait_avg);
    printf("Tempo medio di erogazione: giornata=%.2f min simulazione=%.2f min\n", day_service_avg, total_service_avg);
    printf("Operatori attivi: giornata=%d simulazione=%d\n", active_today, active_total);
    printf("Pause: media_giornata=%.2f totale_simulazione=%ld\n",
           active_today > 0 ? (double) day_pause_total / active_today : 0.0,
           total_pause_total);
    printf("Statistiche per servizio:\n");

    for (i = 0; i < SERVICE_COUNT; ++i) {
        print_service_statistics_line(service_names[i], &stats->day_by_service[i], &stats->total_by_service[i], day_number);
    }

    printf("Rapporto operatori disponibili / sportelli esistenti:\n");
    for (i = 0; i < config->NOF_WORKER_SEATS; ++i) {
        int service = counters[i].service;
        int available_workers = count_workers_for_service(service);
        int seats = counters_per_service[service];
        double ratio = seats > 0 ? (double) available_workers / seats : 0.0;

        printf("  sportello %d (%s): %d/%d = %.2f\n",
               i,
               service_names[service],
               available_workers,
               seats,
               ratio);
    }
    unlock_stats();
}

static void print_final_statistics(void) {
    const char *cause = "UNKNOWN";
    double total_wait_avg;
    double total_service_avg;

    if (config->TERMINATION_CAUSE == TERMINATION_TIMEOUT) {
        cause = "timeout";
    } else if (config->TERMINATION_CAUSE == TERMINATION_EXPLODE) {
        cause = "explode";
    }

    lock_stats();
    total_wait_avg = stats->total_services_served > 0 ? stats->total_wait_minutes_sum / stats->total_services_served : 0.0;
    total_service_avg = stats->total_services_served > 0 ? stats->total_service_minutes_sum / stats->total_services_served : 0.0;
    printf("\n=== FINE SIMULAZIONE ===\n");
    printf("Causa terminazione: %s\n", cause);
    printf("Giorni simulati: %d\n", config->CURRENT_DAY);
    printf("Utenti serviti totali: %ld\n", stats->total_users_served);
    printf("Servizi erogati totali: %ld\n", stats->total_services_served);
    printf("Servizi non erogati totali: %ld\n", stats->total_services_not_served);
    printf("Tempo medio di attesa complessivo: %.2f min\n", total_wait_avg);
    printf("Tempo medio di erogazione complessivo: %.2f min\n", total_service_avg);
    unlock_stats();
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

        perror("msgsnd user reply");
        exit(EXIT_FAILURE);
    }
}

static int flush_pending_users(void) {
    int service;
    int pending_total = 0;

    for (service = 0; service < SERVICE_COUNT; ++service) {
        if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), -1) == -1) {
            perror("lock service queue");
            exit(EXIT_FAILURE);
        }

        while (service_queue_length(&queues[service]) > 0) {
            queue_ticket ticket = service_queue_pop(&queues[service]);

            pending_total += 1;
            send_user_reply(ticket.user_pid, USER_REPLY_NOT_SERVED, service);

            lock_stats();
            stats->day_services_not_served += 1;
            stats->total_services_not_served += 1;
            stats->day_by_service[service].not_served += 1;
            stats->total_by_service[service].not_served += 1;
            unlock_stats();
        }

        if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), 1) == -1) {
            perror("unlock service queue");
            exit(EXIT_FAILURE);
        }

        set_sem_value(service_jobs_sem(config->NOF_WORKER_SEATS, service), 0);
    }

    return pending_total;
}

static void wait_for_workers_day_end(void) {
    int received = 0;

    while (received < config->NOF_WORKERS) {
        director_message message;

        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), DIRECTOR_MTYPE, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("msgrcv day finished");
            exit(EXIT_FAILURE);
        }

        if (message.kind == DIRECTOR_MSG_DAY_FINISHED) {
            received += 1;
        }
    }
}

static int count_pending_users(void) {
    int service;
    int total = 0;

    for (service = 0; service < SERVICE_COUNT; ++service) {
        if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), -1) == -1) {
            perror("lock queue count");
            exit(EXIT_FAILURE);
        }

        total += service_queue_length(&queues[service]);

        if (sem_op_retry(service_queue_mutex_sem(config->NOF_WORKER_SEATS, service), 1) == -1) {
            perror("unlock queue count");
            exit(EXIT_FAILURE);
        }
    }

    return total;
}

static void sleep_for_day_duration(void) {
    struct timespec request;
    struct timespec remaining;
    long long duration_ns = minutes_to_ns(WORKDAY_MINUTES, config->N_NANO_SECONDS);

    request.tv_sec = duration_ns / 1000000000LL;
    request.tv_nsec = duration_ns % 1000000000LL;

    while (nanosleep(&request, &remaining) == -1) {
        if (errno != EINTR) {
            perror("nanosleep");
            exit(EXIT_FAILURE);
        }

        if (shutdown_requested) {
            break;
        }

        request = remaining;
    }
}

static void stop_children(void) {
    int i;
    int status;

    if (ticket_pid > 0) {
        kill(ticket_pid, SIGTERM);
    }

    send_signal_to_workers(SIGTERM);
    send_signal_to_users(SIGTERM);

    for (i = 0; i < config->NOF_WORKERS; ++i) {
        if (workers[i].pid > 0) {
            waitpid(workers[i].pid, &status, 0);
        }
    }

    for (i = 0; i < config->NOF_USERS; ++i) {
        if (users[i].pid > 0) {
            waitpid(users[i].pid, &status, 0);
        }
    }

    if (ticket_pid > 0) {
        waitpid(ticket_pid, &status, 0);
    }
}

static void cleanup_ipc(void) {
    if (config != NULL && config != (void *) -1) {
        shmdt(config);
    }
    if (counters != NULL && counters != (void *) -1) {
        shmdt(counters);
    }
    if (workers != NULL && workers != (void *) -1) {
        shmdt(workers);
    }
    if (users != NULL && users != (void *) -1) {
        shmdt(users);
    }
    if (stats != NULL && stats != (void *) -1) {
        shmdt(stats);
    }
    if (queues != NULL && queues != (void *) -1) {
        shmdt(queues);
    }

    if (msgid != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
    if (semid != -1) {
        semctl(semid, 0, IPC_RMID, 0);
    }
    if (shmid_config != -1) {
        shmctl(shmid_config, IPC_RMID, NULL);
    }
    if (shmid_counters != -1) {
        shmctl(shmid_counters, IPC_RMID, NULL);
    }
    if (shmid_workers != -1) {
        shmctl(shmid_workers, IPC_RMID, NULL);
    }
    if (shmid_users != -1) {
        shmctl(shmid_users, IPC_RMID, NULL);
    }
    if (shmid_stats != -1) {
        shmctl(shmid_stats, IPC_RMID, NULL);
    }
    if (shmid_queues != -1) {
        shmctl(shmid_queues, IPC_RMID, NULL);
    }
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    int day;
    int counters_per_service[SERVICE_COUNT];

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <config_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    srand((unsigned int) (time(NULL) ^ getpid()));
    log_init("log/so2025.log");

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_director_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    cleanup_stale_ipc_resources();

    shmid_config = shmget(SHM_CONFIG_KEY, sizeof(struct simulation_parameter), IPC_CREAT | 0666);
    if (shmid_config == -1) {
        perror("shmget config");
        return EXIT_FAILURE;
    }

    config = shmat(shmid_config, NULL, 0);
    if (config == (void *) -1) {
        perror("shmat config");
        return EXIT_FAILURE;
    }

    if (read_config_file(argv[1]) == -1) {
        cleanup_ipc();
        return EXIT_FAILURE;
    }

    allocate_ipc();
    initialize_shared_state();
    create_children();
    wait_for_initialization();

    printf("Simulazione avviata: %d giorni, %d utenti, %d operatori, %d sportelli\n",
           config->SIM_DURATION,
           config->NOF_USERS,
           config->NOF_WORKERS,
           config->NOF_WORKER_SEATS);
    printf("Assunzione implementativa: una giornata lavorativa dura %d minuti simulati.\n", WORKDAY_MINUTES);

    for (day = 1; day <= config->SIM_DURATION && !shutdown_requested; ++day) {
        int pending_before_flush;

        config->CURRENT_DAY = day;
        reset_day_statistics();
        reset_workers_day_state();
        assign_daily_counters(counters_per_service);
        config->DAY_ACTIVE = 1;

        send_signal_to_workers(SIGUSR1);
        send_signal_to_users(SIGUSR1);

        sleep_for_day_duration();

        config->DAY_ACTIVE = 0;
        send_signal_to_workers(SIGUSR2);
        send_signal_to_users(SIGUSR2);
        wait_for_workers_day_end();

        pending_before_flush = count_pending_users();

        if (pending_before_flush > config->EXPLODE_THRESHOLD) {
            config->TERMINATION_CAUSE = TERMINATION_EXPLODE;
        }

        flush_pending_users();
        print_daily_statistics(day, counters_per_service);

        if (config->TERMINATION_CAUSE == TERMINATION_EXPLODE) {
            break;
        }
    }

    if (config->TERMINATION_CAUSE == TERMINATION_NONE) {
        config->TERMINATION_CAUSE = TERMINATION_TIMEOUT;
    }

    print_final_statistics();
    stop_children();
    cleanup_ipc();
    log_stop();
    return EXIT_SUCCESS;
}
