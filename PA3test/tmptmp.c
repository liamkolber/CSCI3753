/*
 * Liam Kolber
 * CSCI 3753 - Spring 2018
 * Programming Assignment 3
 */
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//Includes
    #include <pthread.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h>
    #include <sys/syscall.h>
    #include "util.h"
    #include "multi-lookup.h"
    #include "sys/time.h"
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//Global Variables
    //char sharedBuffer[2000][20];
    //char files_to_service[20][20];
    //char txtResults[20];
    //char txtServiced[20];
    //int GLOBALFILECOUNT; //set in main
    struct thread_Data{
        char *ptr_to_fileArray;
    };
    //locks for files
    struct mutexes {
        pthread_mutex_t lock[10];
        pthread_mutex_t sharedBufferLock;
        pthread_mutex_t serviceLock;
        pthread_mutex_t resultsLock;  
    };
    //int bufferIndex = 0;
    struct various_variables {
        char sharedBuffer[2000][20];
        char files_to_service[20][20];
        char txtResults[20];
        char txtServiced[20];
        int GLOBALFILECOUNT; //set in main
        int bufferIndex;
    };
    struct globals {
        struct thread_Data *array;
        struct various_variables var;
        struct mutexes mut;
    };
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//takes struct containing file descripter and mutexs
void *requester_entryFunction(void *thd){
    pid_t tid = syscall(SYS_gettid); //process ID
    struct globals *reqG = thd;

    for (int i = 0; i < reqG->var.GLOBALFILECOUNT; i++) {
        if(!pthread_mutex_trylock(&reqG->mut.lock[i])) { //if true, thread now owns lock
            char fileTobeServiced[20];
            strcpy(fileTobeServiced,(reqG->array->ptr_to_fileArray + (i*20)));
            
            pid_t tid = syscall(SYS_gettid);
            printf("\n%d is servicing %s\n", tid, fileTobeServiced);
            
            pthread_mutex_lock(&reqG->mut.serviceLock);
                FILE *f = fopen(reqG->var.txtServiced, "a"); //serviced texts
                if (f == NULL) {
                    printf("Error opening file!\n");
                    exit(1);
                }
                fprintf(f, "thread: %d is servicing %s\n", tid, fileTobeServiced);
                fclose(f);
            pthread_mutex_unlock(&reqG->mut.serviceLock);

            FILE* input_file = fopen(fileTobeServiced, "r");
            if(!input_file) {
                printf("Error: invalid Input File\n");
            }
            char domainName[20];
            while(fgets(domainName, sizeof(domainName), input_file)) {
                domainName[strlen(domainName)-1] = '\0';
                strcpy(reqG->var.sharedBuffer[reqG->var.bufferIndex], domainName);
                reqG->var.bufferIndex++;
            }
            pthread_mutex_destroy(&reqG->mut.lock[i]);
        }
    }
    return 0;
}
//--------------------------------------------------------------------------------------------------
void *resolver_entryFunction(void *thd){
    struct globals *reqG = thd;

    if(!pthread_mutex_trylock(&reqG->mut.sharedBufferLock)) {
        for(int i = 0; i < reqG->var.bufferIndex; i++) {
            char domain[20]; //empty domain string
            char ip[20]; //empty IP string
            strcpy(domain, reqG->var.sharedBuffer[i]);
            printf("%s, ", domain); //print domain
            int flag = 0; //for invalid hostnames
            if (dnslookup(domain, ip, sizeof(ip))) {
                fprintf(stderr, "INVALID HOSTNAME: %s\n", domain);
                flag  = 1;
                printf("\n");
            } 
            else {
                printf("%s\n", ip); //print IP
            }

            pthread_mutex_lock(&reqG->mut.resultsLock);
                FILE *f = fopen(reqG->var.txtResults, "a"); //append data to file
                if (f == NULL) {
                    printf("Error: invalid output file!\n");
                    exit(1);
                }
                /* add some text */
                if (!flag) {
                    fprintf(f, "%s, %s\n", domain, ip);
                }
                else {
                    fprintf(f, "%s,\n", domain);
                }
                fclose(f);
            pthread_mutex_unlock(&reqG->mut.resultsLock);
        }
    pthread_mutex_destroy(&reqG->mut.sharedBufferLock);
    }
}
//--------------------------------------------------------------------------------------------------
int main (int argc, char *argv[]) {
    clock_t start,end;
    start = clock();

    struct globals gbls;
    gbls.var.bufferIndex = 0;

    //serviced file
    strcpy(gbls.var.txtServiced, argv[4]);
    // results file
    strcpy(gbls.var.txtResults, argv[3]);
    //requester threads 
    int numb_req_threads = strtol(argv[1], NULL, 10); //number of request threads
    pthread_t rTID[numb_req_threads];
    struct thread_Data thread_data_array[numb_req_threads];
    gbls.array = thread_data_array;
    
    //resolver threads
    int numb_resolver_threads = strtol(argv[2],NULL,10); //number of resolver threads
    pthread_t resolverThreads[numb_resolver_threads];
    char f[20][20];
    int fileCount = argc-5;
    gbls.var.GLOBALFILECOUNT = fileCount;
    for(int i = 0; i < fileCount; i++){
        strcpy(f[i], argv[i+5]);     
    }
    
    //get files to be serviced
    char inputFiles[20][20];

    //populate thread data and global files to service 
    for (int i = 0; i <= numb_req_threads; i++){
        thread_data_array[i].ptr_to_fileArray = &f;
    }
    
    //create requester threads
    for (int i = 0; i < numb_req_threads; i++){
        pthread_create(&rTID[i], NULL, requester_entryFunction, &gbls);
    } 
    //join requester threads
    for (int i = 0; i < numb_req_threads; i++){
        pthread_join(rTID[i],NULL);
    }

    //create resolver threads
    for (int i = 0; i < numb_resolver_threads; i++){
        pthread_create(&resolverThreads[i], NULL, resolver_entryFunction, &gbls);
    }
    //join resolver threads
    for (int i =0; i< numb_resolver_threads; i++){
        pthread_join(resolverThreads[i],NULL);
    }

    end = clock();
    printf("\nTime elapsed: %d\n", end-start);
}
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------