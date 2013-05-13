#include "twi.h"

uint8_t twi_activity;
uint8_t twi_dataactivity;

uint8_t recvstate = RECVcommand;
uint8_t data_complete;

uint8_t angularh;
uint8_t servo_waiting_for_data;

uint8_t twi_transmit_buffer_index = 0;
uint8_t twi_transmit_buffer[TWITRANSMITBUFFERSIZE];

uint8_t add_to_transmit_buffer(uint8_t data){
  if(twi_transmit_buffer_index >= TWITRANSMITBUFFERSIZE){ //buffer is full -> drop data
    return 0;
  }
  twi_transmit_buffer[twi_transmit_buffer_index]=data;
  twi_transmit_buffer_index++;
  return 1;
}

uint8_t get_from_transmit_buffer(){
  if(twi_transmit_buffer_index>0){
    twi_transmit_buffer_index--;
  }
  return twi_transmit_buffer[twi_transmit_buffer_index];
}

void i2cinit(){
  TWAR = TWIADDR; //slave adresse setzen
  TWAR |= (1<<TWGCE); //wir wollen general calls auch akzeptieren

  TWCR  = (1<<TWIE); //enable twi interrupt generation
  TWCR |= (1<<TWEA); //sende acks wenn daten an mich adressiert sind
  TWCR |= (1<<TWEN); //ENABLE TWI
  TWCR &= ~(1<<TWINT);
}

ISR(TWI_vect){
  //led1_toggle;
  uint8_t status = TWSR;
  twi_activity = 1;
  switch (status){
    case 0x80:
    case 0x90: //daten empfangen an eigene adresse oder an general address
      //led2_toggle;
      twi_dataactivity = 1;
      //buffer_write(TWDR); //daten in buffer schreiben
      twi_handle(TWDR);
      break;
    case 0xA8:
    case 0xB0:
    case 0xB8: //data was requested from master => give him some data :)
      TWDR = get_from_transmit_buffer();
  }
  
  TWCR |= (1<<TWINT);
}

//Protocol description:
// 1st Packet: Preamble
// 2nd Packet: Command (4 msb) + Extention (4 lsb)
// optional following packets: command specific

//packets (not preamble) should not contain preamble
//if packet (not preamble) is preamble (255) or escape (254) a follow up packet is expected, whose content is aded to the first packet modulo 256
//ATTENTION: if preamble is sent as data package result might be corrupt if original preamble was missed. hence is is recommended never to send preamble as data package
//example: packet1=254 packet2=1  => new packet will be 255
//example2: packet1=254 packet2=0 => new packet will be 254
//example3: packet1=254 packet2=2 => new packet will be 1


void twi_handle(uint8_t data){
  /*
  if(twi_buffer_empty == 1){
    return;
  }
  uint8_t data = buffer_read();
  */
  data_complete += data;

  if(data==TWIPREAMBLE){
    recvstate = RECVcommand;
    data_complete = 0;
    return;
  }

  if(data == TWIESCAPE){
    //Do not reset data_complete
    //Just return
    //"Real data" will be available next time, when next value is added to TWIESCAPE
    return;
  }

  //If no escape or preample was received: interpret as data
  //here we have collected 8 bit of data
  switch (recvstate){
    case RECVcommand: //data wird interpretiert als command
      switch(data_complete & (0xf<<4)){
        case CMD_SERVO: //control servo
          servo_waiting_for_data = (data_complete<<4)%8;
          recvstate = RECVangular; //we expect angular to be transmitted as the next byte
          break;
        case CMD_LED: //control LED
          if((data_complete&(1<<4))==0){ //led ausschalten
            switch(data_complete<<5){
              case 0:
                led_controlled_by_user |= (1<<0);
                led1_aus;
                break;
              case 1:
                led_controlled_by_user |= (1<<1);
                led2_aus;
                break;
              case 2:
                led_controlled_by_user |= (1<<2);
                led3_aus;
                break;
            }
          }else{
            switch(data_complete<<5){
              case 0:
                led_controlled_by_user |= (1<<0);
                led1_an;
                break;
              case 1:
                led_controlled_by_user |= (1<<1);
                led2_an;
                break;
              case 2:
                led_controlled_by_user |= (1<<2);
                led3_an;
                break;
            }
          }
          break;
        case CMD_SERVOSonoff:
          if((data_complete&(1<<4))==0){ //servos ausschalten
            servos_off;
          }else{
            servos_on;
          }
          
          break;
        
        //handle transmit requests:
        case CMD_RESET_TRANSMIT_BUFFER:
          twi_transmit_buffer_index =0;
          break;
        case CMD_GET_SERVO:
          {
            uint16_t tmp =  servos_angular[(data_complete<<4)%8];
            add_to_transmit_buffer((uint8_t)tmp);
            add_to_transmit_buffer((uint8_t)(tmp<<8));
          }
          break;
        case CMD_GET_SERVOonoff:
          add_to_transmit_buffer(check_servo_power!=0);
          break;
        case CMD_GET_LEDS:
          add_to_transmit_buffer( (PORTD & ((1<<PD2)|(1<<PD3)|(1<<PD4)) ) >> PD2  ); //last 3 bits represent the three leds
          break;
      }
      break;
    case RECVangular:
      //receive the msbs of angular
      angularh = data_complete;

      recvstate = RECVangular2; //next data byte will be the lsbs of angular
      break;
    case RECVangular2:
      servos_angular[servo_waiting_for_data] = (((uint16_t)angularh)<<8) | (uint16_t)data_complete;

      recvstate = RECVcommand; //next data byte will be a command
      break;
  }//switch recvstate

  data_complete = 0;
}

