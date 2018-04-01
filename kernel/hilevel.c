/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of
 * which can be found via http://creativecommons.org (and should be included as
 * LICENSE.txt within the associated archive or repository).
 */

#include "hilevel.h"
#define pnum 2
#define ni (pnum-1)
pcb_t pcb[ pnum ];
int current = 1;
int new = 1;

extern void     main_P3();
extern uint32_t tos_P3;
extern void     main_P4();
extern uint32_t tos_P4;

// extern void     main_P5();
// extern uint32_t tos_P5;

pid_t findMaxPriority(){
  int maxP = pcb[0].prtc;
  pid_t maxPi = pcb[0].pid;
  for (int i = 1; i < pnum ; i++){
    if (pcb[i].prtc > maxP){
      maxP = pcb[i].prtc;
      maxPi = pcb[i].pid;
    }
  }
  return maxPi;
}

void updatePriority(pid_t current){
  for (int i = 0; i < pnum; i++){
    if (pcb[i].pid != current){
      pcb[i].prtc = pcb[i].prtc + pcb[i].prtb;
    }
  }
}

void scheduler( ctx_t* ctx ) {
  //find max p process
  //preserve old, restore new
  //increment other processes p

  new = findMaxPriority();

  //preserve
  memcpy( &pcb[ current-1 ].ctx, ctx, sizeof( ctx_t ) );
  pcb[ current-1 ].status = STATUS_READY;

  //restore
  memcpy( ctx, &pcb[ (new-1) ].ctx, sizeof( ctx_t ) );
  pcb[ (new-1) ].status = STATUS_EXECUTING;

  //restore current priority counter
  pcb[current-1].prtc = 0;

  updatePriority(current);

  //update current index
  current = new;

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

  memset( &pcb[ 0 ], 0, sizeof( pcb_t ) );
  pcb[ 0 ].pid      = 1;
  pcb[ 0 ].status   = STATUS_READY;
  pcb[ 0 ].ctx.cpsr = 0x50;
  pcb[ 0 ].ctx.pc   = ( uint32_t )( &main_P3 );
  pcb[ 0 ].ctx.sp   = ( uint32_t )( &tos_P3  );
  pcb[ 0 ].prtb     = 1;
  pcb[ 0 ].prtc     = 0;

  memset( &pcb[ 1 ], 0, sizeof( pcb_t ) );
  pcb[ 1 ].pid      = 2;
  pcb[ 1 ].status   = STATUS_READY;
  pcb[ 1 ].ctx.cpsr = 0x50;
  pcb[ 1 ].ctx.pc   = ( uint32_t )( &main_P4 );
  pcb[ 1 ].ctx.sp   = ( uint32_t )( &tos_P4  );
  pcb[ 1 ].prtb     = 1;
  pcb[ 1 ].prtc     = 0;

  // memset( &pcb[ 2 ], 0, sizeof( pcb_t ) );
  // pcb[ 2 ].pid      = 3;
  // pcb[ 2 ].status   = STATUS_READY;
  // pcb[ 2 ].ctx.cpsr = 0x50;
  // pcb[ 2 ].ctx.pc   = ( uint32_t )( &main_P5 );
  // pcb[ 2 ].ctx.sp   = ( uint32_t )( &tos_P5  );

  memcpy( ctx, &pcb[ 0 ].ctx, sizeof( ctx_t ) );
  pcb[ 0 ].status = STATUS_EXECUTING;

  current = 1;
  new = 1;

  int_enable_irq();

  return;
}

void hilevel_handler_irq(ctx_t* ctx) {
  uint32_t id = GICC0->IAR;

  if( id == GIC_SOURCE_TIMER0 ) {
    scheduler( ctx ); TIMER0->Timer1IntClr = 0x01;
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
    case 0x04 : {//0x04 => exit
      //clean pcb for p5

      //call scheduler
    }

    default   : { // 0x?? => unknown/unsupported
      break;
    }
  }
  return;
}
