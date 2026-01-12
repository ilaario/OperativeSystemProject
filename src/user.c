#include "headers/user.h"

// Global variables
parameter config; // Configuration parameters
counter *counters; // Array of counters
int semid; // Semaphore ID
int msgid; // Message queue ID

pid_t ticket_generator_pid;

int probability = -1; // Probability of a user going to a counter

/**
 * @brief Simulates a user going to a counter.
 * @return 0 on success, -1 on error.
 */
int going_to_counter() {
    srand(time(NULL) ^ getpid());

    int going_to_counter = rand() % probability;  // Always 100 for testing
    if (going_to_counter >= 50){
        probability = probability - 50;
    } else {
        probability = probability + 25;
    }

#ifdef DEBUGLOG
    going_to_counter = 100;
#endif

    if (going_to_counter >= 50) {

#ifdef DEBUGLOG
        printf("GOING TO POST OFFICE");
#endif

        clock_t start = clock();
        int ticket_request = rand() % 6;

        int pid = getpid();

        struct msgbuffer msg;

        msg.mtype = ticket_generator_pid;
        msg.mtext = pid; 

        if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
            perror("Error sending PID");
            return -1;
        }

        msg.mtype = pid;
        msg.mtext = ticket_request;

        if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
            perror("Error sending message");
            return -1;
        }

        if (msgrcv(msgid, &msg, sizeof(msg.mtext), getpid() + 10000, 0) == -1) {
            perror("Error receiving message");
            exit(EXIT_FAILURE);
        }

        // TODO Make standard values for work status
        if(msg.mtext == 1) {
            clock_t end = clock();
            double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

            struct msgbuffer_time time;

            time.mtype = 3000 + getppid() + ticket_request;
            time.mtext = time_spent;

#ifdef DEBUGLOG
            printf("SENDING MSG TO %d", getppid());
#endif

            if (msgsnd(msgid, &time, sizeof(time.mtext), 0) == -1) {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            } 
#ifdef DEBUGLOG
            printf("WORK DONE");
#endif

        }
    }
    return 0;
}

/**
 * @brief Signal handler for various signals.
 * @param signal The signal number.
 */
void handle_signal(int signal){
    if(signal == SIGUSR2){
    } else if (signal == SIGTERM){
        exit(EXIT_SUCCESS);
    } else if (signal == SIGABRT){
        printf("Received SIGABRT, exiting");
        sleep(1);
        fflush(stdout);
        kill(getppid(), SIGTERM);
    } else if (signal == SIGALRM){
        // no action needed
    } else if (signal == SIGINT){
        printf("Received SIGINT, pausing");
        fflush(stdout);
    } else if (signal == SIGUSR1){
        if(going_to_counter() != 0){
            perror("Error going to counter");
            kill(getpid(), SIGTERM);
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Received unknown signal: %d", signal);
        fflush(stdout);
    }
}

/**
 * @brief Main function to start the user process.
 * @return Exit status.
 */
int main(int argc, char const *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <et_pid>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* argv[1] è la stringa con il PID */
    char *endptr;
    long tmp = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid PID: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    ticket_generator_pid = (pid_t)tmp;

    log_init("log/so2025.log");

    // Connect to the shared memory
    int shmid_config = shmget(SHM_CONFIG_KEY, sizeof(struct simulation_parameter), 0666);
    if (shmid_config == -1) {
        perror("Error getting shared memory");
        return -1;
    }

    config = (parameter) shmat(shmid_config, NULL, 0);
    if (config == (void*) -1) {
        perror("Error attaching shared memory");
        return -1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error setting SIGUSR1 handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("Error setting SIGUSR2 handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting SIGINT handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Error setting SIGALRM handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Error setting SIGTERM handler");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGABRT, &sa, NULL) == -1) {
        perror("Error setting SIGTERM handler");
        exit(EXIT_FAILURE);
    }

    int shmid_counters = shmget(SHM_COUNTERS_KEY, sizeof(counter) * config->NOF_WORKER_SEATS, 0666);
    if (shmid_counters == -1) {
        perror("Error getting shared memory");
        exit(EXIT_FAILURE);
    }

    counters = (counter*) shmat(shmid_counters, NULL, 0);
    if (counters == (void*) -1) {
        perror("Error attaching shared memory");
        exit(EXIT_FAILURE);
    }

    semid = semget(SEM_KEY, (config->NOF_WORKER_SEATS + 1), 0666);
    if (semid == -1) {
        perror("Error getting semaphores");
        exit(EXIT_FAILURE);
    }

    msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        perror("Error getting message queue");
        exit(EXIT_FAILURE);
    }

    probability = config -> P_SERV_MAX;

    fflush(stdout);
    while(1){
        pause();
    }
}