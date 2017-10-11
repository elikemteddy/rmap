/**@file rmap.h */

/*********************************************************************
Copyright (C) 2017  Marco Baldinetti <m.baldinetti@digiteco.it>
authors:
Paolo patruno <p.patruno@iperbole.bologna.it>
Marco Baldinetti <m.baldinetti@digiteco.it>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
**********************************************************************/

#ifndef _RMAP_H
#define _RMAP_H

#include "rmap-config.h"
#include <typedef.h>
#include <debug.h>
#include <hardware_config.h>
#include <json_config.h>
#include <ntp_config.h>
#include <lcd_config.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <SPI.h>
#include <Wire.h>
#include <SdFat.h>
#include <pcf8563.h>
#include <ntp.h>
#include <Time.h>
#include <SensorDriver.h>
#include <eeprom_utility.h>
#include <rmap_utility.h>
#include <i2c_utility.h>
#include <sdcard_utility.h>

#if (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_ETH || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_ETH)
#include <ethernet_config.h>
#include <Ethernet2.h>

#elif (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_GSM || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_GSM)
#include <gsm_config.h>
#include <sim800Client.h>
#include <Sim800IPStack.h>

#endif

#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

/*********************************************************************
* TYPEDEF
*********************************************************************/
/*!
\struct configuration_t
\brief EEPROM saved configuration.
*/
typedef struct {
   uint8_t module_version;                                  //!< module version
   uint8_t module_type;                                     //!< module type

   uint16_t mqtt_port;                                      //!< mqtt server port
   char mqtt_server[MQTT_SERVER_LENGTH];                    //!< mqtt server
   char mqtt_root_topic[MQTT_ROOT_TOPIC_LENGTH];            //!< mqtt root path
   char mqtt_subscribe_topic[MQTT_SUBSCRIBE_TOPIC_LENGTH];  //!< mqtt subscribe topic
   char mqtt_username[MQTT_USERNAME_LENGTH];                //!< mqtt username
   char mqtt_password[MQTT_PASSWORD_LENGTH];                //!< mqtt password
   char ntp_server[NTP_SERVER_LENGTH];                      //!< ntp server

   sensor_t sensors[USE_SENSORS_COUNT];                     //!< SensorDriver buffer for storing sensors parameter
   uint8_t sensors_count;                                   //!< configured sensors number
   uint16_t report_seconds;                                 //!< seconds for report values

   #if (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_ETH || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_ETH)
   bool is_dhcp_enable;                                     //!< dhcp status
   uint8_t ethernet_mac[ETHERNET_MAC_LENGTH];               //!< ethernet mac
   uint8_t ip[ETHERNET_IP_LENGTH];                          //!< ip address
   uint8_t netmask[ETHERNET_IP_LENGTH];                     //!< netmask
   uint8_t gateway[ETHERNET_IP_LENGTH];                     //!< gateway
   uint8_t primary_dns[ETHERNET_IP_LENGTH];                 //!< primary dns

   #elif (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_GSM || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_GSM)
   char gsm_apn[GSM_APN_LENGTH];                            //!< gsm apn
   char gsm_username[GSM_USERNAME_LENGTH];                  //!< gsm username
   char gsm_password[GSM_PASSWORD_LENGTH];                  //!< gsm password

   #endif
} configuration_t;

/*********************************************************************
* TYPEDEF for Finite State Machine
*********************************************************************/
/*!
\enum state_t
\brief Main loop finite state machine.
*/
typedef enum {
   INIT,                      //!< init tasks and sensors
   #if (USE_POWER_DOWN)
   ENTER_POWER_DOWN,          //!< if no task is running, activate power down
   #endif
   TASKS_EXECUTION,           //!< execute active tasks
   END                        //!< go to ENTER_POWER_DOWN or TASKS_EXECUTION
} state_t;

/*!
\enum supervisor_state_t
\brief Supervisor task finite state machine.
*/
typedef enum {
   SUPERVISOR_INIT,                    //!< init task variables
   SUPERVISOR_RTC_LEVEL_TASK,          //!<
   SUPERVISOR_CONNECTION_LEVEL_TASK,   //!<
   SUPERVISOR_NTP_LEVEL_TASK,          //!<
   SUPERVISOR_MANAGE_LEVEL_TASK,       //!<
   SUPERVISOR_END,                     //!<
   SUPERVISOR_WAIT_STATE               //!<
} supervisor_state_t;

#if (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_ETH || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_ETH)
/*!
\enum ethernet_state_t
\brief Ethernet task finite state machine.
*/
typedef enum {
   ETHERNET_INIT,             //!<
   ETHERNET_CONNECT,          //!<
   ETHERNET_OPEN_UDP_SOCKET,  //!<
   ETHERNET_END,              //!<
   ETHERNET_WAIT_STATE        //!<
} ethernet_state_t;

#elif (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_GSM || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_GSM)
/*!
\enum gsm_state_t
\brief GSM task finite state machine.
*/
typedef enum {
   GSM_INIT,                  //!<
   GSM_SWITCH_ON,             //!<
   GSM_AUTOBAUD,              //!<
   GSM_SETUP,                 //!<
   GSM_START_CONNECTION,      //!<
   GSM_CHECK_OPERATION,       //!<
   GSM_OPEN_UDP_SOCKET,       //!<
   GSM_STOP_CONNECTION,       //!<
   GSM_SUSPEND,               //!<
   GSM_WAIT_FOR_SWITCH_OFF,   //!<
   GSM_SWITCH_OFF,            //!<
   GSM_END,                   //!<
   GSM_WAIT_STATE             //!<
} gsm_state_t;

#endif

/*!
\enum sensors_reading_state_t
\brief Sensors reading task finite state machine.
*/
typedef enum {
   SENSORS_READING_INIT,            //!< init task variables
   SENSORS_READING_PREPARE,         //!< prepare sensor
   SENSORS_READING_IS_PREPARED,     //!< check if the sensor has been prepared
   SENSORS_READING_GET,             //!< read and get values from sensor
   SENSORS_READING_IS_GETTED,       //!< check if the sensor has been readed
   SENSORS_READING_READ,            //!< intermediate state (future implementation...)
   SENSORS_READING_NEXT,            //!< go to next sensor
   SENSORS_READING_END,             //!< performs end operations and deactivate task
   SENSORS_READING_WAIT_STATE       //!< non-blocking waiting time
} sensors_reading_state_t;

/*!
\enum time_state_t
\brief Time task finite state machine.
*/
typedef enum {
   TIME_INIT,                    //!<
   TIME_SEND_ONLINE_REQUEST,     //!<
   TIME_WAIT_ONLINE_RESPONSE,    //!<
   TIME_SET_SYNC_NTP_PROVIDER,   //!<
   TIME_SET_SYNC_RTC_PROVIDER,   //!<
   TIME_END,                     //!<
   TIME_WAIT_STATE               //!<
} time_state_t;

/*!
\enum data_saving_state_t
\brief Data saving task finite state machine.
*/
typedef enum {
   DATA_SAVING_INIT,          //!<
   DATA_SAVING_OPEN_SDCARD,   //!<  if not already open
   DATA_SAVING_OPEN_FILE,     //!<
   DATA_SAVING_SENSORS_LOOP,  //!<  loop from 0 to readable_configuration->sensors_count
   DATA_SAVING_DATA_LOOP,     //!< loop from 0 to data_count
   DATA_SAVING_WRITE_FILE,    //!<
   DATA_SAVING_CLOSE_FILE,    //!<
   DATA_SAVING_END,           //!<
   DATA_SAVING_WAIT_STATE     //!<
} data_saving_state_t;

/*!
\enum mqtt_state_t
\brief MQTT task finite state machine.
*/
typedef enum {
   MQTT_INIT,              //!<

   MQTT_OPEN_SDCARD,       //!<  if not already open
   MQTT_OPEN_PTR_FILE,     //!<
   MQTT_PTR_READ,          //!<
   MQTT_PTR_FIND,          //!<
   MQTT_PTR_FOUND,         //!<
   MQTT_PTR_END,           //!<

   MQTT_OPEN,              //!<
   MQTT_CHECK,             //!<
   MQTT_CONNECT,           //!<
   MQTT_SUBSCRIBE,         //!<

   MQTT_OPEN_DATA_FILE,    //!<

   MQTT_SENSORS_LOOP,      //!<  loop from 0 to readable_configuration->sensors_count
   MQTT_DATA_LOOP,         //!<  loop from 0 to data_count
   MQTT_SD_LOOP,           //!<  loop from first row to last row of data file
   MQTT_PUBLISH,

   MQTT_CLOSE_DATA_FILE,   //!<

   MQTT_DISCONNECT,        //!<

   MQTT_PTR_UPDATE,        //!<
   MQTT_CLOSE_PTR_FILE,    //!<
   MQTT_CLOSE_SDCARD,      //!<

   MQTT_END,               //!<
   MQTT_WAIT_STATE         //!<
} mqtt_state_t;

/*!
\enum stream_state_t
\brief Stream task finite state machine.
*/
typedef enum {
   STREAM_INIT,      //!<
   STREAM_AVAILABLE, //!<
   STREAM_PROCESS,   //!<
   STREAM_END        //!<
} stream_state_t;

/*********************************************************************
* GLOBAL VARIABLE
*********************************************************************/
/*!
\var readable_configuration
\brief Configuration for this module.
*/
configuration_t readable_configuration;

/*!
\var writable_configuration
\brief Configuration for this module.
*/
configuration_t writable_configuration;

/*!
\var ready_tasks_count
\brief Number of tasks ready to execute.
*/
volatile uint8_t ready_tasks_count;

/*!
\var awakened_event_occurred_time_ms
\brief System time (in millisecond) when the system has awakened from power down.
*/
uint32_t awakened_event_occurred_time_ms;

/*!
\var SD
\brief SD-Card structure.
*/
SdFat SD;

/*!
\var read_data_file
\brief File structure for read data stored on SD-Card.
*/
File read_data_file;

/*!
\var write_data_file
\brief File structure for write data stored on SD-Card.
*/
File write_data_file;

/*!
\var mqtt_ptr_file
\brief File structure for read and write data pointer stored on SD-Card for mqtt send.
*/
File mqtt_ptr_file;

#if (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_ETH || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_ETH)
/*!
\var eth_udp_client
\brief Ethernet UDP client structure.
*/
EthernetUDP eth_udp_client;

/*!
\var eth_tcp_client
\brief Ethernet TCP client structure.
*/
EthernetClient eth_tcp_client;

/*!
\fn IPStack ipstack(eth_tcp_client)
\brief Ethernet IPStack MQTTClient structure.
\return void.
*/
IPStack ipstack(eth_tcp_client);

#elif (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_GSM || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_GSM)
/*!
\var s800
\brief SIMCom SIM800C/SIM800L UDP/TCP client structure.
*/
sim800Client s800;

/*!
\var ipstack
\brief SIMCom SIM800C/SIM800L IPStack MQTTClient structure.
*/
IPStack ipstack(s800);

#endif

/*!
\var mqtt_client
\brief MQTT Client structure.
*/
MQTT::Client<IPStack, Countdown, MQTT_ROOT_TOPIC_LENGTH+MQTT_SENSOR_TOPIC_LENGTH+MQTT_MESSAGE_LENGTH, 1> mqtt_client = MQTT::Client<IPStack, Countdown, MQTT_ROOT_TOPIC_LENGTH+MQTT_SENSOR_TOPIC_LENGTH+MQTT_MESSAGE_LENGTH, 1>(ipstack, IP_STACK_TIMEOUT_MS);

/*!
\fn LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS)
\brief LCD structure.
\return void.
*/
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

/*!
\var sensors
\brief SensorDriver array structure.
*/
SensorDriver *sensors[USE_SENSORS_COUNT];

/*!
\var is_first_run
\brief If true, the first reading of the sensors was performed.
*/
bool is_first_run;

/*!
\var is_time_set
\brief If true, the time was readed from rtc or ntp and was setted in system.
*/
bool is_time_set;

/*!
\var is_time_for_sensors_reading_updated
\brief If true, the next time has been calculated to read the sensors.
*/
bool is_time_for_sensors_reading_updated;

/*!
\var is_client_connected
\brief If true, the client (ethernet or gsm) was connected to socket (TCP or UDP).
*/
bool is_client_connected;

/*!
\var is_client_udp_socket_open
\brief If true, the client (ethernet or gsm) was opened the UDP socket.
*/
bool is_client_udp_socket_open;

/*!
\var is_event_client_executed
\brief If true, the client has executed its task.
*/
bool is_event_client_executed;

/*!
\var is_event_time_executed
\brief If true, the time task has executed.
*/
bool is_event_time_executed;

/*!
\var do_ntp_sync
\brief If true, you must update the time from ntp.
*/
bool do_ntp_sync;

/*!
\var last_ntp_sync
\brief Last date and time when ntp sync was performed.
*/
time_t last_ntp_sync;

/*!
\var is_sdcard_open
\brief If true, the SD-Card is ready.
*/
bool is_sdcard_open;

/*!
\var is_sdcard_error
\brief If true, the SD-Card is in error.
*/
bool is_sdcard_error;

/*!
\var is_mqtt_subscribed
\brief If true, MQTT Client is subscribed to receive topic.
*/
bool is_mqtt_subscribed;

/*!
\var json_sensors_data
\brief buffer containing the data read by sensors in json text format.
*/
char json_sensors_data[USE_SENSORS_COUNT][JSON_BUFFER_LENGTH];

/*!
\var next_hour_for_sensor_reading
\brief Next scheduled hour for sensors reading.
*/
uint8_t next_hour_for_sensor_reading;

/*!
\var next_minute_for_sensor_reading
\brief Next scheduled minute for sensors reading.
*/
uint8_t next_minute_for_sensor_reading;

/*!
\var next_second_for_sensor_reading
\brief Next scheduled second for sensors reading.
*/
uint8_t next_second_for_sensor_reading;

/*!
\var sensor_reading_time
\brief Date and time corresponding to the last reading of the sensors.
*/
volatile tmElements_t sensor_reading_time;

/*!
\var ptr_time_data
\brief Readed data pointer stored on SD-Card for mqtt send.
*/
time_t ptr_time_data;

/*!
\var stima_name
\brief Name of this module.
*/
char stima_name[20];

/*!
\var state
\brief Current main loop state.
*/
state_t state;

/*!
\var supervisor_state
\brief Supervisor task state.
*/
supervisor_state_t supervisor_state;

#if (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_ETH || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_ETH)
/*!
\var ethernet_state
\brief Ethernet task state.
*/
ethernet_state_t ethernet_state;

#elif (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_GSM || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_GSM)
/*!
\var gsm_state
\brief GSM task state.
*/
gsm_state_t gsm_state;

#endif

/*!
\var time_state
\brief Time task state.
*/
time_state_t time_state;

/*!
\var sensors_reading_state
\brief Sensors reading task state.
*/
sensors_reading_state_t sensors_reading_state;

/*!
\var data_saving_state
\brief Data saving task state.
*/
data_saving_state_t data_saving_state;

/*!
\var mqtt_state
\brief MQTT task state.
*/
mqtt_state_t mqtt_state;

/*!
\var stream_state
\brief Stream task state.
*/
stream_state_t stream_state;

/*********************************************************************
* FUNCTIONS
*********************************************************************/
/*!
\fn void init_power_down(uint32_t *time_ms, uint32_t debouncing_ms)
\brief Enter power down mode.
\param time_ms pointer to a variable to save the last instant you entered power down.
\param debouncing_ms delay to power down.
\return void.
*/
void init_power_down(uint32_t *time_ms, uint32_t debouncing_ms);

/*!
\fn void init_wdt(uint8_t wdt_timer)
\brief Init watchdog.
\param wdt_timer a time value for init watchdog (WDTO_xxxx).
\return void.
*/
void init_wdt(uint8_t wdt_timer);

/*!
\fn void init_system(void)
\brief Init system.
\return void.
*/
void init_system(void);

/*!
\fn void init_buffers(void)
\brief Init buffers.
\return void.
*/
void init_buffers(void);

/*!
\fn void init_tasks(void)
\brief Init tasks variable and state.
\return void.
*/
void init_tasks(void);

/*!
\fn void init_pins(void)
\brief Init hardware pins.
\return void.
*/
void init_pins(void);

/*!
\fn void init_wire(void)
\brief Init wire (i2c) library and performs checks on the bus.
\return void.
*/
void init_wire(void);

/*!
\fn void init_spi(void)
\brief Init SPI library.
\return void.
*/
void init_spi(void);

/*!
\fn void init_rtc(void)
\brief Init RTC module.
\return void.
*/
void init_rtc(void);

#if (USE_TIMER_1)
/*!
\fn void init_timer1(void)
\brief Init Timer1 module.
\return void.
*/
void init_timer1(void);
#endif

/*!
\fn void init_sensors(void)
\brief Create and setup sensors.
\return void.
*/
void init_sensors(void);

/*!
\fn void print_configuration(void)
\brief Print current configuration.
\return void.
*/
void print_configuration(void);

/*!
\fn void load_configuration(void)
\brief Load configuration from EEPROM.
\return void.
*/
void load_configuration(void);

/*!
\fn void save_configuration(bool is_default)
\brief Save configuration to EEPROM.
\param is_default: if true save default configuration; if false save current configuration.
\return void.
*/
void save_configuration(bool);

/*!
\fn void set_default_configuration()
\brief Set default configuration to global configuration variable.
\return void.
*/
void set_default_configuration(void);

/*!
\fn void setNextTimeForSensorReading(uint8_t *next_hour, uint8_t *next_minute, uint8_t *next_second)
\brief Calculate next hour, minute and second for sensors reading.
\param *next_hour: Pointer to next scheduled hour for sensors reading
\param *next_minute: Pointer to next scheduled minute for sensors reading
\param *next_second: Pointer to next scheduled second for sensors reading
\return void.
*/
void setNextTimeForSensorReading(uint8_t *next_hour, uint8_t *next_minute, uint8_t *next_second);

/*!
\fn bool mqttConnect(char *username, char *password)
\brief Use a open tcp socket to connect to the mqtt server.
\param *username: Username of mqtt server
\param *password: Password of mqtt server
\return true if connection was succesful, false otherwise.
*/
bool mqttConnect(char *username, char *password);

/*!
\fn bool mqttPublish(const char *topic, const char *message)
\brief Publish message on topic
\param *topic: Topic for mqtt publish
\param *message: Message to be publish
\return true if publish was succesful, false otherwise.
*/
bool mqttPublish(const char *topic, const char *message);

/*!
\fn void mqttRxCallback(MQTT::MessageData &md)
\brief Register a receive callback for incoming mqtt message
\param &md: Received data structure
\return void.
*/
void mqttRxCallback(MQTT::MessageData &md);

/*!
\fn char *rpc_process(char *json)
\brief Process and execute a received Remote Procedure Call (RPC). Useful for configuration.
\param *json: message in json text format
\return pointer to RPC response.
*/
char *rpc_process(char *json);

/*********************************************************************
* TASKS
*********************************************************************/
/*!
\var is_event_supervisor
\brief Enable or disable the Supervisor task.
*/
bool is_event_supervisor;

/*!
\fn void supervisor_task(void)
\brief Supervisor task.
Manage RTC and NTP sync and open/close gsm and ethernet connection.
\return void.
*/
void supervisor_task(void);

/*!
\var is_event_sensors_reading
\brief Enable or disable the Sensors reading task.
*/
volatile bool is_event_sensors_reading;

/*!
\fn void sensors_reading_task(void)
\brief Sensors reading Task.
Read data from sensors.
\return void.
*/
void sensors_reading_task(void);

/*!
\var is_event_rtc
\brief Enable or disable the Real Time Clock task.
*/
volatile bool is_event_rtc;

/*!
\fn void rtc_task(void)
\brief Real Time Clock task.
Read RTC time and sync system time with it.
\return void.
*/
void rtc_task(void);

/*!
\var is_event_time
\brief Enable or disable the Time task.
*/
volatile bool is_event_time;

/*!
\fn void time_task(void)
\brief Time task.
Get time from NTP and sync RTC with it.
\return void.
*/
void time_task(void);

#if (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_ETH || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_ETH)
/*!
\var is_event_ethernet
\brief Enable or disable the Ethernet task.
*/
bool is_event_ethernet;

/*!
\fn void ethernet_task(void)
\brief Ethernet task.
Manage Ethernet operation.
\return void.
*/
void ethernet_task(void);

#elif (MODULE_TYPE == STIMA_MODULE_TYPE_SAMPLE_GSM || MODULE_TYPE == STIMA_MODULE_TYPE_REPORT_GSM)
/*!
\var is_event_gsm
\brief Enable or disable the GSM task.
*/
bool is_event_gsm;

/*!
\fn void gsm_task(void)
\brief GSM Task.
Manage GSM operation.
\return void.
*/
void gsm_task(void);

#endif

/*!
\var is_event_data_saving
\brief Enable or disable the Data saving task.
*/
bool is_event_data_saving;

/*!
\fn void data_saving_task(void)
\brief Data Saving Task.
Save acquired sensors data on SD-Card.
\return void.
*/
void data_saving_task(void);

/*!
\var is_event_mqtt
\brief Enable or disable the MQTT task.
*/
bool is_event_mqtt;

/*!
\fn void mqtt_task(void)
\brief MQTT Task.
\return void.
Read data stored on SD-Card and send it over MQTT.
*/
void mqtt_task(void);

/*!
\var is_event_stream
\brief Enable or disable the Strem task.
*/
bool is_event_stream;

/*!
\fn bool stream_task(Stream *stream, uint32_t stream_timeout, uint32_t end_task_timeout)
\brief Stream Task.
\param *stream: Pointer to stream
\param stream_timeout: Timeout for stream
\param end_task_timeout: Timeout for deactivate task after last received bytes
Read a stream and process Remote Procedure Call data.
\return true if end_task_timeout was elapsed, false otherwise.
*/
bool stream_task(Stream *stream, uint32_t stream_timeout, uint32_t end_task_timeout);

/*********************************************************************
* INTERRUPT HANDLER
*********************************************************************/
/*!
\fn void rtc_interrupt_handler(void)
\brief Real Time Clock interrupt handler.
\return void.
*/
void rtc_interrupt_handler(void);

#endif
