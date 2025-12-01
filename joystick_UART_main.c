/* 
 * File:   joystick_UART_main.c
 * Author: Reid Stasiuk
 *
 * Created on November 15, 2024, 5:26 PM
 */

#include "C:\Users\reids\OnseDrive\Desktop\defines.h"
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include "lcd.h"
#include "hd44780.h"

FILE lcd_str = FDEV_SETUP_STREAM ( lcd_putchar, NULL , _FDEV_SETUP_WRITE );

volatile uint16_t x_pos; //x position integer
volatile uint16_t y_pos; //y position integer

// F_CPU = 8MHz (internal oscillator, no pre-scaler)
//LCD and ADC frequencies and UART baud rate are set accordingly


ISR(ADC_vect){ //ADC conversion complete interrupt
    if((ADMUX & (1 << MUX2)) && (ADMUX & (1 << MUX0))){ //if measuring on ADC5 (y channel)
        y_pos = ADC;
        ADMUX &= ~(1 << MUX0); //change to ADC4 (x channel)
    }
    else{ //if measuring on ADC4 (x channel)
        x_pos = ADC;
        ADMUX |= (1 << MUX0); //change to ADC5 (y channel)
    }
}


void UART_transmit(uint16_t value){ //send 2 byte serial data
    
    //separate 2byte value into high and low bytes
    uint8_t valueL = value & 0xFF;
    uint8_t valueH = (value >> 8) & 0xFF;
    
    //polling loop to wait until the transmit buffer is empty from last transmission
    while(!(UCSR0A & (1 << UDRE0))){
    }
    
    UDR0 = valueL; //transmit low byte first
    //polling loop to wait until the transmit buffer is empty from low byte transmission
    while(!(UCSR0A & (1 << UDRE0))){
    }
    
    UDR0 = valueH; //transmit high byte second
    return;
}


unsigned char UART_receive(void){ //receive 1 byte serial data (UNUSED IN LAB 3)
    
    //polling loop to wait while the receive buffer is empty
    while(!(UCSR0A & (1 << RXC0))){
    }
    
    uint8_t value = UDR0; //store and return data from receiver buffer
    
    return value;
}


int main(void) {
    lcd_init(); //initialize HD44780 display
    
    DDRB = 0;
    DDRC = 0;
    DDRD = 0;
    //initialize all pins as input
    
    DDRD |= (1 << PD2)|(1 << PD4)|(1 << PD5)|(1 << PD6)|(1 << PD7);
    DDRB |= (1 << PB0)|(1 << PB1);
    //LCD connections configured as output
    
    PORTB = 0;
    PORTC = 0;
    PORTD = 0;
    //drive all outputs low, keep all pull-ups off
    
    PORTD |= (1 << PD2);
    PORTB |= (1 << PB0);
    //RS driven high for HD44780 data register
    //RW driven low for write selection
    //E driven high for enable write function  
    
    PORTB |= (1 << PB2); //enable pull-up on PB2 (connects to pushbutton)
    
    DIDR0 &= 0x3F; 
    //disable digital input on 5 ADC pins 
    
    ADMUX = 0;
    ADMUX = (1 << REFS0)|(1 << MUX2); 
    //set reference voltage as AVcc, initialize to measure on x channel
    
    ADCSRA = 0;
    ADCSRA |= (1 << ADEN); 
    ADCSRA |= (1 << ADIE)|(1 << ADPS2)|(1 << ADPS1)|(1 << ADPS0);
    //enable ADC interrupts and pre-scale ADC clock by 128 
    //8MHz / 128 = 62.5kHz --> 50kHz < 62.5kHz < 250kHz
    
    PRR &= ~ (1 << PRADC);
    //turn off ADC power reduction as per data sheet instructions
    
    UBRR0 = 12; //UART baud rate = 38.4 bps @ 8MHz
    
    UCSR0C = 0;
    UCSR0C = (1 << UCSZ01)|(1 << UCSZ00); //frame
    //set USART mode to asynchronous
    //set parity to none, 1 stop bit, 8bit character size, no stop bit selected
    
    UCSR0B = 0;
    UCSR0B = (1 << TXEN0)|(1 << RXEN0);
    //enable transmitter, receiver and UART data register empty interrupt
    
    sei(); //enable global interrupts for ADC interrupt 
        
    uint8_t active_channel = 0; //joystick pushbutton state variable
    //if active_channel == 0, x channel selected
    //if active_channel == 1, y channel selected
    
    while(1){ //main loop
        
        if(!(PINB & (1 << PB2))){ //if pushbutton is pressed
            active_channel ^= 1; //toggle the switch state (x,y channel)
        }
        
        ADCSRA |= (1 << ADSC); //start new conversion
        
        while(1){ //polling loop to wait until ADC conversion is over
            if(ADCSRA & (1 << ADSC)){
                continue;
            }
            else{ //ADC conversion complete interrupt executes just before this
                break;
            }
        }
        
        //update new x position and new y position on LCD
        hd44780_outcmd(HD44780_CLR);
        hd44780_outcmd(HD44780_HOME);

        fprintf(&lcd_str, "X: %d    SW: %d", x_pos, active_channel);
        fprintf(&lcd_str, "\x1b\xc0");
        fprintf(&lcd_str, "Y: %d", y_pos);
        
        if(!active_channel){ //check whether to transmit x or y position data
            UART_transmit(x_pos); //transmit x position
        }
        else{
            UART_transmit(y_pos); //transmit y position
        }
     
        _delay_ms(100); 
        //display updates and new transmission approximately every 100ms
        //sampling frequency of approximately 10Hz (1Hz < 10Hz < 1kHz)
        //short enough to have real time updates on the x,y channels
        //long enough so that a button push only toggles sw_state once
    }
    
    return (EXIT_SUCCESS);
}

