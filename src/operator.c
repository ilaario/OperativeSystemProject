#include "headers/operator.h"

parameter config;
counter *counters;
int semid;
int msgid;

pid_t director_pid;


void handle_signal(int signal){
    if(signal == SIGUSR1){

#ifdef DEBUGLOG
        printf("START WORKING AGAIN FOR A NEW DAY");
#endif

        struct msgbuffer msg;

        int current_counter = -1;

        if (msgrcv(msgid, &msg, sizeof(msg.mtext), getpid() + director_pid, 0) == -1) {
            perror("Error receiving message");
            exit(EXIT_FAILURE);
        }

        current_counter = msg.mtext;

        while(1) {
            clock_t start = clock();
            //TODO Change from active to passive wait
            while(counters[current_counter].queue == counters[current_counter].served_requests) {
                struct timespec req = {
                    .tv_sec  = 0,            // secondi interi
                    .tv_nsec = 1000000L    // 0.2 secondi = 200 milioni di nanosecondi
                };

                nanosleep(&req, NULL);
            }

#ifdef DEBUGLOG
            printf("WORKING FOR SOMEONE");
#endif

            // random value between %50 and wait_time[counters[i].purpose]
            int rand_wait = ((wait_times[counters[current_counter].purpose] / 2) + rand() % (wait_times[counters[current_counter].purpose] - (wait_times[counters[current_counter].purpose] / 2)));

            // nanosleep for rand_wait
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = rand_wait * 1000000;
            nanosleep(&ts, NULL);

#ifdef DEBUGLOG
            printf("WAITED TIME");
#endif

            int i = 0;
            // printf("Checking current counter n. %d = ", current_counter);
            do {
                // printf("%d, i = %d", counters[current_counter].pid_array[i], i);
                if(counters[current_counter].pid_array[i] > 0) break;
                i++;
            } while(i < MAX_COUNT_QUEUE);

            if(i == MAX_COUNT_QUEUE || counters[current_counter].pid_array[i] <= 0) {
                //perror("ERROR SEARCHING IN QUEUE, FOUND VALUE %d AT I %d", counters[current_counter].pid_array[i], i);
                continue;
            }

#ifdef DEBUGLOG
            printf("NOTIFYING %d THAT HIS REQUEST IS DONE", counters[current_counter].pid_array[i]);
#endif

            msg.mtype = counters[current_counter].pid_array[i] + 10000;
            msg.mtext = 1;

            if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }   

            if (semop(semid, &(struct sembuf){current_counter, -1, 0}, 1) == -1) {
                perror("Error locking semaphore");
                exit(EXIT_FAILURE);
            }   

            counters[current_counter].pid_array[i] = -1;

            counters[current_counter].served_requests++;

            if (semop(semid, &(struct sembuf){current_counter, 1, 0}, 1) == -1) {
                perror("Error unlocking semaphore");
                exit(EXIT_FAILURE);
            }

            clock_t end = clock();
            double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

            struct msgbuffer_time time;

            time.mtype = 3000 + getppid() + 6 + counters[current_counter].purpose;
            time.mtext = time_spent;

#ifdef DEBUGLOG
            printf("SENDING MSG TO %d", getppid());
#endif

            if (msgsnd(msgid, &time, sizeof(time.mtext), 0) == -1) {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            } 

#ifdef DEBUGLOG
            printf("REQUEST COMPLETED");
#endif

        }
    } else if (signal == SIGTERM){
        exit(EXIT_SUCCESS);
    } else if (signal == SIGALRM){

    } else if (signal == SIGINT){

    } else {

    }
}

int main(int argc, char const *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <et_pid>\n", argv[0]);
        return EXIT_FAILURE;
    }

    log_init("log/so2025.log");

    /* argv[1] è la stringa con il PID */
    char *endptr;
    long tmp = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Invalid PID: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    director_pid = (pid_t)tmp;

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

    msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
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
