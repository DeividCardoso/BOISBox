//float tanqueVazio = 100, tanqueCheio = 0;
int opcao = 0;

bool setNivelMinimo(){
  float distancia = retornaDistancia();
  tanqueVazio = distancia;
  return true;
}

bool setNivelMaximo(){
  float distancia = retornaDistancia();
  tanqueCheio = distancia;
  return true;
}

void resetaSensorNivel(){
  tanqueVazio = 100;
  tanqueCheio = 0;
}

void configuraUltrassonico(){  
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  digitalWrite(trig, LOW);
}

float retornaDistancia(){
  float tempoPulso;     
  float distanciaCalculada;
  float somaDistancia = 0;
  int nMedidas = 0;
  
  for (int i = 0; i < 5; i++){    
         trigPulse();
         tempoPulso = pulseIn(echo, HIGH, 200000);
          distanciaCalculada = tempoPulso/58.82;                  //58.82 Valor de conversão - Datasheet
         if(distanciaCalculada != 0 && distanciaCalculada < 200)
         {
            somaDistancia += distanciaCalculada;
            nMedidas++;
         }
         i++;
   }
  return somaDistancia/nMedidas;   
}

void trigPulse()
{
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
}

float retornaNivel(float distancia){  
  return (100/(tanqueCheio - tanqueVazio))*(distancia - tanqueVazio);  
}

float retornaNivel(){  
  return (100/(tanqueCheio - tanqueVazio))*(retornaDistancia() - tanqueVazio);  
}
