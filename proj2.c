#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/shm.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h> 

#define ARG_COUNT 6 // Number of expected command-line arguments

// Global variables for semaphores, file, and shared memory
sem_t *mutex;
sem_t **stops;
sem_t *output;
sem_t *aboard;
sem_t *finalStop;
FILE *f;
int *lineNumPtr;
int *count;
int *onBoard;
int *bus_stop;
int *waiting;

// Structure to hold command-line arguments
typedef struct Param {
    int L;  // Number of skiers
    int Z;  // Number of stops
    int K;  // Bus capacity
    int TL; // Max waiting time for a skier
    int TB; // Max time between stops for the bus
} Param;

// Function to print formatted output
void print(const char *fmt, ...) {
    sem_wait(output);
    va_list args;
    va_start(args, fmt);

    fprintf(f, "%d: ", (*lineNumPtr)++);
    vfprintf(f, fmt, args);

    fflush(f);
    va_end(args);
    sem_post(output);
}

// Function to print help message
void printhelp() {
    printf("Spusteni:\n");
    printf("$ ./proj2 L Z K TL TB\n");
    printf("* L: pocet lyzaru, L<20000\n");
    printf("* Z: pocet nastupnich zastavek, 0<Z<=10\n");
    printf("* K: kapacita skibusu, 10<=K<=100\n");
    printf("* TL: Maximalni cas v mikrosekundach, ktery lyzar ceka, nez prijde na zastavku, 0<=TL<=10000\n");
    printf("* TB: Maximalni doba jizdy autobusu mezi dvema zastavkami. 0<=TB<=1000\n");
    exit(0);
}

// Function to check if a string represents a number
bool isNumber(char number[]) {
    int i = 0;

    // Checking for negative numbers
    if (number[0] == '-')
        i = 1;
    for (; number[i] != '\0'; i++) {
        if (!isdigit(number[i]))
            return false;
    }
    return true;
}

// Function to initialize command-line arguments
Param *arg_init(int argc, char *argv[]) {

    if (argc != ARG_COUNT) {
        fprintf(stderr,"Zly pocet argumentov!\n");
        exit(1); 
    }

    for (int i=1;i<ARG_COUNT;i++) {
        if (!isNumber(argv[i])) {
            fprintf(stderr,"Argumenty musia byt cisla\n");
            exit(1); 
        }
    }

    Param* arg = (Param*)malloc(sizeof(Param));
    if (arg == NULL) {
        fprintf(stderr,"Parsovanie argumentov zlyhalo!\n");
        free(arg);
        exit(1); 
    }

    sscanf(argv[1], "%d", &arg->L);
    sscanf(argv[2], "%d", &arg->Z);
    sscanf(argv[3], "%d", &arg->K);
    sscanf(argv[4], "%d", &arg->TL);
    sscanf(argv[5], "%d", &arg->TB);

    return arg;
}

// Function to deallocate resources
void destr(Param *arg) {
    // Deallocate and destroy semaphores
    if (sem_destroy(mutex) == -1) {
        perror("sem_destroy mutex failed");
    }
    if (sem_destroy(output) == -1) {
        perror("sem_destroy output failed");
    }
    if (sem_destroy(aboard) == -1) {
        perror("sem_destroy aboard failed");
    }
    if (sem_destroy(finalStop) == -1) {
        perror("sem_destroy finalStop failed");
    }
    for (int i = 0; i < (arg->Z + 1); i++) {
        if (sem_destroy(stops[i]) == -1) {
            perror("sem_destroy stops failed");
        }
    }

    // Deallocate shared memory segments
    if (munmap(lineNumPtr, sizeof(int)) == -1) {
        perror("munmap lineNumPtr failed");
    }
    if (munmap(onBoard, sizeof(int)) == -1) {
        perror("munmap onBoard failed");
    }
    if (munmap(bus_stop, sizeof(int)) == -1) {
        perror("munmap bus_stop failed");
    }
    if (munmap(count, sizeof(int)) == -1) {
        perror("munmap count failed");
    }
    if (munmap(mutex, sizeof(sem_t)) == -1) {
        perror("munmap mutex failed");
    }
    if (munmap(finalStop, sizeof(sem_t)) == -1) {
        perror("munmap finalStop failed");
    }
    if (munmap(aboard, sizeof(sem_t)) == -1) {
        perror("munmap aboard failed");
    }
    if (munmap(output, sizeof(sem_t)) == -1) {
        perror("munmap output failed");
    }
    if (munmap(waiting, sizeof(int) * arg->Z) == -1) {
        perror("munmap waiting failed");
    }

    // Deallocate stops array
    for (int i = 0; i < (arg->Z + 1); i++) {
        if (munmap(stops[i], sizeof(sem_t)) == -1) {
            perror("munmap stops[i] failed");
        }
    }
    if (munmap(stops, sizeof(sem_t*) * (arg->Z + 1)) == -1) {
        perror("munmap stops failed");
    }

    // Free the memory allocated for arg
    free(arg);
    fclose(f);
}

// Function to check command-line arguments validity
void err_check(Param *arg) {
    if (!(arg->L > 0 && arg->L < 20000) ||
        !(arg->Z > 0 && arg->Z <= 10) ||
        !(arg->K >= 10 && arg->K <= 100) ||
        !(arg->TL >= 0 && arg->TL <= 10000) ||
        !(arg->TB >= 0 && arg->TB <= 1000)) {
        fprintf(stderr, "Nieco je zle s tvojim vstupom\n");
        printhelp();
        destr(arg); 
        exit(1);
    }
}

// Function to initialize semaphores
void semaphore_init(Param *arg) {
    // Initialize semaphores
    output = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (output == MAP_FAILED) {
        perror("mmap output failed");
        exit(EXIT_FAILURE);
    }
    if (sem_init(output, 1, 1) == -1) {
        perror("sem_init output failed");
        exit(EXIT_FAILURE);
    }
    
    mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (mutex == MAP_FAILED) {
        perror("mmap mutex failed");
        exit(EXIT_FAILURE);
    }
    if (sem_init(mutex, 1, 1) == -1) {
        perror("sem_init mutex failed");
        exit(EXIT_FAILURE);
    }
    
    finalStop = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (finalStop == MAP_FAILED) {
        perror("mmap finalStop failed");
        exit(EXIT_FAILURE);
    }
    if (sem_init(finalStop, 1, 0) == -1) {
        perror("sem_init finalStop failed");
        exit(EXIT_FAILURE);
    }
    
    aboard = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (aboard == MAP_FAILED) {
        perror("mmap aboard failed");
        exit(EXIT_FAILURE);
    }
    if (sem_init(aboard, 1, 0) == -1) {
        perror("sem_init aboard failed");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for stops array
    stops = mmap(NULL, sizeof(sem_t*) * (arg->Z + 1), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (stops == MAP_FAILED) {
        perror("mmap stops failed");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < (arg->Z + 1); i++) {
        stops[i] = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if (stops[i] == MAP_FAILED) {
            perror("mmap stops[i] failed");
            exit(EXIT_FAILURE);
        }
        if (sem_init(stops[i], 1, 0) == -1) {
            perror("sem_init stops[i] failed");
            exit(EXIT_FAILURE);
        }
    }

    // Allocate memory for other shared variables
    lineNumPtr = mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (lineNumPtr == MAP_FAILED) {
        perror("mmap lineNumPtr failed");
        exit(EXIT_FAILURE);
    }
    count = mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (count == MAP_FAILED) {
        perror("mmap count failed");
        exit(EXIT_FAILURE);
    }
    waiting = mmap(NULL,sizeof(int*)*arg->Z, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (waiting == MAP_FAILED) {
        perror("mmap waiting failed");
        exit(EXIT_FAILURE);
    }
    onBoard = mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (onBoard == MAP_FAILED) {
        perror("mmap onBoard failed");
        exit(EXIT_FAILURE);
    }
    bus_stop = mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (bus_stop == MAP_FAILED) {
        perror("mmap bus_stop failed");
        exit(EXIT_FAILURE);
    }
}

// Function for bus process
void bus_process (Param *arg) {
    print("BUS: started\n");
    while (1) {
        sem_wait(mutex);
        (*bus_stop) = *bus_stop % (arg->Z + 1);
        int rand_time = rand() % (arg->TB+1);
         if (*count == arg->L) {
            print("BUS: finish\n");
            break;
        }
        sem_post(mutex);
        usleep(rand_time);
        if (*bus_stop  < arg->Z) {
            print("BUS: arrived to %i\n", *bus_stop+1);
            sem_wait(mutex);
            int passengers = (*onBoard);
            int cakac = waiting[*bus_stop];
            int waitingAtStop = ((passengers + cakac) <= arg->K) ? cakac : cakac - (passengers + cakac - arg->K); 
            sem_post(mutex);
            for (int i = 0; i < waitingAtStop; i++){
                sem_post(stops[*bus_stop]);
                sem_wait(aboard); 
            }
            print("BUS: leaving %i\n", *bus_stop+1);
            sem_wait(mutex);
            (*bus_stop)+=1;
            sem_post(mutex);
            continue;
        }
        else {
            print("BUS: arrived to final\n");
            sem_wait(mutex);
            int seated = *onBoard;
            sem_post(mutex);
            for (int i=0; i < seated; i++) {
                sem_post(finalStop);
                sem_wait(stops[*bus_stop]);
            }
            print("BUS: leaving final\n");
            sem_wait(mutex);
            (*bus_stop)+=1;
            sem_post(mutex);
            continue;
        }
       
    }
}

// Function for skier process
void skier_process (Param *arg, int i, int stop) {
        print("L %i: started\n", i);
        sem_wait(mutex);
        int rand_time = rand() % (arg->TL + 1);
        sem_post(mutex);
        usleep(rand_time);
        print("L %i: arrived to %i\n", i, stop);
        sem_wait(mutex);
        (waiting[stop-1])+=1;
        sem_post(mutex);
        sem_wait(stops[stop-1]);
        sem_wait(mutex);
        (waiting[stop-1])-=1;
        (*onBoard)+=1;
        sem_post(mutex);
        print("L %i: boarding\n",i);
        sem_post(aboard); 
        sem_wait(finalStop);
        sem_wait(mutex);
        (*count)+=1;
        (*onBoard)-=1;
        sem_post(mutex);
        print("L %i: going to ski\n", i);
        sem_post(stops[*bus_stop]);
} 

// Main function
int main(int argc, char *argv[]) {
    // Initialize command-line arguments
    Param* arg = arg_init(argc, argv);
    // Check for validity of arguments
    err_check(arg);
    // Initialize semaphores and shared memory
    semaphore_init(arg);
    // Open output file
    f = fopen("proj2.out", "w");
    if (f == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    // Seed random number generator
    srand(time(NULL) ^ (getpid() << 16));
    *lineNumPtr = 1;
    *bus_stop = 0;
    *count = 0;
    *onBoard = 0;
    for (int i=0; i < arg->Z; i++) {
        waiting[i]=0;
    }

    // Fork bus process
    pid_t bus_pid = fork();
    if (bus_pid == -1) {
        perror("Failed to fork bus process");
        destr(arg); 
        exit(EXIT_FAILURE);
    }
    if (bus_pid == 0) {
        bus_process(arg);
        destr(arg); 
        exit(0);
    }

    // Fork skier processes
    for (int i = 0; i < arg->L; i++) {
        pid_t skierPid = fork();
        if (skierPid == -1) {
            perror("Failed to fork skier process");
            destr(arg); 
            exit(EXIT_FAILURE);
        }
        if (skierPid == 0) {
            srand(time(NULL) ^ (getpid() << 16));
            int stop = rand() % arg->Z + 1;
            skier_process(arg, i+1, stop);
            destr(arg); 
            exit(0);
        }
    } 

    // Wait for all child processes to finish
    while (wait(NULL) > 0);
    // Clean up resources
    destr(arg); 
    return 0;
}
