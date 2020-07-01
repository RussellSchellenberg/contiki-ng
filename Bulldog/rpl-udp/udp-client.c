#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <msp430.h>
#include "dev/lpm.h"
#include "sys/log.h"
#include <stdio.h>
#include <stdlib.h>
#include "dev/sht11/sht11-sensor.h"

// --- START Code Block for Simulation ---
#include "sys/energest.h"
#include "sys/timer.h"

#include "dev/watchdog.h"
#include "dev/msp430-lpm-override.h"
extern int msp430_lpm4_required;

volatile long start, end;

unsigned long LSPC, DSPC;
unsigned long voltage = 3.0; // Units = Volts
unsigned long ActiveCurrent = 330;
unsigned long LPM1Current = 75;
unsigned long LPM4Current_DIV = 50;
unsigned long ActiveTime = 0;
unsigned long LPM1Time = 0;
unsigned long LPM4Time = 0;
unsigned long TotalTime = 0;

//#define ENERGEST_CONF_ON 1 // This was manually changed in the file for the Sim
// --- END Code Block for Simulation ---

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8800
#define UDP_SERVER_PORT	5700

#define SEND_INTERVAL (5 * CLOCK_SECOND)

enum p_type{SYN, DATA};

int get_temperature(){
  return ((sht11_sensor.value(SHT11_SENSOR_TEMP)/10)-396)/10;
}

static unsigned long to_seconds(uint64_t time){ // Sim
  return (unsigned long)(time / ENERGEST_SECOND);
}

static struct simple_udp_connection udp_conn;
uint8_t p_data[32];

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
PROCESS(udp_client_sleep, "UDP client data");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/

static void encode_packet(uint8_t* dest, uint16_t datalen, uint8_t* data, enum p_type type){
  int i;
  (*dest) = type;
  for(i = 0;i < datalen; i++){
    dest[i+2] = data[i];
  }
}

static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  
  end = clock_seconds(); // For Deep sleep
  watchdog_stop();
  msp430_lpm4_required = 0;
  ENERGEST_OFF(ENERGEST_TYPE_DEEP_LPM);
  ENERGEST_SWITCH(ENERGEST_TYPE_CPU, ENERGEST_TYPE_LPM);
  LPM1;
  watchdog_start();

  P5OUT &= ~(1<<5);
  P5OUT |= (1<<4);
  static char str[32];

  LOG_INFO("Received '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
  LOG_INFO("Sending Temperature: %u\n", get_temperature());
  snprintf(str, sizeof(str), "%u", get_temperature());
  simple_udp_sendto(&udp_conn, str, strlen(str), sender_addr);

  // --- START Code Block for Simulation ---
  /* Update all energest times. */
  energest_flush();
  printf("\nEnergest:\n");

ActiveTime = (to_seconds(energest_type_time(ENERGEST_TYPE_CPU)));
LPM1Time = (to_seconds(energest_type_time(ENERGEST_TYPE_LPM)));
LPM4Time = (to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM))); // Difference in time between going to sleep and waking up
TotalTime = (to_seconds(ENERGEST_GET_TOTAL_TIME()));

LSPC = voltage *((ActiveTime * ActiveCurrent) + ((LPM1Time + LPM4Time) * LPM1Current));
printf("Light Sleep System:\n");
printf("     Active: %5lus, LPM1: %5lus, LPM4:     0s, Total Time: %5lus\n", ActiveTime, (LPM1Time + LPM4Time), TotalTime);
printf("     Energy Used: %10luuW\n\n", LSPC);
printf("Simulation_Data: Light %5lu %5lu 0 %5lu %10lu\n", ActiveTime, (LPM1Time + LPM4Time), TotalTime, LSPC);

DSPC = voltage *((ActiveTime * ActiveCurrent) + (LPM1Time * LPM1Current) + (LPM4Time / LPM4Current_DIV));
printf("Deep Sleep System:\n");
printf("     Active: %5lus, LPM1: %5lus, LPM4: %5lus, Total Time: %5lus\n\n", ActiveTime, LPM1Time, LPM4Time, TotalTime);
printf("     Energy Used: %10luuW\n\n", DSPC);
printf("Simulation_Data: Deep %5lu %5lu %5lu %5lu %10lu\n", ActiveTime, LPM1Time, LPM4Time, TotalTime, DSPC);
//printf("Active: %5lus\n", ActiveTime);
// Trying to get the Deep LPM working
/*
  printf("Active:          %lus, LPM:      %gs, DEEP LPM: %gs,  Total time: %lus\n",
         to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
         (double)()(to_seconds(energest_type_time(ENERGEST_TYPE_LPM))-DeepSleepTime)),
         DeepSleepTime,
         to_seconds(ENERGEST_GET_TOTAL_TIME()));
*/

   //LSPC = voltage *( ((ActiveCurrent)*(to_seconds(energest_type_time(ENERGEST_TYPE_CPU)))) + LPM1Current*(to_seconds(energest_type_time(ENERGEST_TYPE_LPM))));
//   LSPC = LPM1Current*(to_seconds(energest_type_time(ENERGEST_TYPE_LPM)));
  // printf("Light Sleep Power Consumption: %g W\n", (double)LSPC);

// --- END Code Block for Simulation ---

  process_exit(&udp_client_sleep);
  process_start(&udp_client_sleep, NULL);

#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  void clock_init (void);
  static struct etimer periodic_timer;
  static char str[32];
  uip_ipaddr_t dest_ipaddr;
 
  P5DIR |= 0x70;

  PROCESS_BEGIN();

  P5OUT &= ~(1<<5);

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  lpm_on();
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      /* Send to DAG root */
      LOG_INFO("Sending SYN to ");
      LOG_INFO_6ADDR(&dest_ipaddr);
      LOG_INFO_("\n");

      snprintf(str, sizeof(str), "SYN");
      encode_packet(p_data, 0, (uint8_t*)NULL, SYN);
      simple_udp_sendto(&udp_conn, p_data, 2, &dest_ipaddr);
      process_exit(&udp_client_sleep);
      process_start(&udp_client_sleep, NULL);
      break;
    } else {
      LOG_INFO("Not reachable yet\n");
    }

    /* Add some jitter */
    etimer_set(&periodic_timer, SEND_INTERVAL
      - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_sleep, ev, data)
{
  static struct etimer periodic_timer;
  etimer_set(&periodic_timer, 5 * CLOCK_SECOND);
  PROCESS_BEGIN();
  LOG_INFO("Going to sleep\n");

  P5OUT &= ~(1<<4);
  P5OUT |= (1<<5);

  // This is messing up the simulation
  //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

   // For Simulation: Start timer to see how long it sleeps.
  watchdog_stop();
  start = clock_seconds();
  msp430_lpm4_required = 1;//LPM4; //Comment out to stay awake
  ENERGEST_OFF(ENERGEST_TYPE_CPU);
  ENERGEST_SWITCH(ENERGEST_TYPE_LPM, ENERGEST_TYPE_DEEP_LPM);
  LPM4;
  watchdog_start();
  


  //printf("Start_2: %4lu\n", start); // Used for Debugging

  PROCESS_END();
}
