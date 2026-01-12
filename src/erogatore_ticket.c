#include "headers/erogatore_ticket.h"

// Global variables
parameter config; // Configuration parameters
counter *counters; // Array of counters
int semid; // Semaphore ID
int msgid; // Message queue ID

/**
 * @brief Signal handler for various signals.
 * @param signal The signal number.
 */
void handle_signal(int signal) {
    if (signal == SIGUSR1) {
        while(1){
            struct msgbuffer msg;

            if (msgrcv(msgid, &msg, sizeof(msg.mtext), getpid(), 0) == -1) {

            }

#ifdef DEBUGLOG
            printf("RECEIVED REQUEST FROM PID %d",  msg.mtext);
#endif

            int pid_request = msg.mtext;

            if (msgrcv(msgid, &msg, sizeof(msg.mtext), pid_request, 0) == -1) {
                perror("Error receiving message");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUGLOG
            printf("RECEIVED REQUEST N.%d FROM PID %d", msg.mtext, pid_request);
#endif

            int service_request = msg.mtext;

            int min_counter_queue = INT_MAX;
            int found_counter = -1;

            for(int i = 0; i < config->NOF_WORKER_SEATS; i++){
                if(counters[i].purpose == service_request) {
                    if(counters[i].queue < min_counter_queue){
                        min_counter_queue = counters[i].queue;
                        found_counter = i;
                    }
                }
            }

            // TODO Check for error

            if(found_counter < 0) {
                perror("Error finding counter");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUGLOG
            printf("ADDING %d TO %d COUNTER", pid_request, found_counter);
#endif

            if (semop(semid, &(struct sembuf){found_counter, -1, 0}, 1) == -1) {
                perror("Error locking semaphore");
                exit(EXIT_FAILURE);
            }   

            int i = 0;
            while(counters[found_counter].pid_array[i] != 0) {
                i++;
            }

            counters[found_counter].pid_array[i] = pid_request;
            counters[found_counter].queue += 1;

            if (semop(semid, &(struct sembuf){found_counter, 1, 0}, 1) == -1) {
                perror("Error unlocking semaphore");
                exit(EXIT_FAILURE);
            }

#ifdef DEBUGLOG
            printf("ADDED %d TO %d COUNTER", pid_request, found_counter);
#endif
            // TODO check if queue was empty, in case wake up operator
        }

    } else if (signal == SIGTERM) {
        exit(EXIT_SUCCESS);
    } else if (signal == SIGALRM) {

    } else if (signal == SIGINT) {

    } else {

    }
}

/**
 * @brief Main function to start the ticket generator process.
 * @return Exit status.
 */
int main() {

    log_init("log/so2025.log");

    // Connect to the shared memory for configuration
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

    // Connect to the shared memory for configuration
    int shmid_counters = shmget(SHM_COUNTERS_KEY, sizeof(struct simulation_parameter), 0666);
    if (shmid_counters == -1) {
        perror("Error getting counters");
        return -1;
    }

    counters = (counter*)shmat(shmid_counters, NULL, 0);
    if (config == (void*) -1) {
        perror("Error attaching counters");
        return -1;
    }

    msgid = msgget(MSG_KEY, 0666);
    if (msgid == -1) {
        perror("Error getting message queue");
        exit(EXIT_FAILURE);
    }

    semid = semget(SEM_KEY, (config->NOF_WORKER_SEATS + 1), 0666);
    if (semid == -1) {
        perror("Error getting semaphores");
        exit(EXIT_FAILURE);
    }

    // Set up signal handlers
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

    pause();
    return 0;
}