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

#define LED_PIO       PIOC
#define LED_PIO_ID    ID_PIOC
#define LED_IDX       8u
#define LED_IDX_MASK  (1u << LED_IDX)

/************************************************************************/
/* constants                                                            */
/************************************************************************/

/************************************************************************/
/* variaveis globais                                                    */
/************************************************************************/
volatile Bool f_rtt_alarme = false;

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

void io_init(void) {
	/* led */
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_configure(LED_PIO, PIO_OUTPUT_0, LED_IDX_MASK, PIO_DEFAULT);
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

/************************************************************************/
/* Main                                                                 */
/************************************************************************/



int main(void) {
	WDT->WDT_MR = WDT_MR_WDDIS;
	board_init();
	sysclk_init();
	configure_lcd();
	int banana = 0;
	long tempo_total = 0;

	f_rtt_alarme = true;

	font_draw_text(&sourcecodepro_28, "OIMUNDO", 50, 50, 1);
	font_draw_text(&arial_72, "102456", 50, 200, 2);

	while (1) {
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
				
				char tempo[12];
				tempo_total++;
				sprintf(tempo, "totP:%d",  tempo_total);
				font_draw_text(&calibri_36, tempo, 50, 100, 1); // + 36 sempre -> \n
				banana=!banana;
			} else {
				char tempo[12];
				sprintf(tempo, "AAAA:%d",  rtt_read_timer_value(RTT));
				tempo_total++;
				font_draw_text(&calibri_36, tempo, 50, 100, 1); // + 36 sempre -> \n
				banana = !banana;
			}

			f_rtt_alarme = false;
		}


	}
	return 0;
}