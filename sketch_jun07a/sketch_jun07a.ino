#define SW_VER    "1.0.150722"

#include <ESP8266WiFi.h>
#include <ESPSendMail.h>


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
 
/*
 * Werte sollten nicht geändert werden.
 */
#define TRIGGER_PIN     16
#define ECHO_PIN        5
#define SENSOR_OFFSET   1

typedef enum enumWLAN_STATUS_CODES
{
  WLAN_IDLE_STATUS,
  WLAN_NO_SSID_AVAILABLE,
  WLAN_SCAN_COMPLETED,
  WLAN_CONNECTED,
  WLAN_CONNECT_FAILD,
  WLAN_CONNECTION_LOST,
  WLAN_DISCONNECTED,
};

/*
 *  W-LAN Konfiguration
 */
 #ifndef STASSID
 #define STASSID      "J.H - Elec(C) 2,4GHz"
 #define STAPSK       ""
 #endif 

/*
 * E-Mail Server Konfiguration
 */
#define MAILSERVER    "smtp.strato.de"
#define MAILUSERNAME  "no-replay@jh-elec.de"
#define MAILUSERPWD   ""
#define MAILTO        "Matthias_Birke@web.de"

/*
 * Diese Werte müssen an den entsprechenden Tank angepasst werden..
 * -> HEIGHT_TANK:= Höhe des Tankes
 * -> DEVIATION:= Höhe von Füllinhalt max. bis  zu dem Sensor..
 */
#define HEIGHT_TANK     30
#define DEVIATION       7 


#define GET_PERCENT(VALUE) ( 100 - ( ( ( VALUE-DEVIATION ) * 100 ) / HEIGHT_TANK ))

WiFiClientSecure client;
WiFiServer server(80);

typedef struct
{
  uint8_t bNoError :1;
}ErrorCode_t;

typedef struct
{
  long lAVG;
  long lDistance;
  int8_t uiPercent;
  
}HC04_TypeDef;

HC04_TypeDef WaterSense;

char cBuffer[100] = "";

void setup() 
{
  WaterSense.lAVG = 0;
  WaterSense.lDistance = 0;
  WaterSense.uiPercent = 0;
  
  Serial.begin(9600); 

  delay( 500 );

  Serial.println( "W-LAN Netzwerk(e) in der Naehe werden gesucht.." );

  uint8_t uiWIFIn = WiFi.scanNetworks();
  Serial.print( "W-LAN Status: " );
  Serial.println( WiFi.status() );
  Serial.print( "Es wurde(n) " );
  Serial.print( uiWIFIn );
  Serial.println( " W-LAN Netzwerk(e) in der Naehe gefunden!." );
  Serial.println( "" );

  Serial.println("Verbindung zum W-LAN herstellen.." );
  WiFi.begin( STASSID, STAPSK );
  
  while( WiFi.status() != WL_CONNECTED )
  {
    delay(1000);
    Serial.print( "." );
  }
  Serial.println( "" );

  Serial.print( "W-LAN Status: " );
  Serial.println( WiFi.status() );
  Serial.println( "" );
  Serial.println( "W-LAN verbunden!." );
  Serial.print( "IP Addresse: " );
  Serial.println( WiFi.localIP() );
  
  Serial.println( "" );
  server.begin();
  Serial.println( "Webserver wurde gestartet.." );
  
  pinMode(TRIGGER_PIN, OUTPUT); 
  pinMode(ECHO_PIN, INPUT); 

  SendMailMessage( "Der Kontroller wurde gestartet!. \r\nSobald der Behaelterinhalt die unterste Schwelle erreicht hat wird eine Meldung via. E-Mail versendet." );

}

void loop() 
{  
  static uint8_t uiAVGCnt = 0;
  static bool bAVGRdy = false;
  static bool bMailSended = false;
  

  if ( uiAVGCnt >= 50 ) 
  {
    WaterSense.lAVG /= uiAVGCnt;
    WaterSense.lDistance = WaterSense.lAVG;
    WaterSense.uiPercent = GET_PERCENT( WaterSense.lDistance );
    uiAVGCnt = 0;
    bAVGRdy = true;
  }
  else
  {
    WaterSense.lAVG += GetDistance();
    uiAVGCnt++;
    delay(200);
  }

  if ( bAVGRdy )
  {
    bAVGRdy = false;
       
    if ( WaterSense.lDistance >= DEVIATION ) 
    {
      Serial.println ( "********************************" );
      Serial.print( "Distanz: " );
      Serial.println( WaterSense.lDistance );
    
      Serial.print( "Füllstand in %: " );
      Serial.println( WaterSense.uiPercent );
    
      Serial.print( "W-LAN Status: " );
      Serial.println( WiFi.status() );

      Serial.print( "IP - Adresse: " );
      Serial.println( WiFi.localIP() );
      Serial.println ( "********************************" );     

      if ( WaterSense.uiPercent < 30 )
      {
        if ( !bMailSended )
        {
          bMailSended = true;
          SendWaterLevelViaMail( WaterSense.uiPercent );
        }
      }
    }
    else
    {
      Serial.println ( "********************************" );
      Serial.println( "Es ist ein Fehler aufgetreten.. " );
      Serial.println( "Die gemessene Höhe stimmt nicht mit den Parametern überein.." );
      Serial.print( "gemessen wurden: " );
      Serial.print( WaterSense.lDistance );
      Serial.println( "cm" );
      Serial.println ( "********************************" );
    }    
  }

  WiFiClient client = server.available();

  if (client)
  {
    Serial.println("\n[Client hat sich verbunden]");
    while (client.connected())
    {
      if (client.available())
      {
        String line = client.readStringUntil('\r');
        Serial.print(line);

        if (line.length() == 1 && line[0] == '\n')
        {
          client.println(prepareHtmlPage());
          break;
        }
      }
    }
    delay(1000); 
    
    client.stop();
  }
  
}


long GetDistance()
{
  long lDistance = 0;
  long lLength = 0;
  
  digitalWrite(TRIGGER_PIN, LOW);
  delay(5); 
  digitalWrite(TRIGGER_PIN, HIGH); 
  delay(10); 
  digitalWrite(TRIGGER_PIN, LOW);

  lLength = pulseIn(ECHO_PIN, HIGH);
  lDistance = ( lLength / 2 ) * 0.03432; 

  return ( lDistance - SENSOR_OFFSET );
}

void SendWaterLevelViaMail( int8_t uiPercent )
{
  ESPSendMail SendMail(MAILSERVER,MAILUSERNAME,MAILUSERPWD, &client);
  SendMail.From = "no-replay@jh-elec.de";
  //SendMail.DisplayFrom = "Füllstand";
  SendMail.To = MAILTO; 
  SendMail.Subject = "Fuellstand ueberpruefen!";
  SendMail.ClearMessage();

  char *pBuffer = cBuffer;
  char cPercent[] = "100\0";
  
  itoa( uiPercent , cPercent, 10 );

  strcpy( cBuffer, "Der Behälterinhalt beträgt " );
  strcat( cBuffer, cPercent );
  strcat( cBuffer, "% !." );

  SendMail.AddMessageLine( pBuffer );
  SendMail.AddMessageLine( "" ); 
  
  SendMail.AddMessageLine( "Diese Nachricht wird erst wieder nach einem Reset oder Behälterwechsel gesendet!." );
  SendMail.AddMessageLine( "" );

  String sIP = WiFi.localIP().toString();
  char cIP[] = "111.111.111.1\0";
  sIP.toCharArray(cIP,14);

  strcpy( cBuffer, "Alternativ kann der Behälterstand im Browser unter " );
  strcat( cBuffer, cIP );
  strcat( cBuffer, " eingesehen werden." );
  SendMail.AddMessageLine( cBuffer );
  SendMail.AddMessageLine( "" );
  SendMail.AddMessageLine( "_________________________" );
  SendMail.AddMessageLine( "powerd by J.H. - Elec.");
  SendMail.AddMessageLine( "E-Mail: J.Homann@jh-elec.de" );
  SendMail.AddMessageLine( "Software Version: " + (String)SW_VER );
  SendMail.Send();
}

void SendMailMessage( char *pMsg ) 
{
  ESPSendMail SendMail(MAILSERVER,MAILUSERNAME,MAILUSERPWD, &client);
  SendMail.From = "no-replay@jh-elec.de";
  //SendMail.DisplayFrom = "Fuellstand"; 
  SendMail.To = MAILTO; 
  SendMail.Subject = "Status";
  SendMail.ClearMessage();

  SendMail.AddMessageLine( pMsg );
  SendMail.AddMessageLine( "" );
  SendMail.AddMessageLine( "_________________________" );
  SendMail.AddMessageLine( "powerd by J.H. - Elec.");
  SendMail.AddMessageLine( "E-Mail: J.Homann@jh-elec.de" );
  SendMail.AddMessageLine( "Software Version: " + (String)SW_VER );
  SendMail.Send();  
}

String prepareHtmlPage()
{
  String htmlPage =
     String("HTTP/1.1 200 OK\r\n") +
            "Content-Type: text/html\r\n" +
            "Connection: close\r\n" +  // Die Verbindung wird nach der Übertragung geschlossen
            "Refresh: 5\r\n" +  // Automatisch alle 5 Sekunden neu laden
            "\r\n" +
            "<!DOCTYPE HTML>" +
            "<html>" +
              "Aktueller Behaelterinhalt in %: " + String(WaterSense.uiPercent) + 
              "<br/>" +
              "_________________________" + 
              "<br/>" +
              "powerd by J.H. - Elec." + 
              "<br/>" +
              "E-Mail: J.Homann@jh-elec.de"
              "<br/>" +
              "Software Version: " + (String)SW_VER +
            "</html>" +
            "\r\n";
  return htmlPage;
}


ErrorCode_t HandleNewCnfg()
{
  ErrorCode_t Error = 
  {
    .bNoError = 1, 
  };

  
  return Error;
}
