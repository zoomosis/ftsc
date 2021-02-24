/*--------------------------------------------------------------------------*/
/*                                                                          */
/*      ------------         Bit-Bucket Software <no-Inc>                   */
/*      \ 10001101 /         Writers and Distributors of                    */
/*       \ 011110 /          No-Cost<no-tm> Software.                       */
/*        \ 1011 /                                                          */
/*         ------                                                           */
/*                                                                          */
/*  Copyright (C) 1987, 1988, 1989 by Robert Hartman and Vincent Perriello  */
/*                                                                          */
/*                                                                          */
/*               This module was written by Vince Perriello                 */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/*  This file contains a sample YooHoo sender and receiver module. You may  */
/*  use it in any program, provided you give credit to Bit-Bucket Software  */
/*  in any informational screens or literature pertaining to your program   */
/*  that contains other such information (such as your own copyrights).     */
/*                                                                          */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <conio.h>
#include <string.h>

#define WAZOO_SECTION
#define MATRIX_SECTION
#define isBITBRAIN 0x1b

#include "com.h"
#include "xfer.h"
#include "zmodem.h"
#include "keybd.h"
#include "sbuf.h"
#include "sched.h"
#include "externs.h"
#include "prototyp.h"

static int Send_Hello_Packet (int);
static int Recv_Hello_Packet (void);

/*--------------------------------------------------------------------------*/
/* YOOHOO SENDER    (used when I am the CALLING system)                     */
/*--------------------------------------------------------------------------*/
int YooHoo_Sender (void)
{
   char *sptr;

   if (un_attended && fullscreen)
      {
      sb_move (filewin, 2, 2);
      sb_puts (filewin, "YooHoo");
      sb_show ();
      }
   else
      {
      set_xy ("YooHoo ");
      }

   if (!Send_Hello_Packet (1))
      sptr = "No Send";

   else
      {
      if (TIMED_READ (30) == YOOHOO)
         return Recv_Hello_Packet ();

      status_line ("!No YOOHOO/2U2");
      sptr = IDUNNO_msg;
      }

   message (sptr);
   return (0);
}

/*--------------------------------------------------------------------------*/
/* YOOHOO RECEIVER  (Used when I am the CALLED system)                      */
/*--------------------------------------------------------------------------*/

int YooHoo_Receiver (void)
{
   int i;
   int c;

   if (un_attended && fullscreen)
      {
      sb_move (filewin, 2, 2);
      sb_puts (filewin, "YooHoo");
      sb_show ();
      }
   else
      {
      set_xy ("YooHoo ");
      }

   if (!(i = Recv_Hello_Packet()))
      return (0);

   for (i = 0; (CARRIER) && (i < 10); i++)
      {
      if ((c = TIMED_READ (10)) == ENQ)
         return Send_Hello_Packet (0) ? 1 : (b_init (), 0);

      if (c > 0)
         {
         message (NULL);
         cprintf ("[%x] ", c);
         CLEAR_INBOUND ();
         }

      SENDBYTE (YOOHOO);
      }

   hup = 1;
   message (FUBAR_msg);
   b_init ();
   return (0);

}                                                /* YooHoo Receiver */

/*--------------------------------------------------------------------------*/
/* SEND HELLO PACKET                                                        */
/*--------------------------------------------------------------------------*/
static int Send_Hello_Packet (Sender)
int Sender;
{
   int i;
   struct _Hello Hello;
   byte *sptr;
   long response_timer;
   word crc;
   word num_errs = 0;

   /*--------------------------------------------------------------------*/
   /* Setup HELLO structure                                              */
   /*--------------------------------------------------------------------*/
   memset ((char *) &Hello, 0, sizeof (struct _Hello));

   Hello.signal = 'o';
   Hello.hello_version = 1;

   Hello.product = isBITBRAIN;
   Hello.product_maj = BINK_MAJVERSION;
   Hello.product_min = BINK_MINVERSION;

   strncpy (Hello.my_name, system_name, 58);
   Hello.my_name[58] = '\0';

   strncpy (Hello.sysop, sysop, 19);
   Hello.sysop[19] = '\0';

   Hello.my_zone = alias[assumed].Zone;
   if ((pvtnet >= 0) && (Sender) &&
       ((called_zone == alias[assumed].Zone) || (called_zone == 0)) &&
       (called_net == boss_net) && (called_node == boss_node))
      {
      Hello.my_net = boss_net;
      Hello.my_node = boss_node;
      Hello.my_point = alias[assumed].Node;
      }
   else
      {
      Hello.my_net = alias[assumed].Net;
      Hello.my_node = alias[assumed].Node;
      }

   Hello.capabilities = (no_zapzed) ? 0 : ZED_ZAPPER;
   Hello.capabilities |= Y_DIETIFNA;
#ifdef JANUS
   if (janus_baud >= cur_baud)
      Hello.capabilities |= DOES_IANUS;
#endif

   if (n_getpassword (remote_zone, remote_net, remote_node))
      {
      strncpy (Hello.my_password, remote_password, 8);
      }

   if ((matrix_mask & TAKE_REQ) &&
       ((!Sender) || (on_our_nickel)))
      Hello.capabilities |= WZ_FREQ;

   /*--------------------------------------------------------------------*/
   /* Disable handshaking and ^C/^K handling                             */
   /*--------------------------------------------------------------------*/
   XON_DISABLE ();

   /*--------------------------------------------------------------------*/
   /* Send the packet.                                                   */
   /* Load outbound buffer quickly, and get modem busy sending.          */
   /*--------------------------------------------------------------------*/

xmit_packet:

   SENDBYTE (0x1f);

   sptr = (char *) (&Hello);
   SENDCHARS (sptr, 128, 1);

   /*--------------------------------------------------------------------*/
   /* Calculate CRC while modem is sending its buffer                    */
   /*--------------------------------------------------------------------*/
   for (crc = i = 0; i < 128; i++)
      {
      crc = xcrc (crc, (byte) sptr[i]);
      }

   CLEAR_INBOUND ();

   SENDBYTE ((unsigned char) (crc >> 8));
   SENDBYTE ((unsigned char) (crc & 0xff));

   response_timer = timerset (4000);

   while (!timeup(response_timer) && CARRIER)
      {
      if (!CHAR_AVAIL ())
         {
         if (got_ESC ())
            {
            DTR_OFF ();
            sptr  = KBD_msg;
            goto no_response;
            }
	 time_release ();
         continue;
	 }	 

      switch (i = TIMED_READ (0))
         {
         case ACK:
            return (1);

         case '?':
            message ("drats");

         case ENQ:
            if (++num_errs == 10)
                {
                sptr = FUBAR_msg;
                goto no_response;
                }
            goto xmit_packet;

         default:
            if (i > 0)                   /* Could just be line noise */
               {
/*             cprintf ("[%x] ", i);*/
               }
            break;
         }
      }

   if (!CARRIER)
      sptr = CARRIER_msg;
   else
      sptr  = TIME_msg;

no_response:

   message (sptr);
   return (0);

}                                                /* Send Hello */


/*--------------------------------------------------------------------------*/
/* RECEIVE HELLO PACKET                                                     */
/*--------------------------------------------------------------------------*/
static int Recv_Hello_Packet (void)
{
   int i;
   int c;
   struct _Hello Hello;
   byte *sptr;
   byte num_errs = 0;
   word crc;
   word lsb;
   word msb;
   long master_timeout, hello_timeout;
   char junkbuff[128];

   hup = 0;
   sptr = NULL;

   /*--------------------------------------------------------------------*/
   /* Clean up any mess that may be around                               */
   /*--------------------------------------------------------------------*/
   CLEAR_OUTBOUND ();
   CLEAR_INBOUND ();
   XON_DISABLE ();

   /*--------------------------------------------------------------------*/
   /* Get the Hello structure                                            */
   /*--------------------------------------------------------------------*/

/* big_pause (1);  */                           /* Wait for quiet. */
   
   if (un_attended && fullscreen)
      {
      sb_move (filewin, 2, 2);
      sb_puts (filewin, "YooHoo/2U2");
      sb_show ();
      }
   else
      {
      set_xy ("YooHoo/2U2 ");
      }

   SENDBYTE (ENQ);                               /* Let the other system know
                                                  * we heard YooHoo. */

   master_timeout = timerset (12000);		/* No more than 2 mins! */

watch_for_header:

   while (1)
      {
      if (sptr)
         {
         message (sptr);
         sptr = NULL;
         }

      if ((c = TIMED_READ (20)) == 0x1f)
         break;

      if (got_ESC ())
         {
         sptr = KBD_msg;
         goto receive_failed;
         }

      if (!CARRIER)
         {
         sptr = CARRIER_msg;
         goto receive_failed;
         }

      if (timeup(master_timeout))
         goto timeout;

      if (c >= 0)                               /* noise? */
         {
         hello_timeout = timerset (1000);       /* Look for up to 10 secs  */
         while (((c = PEEKBYTE ()) >= 0) && (c != 0x1f) && (CARRIER))
            {
            if (timeup(hello_timeout))
               break;
            MODEM_IN ();                        /* Eat non-YooHoo chars    */
            }

         if (c != 0x1f)                         /* If we didn't get YooHoo */
            {
            CLEAR_INBOUND ();                   /* Throw out what we have  */
            SENDBYTE (ENQ);                     /* Start over with ENQ     */
            }
         }

      }                         /* while */


/*receive_packet:*/

   sptr = (char *) (&Hello);

   hello_timeout = timerset (3000);

   for (i = 0, crc = 0; i < 128; i++)
      {

      while (PEEKBYTE () < 0)
         {
         if (timeup (master_timeout) || timeup (hello_timeout))
            goto timeout;
        
         if (got_ESC ())
            {
            sptr = KBD_msg;
            goto receive_failed;
            }

         if (!CARRIER)
            {
            sptr = CARRIER_msg;
            goto receive_failed;
            }

         time_release ();
	 }

      c = TIMED_READ (0);

      sptr[i] = (char) c;
      crc = xcrc (crc, (byte) c);
      }

   if (!CARRIER)
      {
      sptr = CARRIER_msg;
      goto receive_failed;
      }

   if (((msb = TIMED_READ (10)) < 0) || ((lsb = TIMED_READ (10)) < 0))
      {
      sptr = SHRT_msg;
      goto hello_error;
      }

   if (((msb << 8) | lsb) == crc)
      goto process_hello;

   sptr = CRC_msg;

hello_error:

   if (timeup(master_timeout))
      goto timeout;

   if ((num_errs++) > 10)
      {
      sptr = FUBAR_msg;
      goto receive_failed;
      }

   CLEAR_INBOUND ();
   SENDBYTE ('?');
   goto watch_for_header;

process_hello:

   Hello.my_name[42] = '\0';
   Hello.sysop[19] = '\0';
   remote_zone = Hello.my_zone;
   remote_net = Hello.my_net;
   remote_node = Hello.my_node;
   remote_point = Hello.my_point;

   remote_capabilities = (Hello.capabilities) | Y_DIETIFNA;

   if (nodefind (remote_zone, remote_net, remote_node, 0) && !remote_zone)
      remote_zone = found_zone;

   sprintf (junkbuff, "*%s (%u:%u/%u.%u)",
            Hello.my_name,
            remote_zone,
            remote_net,
            remote_node,
            remote_point);
   status_line (junkbuff);

   if ((pvtnet >= 0) &&
       ((remote_zone == alias[assumed].Zone) || (remote_zone == 0)) &&
       (remote_net == boss_net) && (remote_node == boss_node) &&
       (remote_point > 0))
      {
      remote_net = pvtnet;
      remote_node = Hello.my_point;
      remote_point = 0;
      }

   switch (Hello.product)
      {
      case isBITBRAIN:
      case isFIDO:
      case isSPARK:
      case isSEA:
      case isHENK:
      case isTABBIE:
      case isWOLF:
      case isQMM:
      case isFD:
      case isTERUS:
      case isCYGNUS:
      case isGSPOINT:
      case isBGMAIL:
      case isCROSSBOW:
      case isDBRIDGE:
      case isDAISY:
      case isTHEBOX:
         status_line ("*Remote uses %s Version %d.%02d",
             prodcode[Hello.product], Hello.product_maj, Hello.product_min);
         break;

      case isOPUS:
         status_line ("*Remote Uses Opus Version %d.%02d", Hello.product_maj,
                      (Hello.product_min == 48) ? 0 : Hello.product_min);
         break;
      default:
         status_line ("*Remote Uses Program '%02x' Version %d.%02d",
                      Hello.product, Hello.product_maj, Hello.product_min);
         break;
      }

   if (Hello.sysop[0])
      status_line (":Sysop: %s", Hello.sysop);

   if (remote_point > 0)
      {
      remote_point = 0;
      remote_node = -1;
      }

   if (n_getpassword (remote_zone, remote_net, remote_node))
      {
      if (n_password (Hello.my_password, remote_password))
         {
         DTR_OFF ();                            /* He'll never get it right */
         timer (2);                             /* Wait two secs */
         sptr = NULL;                           /* Already logged something */
         goto receive_failed;
         }
      }

   CLEAR_INBOUND ();

   SENDBYTE (ACK);
   SENDBYTE (YOOHOO);

   return (1);
 
timeout:

   sptr = TIME_msg;

receive_failed:

   hup = 1;
   message (sptr);
   b_init ();
   return (0);
}                                               /* Recv Hello */

/* END OF FILE: yoohoo.c */
