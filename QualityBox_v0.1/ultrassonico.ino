float tanqueVazio = 0, tanqueCheio = 100;
int opcao = 0;

bool setNivelMinimo(){
  float distancia = retornaDistancia();
  if(distancia < tanqueCheio){
    tanqueVazio = distancia;
    return true;
  }
  return false;
}

bool setNivelMaximo(){
  float distancia = retornaDistancia();
  if(distancia > tanqueVazio){
    tanqueCheio = distancia;
    return true;
  }
  return false;
}

void resetaSensorNivel(){
  tanqueVazio = 0;
  tanqueCheio = 100;
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
  
  for (int i = 0; i < 20; i++){    
         trigPulse();
         tempoPulso = pulseIn(echo, HIGH, 200000);
          distanciaCalculada = tempoPulso/58.82;                  //58.82 Valor de conversão - Datasheet
         if(distanciaCalculada != 0 && distanciaCalculada < 100)
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
  return 100 - (100/(tanqueCheio - tanqueVazio))*(distancia - tanqueVazio);  
}

float retornaNivel(){  
  return 100 - (100/(tanqueCheio - tanqueVazio))*(retornaDistancia() - tanqueVazio);  
}
