//
// Created by Dario Bonfiglio on 5/22/25.
//

#include "headers/director.h"

parameter config; // Configuration parameters

pid_t pid_et; // PID for the ticket creator
pid_t *pid_operators; // Array of PIDs for operators
pid_t *pid_users; // Array of PIDs for users

counter *counters; // Array of counters

int msgid; // Message queue ID

int semid; // Semaphore ID
int semid_log; // Semaphore ID
int shmid_config; // Shared memory ID for configuration
int shmid_counters; // Shared memory ID for counters

pthread_t time_receiver_thread;

// Global values for Logging
int tot_users = 0;
int tot_serv = 0;
double avg_serv = 0;
int tot_nserv = 0;
double avg_nserv = 0;

double avg_dwtime = 0;
double avg_twtime = 0;

double avg_ddtime = 0;
double avg_tdtime = 0;

int daily_users = 0;
int daily_nserv = 0;

void *time_receiver() {
    int semid_log = semget(LOGSEM_KEY, 1, IPC_CREAT | 0666);
    if (semid_log == -1) {
        exit(EXIT_FAILURE);
    }

    semctl(semid_log, 0, SETVAL, 1);

    struct msgbuffer_time msg;
    size_t size;

    while(1){
        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid(), IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 1, IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 2, IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 3, IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 4, IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 5, IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6, IPC_NOWAIT);
            if(size > 0) {
                if(avg_dwtime == 0) avg_dwtime = msg.mtext;
                else avg_dwtime = (avg_dwtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6 + 1, IPC_NOWAIT);
            if(size > 0) {
                if(avg_ddtime == 0) avg_ddtime = msg.mtext;
                else avg_ddtime = (avg_ddtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6 + 2, IPC_NOWAIT);
            if(size > 0) {
                if(avg_ddtime == 0) avg_ddtime = msg.mtext;
                else avg_ddtime = (avg_ddtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6 + 3, IPC_NOWAIT);
            if(size > 0) {
                if(avg_ddtime == 0) avg_ddtime = msg.mtext;
                else avg_ddtime = (avg_ddtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6 + 4, IPC_NOWAIT);
            if(size > 0) {
                if(avg_ddtime == 0) avg_ddtime = msg.mtext;
                else avg_ddtime = (avg_ddtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6 + 5, IPC_NOWAIT);
            if(size > 0) {
                if(avg_ddtime == 0) avg_ddtime = msg.mtext;
                else avg_ddtime = (avg_ddtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }

        if(semctl(semid_log, 0, GETVAL) > 0) {
            size = msgrcv(msgid, &msg, sizeof(msg.mtext), 3000 + getpid() + 6 + 6, IPC_NOWAIT);
            if(size > 0) {
                if(avg_ddtime == 0) avg_ddtime = msg.mtext;
                else avg_ddtime = (avg_ddtime + msg.mtext) / 2;
            } else {
                if(errno != ENOMSG) {
                    perror("Unknown error in msgrcv()");
                    // TODO Kill everything
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
    return NULL;
}

void print_log(void){

    if (semop(semid_log, &(struct sembuf){0, -1, 0}, 1) == -1) {
        perror("Error locking semaphore");
        exit(EXIT_FAILURE);
    }

    daily_users = 0;
    for(int i = 0; i < config -> NOF_WORKER_SEATS; i++){
        daily_users += counters[i].queue;
    }

    tot_users += daily_users;

    int daily_served = 0;
    for(int i = 0; i < config -> NOF_WORKER_SEATS; i++){
        daily_served += counters[i].served_requests;
    }

    for(int i = 0; i < config -> NOF_WORKER_SEATS; i++){
        tot_serv += counters[i].served_requests;
    }

    double daily_avg_serv = counters[0].served_requests;
    for(int i = 1; i < config -> NOF_WORKER_SEATS; i++){
        daily_avg_serv += counters[i].served_requests;
        daily_avg_serv = daily_avg_serv / 2; 
    }

    if(avg_serv == 0) avg_serv = daily_avg_serv;
    else avg_serv = (avg_serv + daily_avg_serv) / 2;

    daily_nserv = 0;
    for(int i = 0; i < config -> NOF_WORKER_SEATS; i++){
        daily_nserv += (counters[i].queue - counters[i].served_requests);
    }

    daily_nserv += (daily_users - daily_served - daily_nserv);

    for(int i = 0; i < config -> NOF_WORKER_SEATS; i++){
        tot_nserv += (counters[i].queue - counters[i].served_requests);
    }

    double daily_avg_nserv = (counters[0].queue - counters[0].served_requests);
    for(int i = 1; i < config -> NOF_WORKER_SEATS; i++){
        daily_avg_nserv += (counters[i].queue - counters[i].served_requests);
        daily_avg_nserv = daily_avg_nserv / 2; 
    }

    if(avg_nserv == 0) avg_nserv = daily_avg_nserv;
    else avg_nserv = (avg_nserv + daily_avg_nserv) / 2;
    
    if(avg_twtime == 0) avg_twtime = avg_dwtime;
    else avg_twtime = (avg_twtime + avg_dwtime) / 2;
    
    if(avg_tdtime == 0) avg_tdtime = avg_ddtime;
    else avg_tdtime = (avg_tdtime + avg_ddtime) / 2;

    printf("D - USERS = %d\n"
        "T - USERS = %d\n"
        "D - Users Served = %d\n"
        "T - Users Served = %d\n"
        "D - Average Users Served = %f\n"
        "T - Average Users Served = %f\n"
        "D - Requests Served = %d\n"
        "T - Requests Served = %d\n"
        "D - Requests Not Served = %d\n"
        "T - Requests Not Served = %d\n"
        "D - Average Requests Served = %f\n"
        "T - Average Requests Served = %f\n"
        "D - Average Requests Not Served = %f\n"
        "T - Average Requests Not Served = %f\n"
        "D - Average Waiting Time = %f\n"
        "T - Average Waiting Time = %f\n"
        "D - Average Delivery Service Time = %f\n"
        "T - Average Delivery Service Time = %f\n", 
        daily_users, tot_users, daily_served, 
        tot_serv, daily_avg_serv, avg_serv, daily_served, 
        tot_serv, daily_nserv, tot_nserv, daily_avg_serv, 
        avg_serv, daily_avg_nserv, avg_nserv, avg_dwtime, 
        avg_twtime, avg_ddtime, avg_tdtime);

    avg_dwtime = 0;
    avg_ddtime = 0;

    if (semop(semid_log, &(struct sembuf){0, 1, 0}, 1) == -1) {
        perror("Error unlocking semaphore");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Sets up the counters with operator PIDs and assigns random roles.
 * @param NOF_WORKERS Number of workers.
 * @param NOF_WORKER_SEATS Number of worker seats.
 * @param pid_operators Array of operator PIDs.
 * @param counters Array of counters.
 */
void setting_up_counters(int NOF_WORKERS, int NOF_WORKER_SEATS, pid_t *pid_operators, counter *counters) {
    static int seed_initialized = 0;
    if (!seed_initialized) {
        srand(time(NULL));
        seed_initialized = 1;
    }

    int min_roles = NOF_WORKER_SEATS / 6; // Numero minimo di worker per ogni ruolo
    int extra_roles = NOF_WORKER_SEATS % 6; // Ruoli che avranno un worker in più

    int tracking_counters[6] = {0};

    // Assegna ruoli in modo bilanciato
    int index = 0;
    for (int role = 0; role < 6; ++role) {
        int count = min_roles + (role < extra_roles ? 1 : 0);
        for (int j = 0; j < count; ++j) {
            counters[index].purpose = role;
            tracking_counters[role]++;
            index++;
        }
    }

    // Mescola i ruoli per renderli casuali
    for (int i = NOF_WORKER_SEATS - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int temp = counters[i].purpose;
        counters[i].purpose = counters[j].purpose;
        counters[j].purpose = temp;
    }

    // Assegna worker ai counter
    int min_workers = NOF_WORKERS < NOF_WORKER_SEATS ? NOF_WORKERS : NOF_WORKER_SEATS;
    for (int i = 0; i < min_workers; ++i) {
        counters[i].worker_id = pid_operators[i];

        struct msgbuffer msg;

        msg.mtype = getpid() + pid_operators[i];
        msg.mtext = counters[i].purpose; 

        if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
            perror("Error sending PID");
            exit(EXIT_FAILURE);
        }

    }
    
    /*
    // Stampa i counter
    printf("Counters:");
    for (int i = 0; i < NOF_WORKER_SEATS; ++i) {
        printf("%d: Purpose=%d, Worker=%d", counters[i].counter_id, counters[i].purpose, counters[i].worker_id);
    } */
}

/**
 * @brief Signal handler for SIGTERM.
 * @param signal The signal number.
 */
void handle_signal(int signal){
    // SIGTERM --> kill all the Processes before exiting
    if (signal == SIGTERM){
        printf("Received SIGTERM, exiting");
        printf("Cleaning up...");

        // Terminate and wait for all operator processes
        for (int i = 0; i < config->NOF_WORKERS; ++i) {
            kill(pid_operators[i], SIGTERM);
            waitpid(pid_operators[i], NULL, 0);
        }

        // Terminate and wait for all user processes
        for (int i = 0; i < config->NOF_USERS; ++i) {
            kill(pid_users[i], SIGTERM);
            waitpid(pid_users[i], NULL, 0);
        }

        // Terminate and wait for the ticket creator process
        kill(pid_et, SIGTERM);
        waitpid(pid_et, NULL, 0);

        // Sleep to ensure all processes have terminated
        sleep(config->NOF_WORKERS + config->NOF_USERS + 1);

        // Free allocated memory and clean up shared resources
        free(pid_operators);
        free(pid_users);
        shmctl(shmid_config, IPC_RMID, NULL);
        shmctl(shmid_counters, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID, 0);
        printf("Cleaned up!");
        printf("Goodbye! :(");

        printf("--------------------------------------------------------");
        exit(EXIT_FAILURE);
    } else {
        printf("Received unknown signal: %d", signal);
        printf("Cleaning up...");

        // Terminate and wait for all operator processes
        for (int i = 0; i < config->NOF_WORKERS; ++i) {
            kill(pid_operators[i], SIGTERM);
            waitpid(pid_operators[i], NULL, 0);
        }

        // Terminate and wait for all user processes
        for (int i = 0; i < config->NOF_USERS; ++i) {
            kill(pid_users[i], SIGTERM);
            waitpid(pid_users[i], NULL, 0);
        }

        // Terminate and wait for the ticket creator process
        kill(pid_et, SIGTERM);
        waitpid(pid_et, NULL, 0);

        // Sleep to ensure all processes have terminated
        sleep(config->NOF_WORKERS + config->NOF_USERS + 1);

        // Free allocated memory and clean up shared resources
        free(pid_operators);
        free(pid_users);
        shmctl(shmid_config, IPC_RMID, NULL);
        shmctl(shmid_counters, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID, 0);
        printf("Cleaned up!");
        printf("Goodbye! :(");

        printf("--------------------------------------------------------");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Reads configuration data from a file.
 * @param config Configuration parameters.
 * @param fp File pointer to the configuration file.
 * @return 0 on success, 1 on error.
 */
int scan_data(parameter config, FILE* fp) {
    int value;
    char name_param[100];
    int error = 1;
    while(fscanf(fp, "%s %d", name_param, &value) != EOF) {
        if(strcmp(name_param, "NOF_WORKER_SEATS") == 0) {
            config->NOF_WORKER_SEATS = value;
            error = 0;
        } else if(strcmp(name_param, "NOF_WORKERS") == 0) {
            config->NOF_WORKERS = value;
            error = 0;
        } else if(strcmp(name_param, "NOF_USERS") == 0) {
            config->NOF_USERS = value;
            error = 0;
        } else if(strcmp(name_param, "NOF_PAUSE") == 0) {
            config->NOF_PAUSE = value;
            error = 0;
        } else if(strcmp(name_param, "SIM_DURATION") == 0) {
            config->SIM_DURATION = value;
            error = 0;
        } else if(strcmp(name_param, "P_SERV_MIN") == 0) {
            config->P_SERV_MIN = value;
            error = 0;
        } else if(strcmp(name_param, "P_SERV_MAX") == 0) {
            config->P_SERV_MAX = value;
            error = 0;
        } else if(strcmp(name_param, "N_NANO_SECONDS") == 0) {
            config->N_NANO_SECONDS = value;
            error = 0;
        } else if(strcmp(name_param, "EXPLODE_THRESHOLD") == 0) {
            config->EXPLODE_THRESHOLD = value;
            error = 0;
        } else {
            error = 1;
        }
    }
    appinfo_log("Data read from file!");
    return error;
}

/**
 * @brief Generates a new user process.
 * @return PID of the new user process.
 */

pid_t user_generator(void) {
    fflush(stdout);
    pid_t pid_user;

    switch ((pid_user = fork())) {
        case -1:
            TEST_ERROR;
            perror("Error forking the process!");
            exit(EXIT_FAILURE);

        case 0: {
            /* nel figlio, ricaviamo il PID e lo convertiamo in stringa */
            char pid_str[32];
            snprintf(pid_str, sizeof(pid_str), "%ld", (long)pid_et);

            /* execvp: argv[0] = USER_PATH, argv[1] = pid_str, argv[2] = NULL */
            execvp(USER_PATH, (char* const[]){
                USER_PATH,
                pid_str,
                NULL
            });

            /* se execvp torna, è fallita */
            perror("Error starting the user!");
            exit(EXIT_FAILURE);
        }

        default:
            return pid_user;
    }
}

/**
 * @brief Generates a new user process.
 * @return PID of the new user process.
 */
pid_t operator_generator(void) {
    fflush(stdout);
    pid_t pid_operator;
    pid_t master_pid = getpid();

    switch ((pid_operator = fork())) {
        case -1:
            TEST_ERROR;
            perror("Error forking the process!");
            exit(EXIT_FAILURE);

        case 0: {
            /* nel figlio, ricaviamo il PID e lo convertiamo in stringa */
            char pid_str[32];
            snprintf(pid_str, sizeof(pid_str), "%ld", (long)master_pid);

            /* execvp: argv[0] = OPERATOR_PATH, argv[1] = pid_str, argv[2] = NULL */
            execvp(OPERATOR_PATH, (char* const[]){
                OPERATOR_PATH,
                pid_str,
                NULL
            });

            /* se execvp torna, è fallita */
            perror("Error starting the operator!");
            exit(EXIT_FAILURE);
        }

        default:
            return pid_operator;
    }
}

/**
 * @brief Generates a new ticket creator process.
 * @return PID of the new ticket creator process.
 */
pid_t et_generator() {
    fflush(stdout);
    pid_t pid_et;
    switch (pid_et = fork()) {
        case -1:
            TEST_ERROR;
            perror("Error forking the process!");
            exit(EXIT_FAILURE);
        case 0:
            execvp(EROGATORE_TICKET_PATH, (char* const[]) {NULL});
            perror("Error starting the activator!");
            exit(EXIT_FAILURE);
        default:
            return pid_et;
    }
}

/**
* @brief Main function to start the simulation.
* @return Exit status.
*/
int main(int argc, char const *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: <config_file>\n");
        return EXIT_FAILURE;
    }

    log_init("log/so2025.log");

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Error setting SIGTERM handler");
        syserr_log("Error setting SIGTERM handler");
        exit(EXIT_FAILURE);
    }

    // CREATE CONFIG SHARED MEMORY
    shmid_config = shmget(SHM_CONFIG_KEY, sizeof(struct simulation_parameter), IPC_CREAT | 0666);
    if (shmid_config == -1) {
        perror("Error creating shared memory");
        syserr_log("Error creating shared memory");
        return (-1);
    }

    config = (parameter) shmat(shmid_config, NULL, 0);
    if (config == (void*) -1) {
        perror("Error attaching shared memory");
        syserr_log("Error attaching shared memory");
        shmctl(shmid_config, IPC_RMID, NULL);
        return (-1);
    }

    // READ CONFIG
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL){
        perror("Error opening file");
        syserr_log("Error opening file");
        return (-1);
    }

    if(scan_data(config, fp) == 1) {
        perror("Error reading data from file");
        syserr_log("Error reading data from file");
        return (-1);
    }
    
    fclose(fp);

    msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Error getting message queue");
        exit(EXIT_FAILURE);
    }

    sysinfo_log("All IPCs structure correctly initialised");

    appinfo_log("--------------------------------------------------------\n"
                "NOF_WORKER_SEATS: %d\n"
                "NOF_WORKERS: %d\n"
                "NOF_USERS: %d\n"
                "NOF_PAUSE: %d\n"
                "P_SERV_MIN: %d\n"
                "P_SERV_MAX: %d\n"
                "SIM_DURATION: %d\n"
                "N_NANO_SECONDS: %d\n"
                "EXPLODE_THRESHOLD: %d\n"
                "--------------------------------------------------------",
                config->NOF_WORKER_SEATS, config->NOF_WORKERS, config->NOF_USERS,
                config->NOF_PAUSE, config->P_SERV_MIN, config->P_SERV_MAX,
                config->SIM_DURATION, config->N_NANO_SECONDS, config->EXPLODE_THRESHOLD);

        // Create counters
    printf("Initialising Counters shared memory...");
    fflush(stdout);
    shmid_counters = shmget(SHM_COUNTERS_KEY, sizeof(counter) * config->NOF_WORKER_SEATS, IPC_CREAT | 0666);
    if (shmid_counters == -1) {
        perror("Error creating shared memory");
        shmctl(shmid_config, IPC_RMID, NULL);
        free(pid_operators);
        free(pid_users);
        return (-1);
    }

    counters = (counter*) shmat(shmid_counters, NULL, 0);
    if (counters == (void*) -1) {
        perror("Error attaching shared memory");
        shmctl(shmid_config, IPC_RMID, NULL);
        shmctl(shmid_counters, IPC_RMID, NULL);
        return (-1);
    }

    for (int i = 0; i < config->NOF_WORKER_SEATS; ++i) {
        memset(&counters[i], 0, sizeof counters[i]);
        counters[i].counter_id = i;
        counters[i].purpose = i % 6;
        counters[i].worker_id = -1;
        memset(counters[i].pid_array, 0, sizeof counters[i].pid_array);
        counters[i].queue = 0;
        counters[i].served_requests = 0; 
    }
    sleep(1);
    sysinfo_log("Initialised n. %d Counters in SHM", config->NOF_WORKER_SEATS);
    fflush(stdout);

    // Create semaphores for counters
    printf("Initialising Semaphores...");
    fflush(stdout);
    semid = semget(SEM_KEY, (config->NOF_WORKER_SEATS + 1), IPC_CREAT | 0666);
    if (semid == -1) {
        perror("Error creating semaphores");
        shmctl(shmid_config, IPC_RMID, NULL);
        free(pid_operators);
        free(pid_users);
        shmctl(shmid_counters, IPC_RMID, NULL);
        return (-1);
    }

    for (int i = 0; i < (config->NOF_WORKER_SEATS + 1); ++i) {
        semctl(semid, i, SETVAL, 1);
    }

    semid_log = semget(LOGSEM_KEY, 1, IPC_CREAT | 0666);
    if (semid_log == -1) {
        perror("Error creating semaphores");
        shmctl(shmid_config, IPC_RMID, NULL);
        free(pid_operators);
        free(pid_users);
        shmctl(shmid_counters, IPC_RMID, NULL);
        return (-1);
    }

    semctl(semid_log, 0, SETVAL, 1);

    sleep(1);
    sysinfo_log("Initialised n. %d Semaphores", config->NOF_WORKER_SEATS + 1);
    fflush(stdout);

    // Create ticket creator process
    fflush(stdout);
    pid_et = et_generator();
    sleep(1);
    sysinfo_log("Ticket Creator initialised correctly");
    fflush(stdout);

    // Create operator processes
    printf("Forking Operators...");
    fflush(stdout);
    pid_operators = (pid_t*) malloc(sizeof(pid_t) * config->NOF_WORKERS);
    for (int i = 0; i < config->NOF_WORKERS; ++i) {
        pid_t pid_operator = operator_generator();
        pid_operators[i] = pid_operator;
    }
    sleep(1);
    sysinfo_log("Forked n. %d Operator processes", config->NOF_WORKERS);
    fflush(stdout);

    // Create user processes
    printf("Forking Users...");
    fflush(stdout);
    pid_users = (pid_t*) malloc(sizeof(pid_t) * config->NOF_USERS);
    for (int i = 0; i < config->NOF_USERS; ++i) {
        pid_t pid_user = user_generator();
        pid_users[i] = pid_user;
    }
    sleep(1);
    sysinfo_log("Forked n. %d User processes", config->NOF_USERS);
    fflush(stdout);

    printf("--------------------------------------------------------");

    // Start the simulation
    printf("Starting simulation...");

    
    if (pthread_create(&time_receiver_thread, NULL, time_receiver, NULL) != 0) {
        printf("Errore nella creazione del thread\n");
        return 1;
    }

    kill(pid_et, SIGUSR1);
    printf("--------------------------------------------------------");

    // Run the simulation for the specified duration
    for (int i = 0; i < config->SIM_DURATION; ++i) {

        for (int i = 0; i < config->NOF_WORKERS; ++i) {
            kill(pid_operators[i], SIGUSR1);
        }

        setting_up_counters(config->NOF_WORKERS, config->NOF_WORKER_SEATS, pid_operators, counters);

        for (int i = 0; i < config->NOF_WORKERS; ++i) {
            srand(time(NULL));
            pid_t temp = pid_operators[i];
            int random_index = rand() % config->NOF_WORKERS;
            pid_operators[i] = pid_operators[random_index];
            pid_operators[random_index] = temp; 
        }

        for (int i = 0; i < config->NOF_USERS; ++i) {
            kill(pid_users[i], SIGUSR1);
        }

        sleep(1);

        printf("Day %d", i+1);
        
        printf("--------------------------------------------------------");

        print_log();

        for(int i = 0; i < config->NOF_WORKER_SEATS; i++){
            memset(&counters[i], 0, sizeof counters[i]);
        }

        if(daily_nserv >= config -> EXPLODE_THRESHOLD) {
            printf("POSTAL COUNTER EXPLODED, TODAY %d PEOPLE WERE NOT SERVED!!!", daily_nserv);
                kill(pid_et, SIGTERM);
                waitpid(pid_et, NULL, 0);
                free(pid_operators);
                free(pid_users);
                shmctl(shmid_config, IPC_RMID, NULL);
                shmctl(shmid_counters, IPC_RMID, NULL);
                semctl(semid, 0, IPC_RMID, 0);
                log_stop();
                printf("--------------------------------------------------------");
                printf("Cleaned up!");
                printf("Goodbye! :(");
                printf("--------------------------------------------------------");

                return 0;
        }

        printf("--------------------------------------------------------");
    }

    // End the simulation
    printf("Simulation ended!");
    printf("--------------------------------------------------------");

    // Clean up resources
    printf("Cleaning up...");
    for (int i = 0; i < config->NOF_WORKERS; ++i) {
        kill(pid_operators[i], SIGTERM);
        waitpid(pid_operators[i], NULL, 0);
    }
    for (int i = 0; i < config->NOF_USERS; ++i) {
        kill(pid_users[i], SIGTERM);
        waitpid(pid_users[i], NULL, 0);
    }
    printf("--------------------------------------------------------");
    kill(pid_et, SIGTERM);
    waitpid(pid_et, NULL, 0);
    free(pid_operators);
    free(pid_users);
    shmctl(shmid_config, IPC_RMID, NULL);
    shmctl(shmid_counters, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, 0);
    printf("Cleaned up!");
    printf("--------------------------------------------------------");
    printf("Goodbye! :)");
    printf("--------------------------------------------------------");
    log_stop();
    return 0;
}