/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
#define pnum 10
#define ni (pnum-1)
pcb_t pcb[ pnum ];
int cid = 0;
int nid = 0;
int newpcb = 0;
int pcbcount = 0;
queue_t Q;
int qpoint = 0;

int peek() {
  return (Q.array[Q.front].id);
}
bool isEmpty() {
  return (Q.itemcount==0);
}
bool isFull() {
  return (Q.itemcount==pnum);
}
int size() {
  return (Q.itemcount);
}
void insert(int data) {
   PL011_putc( UART0, ' ', true );
   PL011_putc( UART0, 'I', true );
   PL011_putc( UART0, ' ', true );
   if(!isFull()) {
      if (Q.rear == pnum-1) {
        Q.rear = -1;
      }
      Q.array[++Q.rear].id = data;
      Q.itemcount++;
   }
}
int removeData() {
   PL011_putc( UART0, ' ', true );
   PL011_putc( UART0, 'R', true );
   PL011_putc( UART0, ' ', true );
   int data = Q.array[Q.front++].id;
   if (Q.front == pnum) {
     Q.front = 0;
   }
   Q.itemcount--;
   return data;
}


extern void     main_P3();
extern uint32_t tos_P3;

extern void     main_P4();
extern uint32_t tos_P4;

extern void     main_P5();
extern uint32_t tos_P5;

extern void     main_console();
extern uint32_t tos_console;

extern uint32_t tos_6;
extern uint32_t tos_7;
extern uint32_t tos_8;
extern uint32_t tos_9;
extern uint32_t tos_10;

uint32_t tosArray[] = {(uint32_t)&tos_console, (uint32_t)&tos_P3, (uint32_t)&tos_P4, (uint32_t)&tos_P5,
  (uint32_t)&tos_6, (uint32_t)&tos_7, (uint32_t)&tos_8, (uint32_t)&tos_9, (uint32_t)&tos_10};

pid_t findMaxPriority(){
  pid_t maxP = pcb[0].prtc;
  pid_t maxPi = pcb[0].pid;
  if (pcbcount > 1) {
    for (int i = 1; i < pcbcount ; i++){
      if (pcb[i].prtc > maxP){
        maxP = pcb[0].prtc;
        maxPi = pcb[i].pid;
      }
    }
  }
  return maxPi;
}

void updatePriority(){
  if (pcbcount > 1) {
    for (int i = 0; i < pcbcount; i++){
      if (pcb[i].pid != cid){
        pcb[i].prtc = pcb[i].prtc + pcb[i].prtb;
      }
    }
  }
}

void scheduler( ctx_t* ctx ) {
  //find max p process
  //   //preserve old, restore new
  //   //increment other processes p

  nid = findMaxPriority();

  // if (isEmpty()) {
  //   insert(0);
  //   PL011_putc( UART0, 'Y', true );
  // } else {
  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'S', true );
  PL011_putc( UART0, ' ', true );

  //pid_t nid = removeData();
  //preserve
  memcpy( &pcb[ cid ].ctx, ctx, sizeof( ctx_t ) );
  pcb[ cid ].status = STATUS_READY;
  //restore
  memcpy( ctx, &pcb[ nid ].ctx, sizeof( ctx_t ) );
  pcb[ nid ].status = STATUS_EXECUTING;

  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'a', true );
  PL011_putc( UART0, ' ', true );

  cid = nid;

  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'b', true );
  PL011_putc( UART0, ' ', true );

  updatePriority();

  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'c', true );
  PL011_putc( UART0, ' ', true );


  return;
}

void hilevel_handler_rst(ctx_t* ctx) {

  TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
  TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
  TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
  TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
  TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer

  GICC0->PMR          = 0x000000F0; // unmask all            interrupts
  GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt   pcb[ (current + 1) ].status = STATUS_EXECUTING;
  GICC0->CTLR         = 0x00000001; // enable GIC interface
  GICD0->CTLR         = 0x00000001; // enable GIC distributor

  //initialise console
  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 0;
  pcb[ 0 ].status   = STATUS_READY;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_console  );
  pcb[ 0 ].prtb = 1;
  pcb[ 0 ].prtc = 0;

  // //initialise p3
  // memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
  // pcb[ 1 ].pid      = 1;
  // pcb[ 1 ].status   = STATUS_READY;
  // pcb[ 1 ].ctx.cpsr = 0x50;
  // pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P3 );
  // pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_P3  );
  // pcb[ 1 ].prtb     = 1;
  // pcb[ 1 ].prtc     = 0;
  //
  // //initialise p4
  // memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );
  // pcb[ 2 ].pid      = 2;
  // pcb[ 2 ].status   = STATUS_READY;
  // pcb[ 2 ].ctx.cpsr = 0x50;
  // pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P4 );
  // pcb[ 2 ].ctx.sp   = ( uint32_t )( &tos_P4  );
  // pcb[ 2 ].prtb     = 1;
  // pcb[ 2 ].prtc     = 0;
  //
  // //initialise p5
  // memset( &pcb[ 3 ], 0, sizeof( pcb_t ) );
  // pcb[ 3 ].pid      = 3;
  // pcb[ 3 ].status   = STATUS_READY;
  // pcb[ 3 ].ctx.cpsr = 0x50;
  // pcb[ 3 ].ctx.pc   = ( uint32_t )( &main_P5 );
  // pcb[ 3 ].ctx.sp   = ( uint32_t )( &tos_P5  );

  // execute console
  memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
  pcb[ 0 ].status = STATUS_EXECUTING;

  Q.front = 0;
  Q.rear = -1;
  Q.itemcount = 0;

  cid = 0;
  nid = 0;
  newpcb = 0;
  pcbcount = 1;

  int_enable_irq();

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  uint32_t id = GICC0->IAR;

  if( id == GIC_SOURCE_TIMER0 ) {
    scheduler(ctx);
    TIMER0->Timer1IntClr = 0x01;

  }

  GICC0->EOIR = id;

  return;
}

void hilevel_handler_svc(ctx_t* ctx, uint32_t id) {

  switch( id ) {
    case 0x00 : { // 0x00 => yield()
      scheduler( ctx );
      break;
    }

    case 0x01 : { // 0x01 => write( fd, x, n )
      int   fd = ( int   )( ctx->gpr[ 0 ] );
      char*  x = ( char* )( ctx->gpr[ 1 ] );
      int    n = ( int   )( ctx->gpr[ 2 ] );

      for( int i = 0; i < n; i++ ) {
        PL011_putc( UART0, *x++, true );
      }

      ctx->gpr[ 0 ] = n;
      break;
    }

    case 0x03 : { //0x03 => fork()
      // create new child process with unique PID
      // replicate state (e.g., address space) of parent in child,
      // parent and child both return from fork, and continue to execute after the call point,
      // return value is 0 for child, and PID of child for parent.
      PL011_putc( UART0, ' ', true );
      PL011_putc( UART0, 'F', true );

      newpcb++;

      uint32_t newtos = tosArray[newpcb];

      //assign pcb for child
      memset( &pcb[ newpcb ], 0, sizeof( pcb_t ) );
      pcb[ newpcb ].pid      = newpcb;
      pcb[ newpcb ].status   = STATUS_READY;
      pcb[ newpcb ].ctx.cpsr = 0x50;
      pcb[ newpcb ].ctx.pc   = ( uint32_t )( ctx->pc );
      pcb[ newpcb ].ctx.sp   = ( uint32_t )( newtos );
      pcb[ newpcb ].prtb     = pcb[ cid ].prtb;
      pcb[ newpcb ].prtc     = pcb[ cid ].prtc;

      pcbcount++;

      // if(newpcb < 10){ //if space for new processes
      //   newpcb++;
      // } else{
      //   PL011_putc( UART0, 'K', true );
        //cant make new process
        //do something about executing parent?
      break;
    }
    case 0x04 : { //0x04 => exit( x ), terminate process with status x
      //use cid
      //
      PL011_putc( UART0, ' ', true );
      PL011_putc( UART0, 'X', true );
      int y = pcbcount-1;
      int z = cid+1;
      int x = cid-1;

      if ( cid == y ){ // if cid is last pcb
        //change newpcb to cid
        //wipe pcb[ cid ]
        newpcb = x;
        memset( &pcb[ cid ], 0, sizeof( pcb_t ) );

      } else { //if there is a process after cid, ie at z
        //put pcb[cid+1] into pcb[cid]
        //newpcb = cid+1
        //wipe pcb[ cid+1 ]
        pcb[ cid ] = pcb[ z ];
        newpcb = cid;
        memset( &pcb[ z ], 0, sizeof( pcb_t ) );
      }
      cid = 0;
      pcbcount--;
      scheduler(ctx);
      break;
    }
    case 0x05 : { //0x05 => exec( x ), start executing at address x
      //replace current process image (e.g., text segment) with with new process image: effectively this means execute a new program,
      //reset state (e.g., stack pointer); continue to execute at the entry point of new program,
      //no return, since call point no longer exists
      PL011_putc( UART0, ' ', true );
      PL011_putc( UART0, 'E', true );
      PL011_putc( UART0, ' ', true );

      uint32_t x = (uint32_t)ctx->gpr[0];

      //ctx->pc = x;
      uint32_t a = (uint32_t)&main_P3;
      uint32_t b = (uint32_t)&main_P4;
      uint32_t c = (uint32_t)&main_P5;


      pcb[ newpcb ].ctx.pc = x;

      if ( x == a ){ //p3 has base priority 1
        pcb[ newpcb ].prtb = 1;
      }
      else if ( x == a ){ //p4 has base priority 2
        pcb[ newpcb ].prtb = 2;
      }
      else if ( x == a ){ //p5 has base priority 3
        pcb[ newpcb ].prtb = 3;
      }

      //insert(newpcb);

      break;
    }
    case 0x06 : { //0x06 => kill( pid, x ),
      int x = (uint32_t)ctx->gpr[0];
      int y = pcbcount-1;
      int z = x+1;

      if ( x == y ){ // if x is last pcb
        //change newpcb to x
        //wipe pcb[ x ]
        newpcb = x;
        memset( &pcb[ x ], 0, sizeof( pcb_t ) );

      } else { //if there is a process after x
        //memcpy pcb[x+1] into pcb[x]
        //newpcb = x+1
        //wipe pcb[ x+1 ]
        pcb[ x ] = pcb[ z ];
        newpcb = z;
        memset( &pcb[ z ], 0, sizeof( pcb_t ) );
      }
      if ( x == cid ){
        cid = 0;
      }
      pcbcount--;
      scheduler(ctx);
      break;
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }
  return;
}
// --------------------------------------------------
// CODE FOR 1B (PRIORITY)

// #include "hilevel.h"
// #define pnum 2
// #define ni (pnum-1)
// pcb_t pcb[ pnum ];
// int cid = 1;
// int nid = 1;
// int current = 0;
// int next = 0;
//
// extern void     main_P3();
// extern uint32_t tos_P3;
// extern void     main_P4();
// extern uint32_t tos_P4;
//
// // extern void     main_P5();
// // extern uint32_t tos_P5;
//
// pid_t findMaxPriority(){
//   int maxP = pcb[0].prtc;
//   pid_t maxPi = pcb[0].pid;
//   for (int i = 1; i < pnum ; i++){
//     if (pcb[i].prtc > maxP){
//       maxP = pcb[i].prtc;
//       maxPi = pcb[i].pid;
//     }
//   }
//   return maxPi;
// }
//
// void updatePriority(){
//   for (int i = 0; i < pnum; i++){
//     if (pcb[i].pid != cid){
//       pcb[i].prtc = pcb[i].prtc + pcb[i].prtb;
//     }
//   }
// }
//
// void scheduler( ctx_t* ctx ) {
//   //find max p process
//   //preserve old, restore new
//   //increment other processes p
//
//   nid = findMaxPriority();
//   next = nid-1;
//
//   //preserve
//   memcpy( &pcb[ current ].ctx, ctx, sizeof( ctx_t ) );
//   pcb[ current ].status = STATUS_READY;
//
//   //restore
//   memcpy( ctx, &pcb[ next ].ctx, sizeof( ctx_t ) );
//   pcb[ next ].status = STATUS_EXECUTING;
//
//   updatePriority();
//
//   //update current index
//   cid = nid;
//   current = next;
//
//   return;
// }
//
// void hilevel_handler_rst(ctx_t* ctx) {
//
//   TIMER0->Timer1Load  = 0x00100000; // select period = 2^20 ticks ~= 1 sec
//   TIMER0->Timer1Ctrl  = 0x00000002; // select 32-bit   timer
//   TIMER0->Timer1Ctrl |= 0x00000040; // select periodic timer
//   TIMER0->Timer1Ctrl |= 0x00000020; // enable          timer interrupt
//   TIMER0->Timer1Ctrl |= 0x00000080; // enable          timer
//
//   GICC0->PMR          = 0x000000F0; // unmask all            interrupts
//   GICD0->ISENABLER1  |= 0x00000010; // enable timer          interrupt   pcb[ (current + 1) ].status = STATUS_EXECUTING;
//   GICC0->CTLR         = 0x00000001; // enable GIC interface
//   GICD0->CTLR         = 0x00000001; // enable GIC distributor
//
//   memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
//   pcb[ 0 ].pid      = 1;
//   pcb[ 0 ].status   = STATUS_READY;
//   pcb[ 0 ].ctx.cpsr = 0x50;
//   pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
//   pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_P3  );
//   pcb[ 0 ].prtb     = 3;
//   pcb[ 0 ].prtc     = 0;
//
//   memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
//   pcb[ 1 ].pid      = 2;
//   pcb[ 1 ].status   = STATUS_READY;
//   pcb[ 1 ].ctx.cpsr = 0x50;
//   pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
//   pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_P4  );
//   pcb[ 1 ].prtb     = 1;
//   pcb[ 1 ].prtc     = 0;
//
//   // memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );
//   // pcb[ 2 ].pid      = 3;
//   // pcb[ 2 ].status   = STATUS_READY;
//   // pcb[ 2 ].ctx.cpsr = 0x50;
//   // pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P5 );
//   // pcb[ 2 ].ctx.sp   = ( uint32_t )( &tos_P5  );
//
//   memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
//   pcb[ 0 ].status = STATUS_EXECUTING;
//
//   cid = 1;
//   nid = 1;
//   current = 0;
//   next = 0;
//
//   int_enable_irq();
//
//   return;
// }
//
// void hilevel_handler_irq(ctx_t* ctx) {
//   uint32_t id = GICC0->IAR;
//
//   if( id == GIC_SOURCE_TIMER0 ) {
//     scheduler( ctx ); TIMER0->Timer1IntClr = 0x01;
//   }
//
//   GICC0->EOIR = id;
//
//   return;
// }
//
// void hilevel_handler_svc(ctx_t* ctx, uint32_t id) {
//
//   switch( id ) {
//     case 0x00 : { // 0x00 => yield()
//       scheduler( ctx );
//       break;
//     }
//
//     case 0x01 : { // 0x01 => write( fd, x, n )
//       int   fd = ( int   )( ctx->gpr[ 0 ] );
//       char*  x = ( char* )( ctx->gpr[ 1 ] );
//       int    n = ( int   )( ctx->gpr[ 2 ] );
//
//       for( int i = 0; i < n; i++ ) {
//         PL011_putc( UART0, *x++, true );
//       }
//
//       ctx->gpr[ 0 ] = n;
//       break;
//     }
//     case 0x04 : {//0x04 => exit
//       //clean pcb for p5
//
//       //call scheduler
//     }
//
//     default   : { // 0x?? => unknown/unsupported
//       break;
//     }
//   }
//   return;
// }
//
