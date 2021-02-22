#include <SimpleDS3231.h>

/*
 * Create SimpleDS3231 object.
 */
SimpleDS3231 rtc;

int last_time;

void setup()
{
  Serial.begin(9600);

  /*
   * Initiate SimpleDS3231 object.
   */
  rtc.begin();
}

void loop()
{
  /*
   * Print time and date every second.
   */
   Serial.println(rtc.get_time_str());
   Serial.println(rtc.get_date_str());
   while(millis() - last_time < 1000) {}
   last_time = millis();
}