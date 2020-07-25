#include <stdio.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
  };

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
   int seqnum;
   int acknum;
   int checksum;
   char payload[20];
    };

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

// time 관련 변수
float time = 0.000; // 아래 있던 time 변수 위로 가져옴
struct pkt_Timer { // 타이머
    int seq; // 해당 #
    int receivedACK; // ACKed면 1, 아니면 0
    float limit_time; // 해당 #의 패킷의 limit time, 발생 time + RTT
};
struct pkt_Timer timer[1024];

// init에서 입력 받음
int window_size; 
float RTT;

// sender 관련 변수
struct pkt Sender[1024]; // 송신자(버퍼 겸 송신 패킷의 정보를 담음)
int next_seq_num; // sender에서 다음에 보낼 패킷의 시퀀스 넘버
int real_received_pktnum; // 실제로 layer5로부터 받은 패킷의 시퀀스 넘버
int send_base;
int no_seq_buff_index; // 순서에 맞지 않는 패킷을 버퍼에 저장하기 위한 index

// receiver 관련 변수
struct pkt Receiver[1024]; // 수신자(버퍼 겸 수신 패킷의 정보를 담음)
int rcv_base; // expected seq #

// A가 layer5로부터 메시지를 전달 받음. B로 전송해야 함
void A_output(struct msg message) {
    if (next_seq_num < send_base + window_size) { // seq #이 윈도우 내의 번호이면
        // 패킷 작업
        int i;
        for (i = 0; i < 20; i++) 
            Sender[real_received_pktnum].payload[i] = message.data[i]; // 받은 메시지를 패킷에 넣음
        Sender[real_received_pktnum].seqnum = real_received_pktnum;
        Sender[real_received_pktnum].acknum = 0; // ACK를 받기 전까지는 0인 상태
        Sender[real_received_pktnum].checksum = 0; // packet corrrupt는 신경 쓰지 않으므로 모두 0으로 초기화
        printf("A가 %d전송!", real_received_pktnum);
        tolayer3(0, Sender[real_received_pktnum]); // 해당 패킷을 B에게 전달하기 위해 layer3로 내려보냄

        // 타이머 작업
        timer[real_received_pktnum].seq = real_received_pktnum;
        timer[real_received_pktnum].limit_time = time + RTT; // 해당 시퀀스 패킷의 허용 시간. 현재 시간 + RTT
        if (send_base == next_seq_num) { // 베이스 #의 패킷에 대해서 타이머 실행
            starttimer(0, RTT);
        }
        next_seq_num++; // next seq num과 real received pkt num을 같이 증가
        real_received_pktnum++;
    }
    else {
        // seq #이 윈도우 내의 번호가 아니면 GBN처럼 버퍼에 저장되거나 상위 계층으로 올려 보낸다.
        // 여기서는 없으므로 버퍼에 저장하도록 한다.
        // 이 상황에서 real_received_pktnum이 next_seq_num보다 커진다.
        int i;
        // 버퍼링 과정
        for (i = 0; i < 20; i++)
            Sender[real_received_pktnum].payload[i] = message.data[i]; // 받은 메시지를 패킷에 넣음
        Sender[real_received_pktnum].seqnum = real_received_pktnum;
        Sender[real_received_pktnum].acknum = 0; // ACK를 받기 전까지는 0인 상태
        Sender[real_received_pktnum].checksum = 0;
        no_seq_buff_index++; // 버퍼링한 숫자를 count
        real_received_pktnum++; // next_seq_num은 증가하지 않고 real_received_pktnum만 증가. next_seq_num 이후의 패킷들은 전송(tolayer3)이 되지 않음!     
    }
    printf("send base, next seq : [%d,%d]\n", send_base, next_seq_num); // send base와 next seq #를 출력
    printf("buffered : %d", no_seq_buff_index); // 버퍼된 패킷의 개수 출력
}

/* need be completed only for extra credit */
void B_output(struct msg message) {

}

// layer3으로부터 ACK를 받는 경우.
void A_input(struct pkt packet) { 
    if (Sender[packet.acknum].acknum == 1) { // 이미 받은 ACK라면
        printf("%d 무시한다.\n", packet.acknum);
        return;
    }
    printf("\nA가 %d애크 받았다!\n", packet.acknum);
    Sender[packet.acknum].acknum = 1; // 받았다고 1로 표시
    timer[packet.acknum].receivedACK = 1; // 타이머에도 표시

    int i;
    if (packet.acknum == send_base) { // 받은 ACK가 send base에 대한 ACK였다면
        i = send_base;
        while (Sender[i].acknum == 1) { // send_base를 가장 작은 seq #를 가진 아직 확인응답 되지 않은 곳으로 옮김
            i++;
            send_base++;
        }
    }

    // 이전 send_base부터 nextseq 전까지의(이전 윈도우 내의) 모든 ack를 받고 위에서 send_base를 증가시켜서
    // 현재 완전 새로운 윈도우일 때, 즉 send_base가 next_seq_num와 같을 때
    if (send_base == next_seq_num) {
        stoptimer(0); // 옛날 send_base에 대한 타이머는 정지
    }

    // 위로부터 받은 윈도우 밖의 패킷을 버퍼링 해놓았다면(no_seq_buff_index가 0이 아니라면), 그리고
    // next_seq_num이 윈도우 내의 범위 내에 해당된다면 아래 내용 반복
    while (no_seq_buff_index != 0 && next_seq_num < send_base + window_size) {

        // next_seq_num이 윈도우 내의 #일 때만 next_seq_num이 증가했으므로 현재 버퍼링 되어 있는 첫 위치는 next_sesq_num으로 접근할 수 있다.
        printf("A에 버퍼링된 패킷 전송!\n");
        tolayer3(0, Sender[next_seq_num]); // 버퍼링된 정보 전송
        timer[next_seq_num].seq = next_seq_num; // 타이머 설정
        timer[next_seq_num].receivedACK = 0;
        timer[next_seq_num].limit_time = time + RTT;
        if (send_base == next_seq_num) { // A_output에서처럼 send_base일 경우에 timer 동작
            starttimer(0, RTT);
        }
        next_seq_num++; // 보낸만큼 next seq #는 증가시키고
        no_seq_buff_index--; // 버퍼링 해두었던 개수를 감소시킴
    }
}

// A에 timer intterupt가 걸린 경우
void A_timerinterrupt() {
    int i;    
    int j = send_base;
    starttimer(0, RTT / 2); // ACK를 받음과 상관없이 단순히 timer가 끝나면 그냥 다시 timer 재작동

    for (i = 0; i < next_seq_num - send_base; i++) { // 윈도우 내의 현재 보냈던 패킷 개수 만큼 반복

        // 아직 ACK를 받지 못했는데 현재 시간(time)이 timer에 설정된 한계 시간을 넘었다면 해당 패킷은 loss된 것이다.
        if (Sender[j].acknum == 0 && time > timer[j].limit_time) {
            printf("타이머 인터럽트! %d 다시보냄!\n", Sender[j].seqnum); // 타이머 인터럽트가 발생하면 재전송하고 타이머 재시작
            stoptimer(0);
            starttimer(0, RTT); // 타이머 재시작
            tolayer3(0, Sender[j]);
           
            timer[j].seq = j;
            timer[j].receivedACK = 0;
            timer[j].limit_time = time + RTT;
            break;
        }
    }
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void) {
    int i;
    send_base = 0;
    next_seq_num = 0;
    next_seq_num = 0;
    send_base = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

// B가 A로부터 정상적으로 패킷을 받음
void B_input(struct pkt packet) {
    printf("B가 %d, %c 받음\n", packet.seqnum, packet.payload[0]);
    
    int i;
    if (packet.seqnum == rcv_base) { // rcv_base는 받길 기대하는 정상적인 순서의 패킷이다.
        Receiver[rcv_base] = packet; 

        // 만약 순서에 맞지 않게 도착해서 버퍼링 해둔 연속적인 패킷이 존재한다면 위로 보내줌
        while (Receiver[rcv_base].seqnum != -1) { // 아직 패킷을 받지 않은 곳은 seqnum이 -1로 초기화되어 있다.
            tolayer5(1, Receiver[rcv_base].payload);
            rcv_base++;
        }
        struct pkt ACK; // 간단한 ACK 패킷 생성 후 A에게 전송
        ACK.acknum = packet.seqnum;
        ACK.seqnum = 0;
        ACK.checksum = 0;
        for (i = 0; i < 19; i++) // payload 부분을 공백으로 설정
            ACK.payload[i] = ' ';
        ACK.payload[19] = '\0';
        printf("%d ACK 보냄!", ACK.acknum);
        tolayer3(1, ACK);
    }
    else if (packet.seqnum > rcv_base && packet.seqnum < rcv_base + window_size) { // Out-Of-Order 이지만 window 내의 패킷
        if (Receiver[packet.seqnum].seqnum != -1) {
            printf("중복된 패킷이지만 ACK 전송\n");
        }
        Receiver[packet.seqnum] = packet; // 버퍼링 후 ACK를 전송은 함. 단, tolayer5로 올려보내지는 않음
        struct pkt ACK;
        ACK.acknum = packet.seqnum;
        ACK.seqnum = 0;
        ACK.checksum = 0;
        for (i = 0; i < 19; i++)
            ACK.payload[i] = ' ';
        ACK.payload[19] = '\0';
        printf("%d ACK 보냄!(Out-Of-Order!)", ACK.acknum);
        tolayer3(1, ACK);
    }
    else if (packet.seqnum >= rcv_base - window_size && packet.seqnum < rcv_base) { // 이미 수신자가 확인응답한 것이라도, ACK를 전송해야 함.
        if (Receiver[packet.seqnum].seqnum != -1) {
            printf("중복된 패킷이지만 ACK 전송\n");
        }
        struct pkt ACK; // 역시 ACK 전송은 하지만 tolayer5로 올려보내지는 않음
        ACK.acknum = packet.seqnum;
        ACK.seqnum = 0;
        ACK.checksum = 0;
        for (i = 0; i < 19; i++)
            ACK.payload[i] = ' ';
        ACK.payload[19] = '\0';
        printf("%d ACK 보냄!", ACK.acknum);
        tolayer3(1, ACK);
    }
    else { // 그 이외의 패킷은 무시
        printf("잘못된 패킷 무시\n");
    }
}

/* called when B's timer goes off */
void B_timerinterrupt(void) {

}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void) {
    rcv_base = 0;
    int i;
    for (i = 0; i < 1024; i++) {
        Receiver[i].seqnum = -1;
    }
}

/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event {
   float evtime;           /* event time */
   int evtype;             /* event type code */
   int eventity;           /* entity where event occurs */
   struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
   struct event *prev;
   struct event *next;
 };
struct event *evlist = NULL;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0  
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1

int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */ 
int nsimmax = 0;           /* number of msgs to generate, then stop */
float lossprob;            /* probability that a packet is dropped  */
float corruptprob = 0;         /* 패킷이 corrupt될 조건은 고려하지 않으므로 0으로 초기화 */
float lambda;              /* arrival rate of messages from layer 5 */   
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

main()
{
   struct event *eventptr;
   struct msg  msg2give;
   struct pkt  pkt2give;
   
   int i,j;
   char c; 
  
   init();
   A_init();
   B_init();
   
    while (1) {
        eventptr = evlist;            /* get next event to simulate */

        // 다음 이벤트 가져옴
        if (eventptr==NULL)
           goto terminate;
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist!=NULL)
           evlist->prev=NULL;

        // 이벤트 종류 출력
        if (TRACE>=2) {
            printf("\nEVENT time: %f,",eventptr->evtime);
            printf("  type: %d",eventptr->evtype);
            if (eventptr->evtype==0)
	            printf(", timerinterrupt  ");
            else if (eventptr->evtype==1)
                printf(", fromlayer5 ");
            else
	            printf(", fromlayer3 ");
           printf(" entity: %d\n",eventptr->eventity);
           }
        time = eventptr->evtime;        /* update time to next event time */

        // 끝났으면
        if (nsim==nsimmax)
	        break;                        /* all done with simulation */

        // 새로운 패킷 가져옴
        if (eventptr->evtype == FROM_LAYER5 ) {
            generate_next_arrival();   /* set up future arrival */
            /* fill in msg to give with string of same letter */    
            j = nsim % 26; 
            for (i=0; i<20; i++)  
               msg2give.data[i] = 97 + j;
            if (TRACE>2) {
               printf("          MAINLOOP: data given to student: ");
                 for (i=0; i<20; i++) 
                  printf("%c", msg2give.data[i]);
               printf("\n");
	        }
            nsim++;
            if (eventptr->eventity == A) {
                A_output(msg2give);
            }
            else {
                B_output(msg2give);
            }
        }
        
        // 애크를 받았을 때
        else if (eventptr->evtype ==  FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i=0; i<20; i++)  
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
	        if (eventptr->eventity ==A)      /* deliver packet by calling */
   	            A_input(pkt2give);            /* appropriate entity */
            else
   	            B_input(pkt2give);
	        free(eventptr->pktptr);          /* free the memory for packet */
        }

        // 타이머 인터럽트 발생시
        else if (eventptr->evtype ==  TIMER_INTERRUPT) {
            if (eventptr->eventity == A) 
	            A_timerinterrupt();
            else
	            B_timerinterrupt();
        }

        else {
	        printf("INTERNAL PANIC: unknown event type \n");
        }

        // 출력부
        printf("\n");
        for (i = 0; i < send_base; i++) // 이미 ack까지 다 받은 패킷
            printf("$ ");
        for (i = send_base; i < next_seq_num; i++) // ack는 받지 못했고, 전송은 한 패킷
            printf("%d ", i);
        for (i = next_seq_num; i < send_base + window_size; i++) // window 내의 빈공간 표시
            printf(". ");
        printf("\n");
        for (i = 0; i < rcv_base; i++) { // rcv_base 이전의 패킷들
            if (Receiver[i].seqnum != -1) { 
                if (Sender[i].acknum == 1) // 받았고, sender가 ack까지 받은 경우
                    printf("$ ");
                else {
                    printf("%d ", i); // 받았는데 sender가 ack는 받지 못한 경우
                }
            }
        }
        for (i = rcv_base; i < rcv_base + window_size; i++) { // 순서에 맞지 않게 도착한 패킷
            if (Receiver[i].seqnum == -1)
                printf("  ");
            else { 
                if (Sender[i].acknum == 1) // sender에서 ack까지 잘 받은 경우
                    printf("$ ");
                else
                    printf("%d ", i); // 아직 sender가 ack는 받지 못한 경우
            }
        }
        printf("\n");

        free(eventptr);
    }

    terminate:
    printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time,nsim);
}



init()                         /* initialize the simulator */
{
  int i;
  float sum, avg;
  float jimsrand();
  
  
   printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
   printf("Enter the number of messages to simulate: ");
   scanf("%d",&nsimmax);
   printf("Enter  packet loss probability [enter 0.0 for no loss]:");
   scanf("%f",&lossprob);
   //printf("Enter packet corruption probability [0.0 for no corruption]:");
   //scanf("%f",&corruptprob); --> 패킷의 corrupt에 대한 조건은 문제에서 주어지지 않음
   printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
   scanf("%f",&lambda);
   printf("Enter the window size:");
   scanf("%d", &window_size);
   printf("Enter the timeout:");
   scanf("%f", &RTT);
   printf("Enter TRACE:");
   scanf("%d",&TRACE);

   srand(9999);              /* init random number generator */
   sum = 0.0;                /* test random number generator for students */
   for (i=0; i<1000; i++)
      sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
   avg = sum/1000.0;
   if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n" ); 
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit();
    }

   ntolayer3 = 0;
   nlost = 0;
   ncorrupt = 0;

   time=0.0;                    /* initialize time to 0.0 */
   generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand() 
{
  double mmm = 32767;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  float x;                   /* individual students may need to change mmm */ 
  x = rand()/mmm;            /* x should be uniform in [0,1] */
  return(x);
}  

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
 
generate_next_arrival()
{
   double x,log(),ceil();
   struct event *evptr;
    char *malloc();
   float ttime;
   int tempint;

   if (TRACE>2)
       printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
 
   x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
                             /* having mean of lambda        */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + x;
   evptr->evtype =  FROM_LAYER5;
   evptr->eventity = A; // 메시지 sending의 주체는 항상 A
   insertevent(evptr);
} 

insertevent(p)
   struct event *p;
{
   struct event *q,*qold;

   if (TRACE>2) {
      printf("            INSERTEVENT: time is %lf\n",time);
      printf("            INSERTEVENT: future time will be %lf\n",p->evtime); 
      }
   q = evlist;     /* q points to header of list in which p struct inserted */
   if (q==NULL) {   /* list is empty */
        evlist=p;
        p->next=NULL;
        p->prev=NULL;
        }
     else {
        for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
              qold=q; 
        if (q==NULL) {   /* end of list */
             qold->next = p;
             p->prev = qold;
             p->next = NULL;
             }
           else if (q==evlist) { /* front of list */
             p->next=evlist;
             p->prev=NULL;
             p->next->prev=p;
             evlist = p;
             }
           else {     /* middle of list */
             p->next=q;
             p->prev=q->prev;
             q->prev->next=p;
             q->prev=p;
             }
         }
}

printevlist()
{
  struct event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
    }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
 struct event *q,*qold;

 if (TRACE>2)
    printf("          STOP TIMER: stopping timer at %f\n",time);
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
       /* remove this event */
       if (q->next==NULL && q->prev==NULL)
             evlist=NULL;         /* remove first and only event on list */
          else if (q->next==NULL) /* end of list - there is one in front */
             q->prev->next = NULL;
          else if (q==evlist) { /* front of list - there must be event after */
             q->next->prev=NULL;
             evlist = q->next;
             }
           else {     /* middle of list */
             q->next->prev = q->prev;
             q->prev->next =  q->next;
             }
       free(q);
       return;
     }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{
 struct event *q;
 struct event *evptr;
 char *malloc();

 if (TRACE>2)
    printf("          START TIMER: starting timer at %f\n",time);
 /* be nice: check to see if timer is already started, if so, then  warn */
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
   for (q=evlist; q!=NULL ; q = q->next)  
    if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) { 
      printf("Warning: attempt to start a timer that is already started\n");
      return;
      }
 
/* create future event for when timer goes off */
   evptr = (struct event *)malloc(sizeof(struct event));
   evptr->evtime =  time + increment;
   evptr->evtype =  TIMER_INTERRUPT;
   evptr->eventity = AorB;
   insertevent(evptr);
} 

/************************** TOLAYER3 ***************/
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
 struct pkt *mypktptr;
 struct event *evptr,*q;
 char *malloc();
 float lastime, x, jimsrand();
 int i;

 ntolayer3++;

 /* simulate losses: */
 if (jimsrand() < lossprob)  {
      nlost++;
      if (TRACE>0)    
	printf("          TOLAYER3: packet being lost\n");
      return;
    }  

/* make a copy of the packet student just gave me since he/she may decide */
/* to do something with the packet after we return back to him/her */ 
 mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
 mypktptr->seqnum = packet.seqnum;
 mypktptr->acknum = packet.acknum;
 mypktptr->checksum = packet.checksum;
 for (i=0; i<20; i++)
    mypktptr->payload[i] = packet.payload[i];
 if (TRACE>2)  {
   printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
	  mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<20; i++)
        printf("%c",mypktptr->payload[i]);
    printf("\n");
   }

/* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
 lastime = time;
/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
 for (q=evlist; q!=NULL ; q = q->next) 
    if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) ) 
      lastime = q->evtime;
 evptr->evtime =  lastime + 1 + 9*jimsrand();


 /* simulate corruption: */
 if (jimsrand() < corruptprob)  {
    ncorrupt++;
    if ( (x = jimsrand()) < .75)
       mypktptr->payload[0]='Z';   /* corrupt payload */
      else if (x < .875)
       mypktptr->seqnum = 999999;
      else
       mypktptr->acknum = 999999;
    if (TRACE>0)    
	printf("          TOLAYER3: packet being corrupted\n");
    }  

  if (TRACE>2)  
     printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
} 

tolayer5(AorB,datasent)
  int AorB;
  char datasent[20];
{
  int i;  
  if (TRACE>2) {
     printf("          TOLAYER5: data received: ");
     for (i=0; i<20; i++)  
        printf("%c",datasent[i]);
     printf("\n");
   }
  
}