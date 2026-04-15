#include "headers/user.h"

static parameter config;
static user_info *users;
static int msgid = -1;

static int user_index = -1;
static volatile sig_atomic_t day_started = 0;
static volatile sig_atomic_t day_closed = 0;
static volatile sig_atomic_t terminate_requested = 0;

static void handle_user_signal(int signal) {
    if (signal == SIGUSR1) {
        day_started = 1;
        day_closed = 0;
    } else if (signal == SIGUSR2) {
        day_closed = 1;
    } else if (signal == SIGTERM) {
        terminate_requested = 1;
        day_started = 1;
        day_closed = 1;
    }
}

static void send_director_message(int kind) {
    director_message message;

    message.mtype = DIRECTOR_MTYPE;
    message.kind = kind;
    message.index = user_index;
    message.pid = getpid();

    while (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("msgsnd director");
        exit(EXIT_FAILURE);
    }
}

static void wait_for_next_day(void) {
    sigset_t empty_mask;

    sigemptyset(&empty_mask);
    while (!day_started && !terminate_requested) {
        sigsuspend(&empty_mask);
    }
}

static int sleep_until_arrival(int arrival_minute) {
    struct timespec request;
    struct timespec remaining;
    long long duration_ns = minutes_to_ns(arrival_minute, config->N_NANO_SECONDS);

    request.tv_sec = duration_ns / 1000000000LL;
    request.tv_nsec = duration_ns % 1000000000LL;

    while (nanosleep(&request, &remaining) == -1) {
        if (errno != EINTR) {
            perror("nanosleep user");
            exit(EXIT_FAILURE);
        }

        if (terminate_requested || day_closed || !config->DAY_ACTIVE) {
            return 1;
        }

        request = remaining;
    }

    return 0;
}

static void request_service(int service) {
    ticket_message message;
    user_reply_message reply;

    message.mtype = TICKET_REQUEST_MTYPE;
    message.user_index = user_index;
    message.user_pid = getpid();
    message.service = service;

    while (msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0) == -1) {
        if (errno == EINTR) {
            if (terminate_requested) {
                return;
            }

            continue;
        }

        perror("msgsnd ticket");
        exit(EXIT_FAILURE);
    }

    while (msgrcv(msgid, &reply, sizeof(reply) - sizeof(long), USER_REPLY_BASE + getpid(), 0) == -1) {
        if (errno == EINTR) {
            if (terminate_requested) {
                return;
            }

            continue;
        }

        perror("msgrcv user reply");
        exit(EXIT_FAILURE);
    }
}

static void run_day(void) {
    int go_probability = users[user_index].go_probability;

    if ((rand() % 100) >= go_probability) {
        return;
    }

    {
        int requested_service = rand() % SERVICE_COUNT;
        int arrival_minute = rand() % WORKDAY_MINUTES;

        if (sleep_until_arrival(arrival_minute) != 0) {
            return;
        }

        if (terminate_requested || day_closed || !config->DAY_ACTIVE) {
            return;
        }

        request_service(requested_service);
    }
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    char *endptr;
    long director_pid;

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <user_index> <director_pid>\n", argv[0]);
        return EXIT_FAILURE;
    }

    user_index = (int) strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "user_index non valido\n");
        return EXIT_FAILURE;
    }

    director_pid = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "director_pid non valido\n");
        return EXIT_FAILURE;
    }
    (void) director_pid;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_user_signal;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    config = shmat(shmget(SHM_CONFIG_KEY, sizeof(struct simulation_parameter), 0666), NULL, 0);
    if (config == (void *) -1) {
        perror("user config attach");
        return EXIT_FAILURE;
    }

    users = shmat(shmget(SHM_USERS_KEY, sizeof(user_info) * config->NOF_USERS, 0666), NULL, 0);
    msgid = msgget(MSG_KEY, 0666);

    if (users == (void *) -1 || msgid == -1) {
        perror("user attach");
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
        day_closed = 0;
        run_day();
    }

    shmdt(config);
    shmdt(users);
    return EXIT_SUCCESS;
}
