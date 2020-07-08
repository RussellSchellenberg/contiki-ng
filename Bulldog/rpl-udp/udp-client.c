#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <msp430.h>
#include "dev/lpm.h"
#include "sys/log.h"
#include "dev/battery-sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include "dev/sht11/sht11-sensor.h"
#include "dev/cc2420/cc2420.h"

// --- START Code Block for Simulation ---
#include "sys/energest.h"
#include "sys/timer.h"

#include "dev/watchdog.h"
#include "dev/msp430-lpm-override.h"
extern int msp430_lpm4_required;

#ifndef USE_SLEEP_SCHEDULE
#define USE_SLEEP_SCHEDULE 0
#endif

#ifndef IN_SIMULATION
#define IN_SIMULATION 0
#endif

bool is_sleeping = false;

#if IN_SIMULATION == 1

unsigned long EnergyUsed;
unsigned long voltage = 3.0; // Units = Volts
unsigned long ActiveCurrent = 330;
unsigned long LPM1Current = 75;
unsigned long LPM4Current_DIV = 50;
unsigned long ActiveTime = 0;
unsigned long LPM1Time = 0;
unsigned long LPM4Time = 0;
unsigned long TotalTime = 0;
unsigned long RXTime = 0;
unsigned long TXTime = 0;

#endif

//#define ENERGEST_CONF_ON 1 // This was manually changed in the file for the Sim
// --- END Code Block for Simulation ---

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define RPL_CONF_WITH_PROBING 1

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8800
#define UDP_SERVER_PORT	5700

#define SEND_INTERVAL (5 * CLOCK_SECOND)

enum p_type{SYN, DATA};

static uint32_t get_temperature(){
  return sht11_sensor.value(SHT11_SENSOR_TEMP);
}

static uint32_t get_battery(){
    return battery_sensor.value(0);
}

void sleep(){
  is_sleeping = true;
  #if USE_SLEEP_SCHEDULE == 1
  NETSTACK_RADIO.set_value(RADIO_PARAM_POWER_MODE, RADIO_POWER_MODE_OFF);
  #endif
}

void wake(){
  is_sleeping = false;
  #if USE_SLEEP_SCHEDULE == 1
  NETSTACK_RADIO.set_value(RADIO_PARAM_POWER_MODE, RADIO_POWER_MODE_ON);
  #endif
}

#if IN_SIMULATION == 1

static unsigned long to_seconds(uint64_t time){ // Sim
  return (unsigned long)(time / ENERGEST_SECOND);
}

static void print_energest_info(){
  // --- START Code Block for Simulation ---
  /* Update all energest times. */
  energest_flush();

  ActiveTime = (to_seconds(energest_type_time(ENERGEST_TYPE_CPU)));
  LPM1Time = (to_seconds(energest_type_time(ENERGEST_TYPE_LPM)));
  RXTime = (to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)));
  TXTime = (to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)));
  LPM4Time = (to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM))); // Difference in time between going to sleep and waking up
  TotalTime = (to_seconds(ENERGEST_GET_TOTAL_TIME()));

  EnergyUsed = voltage *((ActiveTime * ActiveCurrent) + (LPM1Time * LPM1Current) + (LPM4Time / LPM4Current_DIV)) + (21*(18800*RXTime + 17400*TXTime))/10;
  printf("\nEnergest:\n");
  printf("     Active: %5lus, LPM1: %5lus, LPM4: %5lus, Total Time: %5lus\n\n", ActiveTime, LPM1Time, LPM4Time, TotalTime);
  printf("     RX: %5lus, TX: %5lus\n", RXTime, TXTime);
  printf("     Energy Used: %10luuW\n\n", EnergyUsed);
  printf("Simulation_Data: %5lu %5lu %5lu %5lu %5lu %10lu\n", ActiveTime, LPM1Time, LPM4Time, RXTime, TXTime, EnergyUsed);
}

#endif

static struct simple_udp_connection udp_conn;
uint8_t p_data[64];

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
  if(!is_sleeping){
    static char str[64];

    LOG_INFO("Received '%.*s' from ", datalen, (char *) data);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");
    LOG_INFO("Sending Battery: %lu\n", get_battery());
    snprintf(str, sizeof(str), "Battery: %lu, Temperature: %lu", get_battery(), get_temperature());
    simple_udp_sendto(&udp_conn, str, strlen(str), sender_addr);

    process_exit(&udp_client_sleep);
    process_start(&udp_client_sleep, NULL);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  void clock_init (void);
  static struct etimer periodic_timer;
  static char str[64];
  uip_ipaddr_t dest_ipaddr;
 

  PROCESS_BEGIN();
  SENSORS_ACTIVATE(battery_sensor);
  SENSORS_ACTIVATE(sht11_sensor);

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
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
  while(1){
    etimer_set(&periodic_timer, 10 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    wake();
    #if IN_SIMULATION == 1
    print_energest_info();
    #endif
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_sleep, ev, data)
{
  static struct etimer periodic_timer;
  PROCESS_BEGIN();
  etimer_set(&periodic_timer, 1*CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

  sleep();

  PROCESS_END();
}
