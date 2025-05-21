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

#define IO_PROBABILITY 5

#define MAX_TIME 10000

enum { ALG_FCFS = 1, ALG_NPSJF = 2, ALG_PSJF = 3,
    ALG_NPPri = 4, ALG_PPri = 5, ALG_RR = 6};

int scheduleAlg;
int timeQuantum =2; 

int gantt[MAX_TIME];
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
  int waitTime;
  int turnAtime;
  int completed;
  int timeslice;
  int totalIOTime;
  int enqueued;
  } Process;


Process prcslist[MAX_PROCESS];
Process *readyQueue[MAX_PROCESS];
Process *waitQueue[MAX_PROCESS];

int readyFront = 0;
int readyRear = 0;
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
Process *check_Shortest(void);
Process *check_topPriority(void);
void remove_from_ready(Process *p);
Process *schedule(Process *current);
void handle_cpu(Process **running, int clk, int *completed);
void start_clock(int clk, Process **running, int *completed);
void print_result(void);

int main(void) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  int clk = 0;
  int completed = 0;
  
  Process *running = NULL;

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

  srand((unsigned)time(NULL));

  Create_Process();
  
  while(completed < MAX_PROCESS)
  {
    clk++;
    start_clock(clk, &running, &completed);
  }

  print_result();
  return 0;
}

Process Initialize_Process(int pid,
                      int arrival,
                      int cpuBurst,
                      int ioRqst,
                      int ioBurst,
                      int priority)
{
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
  p.finTime = 0;
  p.waitTime = 0;
  p.turnAtime = 0;
  p.completed = 0;
  p.timeslice = 0;
  p.totalIOTime = 0;
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
    printf("[Process Generated] P%2d: arrival = %2d, cpuBurst = %2d, I/O Request = %2d, I/OBurst = %2d, priority = %2d\n",
          pid, arrival, cpuBurst, ioRqst, ioBurst, priority);
  }
}

//push into rear of readyQueue
void enqueue_ready(Process *p) {
  if (readyCount < MAX_PROCESS) {
    readyQueue[readyRear] = p;
    readyRear = (readyRear + 1) % MAX_PROCESS;
    readyCount++;
  }
}

Process *dequeue_ready(void) {
  if (readyCount == 0) 
    return NULL;
  Process *p = readyQueue[readyFront];
  readyFront = (readyFront + 1) % MAX_PROCESS;
  readyCount--;
  return p;
}

//push into rear of waitQueue
void enqueue_wait(Process *p) {
  if (waitCount < MAX_PROCESS) {
    waitQueue[waitRear] = p;
    waitRear = (waitRear + 1) % MAX_PROCESS;
    waitCount++;
  }
}

void handle_io(int clk) {
  int cnt = waitCount;
  int rd_idx = waitFront;
  int wrt_idx = waitFront;
  
  //현재시점-io시작포인트가 io버스트보다 크면 레디큐에 넣기, 아니면 waitqueue 갱신
  for (int i = 0; i< cnt; i++) {
    Process *p = waitQueue[rd_idx];
    rd_idx = (rd_idx + 1) % MAX_PROCESS;

    if ((clk - p->ioStartTime) >= p->ioBurst) {
      p->totalIOTime += p->ioBurst;
      enqueue_ready(p);
    }
    else {
      waitQueue[wrt_idx] = p;
      wrt_idx = (wrt_idx + 1) % MAX_PROCESS;
    }
  }
  waitRear = wrt_idx;
  waitCount = (waitRear - waitFront + MAX_PROCESS) % MAX_PROCESS;
}

Process *check_Shortest(void) {
  if (!readyCount)
    return NULL;
  int shortestidx = readyFront;
  int shortestremT = readyQueue[readyFront]->remTime;
  
  for (int i = 1; i < readyCount; i++){
    int idx = (readyFront + i) % MAX_PROCESS;
    Process *p = readyQueue[idx];
    if (p->remTime < shortestremT) {
      shortestidx = idx;
      shortestremT = p->remTime;
    }
  }
  return readyQueue[shortestidx];
}

Process *check_topPriority(void) {
  if (!readyCount) 
    return NULL;
  int topidx = readyFront;
  int topPr = readyQueue[readyFront]->priority;
  for (int i= 1; i < readyCount; i++) {
    int idx = (readyFront + i) % MAX_PROCESS;
    if (readyQueue[idx]->priority < topPr) {
      topidx = idx;
      topPr = readyQueue[idx]->priority;
    }
  }
  return readyQueue[topidx];
}

void remove_from_ready(Process *p) {
  int idx = readyFront;
  for (int i = 0; i < readyCount; i++) {
    if (readyQueue[idx] == p)
      break;
    idx = (idx + 1)% MAX_PROCESS;
  }

  //한 칸씩 당기기
  for (int j = idx; j != readyRear; j = (j + 1)%MAX_PROCESS)
    readyQueue[j] = readyQueue[(j + 1) % MAX_PROCESS];

    readyRear = (readyRear - 1 + MAX_PROCESS) % MAX_PROCESS;
    readyCount--;
}

Process *schedule(Process *current) {
  int preemptive = ((scheduleAlg == ALG_PSJF) || 
                    (scheduleAlg == ALG_PPri));

  if (current && (!preemptive))
    return current;

  // 첫 번째 process인 경우(모든 case다 써둬야 함) or preemptive
  else {
    switch (scheduleAlg) {
      case ALG_FCFS: 
        //RR은 FCFS와 똑같지만 handle_cpu에다가 timequantum 장치 넣어둠
      case ALG_RR:
        return dequeue_ready();
    
      case ALG_NPSJF: {
        //check_Shortest로 다음 process 추출
        Process *candidate = check_Shortest();
        //레디큐 비었는지 체크
        if (!candidate)
          return NULL;
        //추출한 process 제외하고 다시 큐 갱신, 추출한 process는 return
        remove_from_ready(candidate);
        return candidate;
      }

      case ALG_PSJF: {
        Process *candidate = check_Shortest();
        if (current && candidate && (candidate->remTime < current->remTime)) {
          remove_from_ready(candidate);
          return candidate;
        }   
        //첫 번째 시행일 때   
        if (!current && candidate) {
          remove_from_ready(candidate);
          return candidate;
        }
        //candidate가 없을 때
        return current;
      }

      case ALG_NPPri: {
        Process *candidate = check_topPriority();
        //레디큐 비었는지 체크
        if (!candidate) 
          return NULL;
        remove_from_ready(candidate);
        return candidate;    
      }

      case ALG_PPri: {
        Process *candidate = check_topPriority();
        if (current && candidate && (candidate->priority < current -> priority)) {
          remove_from_ready(candidate);
          return candidate;
        }
        //첫 번째 시행일 때
        if (!current && candidate) {
          remove_from_ready(candidate);
          return candidate;
        }
        //candidate가 없을 때
        return current;
      }

      default:
        return NULL;
    }
  }  
}

/*매 TICK마다 호출됨, 매 동작 끝나면 running state null해서 다음 작업 용이하게
  매 TICK마다 running process가 processing 상태에서 내려가야 할지 검사
  IO발생, IO실행, RR Time Quantum, Preemption 체크 후 실행행 */
void handle_cpu(Process **running, int clk, int *completed) {
  if (!(*running))
    return;
  Process *p = *running;

  if (p->startTime < 0)
    p->startTime = clk;


  //1. 확률적으로 I/O 발생(0~99 난수 생성하고 그게 IO_PROB보다 작으면 IO, IO_PROB/100꼴의 확률로 발생하므로 확률적)
  if (((rand() % 100) < IO_PROBABILITY) && (p->remTime > 1)) {
    p->ioStartTime =  clk;
    enqueue_wait(p);
    *running = NULL;
    return;
    //IO발생하면 해당 tick ends -> IO처리 등은 다음 tick에서
  }
  
  p->remTime--;
  int used = p->cpuBurst - p->remTime;

  //2. IO 호출 시점 체크, 호출시점이면 running p waitqueue로 보내고 io시작시간저장(Context Switch)
  if (used == p->ioRqst) {
    p->ioStartTime = clk;
    enqueue_wait(p);
    *running = NULL;
    return;
  }

  //3. RR이면 time quantum 지나고 다시 큐 맨 뒤로 enqueue
  if (scheduleAlg == ALG_RR) {
    p->timeslice++;
    if ((p->timeslice >= timeQuantum) && (p->remTime > 0)) {
      p->timeslice = 0;
      enqueue_ready(p);
      *running = NULL;
      return;
    }
  }

  //4. Preemption
  if ((*running) && (scheduleAlg == ALG_PSJF || scheduleAlg == ALG_PPri)) {
    Process *next = schedule(*running);
    int preempt = 0;
    if (scheduleAlg == ALG_PSJF) {
      if (next && (next->remTime) < (p->remTime))
        preempt = 1;
    }
    //PPri
    else { 
      if (next && (next->priority) < (p->priority))
        preempt = 1;  
    }

    //preemption!
    if (preempt) {
      enqueue_ready(p);
      *running = next;
      return;
    }
  }
  //모두에 대해 종료검사
  if (p->remTime == 0) {
      p->finTime = clk;
      p->turnAtime = p->finTime - p->arrival + 1;
      p->completed = 1;
      (*completed)++;
      *running = NULL;
      return;
  }
}
//one clock ticks
void start_clock(int clk, Process **running, int *completed) {
  //arrival에 맞춰 readyqueue에 push
  for (int i = 0; i < MAX_PROCESS; i++) {
    if ((prcslist[i].arrival <= clk) && !prcslist[i].enqueued) {
      enqueue_ready(&prcslist[i]);
      prcslist[i].enqueued = 1;
    }
  }

  //IO check
  handle_io(clk);

  //프로세스가 존재하면 스케쥴링 결과 반환
  if (!(*running))
    *running = schedule(*running);

  //process cpu work
  handle_cpu(running, clk, completed);

  //Gantt
  gantt[clk] = *running ? (*running)->PId : 0;
  totalTime = clk;
}


void print_result(void) {
  printf(">>> DEBUG: print_result() 진입, totalTime =%d\n", totalTime);
  //Gantt
  printf("\nGantt Chart:\n");

    //| Pid | blocks
  printf("|");
  for (int t = 1; t<= totalTime; t++) {
    if (gantt[t] == 0)
      printf(" %4s|", "idle|");
    else
      printf(" %4d|", gantt[t]);
  }
  printf("\n");

  //time scale
  printf(" ");
  for (int t = 1; t <= totalTime; t++)
    printf("%-4d", t);

  printf("\n\n");

  printf("printing table");
  //table
  printf("\nPID | Arrival | Start | Finish | Turnaround | Waiting\n");
  printf("------------------------------------------------------\n");
  for (int i = 0; i < MAX_PROCESS; i++) {
    Process *p = &prcslist[i];
    printf("%3d | %7d | %5d |%6d | %10d | %7d\n", 
            p->PId, p->arrival, p->startTime, p->finTime, p->turnAtime, p->waitTime);
  }
}
