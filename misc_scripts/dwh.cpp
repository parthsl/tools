/*
    dwh is a test program to run a task with large (or small) numbers of
    pthreads, including worker and dedicated I/O threads. It's purpose
    is to generate heavy workloads for a single task. Combining memory
    alloc/read/write/free, CPU arithmetic and lock controlled random file
    I/O. Multiple instances can be run at the same time on the same host.
    I/O is with shared lock reads and exclusive lock writes.

    Compile example on linux:

        g++ -O3 -D_FILE_OFFSET_BITS=64 -pthread -o dwh dwh.cpp

    Then the output file dwh can be run with --help for information.
    
    Example usage:
    
        ./dwh --minthreads 500 --maxthreads 575 --iothreads 50 --maxmem 1G
            --maxiosize 1M --shortthreads --time 200 dwh.test
    
    Will run dwh with the following configuration:
    
    Setup at least 500 threads (--minthreads)
    Limit total threads to no more than 575 at any time (--maxthreads)
        If no value is specified for --maxthreads the default is the
        --minthreads value
    Use 50 threads for dedicated I/O operations (--iothreads)
        These are included in the instantaneous thread count limit of
        --maxthreads
    Don't use more than 1GiB of memory for processing (--maxmem)
    Perform test I/O using blocks no larger than 1MiB (--maxiosize)
        Consider that the sum of memory used by all threads is the
        limit imposed by --maxmem. Threads only use one block at a time
        so it has to be possible for the number of threads multiplied by
        the --maxiosize value to be less than or equal to the --maxmem
        value at all times.
    Allow worker threads to end and new ones to be created (--shortthreads)
        At any instant the absolute thread count may be less than minthreads
        Without this or by using --longthreads all started threads run until
        program exit (or until they fail on their own)
    Run the test case for 200 seconds (--time)
        Initialization time is not included and may take several seconds
        There is no live output when the test is running and succeeding
    The last argument shown is the file to use for performing the I/O
        Don't run multiple instances with the same test file, each instance
        creates an empty file and pre-populates it and the locking is per-
        task only.

    Also:

    Note that sending a SIGUSR1 to a running instance will cause it to end
    on demand and clean-up as-in a normal exit, e.g.:

    ps -A | grep dwh
    14727 ?        00:00:00 dwh
    kill -USR1 1427

    Sending a SIGKILL will cause it to be terminated without cleanup. That
    should only leave the test file undeleted with no other waste.

    It should also respond to Ctrl-C to exit if it is not completing the
    process.

*/

#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <cstdlib>
#include <string>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <atomic>
#include <time.h>

bool Diagnose = false;

/* Global "switch" to make all threads end themselves */
bool EndAllThreads = false;

/* Global "switch" to stop I/O being queued (but letting pending cases finish) */
bool EndAllIO = false;

/* Global number of active pthreads */
std::atomic<int> nThreads(0);
std::atomic<int> nIOThreads(0);
std::atomic<int> nIOTasks(0);
std::atomic<int> nQueuedIOTasks(0);
std::atomic<int> nTriedIOTasks(0);
std::atomic<int> nPeakThreads(0);
std::atomic<int> nTotalThreads(0);
std::atomic<int> threadNum(0);

/* Global thread info */
struct thread_info *iotinfo = NULL;
struct thread_info *wktinfo = NULL;

/* Global access control for wktinfo */
pthread_mutex_t wktilock;

/* Global thread limits */
unsigned int maxthreads = 0;
unsigned int minthreads = 0;
unsigned int iothreads = 0;

/* Global work done info */
std::atomic<unsigned long long> memUsed(0);
std::atomic<unsigned long long> totalRead(0);
std::atomic<unsigned long long> totalWrite(0);
std::atomic<unsigned long long> totalTriedIORead(0);
std::atomic<unsigned long long> totalTriedIOWrite(0);
std::atomic<unsigned long long> totalIORead(0);
std::atomic<unsigned long long> totalIOWrite(0);
std::atomic<unsigned long long> pendingIOReads(0);
std::atomic<unsigned long long> pendingIOWrites(0);
std::atomic<unsigned long long> pendingIODone(0);

/* Global runtime limit (seconds) */
unsigned int maxruntime = 20;

/* Global memory limits */
unsigned long long maxMem = 0;
unsigned long long maxIOSize = 1 * 1024 * 1024;

/* Global signal mask */
static  sigset_t sigmask;
std::atomic<int> sigStop(0);
std::atomic<int> nSigMaskSets(0);

/* Did we show help */
bool helpShown = false;

/* Flag set by ‘--verbose’ (assume not verbose) */
static int verbose_flag = 0;

/* Flag set by '--shortthreads' (assume threads end and new ones replace them) */
static int short_threads = 1;
#define RESTART_SCOPE (RAND_MAX / 800)

/* I/O filename/stream */
unsigned long long fileSize = 0;
std::string ioFilename;
FILE *ioReadStream = NULL;
FILE *ioWriteStream = NULL;

/* Global access control for read/write queues */
pthread_mutex_t iordlock;
pthread_mutex_t iowrlock;
pthread_mutex_t iodnlock;
pthread_mutex_t iorsklock;
pthread_mutex_t iowsklock;

/* Flags for initialized objects */
#define INIT_OBJ_NONE 0
#define INIT_WKTILOCK 1
#define INIT_IORDLOCK 2
#define INIT_IOWRLOCK 4
#define INIT_IODNLOCK 8
#define INIT_IORSKLOCK 16
#define INIT_IOWSKLOCK 32
unsigned int initObjects = INIT_OBJ_NONE;

struct option long_options[] = {
        {"verbose", no_argument, &verbose_flag, 1},
        {"brief", no_argument, &verbose_flag, 0},
        {"shortthreads", no_argument, &short_threads, 1},
        {"longthreads", no_argument, &short_threads, 0},
        {"help", no_argument, 0, 'h'},
        {"iothreads", required_argument, 0, 'i'},
        {"maxmem", required_argument, 0, 'm'},
        {"minthreads", required_argument, 0, 'n'},
        {"maxthreads", required_argument, 0, 'x'},
        {"maxiosize", required_argument, 0, 'S'},
        {"time", required_argument, 0, 't'},
        {0, 0, 0, 0}
    };

struct thread_info {    /* Used as argument to thread_start() */
           pthread_t thread_id;        /* ID returned by pthread_create() */
           int       thread_num;       /* Application-defined thread # */
           sem_t     my_sem;           /* Application-defined semaphore */
           int       my_fd;            /* Thread specific file descriptor */
           char     *argv_string;      /* From command-line argument */
    };

struct io_queue_node {
        struct io_queue_node *prev;
        struct io_queue_node *next;
        sem_t  *my_sem;           /* Application-defined semaphore */
        int     my_fd;            /* Thread specific file descriptor */
        void *io_buffer;
        unsigned int io_len;
        unsigned int io_done;
    };

struct io_queue_node *io_readQHead = NULL;
struct io_queue_node *io_readQTail = NULL;
struct io_queue_node *io_writeQHead = NULL;
struct io_queue_node *io_writeQTail = NULL;
struct io_queue_node *io_doneQHead = NULL;
struct io_queue_node *io_doneQTail = NULL;

void ShowHelp(void)
{
    /* Help must exist alone */
    if (minthreads || maxthreads || iothreads || verbose_flag)
    {
        printf("-h/--help must be the only argument\n");
        return;
    }

    putchar('\n');
    printf("Usage: dwh --minthreads <num> [OPTION]... file\n");
    printf("Attempt to simulate high thread count single task asynchronous I/O task.\n");
    putchar('\n');
    printf("  -n, --minthreads <num>  Set minimum number of threads to use (Mandatory)\n");
    printf("  -x, --maxthreads <num>  Set maximum number of threads to use (defaults to min)\n");
    printf("  -i, --iothreads <num>   Set number of dedicated I/O threads to use (default 0)\n");
    printf("      --shortthreads      Let threads exit and new ones start\n");
    printf("      --longthreads       All threads run to program exit\n");
    printf("  -m, --maxmem <num>      Set a maximum amount of memory to use\n");
    printf("  -S, --maxiosize <num>   Set a maximum memory to use for I/O tasks (default 1M)\n");
    printf("  -t, --time <num>        How long (seconds) program should run for (default 20)\n");
    printf("      --verbose           Show more information while running\n");
    printf("      --brief             Show limited information while running\n");
    printf("  --help                  Show program information\n");
    putchar('\n');
    printf("If iothreads is not specified or zero is used then I/O is not performed.\n");
    putchar('\n');
    helpShown = true;
}

void *CountingCalloc(size_t nmem, size_t size)
{
    unsigned long long sz;
    void *mem;
    unsigned long long *pSz;

    sz = nmem * size;
    memUsed += sz;
    if ((maxMem > 0) && (memUsed > maxMem))
    {
        memUsed -= sz;
        return NULL;
    }
    mem = calloc(1, sz + sizeof(sz));
    if (mem == NULL)
    {
        memUsed -= sz;
        return NULL;
    }
    pSz = (unsigned long long *)mem;
    pSz[0] = sz;

    if (Diagnose)
        printf("Counting calloc of %p, size %llu, showing %p\n", mem, sz, &pSz[1]);

    return (void *)&pSz[1];
}

void CountingFree(void *mem)
{
    unsigned long long *pSz;

    if (Diagnose)
        printf("Counting free of %p\n", mem);
    if (mem != NULL)
    {
        pSz = (unsigned long long *)((unsigned long long)mem - sizeof(unsigned long long));
        if (Diagnose)
            printf("Accessing %p for size (%llu)\n", pSz, pSz[0]);
        memUsed -= pSz[0];
        free(pSz);
    }
}

int getActivity(unsigned nActivities, int noise)
{
    unsigned s;
    time_t tv;
    
    tv = time(NULL);
    if (tv == ((time_t)-1))
    {
        return -1;
    }
    s = (unsigned)noise + (unsigned)tv;
    srand(s);

    return rand() % nActivities;
}

unsigned int strtoui(const char *s)
{
    unsigned long lresult = std::stoul(s, 0, 10);
    unsigned int result = lresult;

    if (result != lresult)
        result = 0;

    return result;
}

unsigned long long memsztoull(char *s)
{
    int ln;
    char chMult;
    unsigned long long mult = 1;
    unsigned long long result = 0;

    /* We can have one trailing character providing a multiplier */
    if (s)
    {
        ln = strlen(s);
        if (ln > 1)
        {
            chMult = s[ln - 1];
            switch (chMult)
            {
                case 'k':
                    mult = 1024;
                    s[ln - 1] = '\0';
                    break;

                case 'M':
                    mult = 1024 * 1024;
                    s[ln - 1] = '\0';
                    break;

                case 'G':
                    mult = 1024 * 1024 * 1024;
                    s[ln - 1] = '\0';
                    break;

                default:
                    break;
            }
        }

        result = strtoull(s, 0, 10);
        result *= mult;
    }

    return result;
}

bool doubleToScale(double *x, char *scale)
{
    bool result = true;
    double orig;

    if ((x == NULL) || (scale == NULL))
        return false;

    orig = *x;
    *scale = 0;
    while ((*x >= 1024.0) && (*scale != 'T') && result)
    {
        *x /= 1024.0;
        switch(*scale)
        {
            case 0:
                *scale = 'k';
                break;

            case 'k':
                *scale = 'M';
                break;

            case 'M':
                *scale = 'G';
                break;

            case 'G':
                *scale = 'T';
                break;

            default:
                *x = orig;
                *scale = 0;
                result = false;
                break;
        }
    }
    
    return result;
}

bool VerifySettings()
{
    /* We must have threads */
    if (minthreads == 0)
    {
        printf("Not enough threads (%d), use --minthreads <num>\n", minthreads);
        return false;
    }
    /* Max threads can't be lower than min threads */
    if (minthreads > maxthreads)
    {
        printf("Max threads (%d) must be greater than or equal to Min threads (%d), use --maxthreads <num>\n", 
               maxthreads, minthreads);
        return false;
    }
    /* I/O threads come from all threads and must leave some */
    if (iothreads >= minthreads)
    {
        printf("I/O threads (%d) must be less than Min Threads (%d), use --iothreads <num>\n", 
               iothreads, minthreads);
        return false;
    }
    /* Time must be greater than zero */
    if (maxruntime < 1)
    {
        printf("Runtime (--time <num>) must be greater than zero (%u)\n", 
               maxruntime);
        return false;
    }
    /* There must be an I/O filename target */
    if (ioFilename.length() < 1)
    {
        printf("No I/O filename specified\n");
        return false;
    }
    /* Max I/O Size must fit inside Max Memory */
    if ((maxMem != 0) && (maxIOSize > maxMem))
    {
        printf("Maximum I/O size (%llu) must be no more than Maximum memory (%llu)\n", 
               maxIOSize, maxMem);
        return false;
    }
    /* Max I/O size has to be less than 2GB */
    if (maxIOSize > (unsigned long long)0x7FFFFFFF)
    {
        printf("Maximum I/O size (%llu) must be less than 2GB\n", 
               maxIOSize);
        return false;
    }
    
    return true;
}

bool ParseArgs(int argc, char *argv[])
{
    int c;
    int option_index = 0;

    while (1) {
        c = getopt_long(argc, argv, "i:x:n:m:h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0)
                    break;
                if (verbose_flag)
                {
                    printf ("option %s", long_options[option_index].name);
                    if (optarg)
                        printf (" with arg %s", optarg);
                    printf ("\n");
                }
                break;

            case 'h':
                if (verbose_flag)
                    printf ("option -h\n", optarg);
                ShowHelp();
                return false;

            case 'i':
                if (verbose_flag)
                    printf ("option -i with value `%s'\n", optarg);
                iothreads = strtoui(optarg);
                break;

            case 'm':
                if (verbose_flag)
                    printf ("option -m with value `%s'\n", optarg);
                maxMem = memsztoull(optarg);
                break;

            case 'n':
                if (verbose_flag)
                    printf ("option -n with value `%s'\n", optarg);
                minthreads = strtoui(optarg);
                if (maxthreads == 0)
                    maxthreads = minthreads;
                break;

            case 't':
                if (verbose_flag)
                    printf("option -t with value `%s'\n", optarg);
                maxruntime = strtoui(optarg);
                break;

            case 'x':
                if (verbose_flag)
                    printf ("option -x with value `%s'\n", optarg);
                maxthreads = strtoui(optarg);
                break;

            case 'S':
                if (verbose_flag)
                    printf ("option -S with value `%s'\n", optarg);
                maxIOSize = memsztoull(optarg);
                break;

            default:
                return false;
        }
    }
    if (verbose_flag)
    {
        /* Instead of reporting ‘--verbose’ and ‘--brief’ as they are encountered,
        we report the final status resulting from them. */
        if (verbose_flag)
            puts ("verbose flag is set");
        else
            puts ("verbose flag is not set");
    }

    if (verbose_flag)
    {
        /* Instead of reporting ‘--shortthreads’ and ‘--longthreads as they are encountered,
        we report the final status resulting from them. */
        if (short_threads)
            puts ("Worker threads start and stop while program runs");
        else
            puts ("Worker threads are started at launch and left running");
    }

    /* Any remaining arguments */
    if (optind < argc)
    {
        ioFilename = argv[optind];
        if (verbose_flag)
        {
            printf ("non-option ARGV-elements: ");
            while (optind < argc)
                printf ("%s ", argv[optind++]);
            putchar ('\n');
        }
    }

    putchar('\n');
    printf(" Max Threads: %u\n", maxthreads);
    printf(" Min Threads: %u\n", minthreads);
    printf(" I/O Threads: %u\n", iothreads);
    putchar ('\n');
    printf("     Run for: %u s\n", maxruntime);
    putchar('\n');
    printf("  Max Memory: %llu", maxMem);
    if (maxMem == 0)
        puts(" (No limit)");
    putchar('\n');
    printf("Max I/O size: %llu\n", maxIOSize);
    printf("I/O file: %s\n", ioFilename.c_str());
    putchar('\n');

    /* Get Started */
    
    return VerifySettings();
}

bool setupSyncObjects(void)
{
    if (pthread_mutex_init(&wktilock, NULL) != 0)
    {
        printf("wktinfo mutex setup failed\n");
        return false;
    }
    initObjects |= INIT_WKTILOCK;

    if (pthread_mutex_init(&iordlock, NULL) != 0)
    {
        printf("I/O read mutex setup failed\n");
        return false;
    }
    initObjects |= INIT_IORDLOCK;

    if (pthread_mutex_init(&iowrlock, NULL) != 0)
    {
        printf("I/O write mutex setup failed\n");
        return false;
    }
    initObjects |= INIT_IOWRLOCK;

    if (pthread_mutex_init(&iodnlock, NULL) != 0)
    {
        printf("I/O done mutex setup failed\n");
        return false;
    }
    initObjects |= INIT_IODNLOCK;

    if (pthread_mutex_init(&iorsklock, NULL) != 0)
    {
        printf("I/O seek/read mutex setup failed\n");
        return false;
    }
    initObjects |= INIT_IORSKLOCK;

    if (pthread_mutex_init(&iowsklock, NULL) != 0)
    {
        printf("I/O seek/write mutex setup failed\n");
        return false;
    }
    initObjects |= INIT_IOWSKLOCK;

    return true;
}

bool destroySyncObjects(void)
{
    if (initObjects & INIT_IOWSKLOCK)
    {
        if (pthread_mutex_destroy(&iowsklock) != 0)
        {
            printf("I/O seek/write mutex destroy failed\n");
            return false;
        }
        initObjects ^= INIT_IOWSKLOCK;
    }

    if (initObjects & INIT_IORSKLOCK)
    {
        if (pthread_mutex_destroy(&iorsklock) != 0)
        {
            printf("I/O seek/read mutex destroy failed\n");
            return false;
        }
        initObjects ^= INIT_IORSKLOCK;
    }

    if (initObjects & INIT_IODNLOCK)
    {
        if (pthread_mutex_destroy(&iodnlock) != 0)
        {
            printf("I/O done mutex destroy failed\n");
            return false;
        }
        initObjects ^= INIT_IODNLOCK;
    }

    if (initObjects & INIT_IOWRLOCK)
    {
        if (pthread_mutex_destroy(&iowrlock) != 0)
        {
            printf("I/O write mutex destroy failed\n");
            return false;
        }
        initObjects ^= INIT_IOWRLOCK;
    }

    if (initObjects & INIT_IORDLOCK)
    {
        if (pthread_mutex_destroy(&iordlock) != 0)
        {
            printf("I/O read mutex destroy failed\n");
            return false;
        }
        initObjects ^= INIT_IORDLOCK;
    }

    if (initObjects & INIT_WKTILOCK)
    {
        if (pthread_mutex_destroy(&wktilock) != 0)
        {
            printf("wktinfo mutex destroy failed\n");
            return false;
        }
        initObjects ^= INIT_WKTILOCK;
    }

    return true;
}

bool setupDataFile(void)
{
    size_t s;
    unsigned long long writeSize;
    unsigned long long curSize;
    unsigned char *fillData = NULL;

    if ((ioReadStream == NULL) && (ioWriteStream == NULL))
    {
        printf("Pre-populating I/O file %s\n (This may take a short time)\n", ioFilename.c_str());
        if (ioFilename.length() == 0)
            return false;

        ioWriteStream = fopen(ioFilename.c_str(), "w+");
        if (ioWriteStream == NULL)
            return false;

        ioReadStream = fopen(ioFilename.c_str(), "r");
        if (ioReadStream == NULL)
            return false;

        /* Initialize the file to twice the memory limit or 6G if no limit */
        fileSize = maxMem;
        if (fileSize == 0)
        {
            fileSize = 1024 * 1024;
            fileSize *= 1024;
            fileSize *= 6;
        }
        else
        {
            fileSize *= 2;
        }
        writeSize = 1024 * 1024;
        writeSize *= 1024;
        if (writeSize > fileSize)
            writeSize = fileSize;
        fillData = (unsigned char *)calloc(1, writeSize);
        if (fillData == NULL)
        {
            fileSize = 0;
            return false;
        }

        curSize = 0;
        while (curSize <= fileSize)
        {
            s = fwrite(fillData, writeSize, 1, ioWriteStream);
            if (s < 1)
            {
                fileSize = 0;
                free(fillData);
                fillData = NULL;
                return false;
            }

            curSize += writeSize;
        }
    }
    if (fillData != NULL)
    {
        free(fillData);
        fillData = NULL;
    }

    return true;
}

bool cleanupDataFile(void)
{
    bool result = true;

    while (nThreads > 0)
    {
        sleep(1);
    }
    if (ioReadStream != NULL)
    {
        if (fclose(ioReadStream) != 0)
            if (result)
                result = false;
    }
    if (ioWriteStream != NULL)
    {
        if (fclose(ioWriteStream) != 0)
            if (result)
                result = false;
        if (remove(ioFilename.c_str()) != 0)
            if (result)
                result = false;
    }

    return result;
}

void EndPThreads(void)
{
    if (Diagnose)
        printf("Ending all threads (%d)\n", nThreads.load(std::memory_order_relaxed));
    EndAllThreads = true;
    do
    {
        sleep(1);
    } while (nThreads.load(std::memory_order_relaxed) > 0);

    CountingFree(iotinfo);
    iotinfo = NULL;
    CountingFree(wktinfo);
    wktinfo = NULL;
}

bool readQLock()
{
    if (pthread_mutex_lock(&iordlock) == 0)
    {
        return true;
    }

    return false;
}

bool readQUnlock()
{
    if (pthread_mutex_unlock(&iordlock) == 0)
    {
        return true;
    }

    return false;
}

bool isReadQEmpty(void)
{
    bool result = false;

    if (readQLock())
    {
        result = (io_readQHead != NULL);
        if (!readQUnlock())
        {
            printf("Failed to exit I/O read lock critical section (isReadQEmpty), exiting\n");
            EndAllThreads = true;
        }
    }

    return result;
}

io_queue_node *getIOReadNode(void)
{
    io_queue_node *result = NULL;

    if (readQLock())
    {
        if (Diagnose)
        {
            printf("read queue LOCKED for get\n");
            printf("Read queue %p/%p\n", io_readQHead, io_readQTail);
        }
        if (io_readQHead != NULL)
        {
            if (Diagnose)
                printf("Read queue POPULATED\n");
            result = io_readQHead;
            io_readQHead = result->next;
            if (io_readQHead != NULL)
            {
                io_readQHead->prev = NULL;
            }
            else
            {
                io_readQTail = NULL;
            }
            result->prev = NULL;
            result->next = NULL;
            pendingIOReads--;
        }
        else
        {
            if (Diagnose)
                printf("Read queue EMPTY\n");
        }

        if (!readQUnlock())
        {
            printf("Failed to exit I/O read lock critical section, exiting\n");
//            free(result);
//            result = NULL;
            EndAllThreads = true;
        }
        else
        {
            if (Diagnose)
                printf("read queue UNLOCKED get\n");
        }
    }
    else
    {
        if (Diagnose)
            printf("Failed to lock read queue\n");        
    }

    return result;
}

bool queueIORead(io_queue_node *node)
{
    bool result = false;
    io_queue_node *prev;

    /* Process if I/O not finished yet */
    if (!EndAllIO)
    {
        if (node != NULL)
        {
            if (Diagnose)
                printf("Queueing a READ\n");
            if (readQLock())
            {
                if (Diagnose)
                    printf("Read queue LOCKED\n");
                if (io_readQHead != NULL)
                {
                    if (Diagnose)
                        printf("Adding node to POPULATED queue\n");
                    node->prev = io_readQTail;
                    node->next = NULL;
                    io_readQTail = node;
                    node->prev->next = node;
                }
                else
                {
                    if (Diagnose)
                        printf("Adding node to EMPTY queue\n");
                    node->prev = NULL;
                    node->next = NULL;
                    io_readQHead = node;
                    io_readQTail = node;
                }
                pendingIOReads++;
                if (Diagnose)
                    printf("Read queue %p/%p\n", io_readQHead, io_readQTail);

                if (readQUnlock())
                {
                    if (Diagnose)
                        printf("Read queue UNLOCKED\n");
                    result = true;
                }
                else
                {
                    printf("Failed to exit I/O read lock critical section, exiting\n");
                    EndAllThreads = true;
                }
            }
        }
        else
        {
            if (Diagnose)
                printf("Attempt to queue NULL for read\n");
        }
    }

    return result;
}

void clearIOReadQ(void)
{
    io_queue_node *node;

    if (readQLock())
    {
        while (io_readQHead != NULL)
        {
            node = io_readQHead;
            io_readQHead = node->next;
            if (io_readQHead != NULL)
            {
                io_readQHead->prev = NULL;
            }
            else
            {
                io_readQTail = NULL;
            }
            free(node);
            node = NULL;
            pendingIOReads--;
        }
        io_readQHead = NULL;
        io_readQTail = NULL;

        if (!readQUnlock())
        {
            printf("Failed to exit I/O read lock critical section, exiting\n");
            EndAllThreads = true;
        }
    }
}

bool writeQLock()
{
    if (pthread_mutex_lock(&iowrlock) == 0)
    {
        return true;
    }

    return false;
}

bool writeQUnlock()
{
    if (pthread_mutex_unlock(&iowrlock) == 0)
    {
        return true;
    }

    return false;
}

bool isWriteQEmpty(void)
{
    bool result = false;

    if (writeQLock())
    {
        result = (io_writeQHead != NULL);
        if (!writeQUnlock())
        {
            printf("Failed to exit I/O read lock critical section (isWriteQEmpty), exiting\n");
            EndAllThreads = true;
        }
    }

    return result;
}

io_queue_node *getIOWriteNode()
{
    io_queue_node *result = NULL;

    if (writeQLock())
    {
        if (io_writeQHead != NULL)
        {
            result = io_writeQHead;
            io_writeQHead = result->next;
            if (io_writeQHead != NULL)
            {
                io_writeQHead->prev = NULL;
            }
            result->prev = NULL;
            result->next = NULL;
            pendingIOWrites--;
        }

        if (!writeQUnlock())
        {
            printf("Failed to exit I/O write lock critical section, exiting\n");
            EndAllThreads = true;
        }
    }

    return result;
}

bool queueIOWrite(io_queue_node *node)
{
    bool result = false;
    io_queue_node *prev;

    /* Process if I/O not finished yet */
    if (!EndAllIO)
    {
        if (node != NULL)
        {
            if (Diagnose)
                printf("Queueing a WRITE\n");
            if (writeQLock())
            {
                if (Diagnose)
                    printf("Write queue LOCKED\n");
                if (io_writeQHead != NULL)
                {
                    if (Diagnose)
                        printf("Adding node to POPULATED queue\n");
                    node->prev = io_writeQTail;
                    node->next = NULL;
                    io_writeQTail = node;
                    node->prev->next = node;
                }
                else
                {
                    if (Diagnose)
                        printf("Adding node to EMPTY queue\n");
                    node->prev = NULL;
                    node->next = NULL;
                    io_writeQHead = node;
                    io_writeQTail = node;
                }
                pendingIOWrites++;

                if (writeQUnlock())
                {
                    if (Diagnose)
                        printf("Write queue UNLOCKED\n");
                    result = true;
                }
                else
                {
                    printf("Failed to exit I/O read lock critical section, exiting\n");
                    EndAllThreads = true;
                }
            }
        }
        else
        {
            if (Diagnose)
                printf("Attempt to queue NULL for read\n");
        }
    }

    return result;
}

void clearIOWriteQ(void)
{
    io_queue_node *node;

    if (writeQLock())
    {
        while (io_writeQHead != NULL)
        {
            node = io_writeQHead;
            io_writeQHead = node->next;
            if (io_writeQHead != NULL)
            {
                io_writeQHead->prev = NULL;
            }
            else
            {
                io_writeQTail = NULL;
            }
            free(node);
            node = NULL;
            pendingIOWrites--;
        }
        io_writeQHead = NULL;
        io_writeQTail = NULL;

        if (!writeQUnlock())
        {
            printf("Failed to exit I/O write lock critical section, exiting\n");
            EndAllThreads = true;
        }
    }
}

bool doneQLock()
{
    if (pthread_mutex_lock(&iodnlock) == 0)
    {
        return true;
    }

    return false;
}

bool doneQUnlock()
{
    if (pthread_mutex_unlock(&iodnlock) == 0)
    {
        return true;
    }

    return false;
}

bool isDoneQEmpty(void)
{
    bool result = false;

    if (doneQLock())
    {
        result = (io_doneQHead != NULL);
        if (!doneQUnlock())
        {
            printf("Failed to exit I/O done lock critical section (isDoneQEmpty), exiting\n");
            EndAllThreads = true;
        }
    }

    return result;
}

/* Assuming a node in the I/O done queue, remove it alone */
bool getIODoneNode(io_queue_node *node)
{
    bool result = false;

    if (doneQLock())
    {
        if ((node != NULL) && (io_doneQHead != NULL))
        {
            /* NB: Not checking which list it is on, just a list, caller is in control */
            if ((node->next != NULL) || (node->prev != NULL)
                    || ((io_doneQHead == node) && (io_doneQTail == node)))
            {
                /* Extract the node from the list */
                if (node->prev != NULL)
                    node->prev->next = node->next;
                else
                    io_doneQHead = node->next;
                if (node->next != NULL)
                    node->next->prev = node->prev;
                else
                    io_doneQTail = node->prev;
                
                node->prev = NULL;
                node->next = NULL;
                pendingIODone--;

                result = true;
            }
        }

        if (!doneQUnlock())
        {
            printf("Failed to exit I/O done lock critical section, exiting\n");
            EndAllThreads = true;
        }
    }

    return result;
}

bool queueIODone(io_queue_node *node)
{
    bool result = false;
    int ts;

    if (node != NULL)
    {
        if (doneQLock())
        {
            if (Diagnose)
                printf("I/O Done queue LOCKED\n");
            if (io_doneQHead != NULL)
            {
                if (Diagnose)
                    printf("Placing node on POPULATED done queue\n");
                node->prev = io_doneQTail;
                node->next = NULL;
                io_doneQTail = node;
                if (node->prev != NULL)
                    node->prev->next = node;
            }
            else
            {
                if (Diagnose)
                    printf("Placing node on EMPTY done queue\n");
                node->prev = NULL;
                node->next = NULL;
                io_doneQHead = node;
                io_doneQTail = node;
            }
            pendingIODone++;
            if (node->my_sem != NULL)
            {
                if (Diagnose)
                    printf("Signaling I/O waiter\n");
                ts = sem_post(node->my_sem);
                if (ts != 0)
                {
                    if (Diagnose)
                        printf("Failed to signal I/O waiter, exiting\n");
                    EndAllThreads = true;
                }
            }
            else
                if (Diagnose)
                    printf("No I/O waiter to signal\n");

            if (doneQUnlock())
            {
                if (Diagnose)
                    printf("I/O done queue UNLOCKED\n");
                result = true;
            }
            else
            {
                printf("Failed to leave I/O done lock critical section, exiting\n");
                EndAllThreads = true;
            }
        }
    }
    else
    {
        if (Diagnose)
            printf("Attempt to place NULL node on I/O done queue\n");
    }

    return result;
}

void clearIODoneQ(void)
{
    io_queue_node *node;

    if (doneQLock())
    {
        while (io_doneQHead != NULL)
        {
            node = io_doneQHead;
            io_doneQHead = node->next;
            if (io_doneQHead != NULL)
            {
                io_doneQHead->prev = NULL;
            }
            else
            {
                io_doneQTail = NULL;
            }
            free(node);
            node = NULL;
            pendingIODone--;
        }
        io_doneQHead = NULL;
        io_doneQTail = NULL;

        if (!doneQUnlock())
        {
            printf("Failed to end I/O done lock critical section, exiting\n");
            EndAllThreads = true;
        }
    }
}

/* If there is pending I/O, let it finish */
void EndIOTasks(void)
{
    /* Stop new I/O */
    EndAllIO = true;

    if (Diagnose)
    {
        putchar('\n');
        puts("I/O cleanup:");
    }

    /* 
     * Wait for I/O to end itself:
     * If there are remaining worker threads (to clear "done" I/O)
     * If there are read, write or done requests pending service
     */
    while ((nThreads > nIOThreads) && (!isReadQEmpty() || !isWriteQEmpty() || !isDoneQEmpty()))
    {
        /* Cancel if there is pending read or write but no I/O threads */
        if ((nIOThreads < 1) && (!isReadQEmpty() || !isWriteQEmpty()))
            break;

        sleep(1);
        if (Diagnose)
        {
            printf("   I/O threads = %d\n", nIOThreads.load(std::memory_order_relaxed));
            printf("       threads = %d\n", nThreads.load(std::memory_order_relaxed));
            if (!isReadQEmpty())
                printf("R/");
            if (!isWriteQEmpty())
                printf("W/");
            if (!isDoneQEmpty())
                printf("D");
            putchar('\n');
        }
    }
}

bool ioFileRead(io_queue_node *node)
{
    int fs = -2;
    int ts;
    size_t pos, rs = 0;
    size_t ra = -5;
    off64_t newPos;
    struct flock fLock;

    if ((ioReadStream == NULL) || (node == NULL))
        return false;
    if (node->my_fd == -1)
        return false;
    if ((node->io_buffer == NULL) || (node->io_len < 1))
        return false;

    pos = fileSize - node->io_len - 1;
    pos = (rand() * pos) / RAND_MAX;
    newPos = lseek64(node->my_fd, pos, SEEK_SET);
    if (newPos != (off_t)-1)
    {
        /* Lock our read region */
        fLock.l_type = F_RDLCK;
        fLock.l_whence = SEEK_SET;
        fLock.l_start = pos;
        fLock.l_len = node->io_len;
        fLock.l_pid = 0;
        fs = fcntl(node->my_fd, F_OFD_SETLKW, &fLock);
        if (fs != -1)
        {
            totalTriedIORead += node->io_len;
            rs = read(node->my_fd, node->io_buffer, node->io_len);
            ra = rs;
            if (ra != -1)
            {
                node->io_done = node->io_len;
            }
            else
            {
                ra = -1;
            }

            /* Unlock our read region */
            fLock.l_type = F_UNLCK;
            fLock.l_whence = SEEK_SET;
            fLock.l_start = pos;
            fLock.l_len = node->io_len;
            fLock.l_pid = 0;
            fs = fcntl(node->my_fd, F_OFD_SETLKW, &fLock);
            if (fs == -1)
            {
                printf("Failed to unlock data file read lock, exiting\n");
                EndAllThreads = true;
                ra = -2;
            }
        }
        else
        {
            if (Diagnose)
                printf("Failed to obtain data file %llu byte read lock (%llu/%d) - %d\n", 
                       node->io_len, pos, sizeof(fLock.l_start), errno);
            ra = -3;
        }
    }
    else
    {
        ra = -4;
    }
    nIOTasks++;
    if ((fs != 0) || (ra < 0))
        return false;

    return true;
}

bool ioFileWrite(io_queue_node *node)
{
    int fs = -2;
    int ts;
    size_t pos, ws = 0;
    size_t ra = -5;
    off64_t newPos;
    struct flock fLock;

    if (node == NULL)
        return false;
    if (node->my_fd == -1)
        return false;
    if ((node->io_buffer == NULL) || (node->io_len < 1))
        return false;

    pos = fileSize - node->io_len - 1;
    pos = (rand() * pos) / RAND_MAX;
    newPos = lseek64(node->my_fd, pos, SEEK_SET);
    if (newPos != (off_t)-1)
    {
        /* Lock our write region */
        fLock.l_type = F_WRLCK;
        fLock.l_whence = SEEK_SET;
        fLock.l_start = pos;
        fLock.l_len = node->io_len;
        fLock.l_pid = 0;
        fs = fcntl(node->my_fd, F_OFD_SETLKW, &fLock);
        if (fs != -1)
        {
            totalTriedIOWrite += node->io_len;
            ws = write(node->my_fd, node->io_buffer, node->io_len);
            ra = ws;
            if (ra != -1)
            {
                node->io_done = node->io_len;
            }
            else
            {
                ra = -1;
            }

            /* Unlock our write region */
            fLock.l_type = F_UNLCK;
            fLock.l_whence = SEEK_SET;
            fLock.l_start = pos;
            fLock.l_len = node->io_len;
            fLock.l_pid = 0;
            fs = fcntl(node->my_fd, F_OFD_SETLKW, &fLock);
            if (fs == -1)
            {
                printf("Failed to unlock data file write lock, exiting\n");
                EndAllThreads = true;
                ra = -2;
            }
        }
        else
        {
            if (Diagnose)
                printf("Failed to obtain data file %llu byte write lock (%llu/%d) - %d\n", 
                       node->io_len, pos, sizeof(fLock.l_start), errno);
            ra = -3;
        }
    }
    else
    {
        ra = -4;
    }

    nIOTasks++;
    if ((fs != 0) || (ra < 0))
        return false;

    return true;
}

void *IOThreadStart(void *arg)
{
    struct thread_info *mytinfo = (struct thread_info *)arg;
    unsigned long long p;
    int activity;
    io_queue_node *node;

    nTotalThreads++;
    nThreads++;
    nIOThreads++;

    if (arg == NULL)
    {
        printf("I/O thread started with no arguments, exiting\n");
        EndAllThreads = true;
        return NULL;
    }
    if (Diagnose)
        printf("I/O thread %d: top of stack near %p; argv_pointer=%p\n",
                   mytinfo->thread_num, &p, mytinfo->argv_string);

    if (sem_init(&mytinfo->my_sem, 0, 0) != 0)
    {
        printf("Failed to initialize semaphore for I/O thread %d, exiting", mytinfo->thread_num);
        EndAllThreads = true;
        return NULL;
    }

    /* Get our own stream handle */
    mytinfo->my_fd = open(ioFilename.c_str(), O_RDWR | O_LARGEFILE);
    if (mytinfo->my_fd == -1)
    {
        printf("Failed to open file for I/O thread %d, exiting", mytinfo->thread_num);
        EndAllThreads = true;
        return NULL;
    }

    while (!EndAllThreads)
    {
        activity = getActivity(2, mytinfo->thread_num);
        switch(activity)
        {
            /* Read */
            case 0:
                node = getIOReadNode();
                if (node != NULL)
                {
                    node->my_fd = mytinfo->my_fd;
                    nTriedIOTasks++;
                    if (!ioFileRead(node))
                    {
                        if (verbose_flag)
                            printf("Read node failure of size %llu\n", node->io_len);
                    }
                    node->my_fd = -1;
                    if (queueIODone(node))
                    {
                        node = NULL;
                    }
                    else
                    {
                        printf("Failed to queue read I/O done, exiting\n");
                        EndAllThreads = true;
                    }
                }
                else
                {
                    sleep(1);
                }
                break;

            /* Write */
            case 1:
                node = getIOWriteNode();
                if (node != NULL)
                {
                    node->my_fd = mytinfo->my_fd;
                    nTriedIOTasks++;
                    if (!ioFileWrite(node))
                    {
                        if (verbose_flag)
                            printf ("Write (node) failure of size %llu\n", node->io_len);
                    }
                    node->my_fd = -1;
                    if (queueIODone(node))
                    {
                        node = NULL;
                    }
                    else
                    {
                        printf("Failed to queue write I/O done (%llu/%llu), exiting\n", node->io_done, node->io_len);
                        EndAllThreads = true;
                    }
                }
                else
                {
                    sleep(1);
                }
                break;

            default:
                sleep(1);
                break;
        }
    };

    /* No more I/O */
    if (mytinfo->my_fd != -1)
    {
        if (close(mytinfo->my_fd) != 0)
        {
            printf("Failed to close I/O file for I/O thread %d, exiting", mytinfo->thread_num);
            EndAllThreads = true;
        }
        mytinfo->my_fd = -1;
    }

    
    if (sem_destroy(&mytinfo->my_sem) != 0)
    {
        printf("Failed to destroy semaphore for I/O thread %d, exiting", mytinfo->thread_num);
        EndAllThreads = true;
    }

    if (Diagnose)
        printf("I/O thread %d ending\n", mytinfo->thread_num);

    mytinfo->thread_num = 0;

    nIOThreads--;
    nThreads--;

    return NULL;
}

int SetupIOThreads(void)
{
    int s, tnum, t;
    pthread_attr_t attr;

    s = pthread_attr_init(&attr);
    if (s != 0)
        return s;

    iotinfo = (struct thread_info *)CountingCalloc(iothreads, sizeof(struct thread_info));
    if (iotinfo == NULL)
        return -1;

    /* Create one thread for each I/O thread */
    for (tnum = 0; tnum < iothreads; tnum++)
    {
        t = threadNum++;
        iotinfo[tnum].thread_num = t;
        iotinfo[tnum].argv_string = NULL;
        s = pthread_create(&iotinfo[tnum].thread_id, &attr, IOThreadStart, &iotinfo[tnum]);
        if (s != 0)
            return s;
    }

    s = pthread_attr_destroy(&attr);
    if (s != 0)
        return s;
    
    return 0;
}

void *WorkerThreadStart(void *arg)
{
    unsigned long long p;
    bool ab, cd, memQueued;
    bool endMe = false;
    char mChar;
    int activity, s, iotype;
    unsigned long long avail, sz, ts, dv, sum, num, pos;
    double dVal;
    void *myMem = NULL;
    unsigned long long *wspace;
    struct thread_info *mytinfo = (struct thread_info *)arg;
    siginfo_t sigs;
    struct timespec waitfor;
    io_queue_node *node;

    nTotalThreads++;
    nThreads++;

    if (Diagnose)
        printf("Worker thread %d: top of stack near %p; argv_pointer=%p\n",
                   mytinfo->thread_num, &p, mytinfo->argv_string);

    if (sem_init(&mytinfo->my_sem, 0, 0) != 0)
    {
        printf("Failed to initialize semaphore for worker thread %d, exiting", mytinfo->thread_num);
        EndAllThreads = true;
        goto workerFinished;
    }
    memQueued = false;

    while (!EndAllThreads)
    {
        activity = getActivity(7, mytinfo->thread_num);
        switch(activity)
        {
            case 0:
                /* Allocate some memory */
                if (myMem == NULL)
                {
                    if (maxMem != 0)
                    {
                        ab = true;
                        avail = maxMem - memUsed.load(std::memory_order_relaxed);
                        sz = (rand() * maxIOSize) / RAND_MAX;
                    }
                    else
                    {
                        ab = false;
                        avail = memUsed.load(std::memory_order_relaxed);
                        sz = (rand() * maxIOSize) / RAND_MAX;
                    }
                    if (sz == 0)
                    {
                        if (avail > 4096)
                        {
                            cd = true;
                            sz = 4096;
                        }
                        else
                        {
                            cd = false;
                            sz = avail / 2;
                            if (sz == 0)
                            {
                                sz = 8;
                            }
                        }
                    }
#if 0
                    if (maxMem)
                    {
                        ab = true;
                        dv = 4;
                        if (dv > ts)
                        {
                            dv = ts - 1;
                        }
                        if (ts > 1)
                        {
                            cd = true;
                            sz = (maxMem - memUsed.load(std::memory_order_relaxed)) / (ts / (1 + rand() % dv));
                        }
                        else
                        {
                            cd = false;
                            sz = (maxMem - memUsed.load(std::memory_order_relaxed)) / (1 + rand() % 4);
                        }
                    }
                    else
                    {
                        ab = false;
                        dv = 6;
                        if (dv > ts)
                        {
                            dv = ts - 1;
                        }
                        if (ts > 1)
                        {
                            cd = true;
                            sz = memUsed.load(std::memory_order_relaxed) / (ts / (1 + rand() % dv));
                        }
                        else
                        {
                            cd = false;
                            sz = memUsed.load(std::memory_order_relaxed) / (1 + rand() % 6);
                        }
                    }
                    if (sz == 0)
                        sz = 4096;
#endif

                    myMem = CountingCalloc(1, sz);
                    if (myMem == NULL)
                    {
                        printf("Worker thread %d: failed to allocate %llu bytes (",
                                    mytinfo->thread_num, sz);
                        if (ab)
                            putchar('A');
                        else
                            putchar('B');
                        putchar('/');
                        if (cd)
                            putchar('A');
                        else
                            putchar('B');
                        puts(")\n");
                    }
                    else
                    {
                        if (Diagnose)
                        {
                            dVal = memUsed.load(std::memory_order_relaxed);
                            mChar = 0;
                            printf("Memory used = ");
                            if (doubleToScale(&dVal, &mChar))
                            {
                                printf("%g %ciB\n", dVal, mChar);
                            }
                            else
                            {
                                printf("%llu B\n", memUsed.load(std::memory_order_relaxed));
                            }
                        }
                    }
                }
                break;

            case 1:
                /* Free any memory we have allocated */
                if (myMem != NULL)
                {
                    CountingFree(myMem);
                    myMem = NULL;
                    sz = 0;
                    if (Diagnose)
                    {
                        dVal = memUsed.load(std::memory_order_relaxed);
                        mChar = 0;
                        printf("Memory used = ");
                        if (doubleToScale(&dVal, &mChar))
                        {
                            printf("%g %ciB\n", dVal, mChar);
                        }
                        else
                        {
                            printf("%llu B\n", memUsed.load(std::memory_order_relaxed));
                        }
                    }
                }
                break;

            case 2:
                /* Zero any memory we have allocated (write) */
                if ((myMem != NULL) && (sz > 0))
                {
                    memset(myMem, 0, sz);
                    totalWrite += sz;
                }
                break;

            case 3:
                /* Read any memory we have allocated */
                if ((myMem != NULL) && (sz > sizeof(sum)))
                {
                    sum = 0;
                    num = sz / sizeof(sum);
                    wspace = (unsigned long long *)myMem;
                    for (pos = 0; pos < num; pos++)
                    {
                        sum += wspace[pos];
                    }
                    totalRead += (num * sizeof(sum));
                }
                break;

            case 4:
                /* End this thread, another can be started by main if it's not the only one */
                if (((nThreads - nIOThreads) > 0) && (rand() < RESTART_SCOPE))
                {
                    if (Diagnose)
                        printf("Ending thread number %d (%d of %d)\n", mytinfo->thread_num, nThreads.load(std::memory_order_relaxed), nTotalThreads.load(std::memory_order_relaxed));
                    endMe = true;
                }
                break;

            case 5:
                if (rand() % 2)
                {
                    waitfor.tv_sec = 1;
                    waitfor.tv_nsec = 0;
                }
                else
                {
                    waitfor.tv_sec = 0;
                    waitfor.tv_nsec = 100000;
                }
                s = sigtimedwait(&sigmask, &sigs, &waitfor);
                if (s < 0)
                {
                    /* Reset the signal mask without changing it */
                    s = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
                    nSigMaskSets++;
                }
                else
                {
                    if (s == SIGUSR1)
                    {
                        sigStop++;
                    }
                }
                break;

            case 6:
                if ((myMem != NULL) && (nIOThreads > 0))
                {
                    /* Let an I/O thread use our buffer */
                    node = (io_queue_node *)calloc(1, sizeof(*node));
                    if (node != NULL)
                    {
                        node->io_buffer = myMem;
                        node->io_len = (sz * rand()) / RAND_MAX;
                        if (node->io_len < 1)
                            node->io_len = 1;
                        node->my_sem = &mytinfo->my_sem;
                        iotype = getActivity(2, mytinfo->thread_num);
                        if (iotype == 0)
                        {
                            if (queueIORead(node))
                                nQueuedIOTasks++;
                        }
                        else
                        {
                            if (queueIOWrite(node))
                                nQueuedIOTasks++;
                        }
                        memQueued = true;
                        
                        /* Wait for completion */
                        do
                        {
                            ts = sem_trywait(node->my_sem);
                            if (ts == 0)
                            {
                                if (verbose_flag)
                                    printf("read/write waiter signaled\n");
                                /* Remove the buffer from the I/O done queue */
                                if (getIODoneNode(node))
                                {
                                    memQueued = false;
                                    if (iotype == 0)
                                        totalIORead += node->io_done;
                                    else
                                        totalIOWrite += node->io_done;
                                    free(node);
                                    node = NULL;
                                }
                                else
                                {
                                    printf("read/write wait for completion fails to find node on done queue\n");
                                    /* End program on failure to get node back */
                                    EndAllThreads = true;
                                }
                            }
                            else
                            {
                                sleep(1);
                            }
                        } while (!EndAllThreads && (ts != 0));
                    }
                }
                break;

            default:
                if (Diagnose)
                    printf("Worker thread %d: IDLE (memory used is %llu)\n",
                            mytinfo->thread_num, memUsed.load(std::memory_order_relaxed));
                sleep(1 + (rand() % 2));
                break;
        }
    };
    
    if ((myMem != NULL) && !memQueued)
    {
        CountingFree(myMem);
        myMem = NULL;
    }

    if (sem_destroy(&mytinfo->my_sem) != 0)
    {
        printf("Failed to destroy semaphore for worker thread %d, exiting", mytinfo->thread_num);
        EndAllThreads = true;
    }

workerFinished:
    if (Diagnose)
        printf("Worker thread %d ending\n", mytinfo->thread_num);


    /* This thread is finished with it's thread_info */
    mytinfo->thread_num = 0;

    nThreads--;

    return NULL;
}

/* Caller must hold wktilock */
int getUnusedWorkThreadNum(void)
{
    int wNum, maxworkers;

    if (Diagnose)
        printf("Finding an unused worker\n");
    maxworkers = maxthreads - iothreads;

    /* Trawl through wktinfo looking for a zero thread_num */
    for (wNum = 0; wNum < maxworkers; wNum++)
    {
        if (wktinfo[wNum].thread_num == 0)
        {
            if (Diagnose)
                printf("Found unused worker %d\n", wNum);
            return wNum;
        }
    }

    return -1;
}

int startOneWorkThread(int wNum, pthread_attr_t *attr)
{
    int result, s, t;

    result = -1;
    
    if (pthread_mutex_lock(&wktilock) == 0)
    {
        if (wNum < 0)
            wNum = getUnusedWorkThreadNum();
        if (wNum >= 0)
        {
            t = threadNum++;
            wktinfo[wNum].thread_num = t;
            wktinfo[wNum].argv_string = NULL;
            s = pthread_create(&wktinfo[wNum].thread_id, attr, WorkerThreadStart, &wktinfo[wNum]);
            if (s != 0)
            {
                if (verbose_flag)
                    printf("Failed to create thread number %d\n", wktinfo[wNum].thread_num);

                wktinfo[wNum].thread_num = 0;
                return s;
            }
            if (nThreads > nPeakThreads)
                nPeakThreads = nThreads.load(std::memory_order_relaxed);
            result = 0;
        }
        if (pthread_mutex_unlock(&wktilock) != 0)
        {
            if (Diagnose)
                printf("Unlock critical section of getUnusedWorkThreadNum failed\n");
        }
    }

    return result;
}

int SetupWorkThreads(void)
{
    int s, minworkers, maxworkers, tnum, wnum;
    pthread_attr_t attr;

    s = pthread_attr_init(&attr);
    if (s != 0)
    {
        if (verbose_flag)
            printf("Failed to initialize thread attr\n");
        return s;
    }

    minworkers = minthreads - iothreads;
    maxworkers = maxthreads - iothreads;

    wktinfo = (struct thread_info *)CountingCalloc(maxworkers, sizeof(struct thread_info));
    if (wktinfo == NULL)
    {
        if (verbose_flag)
            printf("Failed to allocate memory for thread info data\n");
        (void)pthread_attr_destroy(&attr);
        return -1;
    }

    /* Create one thread for each worker */
    for (tnum = 0; tnum < minworkers; tnum++)
    {
        s = startOneWorkThread(tnum, &attr);
        if (s != 0)
        {
            if (verbose_flag)
                printf("Failed to create thread number %d\n", wktinfo[tnum].thread_num);
            (void)pthread_attr_destroy(&attr);
            return s;
        }
    }

    printf("All %d workers created\n", minworkers);

    s = pthread_attr_destroy(&attr);
    if (s != 0)
    {
        if (verbose_flag)
            printf("Failed to destroy thread attr\n");
        return s;
    }

    return 0;
}

time_t GetElapsed(time_t start, time_t finish)
{
    time_t result = (time_t)-1;

    if ((start != (time_t)-1) && (finish != (time_t)-1))
    {
        result = finish;
        result -= start;
        if ((result >= (time_t)0) && (result < (time_t)1))
            result = 1;

        if (result < (time_t)0)
            result = (time_t)-1;
    }

    return result;
}

time_t GetElapsedFrom(time_t someTime)
{
    time_t curTime = time(NULL);

    return GetElapsed(someTime, curTime);
}

int main(int argc, char*argv[])
{
    char mChar;
    int i, r, s, t;
    double dVal, dElapsed, rate;
    time_t start, tElapsed;
    pthread_attr_t attr;
    
    printf("\nTEST PROGRAM\n");

    if (!ParseArgs(argc, argv))
    {
        if (helpShown)
            exit(0);
        abort();
    }

    start = (time_t)-1;
    tElapsed = start;
    dElapsed = 0.0;

    /* Signal mask */
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGQUIT);
    sigaddset(&sigmask, SIGUSR1);
    s = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
    if (s != 0)
        goto finished;

    if (!setupSyncObjects())
    {
        goto finished;
    }

    if (!setupDataFile())
    {
        printf("Failed to setup data file %s\n", ioFilename.c_str());
        goto finished;
    }

    /* Ready to start */
    if (iothreads > 0)
    {
        if (SetupIOThreads() == 0)
        {
            while (nIOThreads < iothreads)
                sleep(1);
        }
    }

    printf("Current I/O threads = %d\n", nIOThreads.load(std::memory_order_relaxed));
    dVal = memUsed.load(std::memory_order_relaxed);
    mChar = 0;
    printf("Memory used = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", memUsed.load(std::memory_order_relaxed));
    }

    if (SetupWorkThreads() == 0)
    {
        while (nThreads.load(std::memory_order_relaxed) < 1)
            sleep(1);
    }
    else
    {
        printf("Failed to setup worker threads\n");
    }

    putchar('\n');
    printf("TESTING\n");
    start = time(NULL);
    tElapsed = (time_t)0;
    printf("Current threads = %d\n", nThreads.load(std::memory_order_relaxed));
    dVal = memUsed.load(std::memory_order_relaxed);
    mChar = 0;
    printf("Memory used = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", memUsed.load(std::memory_order_relaxed));
    }
    putchar('\n');
    i = 0;
    while((tElapsed < (time_t)maxruntime) || ((tElapsed == (time_t)-1) && (i < maxruntime)))
    {
        if (sigStop.load(std::memory_order_relaxed))
        {
            printf("Stopped by signal\n");
            break;
        }

        if (short_threads && (!EndAllThreads) && (nThreads < maxthreads) && (rand() >= RESTART_SCOPE))
        {
            if (verbose_flag)
                printf("Want to start another thread\n");
            s = pthread_attr_init(&attr);
            if (s == 0)
            {
                if (verbose_flag)
                    printf("Starting a new worker thread\n");
                s = startOneWorkThread(-1, &attr);
                if (s != 0)
                {
                    if (verbose_flag)
                        printf("Failed to start a new thread\n");
                }
                s = pthread_attr_destroy(&attr);
                if (s != 0)
                {
                    if (verbose_flag)
                        printf("Failed to destroy thread attr for a new thread\n");
                    break;
                }
            }
            else
            {
                if (verbose_flag)
                    printf("Failed to initialize thread attr for a new worker\n");
            }
        }

        sleep(1);
        tElapsed = GetElapsedFrom(start);
        i++;
        if (Diagnose)
            printf("TICK (%d with %d threads) elapsed %d, max %d\n", i, nThreads.load(std::memory_order_relaxed), tElapsed, maxruntime);
    }
    if (Diagnose)
        printf("FINISHING (CLEANUP)\n");

    /* If there is pending I/O, let it finish before killing threads */
    EndIOTasks();
    tElapsed = GetElapsedFrom(start);

    /* Now end all the threads */
    EndPThreads();

    /* If we get here there should be no running threads that compete for the 
     * nodes on the queues and no pending I/O for execution or completion */

finished:
    if (tElapsed != (time_t)-1)
    {
        tElapsed = (time_t)i;
        dElapsed = (int)tElapsed;
        if (dElapsed <= 0.0)
            dElapsed = 1.0;
    }
    putchar('\n');
    puts("I/O Data (before cleanup):");
    printf("  Queued I/O tasks = %llu\n", nQueuedIOTasks.load(std::memory_order_relaxed));
    printf("   Tried I/O tasks = %llu\n", nTriedIOTasks.load(std::memory_order_relaxed));
    printf("         I/O tasks = %llu\n", nIOTasks.load(std::memory_order_relaxed));
    printf("    I/O read nodes = %llu remaining\n", pendingIOReads.load(std::memory_order_relaxed));
    printf("   I/O write nodes = %llu remaining\n", pendingIOWrites.load(std::memory_order_relaxed));
    printf("    I/O done nodes = %llu remaining\n", pendingIODone.load(std::memory_order_relaxed));
    putchar('\n');
    clearIOReadQ();
    clearIOWriteQ();
    clearIODoneQ();

    if (!cleanupDataFile())
        printf("  Failed to close/delete data file %s\n", ioFilename.c_str());

    (void)destroySyncObjects();

    puts("Thread/Memory Data:");
    printf("     Total threads = %d\n", nTotalThreads.load(std::memory_order_relaxed));
    printf("      Peak threads = %d\n", nPeakThreads.load(std::memory_order_relaxed));
    printf("   End I/O threads = %d\n", nIOThreads.load(std::memory_order_relaxed));
    printf("       End threads = %d\n", nThreads.load(std::memory_order_relaxed));
    dVal = memUsed.load(std::memory_order_relaxed);
    mChar = 0;
    printf("   End memory used = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", memUsed.load(std::memory_order_relaxed));
    }

    dVal = totalRead.load(std::memory_order_relaxed);
    mChar = 0;
    printf("        Bytes read = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", totalRead.load(std::memory_order_relaxed));
    }
    dVal = totalWrite.load(std::memory_order_relaxed);
    mChar = 0;
    printf("     Bytes written = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", totalWrite.load(std::memory_order_relaxed));
    }
    printf("   Set signal mask = %llu times\n", nSigMaskSets.load(std::memory_order_relaxed));
    putchar('\n');

    if (dElapsed != 0.0)
    {
        dVal = totalRead.load(std::memory_order_relaxed);
        dVal /= dElapsed;
        mChar = 0;
        printf("  Mem Read rate: ");
        if (doubleToScale(&dVal, &mChar))
        {
            printf("%g %ciB/s\n", dVal, mChar);
        }
        else
        {
            printf("%g B/s\n", dVal);
        }

        dVal = totalWrite.load(std::memory_order_relaxed);
        dVal /= dElapsed;
        mChar = 0;
        printf(" Mem Write rate: ", dVal);
        if (doubleToScale(&dVal, &mChar))
        {
            printf("%g %ciB/s\n", dVal, mChar);
        }
        else
        {
            printf("%g B/s\n", dVal);
        }

        dVal = totalRead.load(std::memory_order_relaxed);
        dVal += totalWrite.load(std::memory_order_relaxed);
        dVal /= dElapsed;
        mChar = 0;
        printf("   Abs Mem rate: ");
        if (doubleToScale(&dVal, &mChar))
        {
            printf("%g %ciB/s\n", dVal, mChar);
        }
        else
        {
            printf("%g B/s\n", dVal);
        }
        putchar('\n');
    }

    puts("I/O Data (final):");
    dVal = totalTriedIORead.load(std::memory_order_relaxed);
    mChar = 0;
    printf("   Tried I/O reads = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", totalTriedIORead.load(std::memory_order_relaxed));
    }
    
    dVal = totalIORead.load(std::memory_order_relaxed);
    mChar = 0;
    printf("    I/O read bytes = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", totalIORead.load(std::memory_order_relaxed));
    }

    dVal = totalTriedIOWrite.load(std::memory_order_relaxed);
    mChar = 0;
    printf("  Tried I/O writes = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", totalTriedIOWrite.load(std::memory_order_relaxed));
    }

    dVal = totalIOWrite.load(std::memory_order_relaxed);
    mChar = 0;
    printf("   I/O write bytes = ");
    if (doubleToScale(&dVal, &mChar))
    {
        printf("%g %ciB\n", dVal, mChar);
    }
    else
    {
        printf("%llu B\n", totalIOWrite.load(std::memory_order_relaxed));
    }

    printf("    I/O read nodes = %llu remaining\n", pendingIOReads.load(std::memory_order_relaxed));
    printf("   I/O write nodes = %llu remaining\n", pendingIOWrites.load(std::memory_order_relaxed));
    printf("    I/O done nodes = %llu remaining\n", pendingIODone.load(std::memory_order_relaxed));
    putchar('\n');

    if (dElapsed != 0.0)
    {
        dVal = totalIORead.load(std::memory_order_relaxed);
        dVal /= dElapsed;
        mChar = 0;
        printf("  I/O Read rate: ");
        if (doubleToScale(&dVal, &mChar))
        {
            printf("%g %ciB/s\n", dVal, mChar);
        }
        else
        {
            printf("%g B/s\n", dVal);
        }

        dVal = totalIOWrite.load(std::memory_order_relaxed);
        dVal /= dElapsed;
        mChar = 0;
        printf(" I/O Write rate: ");
        if (doubleToScale(&dVal, &mChar))
        {
            printf("%g %ciB/s\n", dVal, mChar);
        }
        else
        {
            printf("%g B/s\n", dVal);
        }

        dVal = totalIORead.load(std::memory_order_relaxed);
        dVal += totalIOWrite.load(std::memory_order_relaxed);
        dVal /= dElapsed;
        mChar = 0;
        printf("   Abs I/O rate: ");
        if (doubleToScale(&dVal, &mChar))
        {
            printf("%g %ciB/s\n", dVal, mChar);
        }
        else
        {
            printf("%g B/s\n", dVal, mChar);
        }
        putchar('\n');
    }
    putchar('\n');

    exit(0);
}
