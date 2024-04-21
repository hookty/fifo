#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

#include "eventbuf.h"

struct eventbuf *eb;

sem_t *items;
sem_t *spaces;
sem_t *mutex;

int quitting_time = 0;

sem_t *sem_open_temp(const char *name, int value)
{
    sem_t *sem;

    // Create the semaphore
    if ((sem = sem_open(name, O_CREAT, 0600, value)) == SEM_FAILED)
        return SEM_FAILED;

    // Unlink it so it will go away after this process exits
    if (sem_unlink(name) == -1)
    {
        sem_close(sem);
        return SEM_FAILED;
    }

    return sem;
}

typedef struct
{
    int id;
    int events;
} producer_data;

void *producer(void *arg)
{
    producer_data *data = (producer_data *)arg;
    int id = data->id;
    int events = data->events;
    for (int i = 0; i < events; i++)
    {
        sem_wait(spaces);
        sem_wait(mutex);

        int event = id * 100 + i;
        printf("P%d: adding event %d\n", id, event);
        eventbuf_add(eb, event);

        sem_post(mutex);
        sem_post(items);
    }
    printf("P%d: exiting\n", id);
    return NULL;
}

void *consumer(void *arg)
{
    int id = *((int *)arg);
    while (1)
    {
        sem_wait(items);
        sem_wait(mutex);

        if (quitting_time || eventbuf_empty(eb))
        {
            sem_post(mutex);
            sem_post(items);
            break;
        }

        int event = eventbuf_get(eb);
        printf("C%d: got event %d\n", id, event);

        sem_post(mutex);
        sem_post(spaces);
    }
    printf("C%d: exiting\n", id);
    return NULL;
}

int main(int argc, char *argv[])
{

    if (argc != 5)
    {
        printf("Usage: %s <producers> <consumers> <events> <buffer_size>\n", argv[0]);
        return 1;
    }

    int producers = atoi(argv[1]);
    int consumers = atoi(argv[2]);
    int events = atoi(argv[3]);
    int buffer_size = atoi(argv[4]);

    eb = eventbuf_create();

    items = sem_open_temp("/items", 0);
    spaces = sem_open_temp("/spaces", buffer_size);
    mutex = sem_open_temp("/mutex", 1);

    if (items == SEM_FAILED || spaces == SEM_FAILED || mutex == SEM_FAILED)
    {
        perror("sem_open");
        return 1;
    }

    pthread_t producer_threads[producers];
    pthread_t consumer_threads[consumers];

    int consumer_ids[consumers];

    for (int i = 0; i < producers; i++)
    {
        producer_data *data = malloc(sizeof(producer_data));
        data->id = i;
        data->events = events;
        pthread_create(&producer_threads[i], NULL, producer, data);
    }

    for (int i = 0; i < consumers; i++)
    {
        consumer_ids[i] = i;
        pthread_create(&consumer_threads[i], NULL, consumer, &consumer_ids[i]);
    }

    for (int i = 0; i < producers; i++)
    {
        pthread_join(producer_threads[i], NULL);
    }

    quitting_time = 1;

    for (int i = 0; i < consumers; i++)
    {
        sem_post(items); // wake up each consumer
    }

    for (int i = 0; i < consumers; i++)
    {
        pthread_join(consumer_threads[i], NULL);
    }

    eventbuf_free(eb);

    sem_close(items);

    sem_close(spaces);

    sem_close(mutex);

    return 0;
}