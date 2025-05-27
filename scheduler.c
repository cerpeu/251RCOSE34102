#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define MAX_PROCESS 5

#define MAX_ARRIVAL 15
#define MAX_CPU_BURST 8
#define MIN_CPU_BURST 2
#define MAX_IO_BURST 4
#define MIN_IO_BURST 1
#define MAX_PRIORITY 5
#define TIME_QUANTUM 2

#define IO_PROBABILITY 5


enum { ALG_FCFS = 1, ALG_NPSJF = 2, ALG_PSJF = 3,
    ALG_NPPri = 4, ALG_PPri = 5, ALG_RR = 6 };
int scheduleAlg;    

typedef enum { 
  CRIT_SHORTEST, 
  CRIT_PRIORITY
} Criterion;


int rr_slice = 0;

int gantt[MAX_PROCESS * 100];
int totalTime = 0;

//priority 숫자 작을 수록 먼저
typedef struct {
  int PId;
  int arrival;
  int cpuBurst;
  int ioBurst;
  int ioRqst;
  int priority;
  int remTime;
  int ioStartTime;
  int startTime;
  int finTime;
  int enqueued;
  } Process;


Process prcslist[MAX_PROCESS];
Process *readyQueue[MAX_PROCESS];
Process *waitQueue[MAX_PROCESS];

int readyCount = 0;
int waitFront = 0;
int waitRear = 0;
int waitCount = 0;

Process Initialize_Process(int pid,
                      int arrival,
                      int cpuBurst,
                      int ioRqst,
                      int ioBurst,
                      int priority);
void Create_Process(void);
void enqueue_ready(Process *p);
Process *dequeue_ready(void);
void enqueue_wait(Process *p);
void handle_io(int clk);
Process *check_best(Criterion crit);
void remove_from_ready(Process *p);
Process *schedule(Process *current);
void handle_cpu(Process **running, int clk, int *completed);
void start_clock(int clk, Process *temp, int *completed);
void print_result(void);

int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  //프로세스 time, priority 관련 정보 랜덤 초기화, handle_cpu에서 IO발생 난수 생성 위함
  srand((unsigned)time(NULL));
    
  printf("Select scheduling algorithm:\n"
          "1. FCFS\n"
          "2. Non-Preemptive SJF\n"
          "3. Preemptive SJF\n"
          "4. Non-Preemptive Priority\n"
          "5. Preemptive Priority\n"
          "6. Round-Robin\n"
        
          "Enter your choice: ");

  if ((scanf("%d", &scheduleAlg) != 1)
    || (scheduleAlg < ALG_FCFS)
    || (scheduleAlg > ALG_RR)) {
    fprintf(stderr, "Invalid Algorithm Choice\n");
    exit(EXIT_FAILURE);
  }

  Create_Process();

  Process *running = NULL;
  int clk = 0;
  int completed = 0;
  
  //모든 프로세스 처리 전까지 틱 돌리기기
  for (int clk = 0; completed < MAX_PROCESS; clk++) {
    //레디큐에 삽입
    for (int i = 0; i < MAX_PROCESS; i++) {
      Process *p = &prcslist[i];
      if (p->arrival == clk) {
        enqueue_ready(p);
        printf("[ARRIVAL] clk=%2d P%d arrived\n", clk, p->PId);
      }
    }
    //IO 끝나는거 있는지 먼저 체크(끝난 프로세스 레디큐로 다시 들어가니까)
    handle_io(clk);
    //스케쥴링
    running = schedule(running);
    //현재 돌아가는 프로세스 정보 처리
    start_clock(clk, running, &completed);   
    //현재 돌아가는 프로세스 처리
    handle_cpu(&running, clk, &completed);
  }
  print_result();
  return 0;
}

Process Initialize_Process(int pid,
                      int arrival,
                      int cpuBurst,
                      int ioRqst,
                      int ioBurst,
                      int priority) {
  Process p;
  p.PId = pid;
  p.arrival = arrival;
  p.cpuBurst = cpuBurst;
  p.ioBurst = ioBurst;
  p.ioRqst = ioRqst;
  p.priority = priority;
  p.remTime = cpuBurst;
  p.ioStartTime = -1;
  p.startTime = -1;
  p.finTime = -1;
  p.enqueued = 0;

  return p;
}

void Create_Process(void) {
  for (int i = 0; i < MAX_PROCESS; i++) {
    int pid = i + 1;
    int arrival = rand() % (MAX_ARRIVAL + 1);
    int cpuBurst = rand() % (MAX_CPU_BURST - MIN_CPU_BURST + 1) + MIN_CPU_BURST;
    int ioRqst = rand() % cpuBurst;
    int ioBurst = rand() % (MAX_IO_BURST - MIN_IO_BURST + 1) + MIN_IO_BURST;
    int priority = rand() % MAX_PRIORITY;

    prcslist[i] = Initialize_Process(pid, arrival, cpuBurst, ioRqst, ioBurst, priority);
    printf("[Process Generated]\nP%2d: arrival = %2d, cpuBurst = %2d, I/O Request = %2d, I/OBurst = %2d, priority = %2d\n",
          pid, arrival, cpuBurst, ioRqst, ioBurst, priority);
  }
}

//push into rear of readyQueue
void enqueue_ready(Process *p) {
  if ((!p->enqueued) && (readyCount < MAX_PROCESS)) {
    readyQueue[readyCount++] = p;
    p->enqueued = 1;
  }
}

Process *dequeue_ready(void) {
  if (readyCount == 0) 
    return NULL;
  Process *p = readyQueue[0];
  for (int i = 1; i<readyCount; i++) 
    readyQueue[i - 1] = readyQueue[i];
  readyCount--;
  p->enqueued = 0;
  return p;
}

void remove_from_ready(Process *p) {
  //p가 큐에 있는지 우선 체크
  int idx = -1;
  for (int i = 0; i < readyCount; i++) {
    if (readyQueue[i] == p) {
      idx = i;
      break;
    }
  }
  if (idx < 0)
    return;

  //p 지우기, idx 다음 칸부터 한 칸씩 당기기
  for (int i = idx + 1; i < readyCount; i++) 
    readyQueue[i -1] = readyQueue[i];

  readyCount--;
  p->enqueued = 0;
}

//push into rear of waitQueue
void enqueue_wait(Process *p) {
  if (waitCount < MAX_PROCESS) {
    waitQueue[waitRear] = p;
    waitRear = (waitRear + 1) % MAX_PROCESS;
    waitCount++;
  }
}

Process *check_best(Criterion crit) {
  if (!readyCount)
    return NULL;

  Process *best = readyQueue[0];
  
  //readyQueue[0]과 그 뒤 인덱스 비교해서 최소시간, top priority 프로세스 구하기
  for (int i = 1; i < readyCount; i++){
    Process *candidate = readyQueue[i];

    if (((crit == CRIT_SHORTEST) && (candidate->remTime < best->remTime))
      || ((crit == CRIT_PRIORITY) && (candidate->priority < best->priority))) {
      best = candidate;
    }
  }
  return best;
}

//매 틱 돌아감
Process *schedule(Process *current) {
  int preemptive = ((scheduleAlg == ALG_PSJF) || 
                    (scheduleAlg == ALG_PPri));
                    
  Process *candidate = NULL;
  //현재 프로세스가 있고 preemptive 알고리즘이 아닐 때: current 프로세스 끝나고 스케쥴링하면 되니까 그대로 return current
  if (current && (!preemptive))
    return current;

  // 첫 번째 process인 경우 or running process is just finished or preemptive (모든 case다 써둬야 함)
  else {
    switch (scheduleAlg) {
      case ALG_FCFS: 
        if (!current)
          return dequeue_ready();
        return current;


      case ALG_RR:
        if(!current) {
          rr_slice = 0;
          return dequeue_ready();
        }
        return current;
        
    
      case ALG_NPSJF: 
        //check_Shortest로 다음 process 추출
        candidate = check_best(CRIT_SHORTEST);
        //레디큐 비었는지 체크
        if (!candidate)
          return current;
        //추출한 process 제외하고 다시 큐 갱신, 추출한 process는 return
        remove_from_ready(candidate);
        return candidate;
      

      case ALG_PSJF: 
        candidate = check_best(CRIT_SHORTEST);
        if (!current && candidate) {
          remove_from_ready(candidate);
          return candidate;
        }
        if (current && candidate && (candidate->remTime < current->remTime)) {
          remove_from_ready(candidate);
          enqueue_ready(current);
          return candidate;
        }  
        //candidate가 없을 때
        return current;
      

      case ALG_NPPri: 
        candidate = check_best(CRIT_PRIORITY);
        //레디큐 비었는지 체크
        if (!candidate) 
          return NULL;
        remove_from_ready(candidate);
        return candidate;    
      

      case ALG_PPri: 
        candidate = check_best(CRIT_PRIORITY);
        if (!current && candidate) {
          remove_from_ready(candidate);
          return candidate;
        }
        if (current && candidate && (candidate->priority < current -> priority)
          || (!current && candidate)) {
          remove_from_ready(candidate);
          enqueue_ready(current);
          return candidate;
        }
        //candidate가 없을 때
        return current;
      

      default:
        return NULL;
    }
  }  
}
 
//매 틱 돌아감, 스케쥴링된 process 간트에 넣고, 현재 상태 출력력
void start_clock(int clk, Process *running, int *completed) {
  if (running) 
    gantt[totalTime++] = (running)->PId;
  else
    gantt[totalTime++] = 0;

  if (running) {
    printf("[STATE] clk=%2d running=P%d ready=%d wait=%d done=%d\n",
        clk,running->PId,readyCount, waitCount, *completed);
  }
  else {
    printf("[STATE] clk=%2d running=- ready=%d wait=%d done=%d\n",
        clk, readyCount, waitCount, *completed);
  }
}


//매 틱 돌아감, IO 종료검사
void handle_io(int clk) {
  //전역변수 조심!
  int cnt = waitCount;
  waitCount = 0;
  int idx = waitFront;
  for (int i = 0; i < cnt; i++, idx = (idx + 1) % MAX_PROCESS) {
    Process *p = waitQueue[idx];

    // 1) If I/O completed, ->readyqueue
    if (clk - p->ioStartTime >= p->ioBurst) {
      enqueue_ready(p);
      printf("[I/O->Ready] clk=%2d P%d I/O done\n", clk, p->PId);
    }
    // 2) Unless back to waitqueue
    else {
      enqueue_wait(p);   
    }
  }
  waitFront = idx;
}

/*매 TICK마다 호출됨, 매 동작 끝나면 running state null해서 다음 작업 용이하게
  매 TICK마다 IO 실행, Process end, IO 발생, RR 체크
  Next tick scheduling(상태변화 있어도 다음 tick에서 반영)*/
void handle_cpu(Process **running, int clk, int *completed) {
  Process *p = *running;
  if (!p)
    return;

  if (p->startTime < 0)
    p->startTime = clk;


  //1) 시작하자마자 remtime감소, IO 발생시키고 감소시키면 조건문에서 return돼서 remtime 안줄어듦
  p->remTime--;

  //2) 고정 IO start 여부 검사
  if ((p->ioRqst > 0) && (p->cpuBurst - p->remTime == p->ioRqst)) {
    p->ioStartTime = clk;
    enqueue_wait(p);
    *running = NULL;
    printf("[I/O] clk=%2d P%d starts I/O\n", clk, p->PId);
    return;
  }

  //3) 확률적으로 I/O 발생, rqst검사는 애초에 그 프로세스가 IO 가능한지 확인 위함
  if (((rand() % 100) < IO_PROBABILITY) && (p->remTime > 0) && (p->ioRqst > 0)) {
    p->ioStartTime =  clk;
    p->enqueued = 0;
    enqueue_wait(p);
    printf("[I/O] P%d: at %d tick random I/O happened (I/O burst = %d)\n", p->PId, clk, p->ioBurst);
    fflush(stdout);
    *running = NULL;
    return;
    //IO발생하면 해당 tick ends -> IO처리 등은 다음 tick에서
  }

  //4) process end 검사
  if (p->remTime == 0) {
    p->finTime = clk;
    (*completed)++;
    *running = NULL;
    printf("[FINISH] clk=%2d P%d finished\n", clk, p->PId);
    
    return;
  }
  
  //5) RR이면 time quantum 지나고 다시 큐 맨 뒤로 enqueue
  if (scheduleAlg == ALG_RR) {
    rr_slice ++;
    if (rr_slice >= TIME_QUANTUM) {
      rr_slice = 0;
      enqueue_ready(p);
      *running = NULL;
      printf("[RR]   clk=%2d P%d time quantum expired\n", clk, p->PId);
    }
  }
}

void print_result(void) {
  puts("\nGantt Chart:");
    printf("|");
    for (int t = 0; t < totalTime; t++) {
        if (gantt[t] > 0)
            printf(" P%-2d |", gantt[t]);
        else
            printf(" idle |");
    }
    putchar('\n');

    printf("Total time = %d ticks\n\n", totalTime);

    printf("\nResults:\nPID | Arrival | Start | Finish | Turnaround | Waiting\n");
    printf("--------------------------------------------------------\n");
    for (int i = 0; i < MAX_PROCESS; i++) {
        Process *p = &prcslist[i];
        int turnaround = p->finTime - p->arrival + 1;
        int waiting    = turnaround - p->cpuBurst;
        printf(" P%-2d|   %3d   |  %3d |  %3d  |     %3d    |   %3d\n",
               p->PId, p->arrival, p->startTime, p->finTime, turnaround, waiting);
    }
}