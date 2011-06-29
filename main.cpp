#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>

const uint8_t tall[] = { 0xB7, 0x84, 0x3D, 0x9D, 0x8E, 0x9B, 0xBB, 0x85, 0xBF, 0x9F };

volatile uint8_t currentDigit;
volatile uint8_t numbers[4];
volatile uint8_t backupNumbers[4];
volatile bool blink;
volatile bool running;

volatile uint8_t mainFlags;
#define updateDisplayFlag 0
#define checkForKeysFlag 1

#define SPI_SERIAL PA5
#define SPI_SCK PA3
#define SPI_RCK PA4
#define SPI_PORT PORTA
#define SPI_DDR DDRA

#define OUTPUT_0 PA6
#define OUTPUT_1 PA7
#define OUTPUT_DDR DDRA
#define OUTPUT_PORT PORTA

#define BUTTON_PORT PORTA
#define BUTTON_PINS PINA
#define BUTTON_UP PINA0
#define BUTTON_DOWN PINA1
#define BUTTON_ACTION PINA2

void init(void)
{
    blink = true;
    uint8_t tempInt1 = eeprom_read_byte((uint8_t*)0);                       //Load last setting from eeprom
    uint8_t tempInt2 = eeprom_read_byte((uint8_t*)1);

    if(tempInt1 < 10)                                                       //Only update display if the numbers are sane.
        backupNumbers[0] = tempInt1;
    else
        backupNumbers[0] = 0;
    if(tempInt2 < 10)
        backupNumbers[1] = tempInt2;
    else
        backupNumbers[1] = 0;

    SPI_DDR |= (1 << SPI_SCK) | (1 << SPI_RCK) | (1 << SPI_SERIAL);         		//SPI Ports
    OUTPUT_DDR |= (1 << OUTPUT_0) | (1 << OUTPUT_1);                        		//Output Ports

    BUTTON_PORT |= (1 << BUTTON_UP) | (1 << BUTTON_DOWN) | (1 << BUTTON_ACTION); 	//Internal pullup
    CLKPR = 0x80;                                                           		//Clock prescaler 1
    CLKPR = 0x00;                                                           		//
    TCCR0B |= (1 << CS02);                                                  		//Timer 0 Prescaler 256
    TCCR1B |= (1 << WGM12);                                                 		//Clear Timer on OCR1A Compare
    TIMSK0 |= (1 << TOIE0);                                                 		//Timer 0 overflow interrupt enable
    TIMSK1 |= (1 << OCIE1A);                                                		//Output Compare, Interrupt enable A
    sei();                                                                  		//Interrupt enable
}

void timer_start(void)
{
    OCR1A = 0x1387;                                                         //Timer1 A: TOP = 4999
    TCNT1 = 0x00;                                                           //Reset timer to 0
    TCCR1B |= (1 << CS11);                                                  //Timer prescaler 8
}

void timer_stop(void)
{
    TCCR1B &= ~(1 << CS11);                                                 //Timer prescaler 0, ie. timer stopped.
    TCNT1 = 0x00;                                                           //Reset timer to 0
}

void tossByte(const uint8_t &byteToToss)
{
    for(uint8_t teller = 0; teller != 8; ++teller)                          //Push the data to the 595
    {
        if(byteToToss & (1 << teller))
            SPI_PORT |= (1 << SPI_SERIAL);
        else
            SPI_PORT &= ~(1 << SPI_SERIAL);
        SPI_PORT |= (1 << SPI_SCK);
        SPI_PORT &= ~(1 << SPI_SCK);
    }
}

void updateDisplay(void)
{
    static uint8_t plexDigit;
    uint8_t firstByte = (~(1 << plexDigit) & 0x0F);
    uint8_t curNum = tall[numbers[3 - plexDigit]];
    if((numbers[0] == 0 && numbers[1] == 0 && numbers[2] == 0 && numbers[3] == 0) && running)
    {
        if(blink)                                                           //Blink if timer is done
        {
            firstByte |= (1 << 6);
            curNum = 0;
        }
    }
    if(3 - plexDigit == 1 && blink)                                          //Turn on the middle double dots
        curNum |= (1 << 6);

    SPI_PORT &= ~(1 << SPI_SCK);
    SPI_PORT &= ~(1 << SPI_RCK);
    SPI_PORT &= ~(1 << SPI_SERIAL);

    tossByte(firstByte);
    tossByte(curNum);

    SPI_PORT  |= (1 << SPI_RCK);                                                //Latchpin on(toss the data to the leds)
    SPI_PORT  &= ~(1 << SPI_RCK);                                               //Latchpin off(done)

    if(++plexDigit == 4)
        plexDigit = 0;

}

void waitabit(uint8_t abit)
{
    for(volatile uint8_t thisBit = 0; thisBit != abit; ++thisBit)
        for(volatile uint8_t soove = 0; soove != 0xFF; ++soove)
            for(volatile uint8_t sove = 0; sove != 0xFF; ++sove)
                asm volatile ("nop");
}

void post(void)
{
    numbers[0] = 1;
    numbers[1] = 2;
    numbers[2] = 3;
    numbers[3] = 4;

    updateDisplay();
    waitabit(10);

    updateDisplay();
    waitabit(10);

    updateDisplay();
    waitabit(10);

    updateDisplay();
    waitabit(10);
}

void decreaseNumber(void)
{
    --numbers[3];
    if(numbers[3] == 255)
    {
        --numbers[2];
        numbers[3] = 9;
    }
    if(numbers[2] == 255)
    {
        --numbers[1];
        numbers[2] = 5;
    }
    if(numbers[1] == 255)
    {
        --numbers[0];
        numbers[1] = 9;
    }
    if(numbers[0] == 255)
    {
        numbers[0] = 0;
        numbers[1] = 0;
        numbers[2] = 0;
        numbers[3] = 0;
        OUTPUT_PORT &= ~(1 << OUTPUT_0);                                                            //Off with the light
        OUTPUT_PORT &= ~(1 << OUTPUT_1);                                                            //Off with the light
    }
    blink = !blink;
}


void upPressed(void)
{
    ++numbers[1];
    if(numbers[1] == 10)
    {
        numbers[1] = 0;
        ++numbers[0];
        if(numbers[0] == 10)
            numbers[0] = 0;
    }
}

void downPressed(void)
{
    --numbers[1];
    if(numbers[1] == 255)
    {
        numbers[1] = 9;
        --numbers[0];
        if(numbers[0] == 255)
            numbers[0] = 9;
    }
}

void actionPressed(void)
{
    if(!running)                                                                                            //We want to start the countdown
    {
        uint8_t volatile tempInt1 = eeprom_read_byte((uint8_t*)0);
        uint8_t volatile tempInt2 = eeprom_read_byte((uint8_t*)1);

        if(tempInt1 != numbers[0] && tempInt2 != numbers[1])                                                //We only update eeprom if needed
        {
            eeprom_write_byte((uint8_t*)0, numbers[0]);
            eeprom_write_byte((uint8_t*)1, numbers[1]);
        }
        backupNumbers[0] = numbers[0];
        backupNumbers[1] = numbers[1];
        backupNumbers[2] = numbers[2];
        backupNumbers[3] = numbers[3];
        running = true;
        OUTPUT_PORT |= (1 << OUTPUT_0) | (1 << OUTPUT_1);                                                                    //Flip on the light
        timer_start();
    }
    else if(running)
    {
        running = false;
        OUTPUT_PORT &= ~(1 << OUTPUT_0) & ~(1 << OUTPUT_1);                                                                   //Flip off the light
        timer_stop();
        blink = true;
        numbers[0] = backupNumbers[0];
        numbers[1] = backupNumbers[1];
        numbers[2] = backupNumbers[2];
        numbers[3] = backupNumbers[3];
    }
}

void checkForKeys(void)
{
    static uint16_t pinUPDebounce;
    static uint16_t pinDOWNDebounce;
    static uint16_t pinACTIONDebounce;
    static uint16_t debounceTime = 0x1FF;
    uint8_t iPins;
    iPins = BUTTON_PINS;
    if(running)
    {
        pinUPDebounce = 0xFFFF;
        pinDOWNDebounce = 0xFFFF;
    }
    if(!(iPins & (1 << BUTTON_UP)) && pinUPDebounce <= debounceTime)
    {
        if(pinUPDebounce == debounceTime)
             upPressed();
        if(pinUPDebounce < (debounceTime +1))
            ++pinUPDebounce;
    }
    else if(!(iPins & (1 << BUTTON_DOWN)) && pinDOWNDebounce <= debounceTime)
    {
        if(pinDOWNDebounce == debounceTime)
            downPressed();
        if(pinDOWNDebounce < (debounceTime +1))
            ++pinDOWNDebounce;
    }
    if(!(iPins & (1 << BUTTON_ACTION)) && pinACTIONDebounce <= debounceTime)
    {
        if(pinACTIONDebounce == debounceTime)
            actionPressed();
        if(pinACTIONDebounce < (debounceTime +1))
            ++pinACTIONDebounce;
    }

    if(iPins & (1 << BUTTON_UP))
        pinUPDebounce = 0;
    if(iPins & (1 << BUTTON_DOWN))
        pinDOWNDebounce = 0;
    if(iPins & (1 << BUTTON_ACTION))
        pinACTIONDebounce = 0;
}

int main(void)
{
    init();
    post();
    numbers[0] = backupNumbers[0];
    numbers[1] = backupNumbers[1];
    numbers[2] = 0;
    numbers[3] = 0;
    updateDisplay();
    while(1)
    {
        if(mainFlags & (1 << updateDisplayFlag))
        {
            mainFlags &= ~(1 << updateDisplayFlag);
            updateDisplay();
        }
        checkForKeys();
    }
    return 0;
}

ISR(TIM1_COMPA_vect)
{
    static uint16_t teller;
    if(++teller == 500)
    {
        decreaseNumber();
        teller = 0;
    }
}

ISR(TIM0_OVF_vect)
{
    mainFlags |= (1 << checkForKeysFlag);
    mainFlags |= (1 << updateDisplayFlag);
}
