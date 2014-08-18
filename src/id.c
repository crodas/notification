#include "notifd.h"

static uint64 process_id = 0;
static uint64 counter = 0;

/**
 *  Generate Unique ID that is used for each message,
 *  it was inspired by MongoID's
 */
void get_unique_id(char * uuid)
{
    struct timeval tp;
    uint64 time = (uint64) now();
    uint64 rnd0;

    gettimeofday(&tp, NULL);
    srand(tp.tv_usec);

    if (!process_id) {
        process_id = random() & 0xFFFFFF;
    }

    if (!counter || counter == 0xFFFFFE) {
        counter = random() & 0xFFFFFF;
    }

    sprintf(uuid,  "%08llx%06llx%04llx%06llx", 
        time & 0xFFFFFFFF, process_id, random() & 0xFFFF, ++counter);
}
