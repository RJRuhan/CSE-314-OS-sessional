#include <bits/stdc++.h>
#include <semaphore.h>
using namespace std;

#define NUM_OF_PRINTERS 4
#define NUM_OF_BINDERS 2
#define NUM_OF_STAFFS 2

int intervalStaff[NUM_OF_STAFFS];

int freeBinder = NUM_OF_BINDERS;
pthread_cond_t freeBinderCond;
pthread_mutex_t freeBinderLock;

pthread_mutex_t binderLock[NUM_OF_BINDERS];

pthread_mutex_t readCntLock;
pthread_mutex_t writerLock;
int readCnt = 0;

int numOfSubmissions = 0;

int N, M;

FILE *inFile, *outFile;

time_t startTime = -1;

enum
{
    WRITING,
    WAITING_ON_PRINTER,
    USING_PRINTER,
    WAITING_ON_BINDER,
    USING_BINDER,
    WAITING_ON_ENTRYBOOK,
    UPDATING_ENTRYBOOK,
    SUBMITTED
};

enum PrinterState
{
    BUSY,
    FREE
};

struct Group
{
    int id;
    int studentNum;
    pthread_cond_t totalPrintDoneCond;
    pthread_mutex_t totalPrintDoneMutex;
    int totalPrintDone = 0;
};

struct Student
{
    int timeToWrite;
    int id;
    struct Group *group;
    bool GroupLeader = false;
    struct Student *allStudents;

    sem_t printerSem;
    int state;
    pthread_mutex_t lock;
};

struct Staff
{
    int id;
};

PrinterState printerState[NUM_OF_PRINTERS];
pthread_mutex_t printerLock[NUM_OF_PRINTERS];

pthread_barrier_t bar;

int PrintingTime, BindingTime, ReadWriteTime;

void *studentFunc(void *arg)
{
    pthread_barrier_wait(&bar);

    std::random_device rd;
    std::default_random_engine generator(rd());
    std::poisson_distribution<int> distribution(3);
    
    int randWait;

    struct Student *student = (struct Student *)arg;

    pthread_mutex_lock(&readCntLock);
    if (startTime == -1)
        startTime = time(NULL);
    pthread_mutex_unlock(&readCntLock);

    fprintf(outFile, "Student %d will need %d seconds to complete his writing at time %ld \n", student->id, student->timeToWrite, time(NULL) - startTime);
    sleep(student->timeToWrite);

    randWait = distribution(generator);
    while (randWait > 10)
    {
        randWait = distribution(generator);
    }

    sleep(randWait);

    int assignedPrinter = student->id % NUM_OF_PRINTERS;

    fprintf(outFile, "Student %d is waiting on printer %d at time %ld \n", student->id, assignedPrinter + 1, time(NULL) - startTime);

    pthread_mutex_lock(&student->lock);
    student->state = WAITING_ON_PRINTER;
    pthread_mutex_unlock(&student->lock);

    pthread_mutex_lock(&printerLock[assignedPrinter]);
    if (printerState[assignedPrinter] == FREE)
    {
        sem_post(&student->printerSem);
        printerState[assignedPrinter] = BUSY;
    }
    pthread_mutex_unlock(&printerLock[assignedPrinter]);

    sem_wait(&student->printerSem);

    pthread_mutex_lock(&student->lock);
    student->state = USING_PRINTER;
    pthread_mutex_unlock(&student->lock);

    fprintf(outFile, "Student %d is using printer %d at time %ld \n", student->id, assignedPrinter + 1, time(NULL) - startTime);
    sleep(PrintingTime);
    fprintf(outFile, "Student %d has completed his printing at printer %d at time %ld \n", student->id, assignedPrinter + 1, time(NULL) - startTime);

    pthread_mutex_lock(&printerLock[assignedPrinter]);
    struct Student *students = student->allStudents;
    bool found = false;
    for (int i = 0; i < N; i++)
    {
        int assignedPrinter2 = students[i].id % NUM_OF_PRINTERS;
        if (students[i].group->id == student->group->id && assignedPrinter2 == assignedPrinter)
        {
            pthread_mutex_lock(&students[i].lock);
            if (students[i].state == WAITING_ON_PRINTER)
            {
                sem_post(&students[i].printerSem);
                found = true;
                pthread_mutex_unlock(&students[i].lock);
                break;
            }
            pthread_mutex_unlock(&students[i].lock);
        }
    }

    if (!found)
    {
        for (int i = 0; i < N; i++)
        {
            int assignedPrinter2 = students[i].id % NUM_OF_PRINTERS;
            if (assignedPrinter2 == assignedPrinter)
            {
                pthread_mutex_lock(&students[i].lock);
                if (students[i].state == WAITING_ON_PRINTER)
                {
                    sem_post(&students[i].printerSem);
                    found = true;
                    pthread_mutex_unlock(&students[i].lock);
                    break;
                }
                pthread_mutex_unlock(&students[i].lock);
            }
        }
    }

    if (!found)
    {
        printerState[assignedPrinter] = FREE;
    }

    pthread_mutex_unlock(&printerLock[assignedPrinter]);

    randWait = distribution(generator);
    while (randWait > 10)
    {
        randWait = distribution(generator);
    }

    sleep(randWait);

    pthread_mutex_lock(&student->group->totalPrintDoneMutex);
    student->group->totalPrintDone++;
    pthread_mutex_unlock(&student->group->totalPrintDoneMutex);
    pthread_cond_signal(&student->group->totalPrintDoneCond);

    if (!student->GroupLeader)
    {
        return 0;
    }

    pthread_mutex_lock(&student->group->totalPrintDoneMutex);
    while (student->group->totalPrintDone != student->group->studentNum)
    {
        fprintf(outFile, "Group %d waiting for its members to finish printing at time %ld.Total Printing Done : %d \n", student->group->id, time(NULL) - startTime, student->group->totalPrintDone);
        pthread_cond_wait(&student->group->totalPrintDoneCond, &student->group->totalPrintDoneMutex);
    }
    pthread_mutex_unlock(&student->group->totalPrintDoneMutex);

    randWait = distribution(generator);
    while (randWait > 10)
    {
        randWait = distribution(generator);
    }

    sleep(randWait);

    pthread_mutex_lock(&freeBinderLock);

    pthread_mutex_lock(&student->lock);
    student->state = WAITING_ON_BINDER;
    pthread_mutex_unlock(&student->lock);

    fprintf(outFile, "Group %d waiting on Binder at time %ld\n", student->group->id, time(NULL) - startTime);

    while (freeBinder == 0)
    {
        pthread_cond_wait(&freeBinderCond, &freeBinderLock);
    }
    freeBinder--;
    pthread_mutex_unlock(&freeBinderLock);

    int binder = 0;
    for (; binder < NUM_OF_BINDERS; binder++)
    {
        if (pthread_mutex_trylock(&binderLock[binder]) == 0)
            break;
    }

    if (binder == NUM_OF_BINDERS)
    {
        perror("no free binder error");
        exit(4);
    }

    pthread_mutex_lock(&student->lock);
    student->state = USING_BINDER;
    pthread_mutex_unlock(&student->lock);

    fprintf(outFile, "Group %d starts its Binding at Binder %d at time %ld \n", student->group->id, binder, time(NULL) - startTime);
    sleep(BindingTime);
    fprintf(outFile, "Group %d finished its Binding at Binder %d at time %ld \n", student->group->id, binder, time(NULL) - startTime);

    pthread_mutex_unlock(&binderLock[binder]);

    pthread_mutex_lock(&freeBinderLock);
    freeBinder++;
    pthread_mutex_unlock(&freeBinderLock);

    pthread_cond_broadcast(&freeBinderCond);

    randWait = distribution(generator);
    while (randWait > 10)
    {
        randWait = distribution(generator);
    }

    sleep(randWait);

    pthread_mutex_lock(&student->lock);
    student->state = WAITING_ON_ENTRYBOOK;
    pthread_mutex_unlock(&student->lock);

    fprintf(outFile, "Group %d is waiting to update entry book at time %ld \n", student->group->id, time(NULL) - startTime);

    pthread_mutex_lock(&writerLock);

    fprintf(outFile, "Group %d is updating entry book at time %ld \n", student->group->id, time(NULL) - startTime);

    pthread_mutex_lock(&student->lock);
    student->state = UPDATING_ENTRYBOOK;
    pthread_mutex_unlock(&student->lock);

    sleep(ReadWriteTime);
    numOfSubmissions++;

    fprintf(outFile, "Group %d submitted the report at time %ld \n", student->group->id, time(NULL) - startTime);
    
    pthread_mutex_unlock(&writerLock);

    pthread_mutex_lock(&student->lock);
    student->state = SUBMITTED;
    pthread_mutex_unlock(&student->lock);


    return 0;
}

void init_mutex_sem()
{
    for (int i = 0; i < NUM_OF_PRINTERS; i++)
        pthread_mutex_init(&printerLock[i], 0);

    for (int i = 0; i < NUM_OF_BINDERS; i++)
        pthread_mutex_init(&binderLock[i], 0);

    pthread_barrier_init(&bar, 0, N + NUM_OF_STAFFS);
    pthread_mutex_init(&freeBinderLock, 0);
    pthread_cond_init(&freeBinderCond, 0);
    pthread_mutex_init(&readCntLock, 0);
    pthread_mutex_init(&writerLock, 0);

    for (int i = 0; i < NUM_OF_PRINTERS; i++)
    {
        printerState[i] = FREE;
    }
}

void destroy_mutex_sem()
{
    for (int i = 0; i < NUM_OF_PRINTERS; i++)
        pthread_mutex_destroy(&printerLock[i]);

    for (int i = 0; i < NUM_OF_BINDERS; i++)
        pthread_mutex_destroy(&binderLock[i]);

    pthread_mutex_destroy(&freeBinderLock);
    pthread_barrier_destroy(&bar);
    pthread_cond_destroy(&freeBinderCond);
    pthread_mutex_destroy(&readCntLock);
    pthread_mutex_destroy(&writerLock);
}

int readEntry(int id)
{
    fprintf(outFile, "Staff %d waiting to read the entry book at time %ld\n", id, time(NULL) - startTime);
    pthread_mutex_lock(&readCntLock);

    if (readCnt == 0)
    {
        pthread_mutex_lock(&writerLock);
    }
    readCnt++;
    pthread_mutex_unlock(&readCntLock);

    fprintf(outFile, "Staff %d is reading the entry book at time %ld\n", id, time(NULL) - startTime);
    sleep(ReadWriteTime);
    int subs = numOfSubmissions;
    pthread_mutex_lock(&readCntLock);
    fprintf(outFile, "Staff %d finished reading the entry book at time %ld.No of Submissions : %d \n", id, time(NULL) - startTime, subs);
    readCnt--;
    if (readCnt == 0)
    {
        pthread_mutex_unlock(&writerLock);
    }

    pthread_mutex_unlock(&readCntLock);

    fflush(outFile);

    if (subs == N / M)
        return 1;

    return 0;
}

void *staffFunc(void *arg)
{
    pthread_barrier_wait(&bar);

    pthread_mutex_lock(&readCntLock);
    if (startTime == -1)
        startTime = time(NULL);
    pthread_mutex_unlock(&readCntLock);

    struct Staff *staff = (struct Staff *)arg;

    std::random_device rd;
    std::default_random_engine generator(rd());
    std::poisson_distribution<int> distribution(3);

    while (true)
    {

        int number = distribution(generator);
        while (number > 10)
        {
            number = distribution(generator);
        }

        sleep(number);

        if (readEntry(staff->id))
            break;

        // readEntry(staff->id);
    }

    return 0;
}

int main()
{

    // use appropriate location if you are using MacOS or Linux
    inFile = fopen("input.txt", "r");
    outFile = fopen("output.txt", "w");

    if (inFile == NULL || outFile == NULL)
    {
        fprintf(outFile, "Error!");
        exit(1);
    }

    // fprintf(outFile,"Please Enter total number of students : ");
    fscanf(inFile, "%d %d", &N, &M);

    if (N <= 0)
    {
        fprintf(outFile, "Invalid input\n");
        exit(1);
    }

    // fprintf(outFile,"Please Enter number of students in a group : ");
    // scanf("%d",&M);

    if (M <= 0)
    {
        fprintf(outFile, "Invalid input\n");
        exit(1);
    }

    if (N % M != 0)
    {
        fprintf(outFile, "cant divide all students into groups uniformly\n");
        exit(1);
    }

    fscanf(inFile, "%d %d %d", &PrintingTime, &BindingTime, &ReadWriteTime);

    if (PrintingTime < 0 || BindingTime < 0 || ReadWriteTime < 0)
    {
        fprintf(outFile, "Invalid input\n");
        exit(1);
    }

    for (int i = 0; i < NUM_OF_STAFFS; i++)
    {
        intervalStaff[i] = 5;
    }

    int groupNum = N / M;

    init_mutex_sem();

    std::default_random_engine generator;
    std::poisson_distribution<int> distribution(4.3);

    pthread_t studentThread[N];
    pthread_t staffThread[N];
    struct Student students[N];
    struct Group groups[groupNum];
    struct Staff staffs[NUM_OF_STAFFS];

    for (int i = 0; i < NUM_OF_STAFFS; i++)
    {
        staffs[i].id = i;
        if (pthread_create(&staffThread[i], NULL, &staffFunc, &staffs[i]) != 0)
        {
            perror("Error when creating student Thread \n");
        }
    }

    for (int i = 0; i < groupNum; i++)
    {
        pthread_mutex_init(&groups[i].totalPrintDoneMutex, 0);
        pthread_cond_init(&groups[i].totalPrintDoneCond, 0);
        groups[i].studentNum = M;
        groups[i].id = i + 1;
    }

    for (int i = 0; i < N; i++)
    {
        int number = distribution(generator);
        // fprintf(outFile,"------- %d\n",number);
        while (number > 10)
        {
            number = distribution(generator);
            // fprintf(outFile,"happ %d\n", number);
        }
        students[i].id = i + 1;
        students[i].timeToWrite = number;
        students[i].group = &groups[i / M];
        students[i].state = WRITING;
        pthread_mutex_init(&students[i].lock, 0);
        sem_init(&students[i].printerSem, 0, 0);
        students[i].allStudents = students;

        if ((i + 1) % M == 0)
            students[i].GroupLeader = true;
        if (pthread_create(&studentThread[i], NULL, &studentFunc, &students[i]) != 0)
        {
            perror("Error when creating student Thread \n");
        }
    }

    for (int i = 0; i < N; i++)
    {
        if (pthread_join(studentThread[i], NULL) != 0)
        {
            perror("Failed to join thread\n");
        }
    }

    for (int i = 0; i < NUM_OF_STAFFS; i++)
    {
        if (pthread_join(staffThread[i], NULL) != 0)
        {
            perror("Failed to join thread\n");
        }
    }

    for (int i = 0; i < groupNum; i++)
    {
        pthread_mutex_destroy(&groups[i].totalPrintDoneMutex);
        pthread_cond_destroy(&groups[i].totalPrintDoneCond);
    }

    for (int i = 0; i < N; i++)
    {
        pthread_mutex_destroy(&students[i].lock);
        sem_destroy(&students[i].printerSem);
    }

    destroy_mutex_sem();

    fclose(inFile);
    fclose(outFile);

    return 0;
}