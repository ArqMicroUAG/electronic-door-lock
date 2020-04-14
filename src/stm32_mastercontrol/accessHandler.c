/* Task based USART demo:
 * Rewritten by Alejandro Enriquez 
 * 16 March 2020
 * Comments:
 * Really Ugly Implementation, it halts the system when attempting multiple and fast subsecuents
 * inputs like an antighosting test from a fast keyboard.
 * A better way could be to implement a semaphore or a YIELD from ISR freertos method.
 *
 * STM32F103C8T6:
 *	TX:	A9  <====> RX of TTL serial
 *	RX:	A10 <====> TX of TTL serial
 *	CTS:	A11 (not used)
 *	RTS:	A12 (not used)
 *	Config:	8N1
 *	Baud:	9600
 * Caution:
 *	Not all GPIO pins are 5V tolerant, so be careful to
 *	get the wiring correct.
 */
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/f1/nvic.h>

/*********************************************************************
 * Handlers
 *********************************************************************/
//Task and Message Queue Handlers
TaskHandle_t tSendHandler;
TaskHandle_t tReceiveHandler;
TaskHandle_t tLedHandler;
QueueHandle_t qMessageHandler;

/*********************************************************************
 * Setup the Clocks
 *********************************************************************/
static void init_clock(void) {

	rcc_clock_setup_in_hse_8mhz_out_24mhz();
	rcc_periph_clock_enable(RCC_GPIOC);

	// Clock for GPIO port A: GPIO_USART1_TX, USART1
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);
	//enable the AFIO clock otherwise shit happens i beleive, need to investigate
	rcc_periph_clock_enable(RCC_AFIO);
}

/*********************************************************************
 * Setup the GPIO(LEDS)
 *********************************************************************/
static void init_gpio(void) {

	gpio_set(GPIOA, GPIO6 | GPIO7);
	gpio_set(GPIOA, GPIO11 | GPIO12);
	gpio_toggle(GPIOC,GPIO13);
	gpio_toggle(GPIOA,GPIO7);
	
	gpio_set_mode(GPIOC,GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,GPIO13);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO6 | GPIO7);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,GPIO_CNF_OUTPUT_PUSHPULL, GPIO11 | GPIO12);


}
/*********************************************************************
 * Setup the UART
 *********************************************************************/
static void uart_setup(void) {

	nvic_enable_irq(NVIC_USART1_IRQ);
	// UART TX on PA9 (GPIO_USART1_TX)
	gpio_set_mode(GPIOA,
		GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
		GPIO_USART1_TX);
	
	
	// Configure UART settings
	usart_set_baudrate(USART1,9600);
	usart_set_databits(USART1,8);
	usart_set_stopbits(USART1,USART_STOPBITS_1);
	usart_set_mode(USART1,USART_MODE_TX_RX);
	usart_set_parity(USART1,USART_PARITY_NONE);
	usart_set_flow_control(USART1,USART_FLOWCONTROL_NONE);
	usart_enable_rx_interrupt(USART1);
	usart_enable(USART1);

	
}
/*********************************************************************
 * ISR USART_RX handler function (Name definded by libopencm3)
 *********************************************************************/
void usart1_isr(void){
	
	usart_disable_rx_interrupt(USART1);
	gpio_toggle(GPIOA,GPIO6);
	xTaskResumeFromISR(tReceiveHandler);
	gpio_toggle(GPIOA,GPIO7);
}



/*********************************************************************
 * Task that Sends Characters to the UART
 *********************************************************************/
static void tSend(void *args __attribute__((unused))) {
	unsigned char tx_char=0x37; // it is a known value the  "7" to see if we are not getting wrong data
	
	for (;;) {
		
		//Read Character received via message queue
		if (!xQueueReceive(qMessageHandler,&tx_char,1000))
		{
			//Error clear our visual aid
			gpio_clear(GPIOA,GPIO11);
		}else {		
			//toggle our visual aid
			gpio_toggle(GPIOA,GPIO11);
			//send it to the usar the loopback
			usart_send(USART1,tx_char);
			//re-enable the interrupt flag celared by libopncm3
			usart_enable_rx_interrupt(USART1);
		}
		//Block task
		vTaskSuspend(tSendHandler);
		
	}
}

/*********************************************************************
 * Task that Receives Characters from UART via Interruption
 * Send a message queue to the Send function for the loopback
 *********************************************************************/
static void tReceive(void *args __attribute__((unused))){
	unsigned char rx_char=0x38; // this is a known value of "8" to check that we are not queing wrong data for the lb
	unsigned char rx_access=0;

	for (;;){

		//read from usart RX channel		
		rx_char=usart_recv(USART1);

		//check that the data is an A or a 0x41 of "Accepted"
		if(rx_char == 0x41 )
		{
			rx_access=0x59;		
		}
		else
		{
			rx_access=0x4E;
		}


		//Send Message queue to tSend Task to unblck it
        if(!xQueueSend(qMessageHandler,&rx_access,1000))
		{
			//error clear our visual aid
			gpio_clear(GPIOA,GPIO12);
		}else{
			//resume the send task
		   vTaskResume(tSendHandler);
		   //toggle our visual aid
		   gpio_toggle(GPIOA,GPIO12);
		   		   
		}
		
		//Block task*/
		vTaskSuspend(tReceiveHandler);
		
	}
}


/*********************************************************************
 * Task that blinks Pin13 (LED)
 *********************************************************************/
static void tLed(void *args __attribute((unused))) {
	for (;;) {
		gpio_toggle(GPIOC,GPIO13);
		vTaskDelay(100);
	}
}

/*********************************************************************
 * Main program
 *********************************************************************/
int main(void) {

	init_clock();
	init_gpio();
	uart_setup();
	
	//Define Message Queue with 10 elements of one byte before creating task
	qMessageHandler = xQueueCreate( 100, sizeof(char));	
	//Send Task
	 xTaskCreate(tSend, "Send", 100, NULL, tskIDLE_PRIORITY,&tSendHandler);
	//Receive Task
	xTaskCreate(tReceive, "Receive" , 100 , NULL , tskIDLE_PRIORITY, &tReceiveHandler);
	//Led Task
	xTaskCreate(tLed,"Led", 100, NULL,tskIDLE_PRIORITY, &tLedHandler);
	
	//Suspend the tSend task, it won't run untill enabled by the tReceive task
	vTaskSuspend(tSendHandler);
	vTaskSuspend(tReceiveHandler);
	
	//Start Scheduler
	vTaskStartScheduler();

    //never reach this point
	for (;;);
	return 0;
}

// End
