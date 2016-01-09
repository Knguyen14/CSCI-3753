/*
 * File: multilookup.c
 * Project: CSCI 3753 PA2
 * Created Date: 2015/10/15
 * Threaded solution
 */

#include "multi-lookup.h"

pthread_cond_t full;
pthread_cond_t empty;


queue q;
int FILES_FINISHED;
int NUM_INPUT_FILES;
char* OUT_FILE;
int THREAD_MAX;

pthread_mutex_t queue_lock;
pthread_mutex_t inc_lock;
pthread_mutex_t out_lock;

void* read_file(char* filename){
    FILE* input = fopen(filename, "r");

    if(!input){
        perror("Error opening input file.\n");
        return NULL;
    }
    char hostname[SBUFSIZE];

    // Push hostname to queue while locked
    while(fscanf(input, INPUTFS, hostname) > 0){
        pthread_mutex_lock(&queue_lock);
        while(queue_is_full(&q)){
            pthread_cond_wait(&full, &queue_lock);
        }

        queue_push(&q, strdup(hostname));

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&queue_lock);
    }

    // Increment counter
    pthread_mutex_lock(&inc_lock);
    FILES_FINISHED++;
    pthread_mutex_unlock(&inc_lock);

    // Close file and return
    fclose(input);
    return NULL;
}

void* producer_pool(char* input_files){
    // pointer stuff
    char** input_file_names = (char **) input_files;
    pthread_t producer_threads[NUM_INPUT_FILES];
    fflush(stdout);

    int i;
    for (i=0 ; i < NUM_INPUT_FILES ; i++){

        char* input_file_name = input_file_names[i];
        pthread_create(&producer_threads[i], NULL, (void*) read_file, (void*) input_file_name);
        pthread_join(producer_threads[i], NULL);
    }

    for(i=0 ; i < NUM_INPUT_FILES ; i++){
        pthread_cond_signal(&empty);
    }

    return NULL;
}

void* resolve_dns(){
    FILE* out_fp =fopen(OUT_FILE, "w");
    if(!out_fp){
        perror("Error opening output file");
	return NULL;
    }

    while(1){
        pthread_mutex_lock(&queue_lock);

        // check if queue is empty
        while(queue_is_empty(&q)){
            pthread_mutex_lock(&inc_lock);
            int que_empty = 0;
            if(FILES_FINISHED == NUM_INPUT_FILES) que_empty = 1;
            pthread_mutex_unlock(&inc_lock);

            if (que_empty){
                pthread_mutex_unlock(&queue_lock);
                return NULL;

            }

            pthread_cond_wait(&empty, &queue_lock);

        }
        char* single_hostname = (char*) queue_pop(&q);
        pthread_cond_signal(&full);


        char first_ip[INET6_ADDRSTRLEN];

        if(dnslookup(single_hostname, first_ip, sizeof(first_ip)) == UTIL_FAILURE){
            fprintf(stderr, "DNS lookup error hostname: %s\n", single_hostname);
            strncpy(first_ip, "", sizeof(first_ip));
        }

        pthread_mutex_lock(&out_lock);
        fprintf(out_fp, "%s, %s\n", single_hostname, first_ip);
        pthread_mutex_unlock(&out_lock);

	free(single_hostname);

        pthread_mutex_unlock(&queue_lock);
    }
    fclose(out_fp);
    return NULL;
}

void* consumer_pool(){
    // Creates threads for resolver
    pthread_t consumer_threads[NUM_INPUT_FILES];
    int i;
    for (i=0; i < THREAD_MAX ; i++){
        pthread_create(&consumer_threads[i], NULL, resolve_dns, NULL);
        pthread_join(consumer_threads[i], NULL);
    }
    return NULL;
}

int main(int argc, char* argv[]){
    FILES_FINISHED = 0;
    NUM_INPUT_FILES = argc - 2;
    char* input_files[NUM_INPUT_FILES];
    OUT_FILE = argv[argc-1];
    THREAD_MAX = sysconf(_SC_NPROCESSORS_ONLN);
    printf("Resolving threads dynamically, set to %d\n",THREAD_MAX);

    fflush(stdout);

    queue_init(&q, 16);
    pthread_cond_init(&empty, NULL);
    pthread_cond_init(&full, NULL);
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&inc_lock, NULL);
    pthread_mutex_init(&out_lock, NULL);

    if(argc < MINARGS){
        fprintf(stderr, "Not enough arguments: %d\n", (argc - 1));
        fprintf(stderr, "Using:\n %s %s\n", argv[0], USAGE);
        return EXIT_FAILURE;
    }

    // input array
    int i;
    for (i=0 ; i < NUM_INPUT_FILES ; i++){
        input_files[i] = argv[i+1];
    }


    pthread_t producer_id, consumer_id;

    int producer = pthread_create(&producer_id, NULL, (void*) producer_pool, input_files);
    int consumer = pthread_create(&consumer_id, NULL, (void*) consumer_pool, NULL);

    if(producer || consumer) printf("error\n");
    pthread_join(consumer_id, NULL);
    pthread_join(producer_id, NULL);

    queue_cleanup(&q);
    pthread_mutex_destroy(&out_lock);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&inc_lock);

    return EXIT_SUCCESS;
}