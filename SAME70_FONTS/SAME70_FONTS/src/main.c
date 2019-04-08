/*
* main.c
*
* Created: 05/03/2019 18:00:58
*  Author: eduardo
*/

/************************************************************************/
/* includes                                                             */
/************************************************************************/

#include <asf.h>
#include "tfont.h"
#include "sourcecodepro_28.h"
#include "calibri_36.h"
#include "arial_72.h"



struct ili9488_opt_t g_ili9488_display_opt;

void configure_lcd(void) {
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH - 1, ILI9488_LCD_HEIGHT - 1);

}


void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while (*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if (letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}
}





/************************************************************************/
/* defines                                                              */
/************************************************************************/
#define PI 3.14159265358979f

#define APERTADO '1'
#define LIBERADO '0'

typedef void (*p_handler) (uint32_t, uint32_t);

typedef struct {
	uint32_t PIO_NAME;
	uint32_t PIO_ID;
	uint32_t PIO_IDX;
	uint32_t PIO_MASK;
	volatile Bool but_flag;
	char BUT_NUM;
} botao;

/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/
volatile Bool but_status;

volatile Bool f_rtt_alarme = false;

botao BUT1 = {.PIO_NAME = PIOA, .PIO_ID = ID_PIOA, .PIO_IDX = 11u, .PIO_MASK = (1u << 11u), .BUT_NUM = '1'}; //botao placa para reset
botao BUT2 = {.PIO_NAME = PIOB, .PIO_ID = ID_PIOB, .PIO_IDX = 2u, .PIO_MASK = (1u << 2u), .BUT_NUM = '2'}; //sensor
botao BUT3 = {.PIO_NAME = PIOA, .PIO_ID = ID_PIOA, .PIO_IDX = 3u, .PIO_MASK = (1u << 3u), .BUT_NUM = '3'};


/************************************************************************/
/* prototypes                                                           */
/************************************************************************/
void pin_toggle(Pio *pio, uint32_t mask);
void io_init(void);
static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);

/************************************************************************/
/* interrupcoes                                                         */
/************************************************************************/

static float get_time_rtt() {
	uint ul_previous_time = rtt_read_timer_value(RTT);
}

void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {}

	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		
		f_rtt_alarme = true;                  // flag RTT alarme
	}
}

/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

void pin_toggle(Pio *pio, uint32_t mask) {
	if (pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio, mask);
}



static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
	uint32_t ul_previous_time;

	/* Configure RTT for a 1 second tick interrupt */
	rtt_sel_source(RTT, false);
	rtt_init(RTT, pllPreScale);

	ul_previous_time = rtt_read_timer_value(RTT);
	while (ul_previous_time == rtt_read_timer_value(RTT));

	rtt_write_alarm_time(RTT, IrqNPulses + ul_previous_time);

	/* Enable RTT interrupt */
	NVIC_DisableIRQ(RTT_IRQn);
	NVIC_ClearPendingIRQ(RTT_IRQn);
	NVIC_SetPriority(RTT_IRQn, 0);
	NVIC_EnableIRQ(RTT_IRQn);
	rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}

void but1_callback(void)
{
	BUT1.but_flag = true;
	if(!pio_get(BUT1.PIO_NAME, PIO_INPUT, BUT1.PIO_MASK))
	but_status = APERTADO;
	else
	but_status = LIBERADO;
}

void but2_callback(void)
{
	BUT2.but_flag = true;
	if(!pio_get(BUT2.PIO_NAME, PIO_INPUT, BUT2.PIO_MASK))
	but_status = APERTADO;
	else
	but_status = LIBERADO;
}

void but3_callback(void)
{
	BUT3.but_flag = true;
	if(!pio_get(BUT3.PIO_NAME, PIO_INPUT, BUT3.PIO_MASK))
	but_status = APERTADO;
	else
	but_status = LIBERADO;
}

void iniciabots(botao BOT, p_handler *funcao){
	
	pio_configure(BOT.PIO_NAME, PIO_INPUT, BOT.PIO_MASK, PIO_PULLUP|PIO_DEBOUNCE);
	pio_set_debounce_filter(BOT.PIO_NAME, BOT.PIO_MASK, 60);

	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BOT.PIO_NAME,
	BOT.PIO_ID,
	BOT.PIO_MASK,
	PIO_IT_FALL_EDGE,
	funcao);

	// Ativa interrupção
	pio_enable_interrupt(BOT.PIO_NAME, BOT.PIO_MASK);

	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BOT.PIO_ID);
	NVIC_SetPriority(BOT.PIO_ID, 4); // Prioridade 4

}




void io_init(void)
{
	
	iniciabots(BUT1, but1_callback);
	iniciabots(BUT2, but2_callback);
	iniciabots(BUT3, but3_callback);

}




/************************************************************************/
/* Main                                                                 */
/************************************************************************/





int main(void) {
	WDT->WDT_MR = WDT_MR_WDDIS;
	board_init();
	sysclk_init();
	configure_lcd();
	io_init();
	int banana = 0;
	long tempo_total = 0;
	long revolucoes = 0;
	long revolucoes_insta = 0;
	float vel_max = 0;
	long idle=0;

	f_rtt_alarme = true;

	

	while (1) {
		if(BUT1.but_flag) {
			revolucoes++;
			revolucoes_insta++;
			BUT1.but_flag = false;
			idle=0;
		}
		if(BUT2.but_flag) {
			revolucoes=0;
			tempo_total=0; //botao para reiniciar
			BUT2.but_flag = false;
		}
		
		
		if (f_rtt_alarme) {

			/*
			* O clock base do RTT é 32678Hz
			* Para gerar outra base de tempo é necessário
			* usar o PLL pre scale, que divide o clock base.
			*
			* Nesse exemplo, estamos operando com um clock base
			* de pllPreScale = 32768/32768/2 = 2Hz
			*
			* Quanto maior a frequência maior a resolução, porém
			* menor o tempo máximo que conseguimos contar.
			*
			* Podemos configurar uma IRQ para acontecer quando
			* o contador do RTT atingir um determinado valor
			* aqui usamos o irqRTTvalue para isso.
			*
			* Nesse exemplo o irqRTTvalue = 8, causando uma
			* interrupção a cada 2 segundos (lembre que usamos o
			* pllPreScale, cada incremento do RTT leva 500ms (2Hz).
			*/
			uint16_t pllPreScale = (int)(((float)32768) / 2.0);
			uint32_t irqRTTvalue = 4;

			// reinicia RTT para gerar um novo IRQ
			RTT_init(pllPreScale, irqRTTvalue);

			/*
			* caso queira ler o valor atual do RTT, basta usar a funcao
			*   rtt_read_timer_value()
			*/

			/*
			* CLEAR FLAG
			*/
			if (banana){
				font_draw_text(&sourcecodepro_28, "NAOMUNDO", 50, 50, 1);
				banana=!banana;
				} else {
				font_draw_text(&sourcecodepro_28, "SIMMUNDO", 50, 50, 1);
				banana = !banana;
			}
			
			if (idle<=20){
				char tempo[12];
				sprintf(tempo, "%d:%d:%d  ",  tempo_total/3600, (tempo_total%3600)/60, tempo_total%3600);
				
				font_draw_text(&calibri_36, tempo, 10, 100-36, 1); // + 36 sempre -> \n
				sprintf(tempo, "t_t:%d  ",  tempo_total);
				tempo_total+=2;
				idle+=2;
				font_draw_text(&calibri_36, tempo, 10, 100, 1); // + 36 sempre -> \n
				sprintf(tempo, "d_t:%f m ",  revolucoes*2*0.325*PI);
				font_draw_text(&calibri_36, tempo, 10 , 100+ 36*1, 1); // + 36 sempre -> \n
				sprintf(tempo, "v_me:%f km/h ",  ((revolucoes*2*0.325*PI)/tempo_total)*3.6);
				font_draw_text(&calibri_36, tempo, 10 , 100+ 36*2, 1); // + 36 sempre -> \n
				sprintf(tempo, "v_i:%f m/s ",  (revolucoes_insta*2*0.325*PI)/2);
				font_draw_text(&calibri_36, tempo, 10 , 100+ 36*3, 1); // + 36 sempre -> \n
				if ((revolucoes_insta*2*0.325*PI)/2 > vel_max){
					vel_max = (revolucoes_insta*2*0.325*PI)/2;
					font_draw_text(&calibri_36, "+vel acelerando      ", 10 , 100+ 36*4, 1); // + 36 sempre -> \n
					} else {
					font_draw_text(&calibri_36, "-vel desacelerando   ", 10 , 100+ 36*4, 1); // + 36 sempre -> \n !!ta errado
				}
				sprintf(tempo, "v_max:%f m/s ",  vel_max);
				font_draw_text(&calibri_36, tempo, 10 , 100+ 36*3, 1); // + 36 sempre -> \n
				revolucoes_insta =0;
				
			} else {
				//paro a contagem
				//desligar LCD? nao sei
				font_draw_text(&calibri_36, "IDLE                      ", 10 , 100- 36*1, 1); // + 36 sempre -> \n
				font_draw_text(&calibri_36, "IDLE                      ", 10 , 100+ 36*0, 1); // + 36 sempre -> \n
				font_draw_text(&calibri_36, "IDLE                      ", 10 , 100+ 36*1, 1); // + 36 sempre -> \n
				font_draw_text(&calibri_36, "IDLE                      ", 10 , 100+ 36*2, 1); // + 36 sempre -> \n
				font_draw_text(&calibri_36, "IDLE                      ", 10 , 100+ 36*3, 1); // + 36 sempre -> \n
				font_draw_text(&calibri_36, "IDLE                      ", 10 , 100+ 36*4, 1); // + 36 sempre -> \n
				
			}
			
			
			
			
			
			

			f_rtt_alarme = false;
		}
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
		
		


	}
	return 0;
}