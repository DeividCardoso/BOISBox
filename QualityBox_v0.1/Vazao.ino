// definir porta de leitura do sensor de Vazao
//const int portaVazao = GPIO_NUM_21;
hw_timer_t * timer = NULL;
volatile int pulsos_vazao = 0;
//double vazao = 0;
//double acumuladoVazao = 0;

// interrupção do sensor
void IRAM_ATTR gpio_isr_handler_up(void* arg)
{
  pulsos_vazao++;
  portYIELD_FROM_ISR();
}

void cb_timer(){
//static unsigned int counter = 1;
//    Serial.println("cb_timer(): ");
//    Serial.print(counter);
//    Serial.print(": ");
//    Serial.print(millis()/1000);
//    Serial.println(" segundos");
//    counter++;
//
//    Serial.print("Pulsos: ");
//    Serial.println(pulsos_vazao);
    
    vazao = pulsos_vazao * fatorConversao;           //8 = Fator de Conversao                                  
    vazao = vazao * 0.278;              //Converte de L/H para ml/s                                    
    acumuladoVazao += vazao;            //Consumo total                           
  
//  Serial.print("Leitura do Sensor de Vazao: ");
//  Serial.print(vazao);
//  Serial.println(" ml/s");
//  Serial.print("Leitura do Sensor Acumulado: ");
//  Serial.print(acumuladoVazao);
//  Serial.println(" ml");
  pulsos_vazao = 0;
}

void iniciaVazao(gpio_num_t Port){
  gpio_set_direction(Port, GPIO_MODE_INPUT);
  gpio_set_intr_type(Port, GPIO_INTR_NEGEDGE);
  gpio_set_pull_mode(Port, GPIO_PULLUP_ONLY);
  gpio_intr_enable(Port);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(Port, gpio_isr_handler_up, (void*) Port);
}

void startTimer(){
    /* 0 - seleção do timer a ser usado, de 0 a 3.
      80 - prescaler. O clock principal do ESP32 é 80MHz. Dividimos por 80 para ter 1us por tick.
    true - true para contador progressivo, false para regressivo
    */
    timer = timerBegin(0, 80, true);

    /*conecta à interrupção do timer
     - timer é a instância do hw_timer
     - endereço da função a ser chamada pelo timer
     - edge=true gera uma interrupção
    */
    timerAttachInterrupt(timer, &cb_timer, true);

    /* - o timer instanciado no inicio
       - o valor em us para 1s
       - auto-reload. true para repetir o alarme
    */
    timerAlarmWrite(timer, 1000000, true); 

    //ativa o alarme
    timerAlarmEnable(timer);
}

void stopTimer(){
    timerEnd(timer);
    timer = NULL; 
}

//void setup() {
//  // inicializar serial
//  Serial.begin(115200);
//  // definir porta do sensor de gas como entrada
//  iniciaVazao((gpio_num_t) portaVazao);
//  startTimer();
//}
//
//void loop() {
//  
//}
