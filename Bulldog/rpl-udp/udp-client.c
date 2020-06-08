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

volatile unsigned long start, end;
volatile unsigned long DeepSleepTime = 0;

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

// ("Start: %4lu\n", start);  // Used for Debugging
// printf("End: %4lu\n", end); // Used for Debugging
DeepSleepTime += (end - start); // Difference in time between going to sleep and waking up
// printf("Deep Sleep Time: %4lus\n", DeepSleepTime); // Used for Debugging


// Trying to get the Deep LPM working
  printf(" CPU:          %4lus, LPM:      %4lus, DEEP LPM: %4lus,  Total time: %lus\n",
         to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
         (to_seconds(energest_type_time(ENERGEST_TYPE_LPM))-DeepSleepTime),
         DeepSleepTime,
         to_seconds(ENERGEST_GET_TOTAL_TIME()));

/*
// Original Energest code
printf("CPU:          %4lus, LPM:      %4lus, DEEP LPM: %4lus,  Total time: %lus\n",
       to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
       to_seconds(energest_type_time(ENERGEST_TYPE_LPM)),
       to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)),
       to_seconds(ENERGEST_GET_TOTAL_TIME()));

  printf(" Radio LISTEN %4lus TRANSMIT %4lus OFF      %4lus\n",
         to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)),
         to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)),
         to_seconds(ENERGEST_GET_TOTAL_TIME()
                    - energest_type_time(ENERGEST_TYPE_TRANSMIT)
                    - energest_type_time(ENERGEST_TYPE_LISTEN)));
*/
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
//    LOG_INFO("\n Hello_1\n");
  //  printf("\n Hello_print_1\n");
  //  LOG_INFO("Staying up late!\n");
  // LOG_INFO("Ticks per second: %u\n", RTIMER_SECOND);
  //  powertrace_start(CLOCK_SECOND * 10);
  P5OUT &= ~(1<<4);
  P5OUT |= (1<<5);

  // This is messing up the simulation
  //  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

  LPM4; //Comment out to stay awake

  // For Simulation: Start timer to see how long it sleeps.
  start = clock_seconds();
  //printf("Start_2: %4lu\n", start); // Used for Debugging

  PROCESS_END();
}
