#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
// poner el servidor web

#include <WiFiClient.h>

WiFiClient wifiClient;
HTTPClient http;

const String serverName = "xxxxxxxxxxxxxxx";

const char *ssid = "xxxxxxxxxxxxx";
const char *password = "xxxxxxxxxxxx";

unsigned long tiempoAnterior;
unsigned long tiempoAnteriorTomates;
unsigned long tiempoAnteriorApi;
int actual;
int cuenta;
bool descanso;
bool descansoTomates;
bool cambioPines = false;
// const int pinesDisponibles[] = {4, 5, 12, 13, 14};
// int pinesHabilitados[] = {0, 1, 2, 3, 4};
const int pinesDisponibles[] = {4, 5, 12, 13};
int pinesHabilitados[] = {0, 1, 2, 3};

const int pinTomates = 14; // D5

String pinesStringPost;
bool arranque;
bool pausa = false;

// Variables de riego
unsigned long tiempoRiego;
unsigned long tiempoDescanso;
unsigned long tiempoRiegoTomates;
unsigned long tiempoDescansoTomates;
unsigned long tiempoApi;

void encender(int pin);
void apagar(int pin);
void descansar();

void iniciarPines();
void CallApi();
void enviarPost(int val, int tipo);
void enviarPostTorres();
void apagarPines();
void encenderTomates();
void apagarTomates();
void descansarTomates();

void setup()
{

  iniciarPines();

  arranque = true;
  tiempoRiego = 30000;
  tiempoDescanso = 600000;

  tiempoRiegoTomates = 45 * 1000;
  tiempoDescansoTomates = 1000 * 60 * 5; // Va a regar los tomates cada 5 minutos

  tiempoApi = 8000;

  // Wifi
  WiFi.begin(ssid, password);
  Serial.println("Conectando a Wifi con SSID: " + String(ssid));
  Serial.println(WiFi.getHostname());
  Serial.println(WiFi.localIP());

  //
  pinesHabilitados[0] = 1;
  // pinesHabilitados[1] = 1;
  // pinesHabilitados[2] = 2;
  // pinesHabilitados[3] = 3;
  cuenta = 1;
  pinesStringPost = "";

  tiempoAnterior = millis();
  tiempoAnteriorTomates = millis();
  tiempoAnteriorApi = millis();
  Serial.begin(9600);
  actual = 0;
  descanso = false;
  descansoTomates = false;
  encender(actual);
  encenderTomates();
}

void loop()
{
  while (pausa)
  {
    apagar(actual);
    CallApi();
    delay(500);
  }

  if (arranque && WiFi.status() == WL_CONNECTED)
  {
    enviarPost(1, 1);
    arranque = false;
  }
  if (millis() - tiempoAnterior >= tiempoRiego && !descanso)
  {
    apagar(actual);
    if (actual == cuenta - 1)
    { // Acá viene el descanso cuando i == cuenta
      descanso = true;
      descansar();
      actual = 0;
      return;
    }
    else
    {

      actual++;
    }
    encender(actual);

    tiempoAnterior = millis();
  }

  if (millis() - tiempoAnterior >= tiempoDescanso + tiempoRiego && descanso)
  {

    tiempoAnterior = millis();
    descanso = false;
    encender(actual);
  }

  // TOMATES

  if (millis() - tiempoAnteriorTomates >= tiempoDescansoTomates + tiempoRiegoTomates && descansoTomates)
  {
    tiempoAnteriorTomates = millis();
    descansoTomates = false;
    encenderTomates();
    Serial.println("*************** Encender pin 14");
  }

  if (!descansoTomates && (millis() - tiempoAnteriorTomates) >= tiempoRiegoTomates)
  {
    apagarTomates();
    Serial.println("*************** Apagar pin 14");
    descansoTomates = true;
    tiempoAnteriorTomates = millis();
  }

  // API

  if (millis() - tiempoAnteriorApi >= tiempoApi)
  {
    Serial.println("Call Api");
    CallApi();
    tiempoAnteriorApi = millis();
  }
}

void encender(int pin)
{
  Serial.println("rele " + String(pinesDisponibles[pinesHabilitados[pin]]));
  digitalWrite(pinesDisponibles[pinesHabilitados[pin]], LOW);
  enviarPost(pinesDisponibles[pinesHabilitados[pin]], 1);
  Serial.print("Enciende pin ");
  Serial.println(pinesDisponibles[pinesHabilitados[pin]]);
}

void encenderTomates()
{
  digitalWrite(pinTomates, LOW);
  enviarPost(pinTomates, 5);
}

void apagarTomates()
{
  digitalWrite(pinTomates, HIGH);
}

void apagar(int pin)
{
  digitalWrite(pinesDisponibles[pinesHabilitados[pin]], HIGH);
  Serial.println("off " + String(pinesDisponibles[pinesHabilitados[pin]]));
}

void descansar()
{
  Serial.println("Descanso");
  enviarPost(0, 1);
}

void descansarTomates()
{
  Serial.println("Descanso Tomates");
  enviarPost(0, 5); // tipo 5 es tomates y 0 significa descanso
}

void CallApi()
{
  Serial.println(WiFi.localIP());
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, password);
    arranque = true;
  }
  else
  {
    http.begin(wifiClient, serverName.c_str());

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200)
    {
      String payload = http.getString();
      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, payload);
      if (error)
      {
        return;
      }
      else
      {
        if ((int)tiempoApi != (int)jsonBuffer["tiempoapi"])
        {
          tiempoApi = (int)jsonBuffer["tiempoapi"]; // En milisegundos
          Serial.print("-----------> ");
          Serial.println(String((int)jsonBuffer["tiempoapi"]));
          tiempoAnteriorApi = millis();
        }

        // Fresas
        if ((int)tiempoRiego != (int)jsonBuffer["tiempo"])
        {
          tiempoRiego = (int)jsonBuffer["tiempo"]; // En milisegundos
          tiempoAnterior = millis();
        }

        if ((int)tiempoDescanso != (int)jsonBuffer["desc"])
        {
          tiempoDescanso = (int)jsonBuffer["desc"]; // En milisegundos
          tiempoAnterior = millis();
        }

        // Tomates
        if ((int)tiempoRiegoTomates != (int)jsonBuffer["tiempoT"])
        {
          tiempoRiegoTomates = (int)jsonBuffer["tiempoT"]; // En milisegundos
          tiempoAnteriorTomates = millis();
        }

        if ((int)tiempoDescansoTomates != (int)jsonBuffer["descT"])
        {
          tiempoDescansoTomates = (int)jsonBuffer["descT"]; // En milisegundos
          tiempoAnteriorTomates = millis();
        }

        Serial.println("** Tiempos json " + String((int)jsonBuffer["tiempo"]) + " - " + String((int)jsonBuffer["desc"]));
        Serial.println("** Tiempos json T " + String((int)jsonBuffer["tiempoT"]) + " - " + String((int)jsonBuffer["descT"]));
        // verificar si hay pausa
        if ((int)jsonBuffer["pausa"] == 1)
        {
          pausa = true;
        }
        else
        {
          pausa = false;
        }

        // Verificar si hay cambio de pines

        // FALTA IMPLEMENTAR EL CAMBIO DE PINES Y PAUSA PARA LOS TOMATES
        int cuentaJson = jsonBuffer["count"];
        cambioPines = false;
        if (cuenta != cuentaJson && cuentaJson > 0)
        {
          cambioPines = true;
          Serial.println("Hay un cambio de cantidad de pines");
        }
        else // Es la misma cantidad de pines, pero verificar si cambia algún pin
        {
          int pinTmp = 0;
          for (int i = 0; i < cuenta; i++)
          {
            pinTmp = jsonBuffer["pines"][i];
            if (pinTmp - 1 != pinesHabilitados[i])
            {
              if (String(pinTmp) == "0")
              {
                i = cuenta;          // Salir del loop...
                cambioPines = false; // y sin cambiar de pines
              }
              else
              {
                Serial.println("Hay un cambio en un pin " + String(pinTmp));
                cambioPines = true;
              }
            }
          }
        }

        if (cambioPines)
        {
          int cuentaAnteriorBkp = cuenta;
          cuenta = cuentaJson;
          Serial.println("................. Se detectó un cambio de pines .................");

          int pinTmp = 0;
          for (int i = 0; i < cuenta; i++)
          {
            if (jsonBuffer["pines"][i] == 0) // IMPORTANTE VERIFICAR QUE NINGUNO TENGA VALOR 0
            {
              i = cuenta;
              cambioPines = false;
              cuenta = cuentaAnteriorBkp;
            }
            else
            {
              pinTmp = jsonBuffer["pines"][i];
              pinesHabilitados[i] = pinTmp - 1;
              actual = 0;
            }
          }
          apagarPines();
          tiempoAnterior = tiempoAnterior - tiempoRiego - tiempoDescanso;
          descanso = false;
          cambioPines = false;
        }
      }
    }
  }

  http.end();
}

void enviarPost(int val, int tipo)
{

  String data;

  if (tipo == 1)
  {
    data = String("rele=") + val;
    http.begin(wifiClient, serverName + "save.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int cod_respuesta = http.POST(data);

    // Websocket:
    // (val == 0) ? descansoSocket = true : descansoSocket = false;

    if (cod_respuesta > 0)
    {
      String cuerpo_respuesta = http.getString();
      if (cod_respuesta != 200)
      {
        Serial.print("El serv respondió con error ");
        Serial.println(cuerpo_respuesta);
      }
    }
  }

  if (tipo == 2)
  { // riego
    data = "tiempo=" + String(val * 1000);
    http.begin(wifiClient, serverName + "grabartiempo.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // int cod_respuesta = http.POST(data);
  }

  if (tipo == 3)
  {                                          // descanso
    data = "descanso=" + String(val * 1000); /// <---------------------
    http.begin(wifiClient, serverName + "grabardescanso.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // int cod_respuesta = http.POST(data);
  }

  // tomates

  if (tipo == 5)
  {
    data = String("rele=") + val;
    http.begin(wifiClient, serverName + "save_tomates.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int cod_respuesta = http.POST(data);

    // Websocket:
    // (val == 0) ? descansoSocket = true : descansoSocket = false;

    if (cod_respuesta > 0)
    {
      String cuerpo_respuesta = http.getString();
      if (cod_respuesta != 200)
      {
        Serial.print("El serv respondió con error ");
        Serial.println(cuerpo_respuesta);
      }
    }
  }
}

void enviarPostTorres()
{
  String data;
  data = String("torres=") + pinesStringPost;
  http.begin(wifiClient, serverName + "grabartorres.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int cod_respuesta = http.POST(data);

  if (cod_respuesta > 0)
  {
    String cuerpo_respuesta = http.getString();
    if (cod_respuesta != 200)
    {
      Serial.print("El serv respondió con error ");
      Serial.println(cuerpo_respuesta);
    }
  }
}

void iniciarPines()
{
  for (int p = 0; p < 4; p++)
  {
    pinMode(pinesDisponibles[p], OUTPUT);
    digitalWrite(pinesDisponibles[p], HIGH);
  }

  if (pinTomates > 0)
  {
    pinMode(pinTomates, OUTPUT);
    digitalWrite(pinTomates, HIGH);
  }
}

void apagarPines()
{
  for (int p = 0; p < 5; p++)
  {
    digitalWrite(p, HIGH);
  }
}

void apagarPinesTomates()
{
  digitalWrite(pinTomates, HIGH);
}