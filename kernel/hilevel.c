/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy ofm,
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
#define pnum 10
#define cprt 9
#define p3prt 8
#define p4prt 9
#define p5prt 10
#define p6prt 8

pcb_t pcb[ pnum ];
int cid = 0;
int nid = 0;
int newpcb = 0;
int pcbcount = 0;

extern void     main_P3();
extern void     main_P4();
extern void     main_P5();
extern void     main_P6();
extern void     main_console();

extern uint32_t tos_console;
extern uint32_t tos_3;
extern uint32_t tos_4;
extern uint32_t tos_5;
extern uint32_t tos_6;
extern uint32_t tos_7;
extern uint32_t tos_8;
extern uint32_t tos_9;
extern uint32_t tos_10;

uint32_t tosArray[] = {(uint32_t)&tos_console, (uint32_t)&tos_3, (uint32_t)&tos_4, (uint32_t)&tos_5,
  (uint32_t)&tos_6, (uint32_t)&tos_7, (uint32_t)&tos_8, (uint32_t)&tos_9, (uint32_t)&tos_10};

pid_t findMaxPriority(){
  pid_t maxP = pcb[0].prtc;
  pid_t maxPi = pcb[0].pid;
  if (pcbcount > 1) {
    for (int i = 1; i < pcbcount ; i++){
      if (pcb[i].prtc > maxP){
        if (pcb[i].status != STATUS_TERMINATED){
          maxP = pcb[0].prtc;
          maxPi = pcb[i].pid;
        }
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

  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'S', true );
  //preserve
  memcpy( &pcb[ cid ].ctx, ctx, sizeof( ctx_t ) );
  pcb[ cid ].status = STATUS_READY;
  //restore
  memcpy( ctx, &pcb[ nid ].ctx, sizeof( ctx_t ) );
  pcb[ nid ].status = STATUS_EXECUTING;

  cid = nid;

  PL011_putc( UART0, ' ', true );
  if(cid == 0) PL011_putc( UART0, 'C', true );

  updatePriority();

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

  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'T', true );

  //initialise console
  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 0;
  pcb[ 0 ].status   = STATUS_READY;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_console );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_console  );
  pcb[ 0 ].prtb = cprt;
  pcb[ 0 ].prtc = 0;

  // execute console
  memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
  pcb[ 0 ].status = STATUS_EXECUTING;

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
    TIMER0->Timer1IntClr = 0x01;        //pcb[ current ].status = STATUS_TERMINATED;
  }

  GICC0->EOIR = id;

  return;
}

void removePCB( int current ){
  PL011_putc( UART0, ' ', true );
  PL011_putc( UART0, 'R', true );

  int lastindex = pcbcount-1;
  int nextindex = current+1;
  int previndex = current-1;

  if ( current == lastindex ){ // if cid is last pcb
    newpcb = previndex;
  }
  else { //if there is a process after cid, ie next index
    pcb[ current ] = pcb[ nextindex ];
    newpcb = current;
  }
  pcbcount--;
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

      if(newpcb == 9){ //if no more spaces, re use first
         newpcb = 0;
      }
      break;
    }

    case 0x04 : { //0x04 => exit( x ), terminate process with status x
      //use cid
      PL011_putc( UART0, ' ', true );
      PL011_putc( UART0, 'X', true );

      removePCB(cid);
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
      uint32_t d = (uint32_t)&main_P6;

      pcb[ newpcb ].ctx.pc = x;

      //set base priority according to process image
      if ( x == a ){
        PL011_putc( UART0, '3', true );
        pcb[ newpcb ].prtb = p3prt;
      }
      else if ( x == b ){
        PL011_putc( UART0, '4', true );
        pcb[ newpcb ].prtb = p4prt;
      }
      else if ( x == c ){
        PL011_putc( UART0, '5', true );
        pcb[ newpcb ].prtb = p5prt;
      }
      else if ( x == d ){
        PL011_putc( UART0, '6', true );
        pcb[ newpcb ].prtb = p6prt;
      }
      break;
    }

    case 0x06 : { //0x06 => kill( pid, x ),
      PL011_putc( UART0, ' ', true );
      PL011_putc( UART0, 'K', true );

      int current = (uint32_t)ctx->gpr[0];
      if (current < pcbcount) {
        removePCB(current);
      }
      else{
        PL011_putc( UART0, 'x', true );
      }
      break;
    }

    case 0x07 : { // 0x07 => nice( pid, x )
      //increase prtb of pcb[ pid ] to x
      PL011_putc( UART0, ' ', true );
      PL011_putc( UART0, 'B', true );

      int x = (uint32_t)ctx->gpr[0];
      int s = (uint32_t)ctx->gpr[1];
      pcb[ x ].prtb = pcb[ x ].prtb + s;
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
