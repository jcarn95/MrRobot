
#include "main.h"
#include "LCD.h"
#include "ser.h"
#include "SPI.h"
#include "ADC.h"
#include "motor.h"

volatile unsigned int time_count;
volatile bit FLAG_1000MS;

unsigned char rxbyte = 0;
signed int stepClosest = 0;
signed int adcClosest = 1000;
signed int highByte = 0;
signed int lowByte = 0;
signed int distTrav = 0;
signed int totalDistTrav = 0;
unsigned char controlByte = 0;
signed int loop = 0;
signed int loop2 = 0;

unsigned char PB6Counter = 0;
unsigned char PB7Counter = 0;
unsigned char PB8Counter = 0;




// Interrupt service routine
void interrupt isr(void){
//Timer 1
    if(TMR0IF){
        TMR0IF = 0;
        TMR0 = TMR0_VAL;
        time_count++;

        if(time_count % 1 == 0) {
            //FLAG_10MS = 1;
            SM_STEP();
        }

        if(time_count % 1000 == 0){
            RB0 = !RB0;     //Toggle LED           
            //    FLAG_1000MS = 1;	// Raise flag for 500ms
            time_count = 0;	// Clear time_count        
        }
        if (PB8 == 1)
            PB8Counter++;
        if (PB7 == 1)
            PB7Counter++;
        if (PB6 == 1)
            PB6Counter++;
        
    }
}

void getDistTrav(void){
    ser_putch(142);     //Requests packet of sensor data
    ser_putch(19);      //Specifies distance packet for request
    
    ser_getch();        //Gets the distance value returned as 2 bytes, high byte first.
    highByte = rxbyte;  //Puts the high byte into variable
    ser_getch();        //Gets the low byte of the sensor packet
    lowByte = rxbyte;   //Puts the low byte into variable
                
    distTrav = (256*highByte + lowByte);    //Distance traveled since data was last requested
    totalDistTrav = ((totalDistTrav + distTrav)/10);  //Total distance traveled since button push in CM
                
    lcdSetCursor(0b11000000);   //Second row, first position
    lcdWriteToDigitBCD(totalDistTrav);  //Print the total distance to LCD
}


void main(void){

//Initialise and setup
    setupSPI();
    ser_init();
    setupLCD();
    setupADC();
    
    unsigned char controlByte = 0b00001101;
    spi_transfer(controlByte);
   
    ser_putch(128);     //Startup
    ser_putch(132);     //Full mode
    
    while(1){       
//        if(FLAG_1000MS){          Moved to interrupt
//            RB0 = !RB0;
//            FLAG_1000MS = 0;
//        }
        
        //Rotates 360 and compares adcRAW at every half step.
        //If it detects a closer object than the previous closest then it stores 
        //the stepCount corresponding to that object.
        if (PB8Counter >= 10 && PB8 == 0){
            for (loop = 0; loop < 400; loop++){           
                moveCW();
                ADCMain();
                if (adcRAW > adcClosest){
                    adcClosest = adcRAW;
                    stepClosest = stepCount;              
                }
            }
            
            //Moves CCW until stepCount(initial -400) matches the step of the closest object
            for (loop = stepCount; loop != stepClosest; loop++){
                moveCCW();
            }            
        }

       
        //This might make it drive 4m forward, 250mm/s and takes 16 seconds. 
        //Or it might not.
        if (PB7Counter >= 10 && PB7 == 0){
            totalDistTrav = 0;  //Resets distance traveled
            
            //Drive command [Velocity high][Velocity low][Radius high][Radius low]
            //Radius of 0x7FFF is a straight line
            //Negative radius turns right
            ser_putch(137);     
                ser_putch(0x00);    
                ser_putch(0b11111010);   //250mm/s 
                ser_putch(0x7F);  
                ser_putch(0xFF);  
            
            //Continuously updates LCD with distance traveled while moving
            for (loop = 0; loop < 400; loop++){
                getDistTrav();    //Gets distance and printd
                __delay_ms(40);   //Loop long enough to travel 4m
            }    
        
            ser_putch(137);     //Drive command [Velocity high][Velocity low][Radius high][Radius low]
                ser_putch(0x00);    //Set velocity to 0 mm/s to stop.
                ser_putch(0x00);    
                ser_putch(0x7F);    
                ser_putch(0xFF);               
        }
        
        //Perform 'Square' manoeuvre... maybe
        if (PB6Counter >= 10 && PB6 == 0){
            totalDistTrav = 0;  //Resets distance traveled
            
            for (loop = 0; loop < 4; loop++){
                ser_putch(137);         //Drive command
                    ser_putch(0x00);
                    ser_putch(0b11111010);  //250mm/s
                    ser_putch(0xFF);        //Turn in place clockwise
                    ser_putch(0xFF);
                __delay_ms(1000);   //Delay some amount of time to turn 90 deg    
                
                for (loop2 = 0; loop2 < 100; loop2++){    
                    ser_putch(137);         //Drive command
                        ser_putch(0x00);
                        ser_putch(0b11111010);  //250mm/s
                        ser_putch(0x7F);        //Drive straight
                        ser_putch(0xFF);
                    getDistTrav();    //Gets distance and print to LCD
                    __delay_ms(40);       
                }
            }
            ser_putch(137);         //Drive command
                ser_putch(0x00);    //Set velocity to 0 mm/s to stop.
                ser_putch(0x00);    
                ser_putch(0x7F);    
                ser_putch(0xFF);  
                
        }


    }
}
