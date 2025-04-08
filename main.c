#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "lwip/sys.h"			// Bibliotecas Light Weight TCP/IP
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/err.h"

//Para ledc
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "soc/ledc_reg.h"
#include "stdio.h"

#include "string.h"

// En este archivo se define el nombre de la red (ssid) a la que me quiero conectar, asi como su pasword.
#include "MyNetInfo.h"	

// ================================================= Constantes globales
#define Anodo GPIO_NUM_25	
#define LEDR GPIO_NUM_26	
#define LEDG GPIO_NUM_33	
#define LEDB GPIO_NUM_32	


//================================================== Configuraciones =================================================
// Configuracion del ledc timer
	ledc_timer_config_t PWM_timer = 
	    {
	        .duty_resolution = LEDC_TIMER_8_BIT, 		// resolution of PWM duty (1024)
	        .freq_hz = 10000,                      		// frequency of PWM signal
	        .speed_mode = LEDC_HIGH_SPEED_MODE,      	// timer mode
	        .timer_num = LEDC_TIMER_0,            		// timer index
	        .clk_cfg = LEDC_APB_CLK,             		// Reloj de 80Mhz
	    };
	    ledc_timer_config_t PWM_timer1 = 
	    {
	        .duty_resolution = LEDC_TIMER_8_BIT, 		// resolution of PWM duty (1024)
	        .freq_hz = 10000,                      		// frequency of PWM signal
	        .speed_mode = LEDC_HIGH_SPEED_MODE,      	// timer mode
	        .timer_num = LEDC_TIMER_1,            		// timer index
	        .clk_cfg = LEDC_APB_CLK,             		// Reloj de 80Mhz
	    };
	 // Configuracion del canal del timer (cada ledc timer tiene dos canales)
	 //Azul (blue)
	 ledc_channel_config_t PWM_channel1 = 
	  {
		  .channel    = LEDC_CHANNEL_0,
		  .duty       = 255, //Ciclo minimo de 325
	      .gpio_num   = LEDB,
		  .speed_mode = LEDC_HIGH_SPEED_MODE,
		  .intr_type = LEDC_INTR_DISABLE,
		  .hpoint     = 0,
		  .timer_sel  = LEDC_TIMER_0
	  };
	  //Verde (green)
	  ledc_channel_config_t PWM_channel2 = 
	  {
		  .channel    = LEDC_CHANNEL_1,
		  .duty       = 255, //Ciclo minimo de 325
	      .gpio_num   = LEDG,
		  .speed_mode = LEDC_HIGH_SPEED_MODE,
		  .intr_type = LEDC_INTR_DISABLE,
		  .hpoint     = 0,
		  .timer_sel  = LEDC_TIMER_0
	  };
	  //Rojo (Red)
	   ledc_channel_config_t PWM_channel3= 
	  {
		  .channel    = LEDC_CHANNEL_2,
		  .duty       = 255, //Ciclo minimo de 325
	      .gpio_num   = LEDR,
		  .speed_mode = LEDC_HIGH_SPEED_MODE,
		  .intr_type = LEDC_INTR_DISABLE,
		  .hpoint     = 0,
		  .timer_sel  = LEDC_TIMER_1
	  };
	  
//  ------------------------------------------------------------------------------------------------------------------------------------------------- (Interpretacion del servidor)
const static char HTML_Header[] = "HTTP/1.1 200 OK\nContent-type: text/html\nConnection: close\n\n";	// Indicamos que el dato anterior es de tipo html 			I_______Estas dos lineas es para que el servidor web sepa de que tipo son los los archivos que estamos enviando 			
const static char icoIMG_Header[] = "HTTP/1.1 200 OK\nContent-type: image/ico\n\n";						// Indicamos que el dato anterior es de tipo image/ico 		I		
extern const uint8_t LEDsPage_html_start[] asm("_binary_LEDsPage_html_start");							// Inicio del archivo de la pagina web			   	I								
extern const uint8_t LEDsPage_html_end[]   asm("_binary_LEDsPage_html_end");							// Final del archivo de la pagia web				I_____	Estas 4 lineas indican los archivos embebidos que se guardaran dentro de la esp32 (especificamente su tamaño)						
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");								// Inicio del Archivo favicon.ico					I	
extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");								// Final del Archivo favicon.ico				    I
//  ------------------------------------------------------------------------------------------------------------------------------------------------- (Redes wifi(Station and access point) )
uint8_t STA_MAC_Addr[6];						// Aqui vamos a almacenar nuestra direccion MAC
static int Reintentos = 0;						// numero de intentos de coneccion a la red.
#define Maxima_Cantidad_DeIntentos 5			// Maximo numero de reintentos de coneccion
static EventGroupHandle_t wifi_event_group;		// Estructura de eventos para el WiFi. Nos sirve para señalar en otras partes del codigo, si estamos conectados a la red, o si sucedio un error.
#define WIFI_CONNECTED_BIT BIT0					// Bit dentro de la estructura anterior usado para indicar que estamos conectados
#define WIFI_FAIL_BIT      BIT1					// Bit dentro de la estructura anterior para indicar que ocurrio un error.
// Esta funcion es el manejador de eventos para el WiFi. Los eventos que vamos a monitorear para la estacion son Los eventos que vamos a monitorear para el punto de acceso son:
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)  // Manejador de eventos del WiFi
{		
//  ------------------------------------------------------------------------------------------------------------------------------------------------- (Station mode (STA))
	// Si (el event es de tipo wifi) y (se indica que el sistema ya ha incializado y empezado el suubsitema de wifi)
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{	
		esp_wifi_connect(); 											// Se trata de conectar al wifi													
		xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);		// Se establece que no se ha conectado en el evento						
	}
	// Si el estado  del wifi esta desconectado
	else if (event_base == WIFI_EVENT	&& event_id == WIFI_EVENT_STA_DISCONNECTED) 
	{		
		// Se trata de conectar solo si no ha reintentado conectarse muchas veces
		if (Reintentos < Maxima_Cantidad_DeIntentos)
		{												
			esp_wifi_connect();						// Se trata de conectar al wifi								
			Reintentos++;							//Cuenta cuantos reintentos lleva										
			printf("Reconectando...\n");			//Indicamos que se sigue reconectando
		}
        // Si ya se llego al maximo numero de reintentos, entonces
		else
		{																				
			xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);					        // Se establece que ha fallado la conexion con el wifi en el evento		
			printf("Maximo numero de intentos de reconeccion alcanzado.\n");
		}
		xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);							// Al final borro el bit de que estamos conectados en el grupo de eventos.
	}
    // Si se detecta el evento de que nos asignaron IP, por lo tanto se conecto a una red
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
	{					
		Reintentos = 0;																		// Reiniciamos los intentos de reconeccion y
		xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);							// Encendemos el bit de que se conecto a la Red en el grupo de eventos.
		ip_event_got_ip_t* Evento = (ip_event_got_ip_t*)event_data;							// Recupero la informacion de la coneccion, para desplegarla.
		printf("\nEstacion WiFi Conectada.\n");
		printf("Direccion IP:" IPSTR "\n",IP2STR(&Evento->ip_info.ip));						// Imprimo la IP,
		printf("Mascara de Red:" IPSTR "\n",IP2STR(&Evento->ip_info.ip));					// La mascara de red
		printf("Puerta de Enlace:" IPSTR "\n",IP2STR(&Evento->ip_info.ip));					// y la puerta de enlace de la coneccion.
		esp_wifi_get_mac(ESP_IF_WIFI_STA, STA_MAC_Addr);									// Por ultimo, obtengo y despliego la direccion MAC
		printf("MAC ADDR:%02X:%02X:%02X:%02X:%02X:%02X:", STA_MAC_Addr[0], STA_MAC_Addr[1], STA_MAC_Addr[2], STA_MAC_Addr[3], STA_MAC_Addr[4], STA_MAC_Addr[5]); //[0], STA_MAC_Addr[1], STA_MAC_Addr[2], STA_MAC_Addr[3], STA_MAC_Addr[4], STA_MAC_Addr[5]
	}
//  ------------------------------------------------------------------------------------------------------------------------------------------------- (Access point mode (AP) )
    // Si alguien se conecta al AP del esp 32
    else if (event_id == WIFI_EVENT_AP_STACONNECTED)
	{
        printf("\nSe han conectado a la AP del esp32.\n");
	    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
	    printf("MAC ADDR:%02X:%02X:%02X:%02X:%02X:%02X:", STA_MAC_Addr[0], STA_MAC_Addr[1], STA_MAC_Addr[2], STA_MAC_Addr[3], STA_MAC_Addr[4], STA_MAC_Addr[5]); //[0], STA_MAC_Addr[1], STA_MAC_Addr[2], STA_MAC_Addr[3], STA_MAC_Addr[4], STA_MAC_Addr[5]
	    printf(" Conectada. AID: %d\n", event->aid);
	}
    // Si alguien se desconecta del AP del esp 32
	else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
	{
        printf("\nSe han desconectado de la AP del esp32.\n");
	    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        printf("MAC ADDR:%02X:%02X:%02X:%02X:%02X:%02X:", STA_MAC_Addr[0], STA_MAC_Addr[1], STA_MAC_Addr[2], STA_MAC_Addr[3], STA_MAC_Addr[4], STA_MAC_Addr[5]); //[0], STA_MAC_Addr[1], STA_MAC_Addr[2], STA_MAC_Addr[3], STA_MAC_Addr[4], STA_MAC_Addr[5]
	    printf(" Desconectada. AID: %d\n", event->aid);
	}
}
//  -------------------------------------------------------------------------------------------------------------------------------------------------
int LED_State = 0;
uint8_t AP_MAC_Addr[6];
uint8_t STA_MAC_Addr[6];
//  -------------------------------------------------------------------------------------------------------------------------------------------------
static void ResponderConexion(struct netconn *Coneccion) {
    struct netbuf *InputBuffer;			                        // Tipo de dato que almacena los datos recibidos de una conexion wifi (buffer porque asi se llama a un almacen de datos temporal)																	
    char *Buffer;					                            // Creo un puntero cadena para guardar el texto de la peticion html.					
	u16_t buflen;			                                    // Longitud del Buffer																			
	err_t err;                                                  //  Tipo de dato para analizar error	
	err = netconn_recv(Coneccion, &InputBuffer);                // Bloquea la tarea mientras espera que lleguen datos en la coneccion. InputBuffer contiene los datos recibidos.
	if (err == ERR_OK) {
	netbuf_data(InputBuffer, (void**) &Buffer, &buflen);	// Los datos obtenidos en InputBuffer se almacenan en Buffer y la longitud en Buflen									
    char *ptr;                                              // Inicializamos un puntero llamado ptr
    
    //  -------------------------------------------------------------------------------------------------------------------------------------------------
        // Si se encontro la cadena hhtp
	    ptr = strstr(Buffer, "GET / HTTP");			            // Esta linea busca la cadena Http dentro del buffer
	    if(ptr != NULL) {
	    	printf("Se recibio GET / HTTP\n");
	    	netconn_write(Coneccion, HTML_Header, sizeof(HTML_Header)-1, NETCONN_NOCOPY);
	    	netconn_write(Coneccion, LEDsPage_html_start, LEDsPage_html_end - LEDsPage_html_start, NETCONN_NOCOPY);
	    }
    //  -------------------------------------------------------------------------------------------------------------------------------------------------
        // Si se encontro la cadena favicon.ico
	   	ptr = strstr(Buffer, "GET /favicon.ico");															//  Se recibio la peticion de envio del favicon
		if(ptr != NULL) {
			printf("Se recibio GET /favicon.ico\n");
			netconn_write(Coneccion, icoIMG_Header, sizeof(icoIMG_Header) - 1, NETCONN_NOCOPY);
			netconn_write(Coneccion, favicon_ico_start, favicon_ico_end - favicon_ico_start, NETCONN_NOCOPY);
		}
    //  ------------------------------------------------------------------------------------------------------------------------------------------------- Enviamos valores
        // Si se encontro la cadena Enviar valores 
        ptr = strstr(Buffer, "EnviarValoresActuales");
        if(ptr != NULL) 
        {
            char Valor[1][8]; // Modificado: solo 3 LEDs
            char Respuesta[30]; // Modificado: tamaño de respuesta reducido
            
            sprintf(Valor[0], "Anodo=%d", gpio_get_level(Anodo)); 
            
            // Almacena en respuesta el estado de los led con el siguiente formato
            sprintf(Respuesta, "%s", Valor[0]); 
            
            printf("Se pide EnviarValoresActuales\n");
            // Se imprime la respuesta
            printf("%s\n", Respuesta);
            
            // Se envían los datos
            netconn_write(Coneccion, Respuesta, sizeof(Respuesta), NETCONN_NOCOPY);
        }
         // Agrega esta línea para imprimir el contenido del buffer
        printf("Contenido del Buffer: %.*s\n", buflen, Buffer);
         //  ------------------------------------------------------------------------------------------------------------------------------------------------- Recibimos valores
	   	 //  ------------------------------------------------------------------------------------------------------------------------------------------------- Prender/Apagar leds
	   	ptr = strstr(Buffer, "Anodo=0");
	   	if(ptr != NULL) 
	   	{
			gpio_set_level(Anodo, 0);
	   		printf("Se apaga el led \n");
	   	}
	   	ptr = strstr(Buffer, "Anodo=1");
	   	if(ptr != NULL) 
	   	{
			gpio_set_level(Anodo, 1);
	   		printf("Se prende el led  \n");
	   	}
	 

	   //  ------------------------------------------------------------------------------------------------------------------------------------------------- Color regb
	   ptr = strstr(Buffer, "RGB=");
		if(ptr != NULL) {
	    int r, g, b;
	    sscanf(ptr, "RGB=%d,%d,%d", &r, &g, &b); // Extrae valores RGB
	    
	    // Mapea los valores (0-255) al duty cycle invertido (255-0) porque es ánodo común
	    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, 255 - r); // Rojo (PWM_channel3)
	    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 255 - g); // Verde (PWM_channel2)
	    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 255 - b); // Azul (PWM_channel1)
	    
	    // Actualiza los duty cycles
	    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
	    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
	    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2);
	    
	    printf("Valores RGB recibidos: R=%d, G=%d, B=%d\n", r, g, b);
}
	   
	}
	netconn_close(Coneccion);																				// Cierra la coneccion.
	netbuf_delete(InputBuffer);																				// Borra el buffer de entrada.
}
//  -------------------------------------------------------------------------------------------------------------------------------------------------

//  -------------------------------------------------------------------------------------------------------------------------------------------------
// Esta es la TAREA del servidor web. Constantemente acepta conecciones y las responde, mientras no se detecte ningun error.
static void http_server(void *pvParameters) {			// Tarea del Servidor Web
	struct netconn *conn, *newconn;						// Crea las estructuras para las conecciones
	err_t err;
	conn = netconn_new(NETCONN_TCP);					// Crea una estructura de abstraccion de conecion TCP.No establece conexion ni envia datos por la red..
	netconn_bind(conn, IP_ADDR_ANY , 80);				// Asigna la coneccion al puerto 80 (html)
	netconn_listen(conn);								// Escucha el puerto
	do {												// Mientras no haya error:
		err = netconn_accept(conn, &newconn);			// Bloquea el la tarea hasta que se reciba una peticion de coneccion en la coneccion (debe estar en estado de escucha). Una vez establecida la coneccion se regresa una nueva estructura de coneccion.
		if (err == ERR_OK) {							// Si no hay error en la peticion de coneccion:
			ResponderConexion(newconn);					// Respondo la peticion
			netconn_delete(newconn);					// borro la peticion de coneccion.
		}
	} while (err == ERR_OK);
	netconn_close(conn);								// Cierro la coneccion TCP
	netconn_delete(conn);								// Borra la coneccion.
}
//  -------------------------------------------------------------------------------------------------------------------------------------------------

//  -------------------------------------------------------------------------------------------------------------------------------------------------
void app_main() {
	
	//Conf
	gpio_config_t ConfiguracionDeLasSalidas = {
        .pin_bit_mask = ( 1ULL << Anodo ),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&ConfiguracionDeLasSalidas);
    // VALORES INCIALES
    gpio_set_level(Anodo, 0);
    //gpio_set_level(LEDR,1);
    // gpio_set_level(LEDG,1);
   // gpio_set_level(LEDB,1);
   
	// Se aplican las configuraciones del PWm
	ledc_timer_config(&PWM_timer);
	ledc_timer_config(&PWM_timer1);
	ledc_channel_config(&PWM_channel1);
	ledc_channel_config(&PWM_channel2);
	ledc_channel_config(&PWM_channel3);
	
	




//  ------------------------------------------------------------------------------------------------------------------------------------------------
	wifi_event_group = xEventGroupCreate();									// Creo el grupo de eventos para el WiFi.
	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	nvs_flash_init();														// Inicializa la Memoria Flash (Non Volatil Storage)
	esp_netif_init();														// Inicia la Capa de abstraccion segura para el Stack TCP/IP
	ESP_ERROR_CHECK(esp_event_loop_create_default());						// Inicializa el manejador de eventos de WiFi
	esp_netif_create_default_wifi_sta();									// Inicializa y crea el objeto WiFi Estacion
	esp_netif_t* My_AccessPoint;
	My_AccessPoint = esp_netif_create_default_wifi_ap();					// Inicializa y crea el objeto WiFi Punto de acceso
	wifi_init_config_t wifi_drvconfig = WIFI_INIT_CONFIG_DEFAULT();			// Configura la estructura del Driver de WiFi
	//Inicializacion
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_drvconfig));						// Configura e inicializa el Driver de WiFi
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id)); // Registro del Manejador de eventos del WiFi.
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip)); 	 // Registra al manejador de eventos para cuando recibe una IP
	wifi_config_t ap_config = {						// Crea la estructura de configuracion del punto de acceso.
		.ap = {
		.ssid = "PIPE_ESP",							// Nombre de la red
		.ssid_len = strlen("PIPE_ESP"),				// Tamaño del nombre de la red
		.channel = 1,								// Canal de Comunicacion
		.password = "12345678",						// Clave
		.max_connection = 10,						// Numero maximo de conecciones Simultaneas (De 4 a 10)
		.authmode = WIFI_AUTH_WPA_WPA2_PSK      	// Si no se quiere password :WIFI_AUTH_OPEN;
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));					// Pone al WiFi en modo punto de acceso + estacion
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));		// Configura el Punto de Acceso
	//Start del wifi
	ESP_ERROR_CHECK(esp_wifi_start());										// Inicia el WiFi para recibir conecciones
	wifi_config_t sta_config = {											// Llena la estructura para conectar el WiFi en modo Estacion.
		.sta = {
		.ssid = MySSID,											// Nombre de la red al la que se va a conectar
		.password = MyPSWD,										// Pasword de la red
		.scan_method = WIFI_FAST_SCAN,							// Metodo de escaneo de ssid
		.sort_method = WIFI_CONNECT_AP_BY_SIGNAL,				// Ordena las redes por fuerza de señal
		.threshold.rssi = -127,									// Minima fuerza de señal para incluir la red en la deteccion
		.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,			// Metodo de autentificacion de la red
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));			// Configura el WiFi para conectarse a la red MySSID con el pasword MyPSWD definidos en MyNetInfo.h
//  ------------------------------------------------------------------------------------------------------------------------------------------------
	esp_netif_ip_info_t ip_info;											// Creo la estructura para guardar la informacion de la coneccion.
	ESP_ERROR_CHECK(esp_netif_get_ip_info(My_AccessPoint, &ip_info));		// Obtiene y Despliega la informacion de la Coneccion
	printf("Configuracion del Punto de Acceso\n");
	printf("Direccion IP:" IPSTR "\n",IP2STR(&ip_info.ip));					// Imprimo la IP,
	printf("Mascara de Red:" IPSTR "\n",IP2STR(&ip_info.ip));				// La mascara de red
	printf("Puerta de Enlace:" IPSTR "\n",IP2STR(&ip_info.ip));				// y la puerta de enlace de la coneccion.
	uint8_t AP_MAC_Addr[6];
	esp_wifi_get_mac(ESP_IF_WIFI_AP, AP_MAC_Addr);							// Obtiene y Despliega la direccion MAC
	printf("MAC ADDR: %02X:%02X:%02X:%02X:%02X:%02X\n", AP_MAC_Addr[0], AP_MAC_Addr[1], AP_MAC_Addr[2], AP_MAC_Addr[3], AP_MAC_Addr[4], AP_MAC_Addr[5]);
	printf("Access Point Configurado. SSID: \"%s\", Pasword: \"%s\", Cannal: %d\n", ap_config.ap.ssid, ap_config.ap.password, ap_config.ap.channel);
//  ------------------------------------------------------------------------------------------------------------------------------------------------
	xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);			// Creo la tarea encargada de manejar el servidor html
	while (1) {
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}





